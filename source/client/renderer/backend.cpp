#include <algorithm> // std::stable_sort
#include <new>

#include "glad/glad.h"

#include "tracy/TracyOpenGL.hpp"

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "client/renderer/renderer.h"

#include "cgame/cg_local.h"

template< typename S, typename T >
struct SameType { static constexpr bool value = false; };
template< typename T >
struct SameType< T, T > { static constexpr bool value = true; };

STATIC_ASSERT( ( SameType< u32, GLuint >::value ) );

enum VertexAttribute : GLuint {
	VertexAttribute_Position,
	VertexAttribute_Normal,
	VertexAttribute_TexCoord,
	VertexAttribute_Color,
	VertexAttribute_JointIndices,
	VertexAttribute_JointWeights,

	VertexAttribute_ParticlePosition,
	VertexAttribute_ParticleVelocity,
	VertexAttribute_ParticleAccelDragRest,
	VertexAttribute_ParticleUVWH,
	VertexAttribute_ParticleStartColor,
	VertexAttribute_ParticleEndColor,
	VertexAttribute_ParticleSize,
	VertexAttribute_ParticleAgeLifetime,
	VertexAttribute_ParticleFlags,
};

static const u32 UNIFORM_BUFFER_SIZE = 64 * 1024;

struct DrawCall {
	PipelineState pipeline;
	Mesh mesh;
	u32 num_vertices;
	u32 index_offset;

	u32 num_instances;
	VertexBuffer instance_data;
	VertexBuffer update_data;
	VertexBuffer feedback_data;
};

static NonRAIIDynamicArray< RenderPass > render_passes;
static NonRAIIDynamicArray< DrawCall > draw_calls;
static NonRAIIDynamicArray< Mesh > deferred_mesh_deletes;
static NonRAIIDynamicArray< TextureBuffer > deferred_tb_deletes;

#if TRACY_ENABLE
alignas( tracy::GpuCtxScope ) static char renderpass_zone_memory[ sizeof( tracy::GpuCtxScope ) ];
static tracy::GpuCtxScope * renderpass_zone;
#endif

static u32 num_vertices_this_frame;

static bool in_frame;

struct UBO {
	GLuint ubo;
	u8 * buffer;
	u32 bytes_used;
};

static UBO ubos[ 16 ]; // 1MB of uniform space
static u32 ubo_offset_alignment;

static float max_anisotropic_filtering;

static PipelineState prev_pipeline;
static GLuint prev_fbo;
static u32 prev_viewport_width;
static u32 prev_viewport_height;

static struct {
	UniformBlock uniforms[ ARRAY_COUNT( &Shader::uniforms ) ] = { };
	const Texture * textures[ ARRAY_COUNT( &Shader::textures ) ] = { };
	TextureBuffer texture_buffers[ ARRAY_COUNT( &Shader::texture_buffers ) ] = { };
	TextureArray texture_arrays[ ARRAY_COUNT( &Shader::texture_arrays ) ] = { };
} prev_bindings;

static GLenum DepthFuncToGL( DepthFunc depth_func ) {
	switch( depth_func ) {
		case DepthFunc_Less:
			return GL_LESS;
		case DepthFunc_Equal:
			return GL_EQUAL;
		case DepthFunc_Always:
			return GL_ALWAYS;
		case DepthFunc_Disabled:
			return GL_ALWAYS;
	}

	assert( false );
	return GL_INVALID_ENUM;
}

static GLenum PrimitiveTypeToGL( PrimitiveType primitive_type ) {
	switch( primitive_type ) {
		case PrimitiveType_Triangles:
			return GL_TRIANGLES;
		case PrimitiveType_TriangleStrip:
			return GL_TRIANGLE_STRIP;
		case PrimitiveType_Points:
			return GL_POINTS;
		case PrimitiveType_Lines:
			return GL_LINES;
	}

	assert( false );
	return GL_INVALID_ENUM;
}

static void TextureFormatToGL( TextureFormat format, GLenum * internal, GLenum * channels, GLenum * type ) {
	switch( format ) {
		case TextureFormat_R_U8:
		case TextureFormat_R_S8:
		case TextureFormat_A_U8:
			*internal = format == TextureFormat_R_S8 ? GL_R8_SNORM : GL_R8;
			*channels = GL_RED;
			*type = GL_UNSIGNED_BYTE;
			return;
		case TextureFormat_R_U16:
			*internal = GL_R16;
			*channels = GL_RED;
			*type = GL_UNSIGNED_SHORT;
			return;

		case TextureFormat_RA_U8:
			*internal = GL_RG8;
			*channels = GL_RG;
			*type = GL_UNSIGNED_BYTE;
			return;
		case TextureFormat_RG_Half:
			*internal = GL_RG16F;
			*channels = GL_RG;
			*type = GL_HALF_FLOAT;
			return;

		case TextureFormat_RGB_U8:
		case TextureFormat_RGB_U8_sRGB:
			*internal = format == TextureFormat_RGB_U8 ? GL_RGB8 : GL_SRGB8;
			*channels = GL_RGB;
			*type = GL_UNSIGNED_BYTE;
			return;
		case TextureFormat_RGB_Half:
			*internal = GL_RGB16F;
			*channels = GL_RGB;
			*type = GL_HALF_FLOAT;
			return;

		case TextureFormat_RGBA_U8:
		case TextureFormat_RGBA_U8_sRGB:
			*internal = format == TextureFormat_RGBA_U8 ? GL_RGBA8 : GL_SRGB8_ALPHA8;
			*channels = GL_RGBA;
			*type = GL_UNSIGNED_BYTE;
			return;

		case TextureFormat_BC1_sRGB:
			*internal = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
			return;

		case TextureFormat_BC3_sRGB:
			*internal = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
			return;

		case TextureFormat_BC4:
			*internal = GL_COMPRESSED_RED_RGTC1;
			return;

		case TextureFormat_BC5:
			*internal = GL_COMPRESSED_RG_RGTC2;
			return;

		case TextureFormat_Depth:
		case TextureFormat_Shadow:
			*internal = GL_DEPTH_COMPONENT24;
			*channels = GL_DEPTH_COMPONENT;
			*type = GL_FLOAT;
			return;
	}

	assert( false );
}

static GLenum TextureWrapToGL( TextureWrap wrap ) {
	switch( wrap ) {
		case TextureWrap_Repeat:
			return GL_REPEAT;
		case TextureWrap_Clamp:
			return GL_CLAMP_TO_EDGE;
		case TextureWrap_Mirror:
			return GL_MIRRORED_REPEAT;
		case TextureWrap_Border:
			return GL_CLAMP_TO_BORDER;
	}

	assert( false );
	return GL_INVALID_ENUM;
}

static void TextureFilterToGL( TextureFilter filter, GLenum * min, GLenum * mag ) {
	switch( filter ) {
		case TextureFilter_Linear:
			*min = GL_LINEAR_MIPMAP_LINEAR;
			*mag = GL_LINEAR;
			return;

		case TextureFilter_Point:
			*min = GL_NEAREST_MIPMAP_NEAREST;
			*mag = GL_NEAREST;
			return;
	}

	assert( false );
}

static void VertexFormatToGL( VertexFormat format, GLenum * type, int * num_components, bool * integral, GLboolean * normalized ) {
	*integral = false;
	*normalized = false;

	switch( format ) {
		case VertexFormat_U8x2:
		case VertexFormat_U8x2_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 2;
			*integral = true;
			*normalized = format == VertexFormat_U8x2_Norm;
			return;
		case VertexFormat_U8x3:
		case VertexFormat_U8x3_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 3;
			*integral = true;
			*normalized = format == VertexFormat_U8x3_Norm;
			return;
		case VertexFormat_U8x4:
		case VertexFormat_U8x4_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 4;
			*integral = true;
			*normalized = format == VertexFormat_U8x4_Norm;
			return;

		case VertexFormat_U16x2:
		case VertexFormat_U16x2_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 2;
			*integral = true;
			*normalized = format == VertexFormat_U16x2_Norm;
			return;
		case VertexFormat_U16x3:
		case VertexFormat_U16x3_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 3;
			*integral = true;
			*normalized = format == VertexFormat_U16x3_Norm;
			return;
		case VertexFormat_U16x4:
		case VertexFormat_U16x4_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 4;
			*integral = true;
			*normalized = format == VertexFormat_U16x4_Norm;
			return;

		case VertexFormat_U32x1:
			*type = GL_UNSIGNED_INT;
			*num_components = 1;
			*integral = true;
			return;

		case VertexFormat_Floatx2:
			*type = GL_FLOAT;
			*num_components = 2;
			return;
		case VertexFormat_Floatx3:
			*type = GL_FLOAT;
			*num_components = 3;
			return;
		case VertexFormat_Floatx4:
			*type = GL_FLOAT;
			*num_components = 4;
			return;
	}

	assert( false );
}

static void TextureBufferFormatToGL( TextureBufferFormat format, GLenum * internal_format, u32 * element_size ) {
	switch( format ) {
		case TextureBufferFormat_U8x2:
			*internal_format = GL_RG8UI;
			*element_size = 2 * sizeof( u8 );
			return;
		case TextureBufferFormat_U8x4:
			*internal_format = GL_RGBA8;
			*element_size = 4 * sizeof( u8 );
			return;
		case TextureBufferFormat_U32:
			*internal_format = GL_R32UI;
			*element_size = sizeof( u32 );
			return;
		case TextureBufferFormat_U32x2:
			*internal_format = GL_RG32UI;
			*element_size = 2 * sizeof( u32 );
			return;
		case TextureBufferFormat_S32x2:
			*internal_format = GL_RG32I;
			*element_size = 2 * sizeof( s32 );
			return;
		case TextureBufferFormat_S32x3:
			*internal_format = GL_RGB32I;
			*element_size = 3 * sizeof( s32 );
			return;
		case TextureBufferFormat_Floatx4:
			*internal_format = GL_RGBA32F;
			*element_size = 4 * sizeof( float );
			return;
	}

	assert( false );
}

static const char * DebugTypeString( GLenum type ) {
	switch( type ) {
		case GL_DEBUG_TYPE_ERROR:
		case GL_DEBUG_CATEGORY_API_ERROR_AMD:
			return "error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
			return "deprecated";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
			return "undefined";
		case GL_DEBUG_TYPE_PORTABILITY:
			return "nonportable";
		case GL_DEBUG_TYPE_PERFORMANCE:
		case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
			return "performance";
		case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
			return "window system";
		case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
			return "shader compiler";
		case GL_DEBUG_CATEGORY_APPLICATION_AMD:
			return "application";
		case GL_DEBUG_TYPE_OTHER:
		case GL_DEBUG_CATEGORY_OTHER_AMD:
			return "other";
		default:
			return "idk";
	}
}

static const char * DebugSeverityString( GLenum severity ) {
	switch( severity ) {
		case GL_DEBUG_SEVERITY_LOW:
			// case GL_DEBUG_SEVERITY_LOW_AMD:
			return S_COLOR_GREEN "low" S_COLOR_WHITE;
		case GL_DEBUG_SEVERITY_MEDIUM:
			// case GL_DEBUG_SEVERITY_MEDIUM_AMD:
			return S_COLOR_YELLOW "medium" S_COLOR_WHITE;
		case GL_DEBUG_SEVERITY_HIGH:
			// case GL_DEBUG_SEVERITY_HIGH_AMD:
			return S_COLOR_RED "high" S_COLOR_WHITE;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			return "notice";
		default:
			return "idk";
	}
}

static void DebugOutputCallback(
	GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar * message, const void * _
) {
	if(
	    source == 33352 || // shader compliation errors
	    id == 131169 ||
	    id == 131185 ||
	    id == 131201 || // TBO resized
	    id == 131218 ||
	    id == 131204
	) {
		return;
	}

	if( severity == GL_DEBUG_SEVERITY_NOTIFICATION || severity == GL_DEBUG_SEVERITY_NOTIFICATION_KHR ) {
		return;
	}

	if( type == GL_DEBUG_TYPE_PERFORMANCE ) {
		return;
	}

	Com_Printf( "GL [%s - %s]: %s (id:%u source:%d)", DebugTypeString( type ), DebugSeverityString( severity ), message, id, source );
	size_t len = strlen( message );
	if( len == 0 || message[ len - 1 ] != '\n' )
		Com_Printf( "\n" );

	if( severity == GL_DEBUG_SEVERITY_HIGH ) {
		Fatal( "GL high severity: %s", message );
	}
}

static void DebugOutputCallbackAMD(
	GLuint id, GLenum type, GLenum severity, GLsizei length,
	const GLchar * message, const void * _
) {
	DebugOutputCallback( GL_DONT_CARE, type, id, severity, length, message, _ );
}

static void DebugLabel( GLenum type, GLuint object, const char * label ) {
	assert( label != NULL );
	if( GLAD_GL_KHR_debug != 0 ) {
		glObjectLabel( type, object, -1, label );
	}
}

static void PlotVRAMUsage() {
	if( !is_public_build && GLAD_GL_NVX_gpu_memory_info != 0 ) {
		GLint total_vram;
		glGetIntegerv( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_vram );

		GLint available_vram;
		glGetIntegerv( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available_vram );

		TracyPlot( "VRAM usage", s64( total_vram - available_vram ) );
	}
}

void RenderBackendInit() {
	ZoneScoped;
	TracyGpuContext;

	if( !is_public_build ) {
		if( GLAD_GL_KHR_debug != 0 ) {
			GLint context_flags;
			glGetIntegerv( GL_CONTEXT_FLAGS, &context_flags );
			if( context_flags & GL_CONTEXT_FLAG_DEBUG_BIT ) {
				Com_Printf( "Initialising debug output\n" );

				glEnable( GL_DEBUG_OUTPUT );
				glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
				glDebugMessageCallback( ( GLDEBUGPROC ) DebugOutputCallback, NULL );
				glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE );
			}
		}
		else if( GLAD_GL_AMD_debug_output != 0 ) {
			Com_Printf( "Initialising AMD debug output\n" );

			glDebugMessageCallbackAMD( ( GLDEBUGPROCAMD ) DebugOutputCallbackAMD, NULL );
			glDebugMessageEnableAMD( 0, 0, 0, NULL, GL_TRUE );
		}
	}

	PlotVRAMUsage();

	render_passes.init( sys_allocator );
	draw_calls.init( sys_allocator );
	deferred_mesh_deletes.init( sys_allocator );
	deferred_tb_deletes.init( sys_allocator );

	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );

	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	glDisable( GL_BLEND );

	glEnable( GL_FRAMEBUFFER_SRGB );

	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	GLint alignment;
	glGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment );
	ubo_offset_alignment = checked_cast< u32 >( alignment );

	glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_filtering );

	GLint max_ubo_size;
	glGetIntegerv( GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size );
	assert( max_ubo_size >= s32( UNIFORM_BUFFER_SIZE ) );

	for( UBO & ubo : ubos ) {
		glGenBuffers( 1, &ubo.ubo );
		glBindBuffer( GL_UNIFORM_BUFFER, ubo.ubo );
		glBufferData( GL_UNIFORM_BUFFER, UNIFORM_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW );
	}

	in_frame = false;

	prev_pipeline = PipelineState();
	prev_fbo = 0;
	prev_viewport_width = 0;
	prev_viewport_height = 0;
}

void RenderBackendShutdown() {
	for( UBO ubo : ubos ) {
		glDeleteBuffers( 1, &ubo.ubo );
	}

	render_passes.shutdown();
	draw_calls.shutdown();
	deferred_mesh_deletes.shutdown();
	deferred_tb_deletes.shutdown();
}

void RenderBackendBeginFrame() {
	assert( !in_frame );
	in_frame = true;

	render_passes.clear();
	draw_calls.clear();
	deferred_mesh_deletes.clear();
	deferred_tb_deletes.clear();

	num_vertices_this_frame = 0;

	for( UBO & ubo : ubos ) {
		glBindBuffer( GL_UNIFORM_BUFFER, ubo.ubo );
		ubo.buffer = ( u8 * ) glMapBufferRange( GL_UNIFORM_BUFFER, 0, UNIFORM_BUFFER_SIZE, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );
		assert( ubo.buffer != NULL );
		ubo.bytes_used = 0;
	}

	if( frame_static.viewport_width != prev_viewport_width || frame_static.viewport_height != prev_viewport_height ) {
		prev_viewport_width = frame_static.viewport_width;
		prev_viewport_height = frame_static.viewport_height;
		glViewport( 0, 0, frame_static.viewport_width, frame_static.viewport_height );
	}

	PlotVRAMUsage();
}

static bool operator!=( PipelineState::Scissor a, PipelineState::Scissor b ) {
	return a.x != b.x || a.y != b.y || a.w != b.w || a.h != b.h;
}

static void SetPipelineState( PipelineState pipeline, bool ccw_winding ) {
	TracyGpuZone( "Set pipeline state" );

	if( pipeline.shader != NULL && ( prev_pipeline.shader == NULL || pipeline.shader->program != prev_pipeline.shader->program ) ) {
		glUseProgram( pipeline.shader->program );
	}

	// uniforms
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->uniforms ); i++ ) {
		u64 name_hash = pipeline.shader->uniforms[ i ];
		UniformBlock prev_block = prev_bindings.uniforms[ i ];

		bool found = prev_block.size == 0;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_uniforms; j++ ) {
				if( pipeline.uniforms[ j ].name_hash == name_hash ) {
					UniformBlock block = pipeline.uniforms[ j ].block;
					if( block.offset != prev_block.offset || block.size != prev_block.size || block.ubo != prev_block.ubo ) {
						glBindBufferRange( GL_UNIFORM_BUFFER, i, block.ubo, block.offset, block.size );
						prev_bindings.uniforms[ i ] = block;
					}
					found = true;
					break;
				}
			}
		}

		if( !found ) {
			glBindBufferBase( GL_UNIFORM_BUFFER, i, 0 );
			prev_bindings.uniforms[ i ] = { };
		}
	}

	// textures
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->textures ); i++ ) {
		u64 name_hash = pipeline.shader->textures[ i ];
		GLenum tex_unit = GL_TEXTURE0 + i;
		const Texture * prev_texture = prev_bindings.textures[ i ];

		bool found = prev_texture == NULL;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_textures; j++ ) {
				if( pipeline.textures[ j ].name_hash == name_hash ) {
					const Texture * texture = pipeline.textures[ j ].texture;
					GLenum target = texture->msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
					GLenum other_target = texture->msaa ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
					if( texture != prev_texture ) {
						glActiveTexture( tex_unit );
						glBindTexture( target, texture->texture );
						if( prev_texture != NULL && texture->msaa != prev_texture->msaa ) {
							glBindTexture( other_target, 0 );
						}
						prev_bindings.textures[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found ) {
			glActiveTexture( tex_unit );
			glBindTexture( GL_TEXTURE_2D, 0 );
			glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
			prev_bindings.textures[ i ] = { };
		}
	}

	// texture buffers
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->texture_buffers ); i++ ) {
		u64 name_hash = pipeline.shader->texture_buffers[ i ];
		GLenum tex_unit = GL_TEXTURE0 + ARRAY_COUNT( pipeline.shader->textures ) + i;
		TextureBuffer prev_texture = prev_bindings.texture_buffers[ i ];
		
		bool found = prev_texture.texture == 0;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_texture_buffers; j++ ) {
				if( pipeline.texture_buffers[ j ].name_hash == name_hash ) {
					TextureBuffer texture = pipeline.texture_buffers[ j ].tb;
					if( texture.texture != prev_texture.texture ) {
						glActiveTexture( tex_unit );
						glBindTexture( GL_TEXTURE_BUFFER, texture.texture );
						prev_bindings.texture_buffers[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found ) {
			glActiveTexture( tex_unit );
			glBindTexture( GL_TEXTURE_BUFFER, 0 );
			prev_bindings.texture_buffers[ i ] = { };
		}
	}

	// texture arrays
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->texture_arrays ); i++ ) {
		u64 name_hash = pipeline.shader->texture_arrays[ i ];
		GLenum tex_unit = GL_TEXTURE0 + ARRAY_COUNT( pipeline.shader->textures ) + ARRAY_COUNT( pipeline.shader->texture_buffers ) + i;
		TextureArray prev_texture = prev_bindings.texture_arrays[ i ];
		
		bool found = prev_texture.texture == 0;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_texture_arrays; j++ ) {
				if( pipeline.texture_arrays[ j ].name_hash == name_hash ) {
					TextureArray texture = pipeline.texture_arrays[ j ].ta;
					if( texture.texture != prev_texture.texture ) {
						glActiveTexture( tex_unit );
						glBindTexture( GL_TEXTURE_2D_ARRAY, texture.texture );
						prev_bindings.texture_arrays[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found ) {
			glActiveTexture( tex_unit );
			glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
			prev_bindings.texture_arrays[ i ] = { };
		}
	}

	// alpha blending
	if( pipeline.blend_func != prev_pipeline.blend_func ) {
		if( pipeline.blend_func == BlendFunc_Disabled ) {
			glDisable( GL_BLEND );
		}
		else {
			if( prev_pipeline.blend_func == BlendFunc_Disabled ) {
				glEnable( GL_BLEND );
			}
			if( pipeline.blend_func == BlendFunc_Blend ) {
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			}
			else {
				glBlendFunc( GL_SRC_ALPHA, GL_ONE );
			}
		}
	}

	// depth testing
	if( pipeline.depth_func != prev_pipeline.depth_func ) {
		if( pipeline.depth_func == DepthFunc_Disabled ) {
			glDisable( GL_DEPTH_TEST );
		}
		else {
			if( prev_pipeline.depth_func == DepthFunc_Disabled ) {
				glEnable( GL_DEPTH_TEST );
			}
			glDepthFunc( DepthFuncToGL( pipeline.depth_func ) );
		}
	}

	// backface culling
	if( pipeline.cull_face != CullFace_Disabled && !ccw_winding ) {
		pipeline.cull_face = pipeline.cull_face == CullFace_Front ? CullFace_Back : CullFace_Front;
	}

	if( pipeline.cull_face != prev_pipeline.cull_face ) {
		if( pipeline.cull_face == CullFace_Disabled ) {
			glDisable( GL_CULL_FACE );
		}
		else {
			if( prev_pipeline.cull_face == CullFace_Disabled ) {
				glEnable( GL_CULL_FACE );
			}
			glCullFace( pipeline.cull_face == CullFace_Front ? GL_FRONT : GL_BACK );
		}
	}

	// scissor
	if( pipeline.scissor != prev_pipeline.scissor ) {
		PipelineState::Scissor s = pipeline.scissor;
		if( s.x == 0 && s.y == 0 && s.w == 0 && s.h == 0 ) {
			glDisable( GL_SCISSOR_TEST );
		}
		else {
			PipelineState::Scissor old = prev_pipeline.scissor;
			if( old.x == 0 && old.y == 0 && old.w == 0 && old.h == 0 ) {
				glEnable( GL_SCISSOR_TEST );
			}
			glScissor( s.x, frame_static.viewport_height - s.y - s.h, s.w, s.h );
		}
	}

	// depth writing
	if( pipeline.write_depth != prev_pipeline.write_depth ) {
		glDepthMask( pipeline.write_depth ? GL_TRUE : GL_FALSE );
	}

	// depth clamping
	if( pipeline.clamp_depth != prev_pipeline.clamp_depth ) {
		if( pipeline.clamp_depth ) {
			glEnable( GL_DEPTH_CLAMP );
		} else {
			glDisable( GL_DEPTH_CLAMP );
		}
	}

	// view weapon depth hack
	if( pipeline.view_weapon_depth_hack != prev_pipeline.view_weapon_depth_hack ) {
		float far = pipeline.view_weapon_depth_hack ? 0.3f : 1.0f;
		glDepthRange( 0.0f, far );
	}

	// polygon fill mode
	if( pipeline.wireframe != prev_pipeline.wireframe ) {
		if( pipeline.wireframe ) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			glEnable( GL_POLYGON_OFFSET_LINE );
			glPolygonOffset( -1, -1 );
		}
		else {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glDisable( GL_POLYGON_OFFSET_LINE );
		}
	}

	prev_pipeline = pipeline;
}

static bool SortDrawCall( const DrawCall & a, const DrawCall & b ) {
	if( a.pipeline.pass != b.pipeline.pass )
		return a.pipeline.pass < b.pipeline.pass;
	if( !render_passes[ a.pipeline.pass ].sorted )
		return false;
	return a.pipeline.shader < b.pipeline.shader;
}

static void SetupAttribute( GLuint index, VertexFormat format, u32 stride = 0, u32 offset = 0 ) {
	const GLvoid * gl_offset = checked_cast< const GLvoid * >( checked_cast< uintptr_t >( offset ) );

	GLenum type;
	int num_components;
	bool integral;
	GLboolean normalized;
	VertexFormatToGL( format, &type, &num_components, &integral, &normalized );

	glEnableVertexAttribArray( index );
	if( integral && !normalized )
		glVertexAttribIPointer( index, num_components, type, stride, gl_offset );
	else
		glVertexAttribPointer( index, num_components, type, normalized, stride, gl_offset );
}

static void SubmitFramebufferBlit( const RenderPass & pass ) {
	Framebuffer src = pass.blit_source;
	Framebuffer target = pass.target;
	glBindFramebuffer( GL_READ_FRAMEBUFFER, src.fbo );
	GLbitfield clear_mask = 0;
	clear_mask |= pass.clear_color ? GL_COLOR_BUFFER_BIT : 0;
	clear_mask |= pass.clear_depth ? GL_DEPTH_BUFFER_BIT : 0;
	glBlitFramebuffer( 0, 0, src.width, src.height, 0, 0, target.width, target.height, clear_mask, GL_NEAREST );
}

static void SetupRenderPass( const RenderPass & pass ) {
	ZoneScoped;
	ZoneText( pass.name, strlen( pass.name ) );
#if TRACY_ENABLE
	renderpass_zone = new (renderpass_zone_memory) tracy::GpuCtxScope( pass.tracy );
#endif

	if( GLAD_GL_KHR_debug != 0 ) {
		glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass.name );
	}

	const Framebuffer & fb = pass.target;
	if( fb.fbo != prev_fbo ) {
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fb.fbo );
		prev_fbo = fb.fbo;

		u32 viewport_width = fb.fbo == 0 ? frame_static.viewport_width : fb.width;
		u32 viewport_height = fb.fbo == 0 ? frame_static.viewport_height : fb.height;

		if( viewport_width != prev_viewport_width || viewport_height != prev_viewport_height ) {
			prev_viewport_width = viewport_width;
			prev_viewport_height = viewport_height;
			glViewport( 0, 0, viewport_width, viewport_height );
		}
	}

	if( pass.type == RenderPass_Blit ) {
		SubmitFramebufferBlit( pass );
		return;
	}

	PipelineState::Scissor & scissor = prev_pipeline.scissor;
	if( scissor.x != 0 || scissor.y != 0 || scissor.w != 0 || scissor.h != 0 ) {
		glDisable( GL_SCISSOR_TEST );
		prev_pipeline.scissor = { };
	}

	if( pass.clear_color ) {
		for( u32 i = 0; i < MAX_RENDER_TARGETS; i++ ) {
			if( fb.targets[ i ].texture.texture != 0 ) {
				glClearBufferfv( GL_COLOR, i, fb.targets[ i ].clear_color.ptr() );
			}
		}
	}

	if( pass.clear_depth ) {
		if( !prev_pipeline.write_depth ) {
			glDepthMask( GL_TRUE );
			prev_pipeline.write_depth = true;
		}
		glClearBufferfv( GL_DEPTH, 0, &fb.depth_target.clear_depth );
	}
}

static void FinishRenderPass() {
	if( GLAD_GL_KHR_debug != 0 )
		glPopDebugGroup();

#if TRACY_ENABLE
	renderpass_zone->~GpuCtxScope();
#endif
}

static void SubmitDrawCall( const DrawCall & dc ) {
	ZoneScoped;
	TracyGpuZone( "Draw call" );

	SetPipelineState( dc.pipeline, dc.mesh.ccw_winding );

	glBindVertexArray( dc.mesh.vao );
	GLenum primitive = PrimitiveTypeToGL( dc.mesh.primitive_type );

	if( dc.num_instances != 0 ) {
		glBindBuffer( GL_ARRAY_BUFFER, dc.instance_data.vbo );

		SetupAttribute( VertexAttribute_ParticlePosition, VertexFormat_Floatx4, sizeof( GPUParticle ), offsetof( GPUParticle, position ) );
		SetupAttribute( VertexAttribute_ParticleVelocity, VertexFormat_Floatx4, sizeof( GPUParticle ), offsetof( GPUParticle, velocity ) );
		SetupAttribute( VertexAttribute_ParticleAccelDragRest, VertexFormat_Floatx3, sizeof( GPUParticle ), offsetof( GPUParticle, acceleration ) );
		SetupAttribute( VertexAttribute_ParticleUVWH, VertexFormat_Floatx4, sizeof( GPUParticle ), offsetof( GPUParticle, uvwh ) );
		SetupAttribute( VertexAttribute_ParticleStartColor, VertexFormat_U8x4_Norm, sizeof( GPUParticle ), offsetof( GPUParticle, start_color ) );
		SetupAttribute( VertexAttribute_ParticleEndColor, VertexFormat_U8x4_Norm, sizeof( GPUParticle ), offsetof( GPUParticle, end_color ) );
		SetupAttribute( VertexAttribute_ParticleSize, VertexFormat_Floatx2, sizeof( GPUParticle ), offsetof( GPUParticle, start_size ) );
		SetupAttribute( VertexAttribute_ParticleAgeLifetime, VertexFormat_Floatx2, sizeof( GPUParticle ), offsetof( GPUParticle, age ) );
		SetupAttribute( VertexAttribute_ParticleFlags, VertexFormat_U32x1, sizeof( GPUParticle ), offsetof( GPUParticle, flags ) );

		if( dc.update_data.vbo ) {
			glEnable( GL_RASTERIZER_DISCARD );
			glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, dc.update_data.vbo );
			if( dc.feedback_data.vbo ) {
				glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 1, dc.feedback_data.vbo );
			}

			glBeginTransformFeedback( primitive );
			GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			const void * offset = ( const void * ) uintptr_t( dc.index_offset );
			glDrawElements( primitive, dc.num_instances, type, offset );
			glEndTransformFeedback();

			glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0 );
			if( dc.feedback_data.vbo ) {
				glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 1, 0 );
			}
			glDisable( GL_RASTERIZER_DISCARD );
		}
		else {
			glVertexAttribDivisor( VertexAttribute_ParticlePosition, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleVelocity, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleAccelDragRest, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleUVWH, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleStartColor, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleEndColor, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleSize, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleAgeLifetime, 1 );
			glVertexAttribDivisor( VertexAttribute_ParticleFlags, 1 );
			GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			glDrawElementsInstanced( primitive, dc.num_vertices, type, 0, dc.num_instances );
		}
	}
	else if( dc.mesh.indices.ebo != 0 ) {
		GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		const void * offset = ( const void * ) uintptr_t( dc.index_offset );
		glDrawElements( primitive, dc.num_vertices, type, offset );
	}
	else {
		glDrawArrays( primitive, dc.index_offset, dc.num_vertices );
	}

	glBindVertexArray( 0 );
}

void RenderBackendSubmitFrame() {
	ZoneScoped;

	assert( in_frame );
	assert( render_passes.size() > 0 );
	in_frame = false;

	{
		ZoneScopedN( "Unmap UBOs" );
		for( UBO ubo : ubos ) {
			glBindBuffer( GL_UNIFORM_BUFFER, ubo.ubo );
			glUnmapBuffer( GL_UNIFORM_BUFFER );
		}
	}

	{
		ZoneScopedN( "Sort draw calls" );
		std::stable_sort( draw_calls.begin(), draw_calls.end(), SortDrawCall );
	}

	SetupRenderPass( render_passes[ 0 ] );
	u8 pass_idx = 0;

	{
		ZoneScopedN( "Submit draw calls" );
		for( const DrawCall & dc : draw_calls ) {
			while( dc.pipeline.pass > pass_idx ) {
				FinishRenderPass();
				pass_idx++;
				SetupRenderPass( render_passes[ pass_idx ] );
			}

			SubmitDrawCall( dc );
		}
	}

	FinishRenderPass();

	while( pass_idx < render_passes.size() - 1 ) {
		pass_idx++;
		SetupRenderPass( render_passes[ pass_idx ] );
		FinishRenderPass();
	}

	{
		// OBS captures the game with glBlitFramebuffer which gets
		// nuked by scissor, so turn it off at the end of every frame
		PipelineState no_scissor_test = prev_pipeline;
		no_scissor_test.scissor = { };
		SetPipelineState( no_scissor_test, true );
	}

	{
		ZoneScopedN( "Deferred mesh deletes" );
		for( const Mesh & mesh : deferred_mesh_deletes ) {
			DeleteMesh( mesh );
		}
	}

	{
		ZoneScopedN( "Deferred texturebuffer deletes" );
		for( const TextureBuffer & tb : deferred_tb_deletes ) {
			DeleteTextureBuffer( tb );
		}
	}

	u32 ubo_bytes_used = 0;
	for( const UBO & ubo : ubos ) {
		ubo_bytes_used += ubo.bytes_used;
	}
	TracyPlot( "UBO utilisation", float( ubo_bytes_used ) / float( UNIFORM_BUFFER_SIZE * ARRAY_COUNT( ubos ) ) );

	TracyPlot( "Draw calls", s64( draw_calls.size() ) );
	TracyPlot( "Vertices", s64( num_vertices_this_frame ) );

	TracyGpuCollect;
}

UniformBlock UploadUniforms( const void * data, size_t size ) {
	assert( in_frame );

	UBO * ubo = NULL;
	u32 offset = 0;

	for( size_t i = 0; i < ARRAY_COUNT( ubos ); i++ ) {
		offset = AlignPow2( ubos[ i ].bytes_used, ubo_offset_alignment );
		if( UNIFORM_BUFFER_SIZE - offset >= size ) {
			ubo = &ubos[ i ];
			break;
		}
	}

	if( ubo == NULL )
		Fatal( "Ran out of UBO space" );

	UniformBlock block;
	block.ubo = ubo->ubo;
	block.offset = offset;
	block.size = AlignPow2( checked_cast< u32 >( size ), u32( 16 ) );

	// memset so we don't leave any gaps. good for write combined memory!
	memset( ubo->buffer + ubo->bytes_used, 0, offset - ubo->bytes_used );
	memcpy( ubo->buffer + offset, data, size );
	ubo->bytes_used = offset + size;

	return block;
}

VertexBuffer NewVertexBuffer( const void * data, u32 len ) {
	// glBufferData's length parameter is GLsizeiptr, so we need to make
	// sure len fits in a signed 32bit int
	assert( len < S32_MAX );

	VertexBuffer vb;
	glGenBuffers( 1, &vb.vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vb.vbo );
	glBufferData( GL_ARRAY_BUFFER, len, data, data != NULL ? GL_STATIC_DRAW : GL_STREAM_DRAW );

	return vb;
}

VertexBuffer NewVertexBuffer( u32 len ) {
	return NewVertexBuffer( NULL, len );
}

void WriteVertexBuffer( VertexBuffer vb, const void * data, u32 len, u32 offset ) {
	glBindBuffer( GL_ARRAY_BUFFER, vb.vbo );
	glBufferSubData( GL_ARRAY_BUFFER, offset, len, data );
}

void ReadVertexBuffer( VertexBuffer vb, void * data, u32 len, u32 offset ) {
	glBindBuffer( GL_ARRAY_BUFFER, vb.vbo );
	glGetBufferSubData( GL_ARRAY_BUFFER, offset, len, data );
}

void DeleteVertexBuffer( VertexBuffer vb ) {
	glDeleteBuffers( 1, &vb.vbo );
}

VertexBuffer NewParticleVertexBuffer( u32 n ) {
	// glBufferData's length parameter is GLsizeiptr, so we need to make
	// sure len fits in a signed 32bit int
	u32 len = n * sizeof( GPUParticle );
	assert( len < S32_MAX );

	VertexBuffer vb;
	glGenBuffers( 1, &vb.vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vb.vbo );
	glBufferData( GL_ARRAY_BUFFER, len, NULL, GL_STATIC_DRAW );

	return vb;
}

IndexBuffer NewIndexBuffer( const void * data, u32 len ) {
	assert( len < S32_MAX );

	IndexBuffer ib;
	glGenBuffers( 1, &ib.ebo );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ib.ebo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, len, data, data != NULL ? GL_STATIC_DRAW : GL_STREAM_DRAW );

	return ib;
}

IndexBuffer NewIndexBuffer( u32 len ) {
	return NewIndexBuffer( NULL, len );
}

void WriteIndexBuffer( IndexBuffer ib, const void * data, u32 len, u32 offset ) {
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ib.ebo );
	glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, offset, len, data );
}

void DeleteIndexBuffer( IndexBuffer ib ) {
	glDeleteBuffers( 1, &ib.ebo );
}

TextureBuffer NewTextureBuffer( TextureBufferFormat format, u32 len ) {
	TextureBuffer tb;
	glGenBuffers( 1, &tb.tbo );
	glGenTextures( 1, &tb.texture );

	glBindBuffer( GL_TEXTURE_BUFFER, tb.tbo );
	glBindTexture( GL_TEXTURE_BUFFER, tb.texture );

	GLenum internal_format;
	u32 element_size;
	TextureBufferFormatToGL( format, &internal_format, &element_size );
	glTexBuffer( GL_TEXTURE_BUFFER, internal_format, tb.tbo );

	assert( len * element_size < S32_MAX );
	glBufferData( GL_TEXTURE_BUFFER, len * element_size, NULL, GL_STREAM_DRAW );

	return tb;
}

void WriteTextureBuffer( TextureBuffer tb, const void * data, u32 len ) {
	glBindBuffer( GL_TEXTURE_BUFFER, tb.tbo );
	glBufferSubData( GL_TEXTURE_BUFFER, 0, len, data );
}

void DeleteTextureBuffer( TextureBuffer tb ) {
	glDeleteBuffers( 1, &tb.tbo );
	glDeleteTextures( 1, &tb.texture );
}

void DeferDeleteTextureBuffer( TextureBuffer tb ) {
	deferred_tb_deletes.add( tb );
}

static Texture NewTextureSamples( TextureConfig config, int msaa_samples ) {
	Texture texture = { };
	texture.width = config.width;
	texture.height = config.height;
	texture.num_mipmaps = config.num_mipmaps;
	texture.msaa = msaa_samples > 1;
	texture.format = config.format;

	glGenTextures( 1, &texture.texture );
	// TODO: should probably update the gl state since we bind a new texture
	// TODO: or glactivetexture 0?
	GLenum target = msaa_samples == 0 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
	glBindTexture( target, texture.texture );

	GLenum internal_format, channels, type;
	TextureFormatToGL( config.format, &internal_format, &channels, &type );

	if( msaa_samples == 0 ) {
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TextureWrapToGL( config.wrap ) );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TextureWrapToGL( config.wrap ) );

		GLenum min_filter = GL_NONE;
		GLenum mag_filter = GL_NONE;
		TextureFilterToGL( config.filter, &min_filter, &mag_filter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, config.num_mipmaps - 1 );

		if( config.wrap == TextureWrap_Border ) {
			glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, ( GLfloat * ) &config.border_color );
		}

		if( !CompressedTextureFormat( config.format ) ) {
			assert( config.num_mipmaps == 1 );

			if( channels == GL_RED ) {
				if( config.format == TextureFormat_A_U8 ) {
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED );
				}
				else {
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE );
				}
			}
			else if( channels == GL_RG && config.format == TextureFormat_RA_U8 ) {
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN );
			}

			glTexImage2D( GL_TEXTURE_2D, 0, internal_format,
				config.width, config.height, 0, channels, type, config.data );
		}
		else {
			if( config.format == TextureFormat_BC4 ) {
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED );
			}

			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filtering );

			const char * cursor = ( const char * ) config.data;
			for( u32 i = 0; i < config.num_mipmaps; i++ ) {
				u32 w = config.width >> i;
				u32 h = config.height >> i;
				u32 size = ( BitsPerPixel( config.format ) * w * h ) / 8;
				assert( size < S32_MAX );

				glCompressedTexImage2D( GL_TEXTURE_2D, i, internal_format,
					w, h, 0, size, cursor );

				cursor += size;
			}
		}
	}
	else {
		glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, msaa_samples,
			internal_format, config.width, config.height, GL_TRUE );
	}

	return texture;
}

Texture NewTexture( const TextureConfig & config ) {
	return NewTextureSamples( config, 0 );
}

void DeleteTexture( Texture texture ) {
	if( texture.texture == 0 )
		return;
	glDeleteTextures( 1, &texture.texture );
}

TextureArray NewTextureArray( const TextureArrayConfig & config ) {
	TextureArray ta;

	glGenTextures( 1, &ta.texture );
	glBindTexture( GL_TEXTURE_2D_ARRAY, ta.texture );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.num_mipmaps - 1 );

	GLenum internal_format, channels, type;
	TextureFormatToGL( config.format, &internal_format, &channels, &type );

	if( config.format == TextureFormat_Shadow ) {
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	}

	if( !CompressedTextureFormat( config.format ) ) {
		assert( config.num_mipmaps == 1 );

		if( channels == GL_RED ) {
			if( config.format == TextureFormat_A_U8 ) {
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_ONE );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_ONE );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_ONE );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_RED );
			}
			else {
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_RED );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_RED );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ONE );
			}
		}
		else if( channels == GL_RG && config.format == TextureFormat_RA_U8 ) {
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED );
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_RED );
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_RED );
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_GREEN );
		}

		glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, internal_format, config.width, config.height, config.layers, 0, channels, type, config.data );
	}
	else {
		glTexParameterf( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filtering );

		const char * cursor = ( const char * ) config.data;
		for( u32 i = 0; i < config.num_mipmaps; i++ ) {
			u32 w = config.width >> i;
			u32 h = config.height >> i;
			u32 size = ( BitsPerPixel( config.format ) * w * h * config.layers ) / 8;
			assert( size < S32_MAX );

			glCompressedTexImage3D( GL_TEXTURE_2D_ARRAY, i, internal_format,
				w, h, config.layers, 0, size, cursor );

			cursor += size;
		}
	}

	return ta;
}

void DeleteTextureArray( TextureArray ta ) {
	if( ta.texture == 0 )
		return;
	glDeleteTextures( 1, &ta.texture );
}

static FramebufferTarget NewFramebufferTarget( const FramebufferTargetConfig & config, int msaa_samples ) {
	if( config.target != NULL ) {
		return *config.target;
	}
	FramebufferTarget target = { };
	target.clear_color = config.clear_color;
	target.clear_depth = config.clear_depth;
	if( config.config.width != 0 ) {
		target.texture = NewTextureSamples( config.config, msaa_samples );
	}
	return target;
}

Framebuffer NewFramebuffer( const FramebufferConfig & config ) {
	GLuint fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );

	Framebuffer fb = { };
	fb.fbo = fbo;

	u32 width = 0;
	u32 height = 0;
	GLenum textarget = config.msaa_samples <= 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
	GLenum bufs[ MAX_RENDER_TARGETS ] = { };

	for( u32 i = 0; i < MAX_RENDER_TARGETS; i++ ) {
		FramebufferTarget target = NewFramebufferTarget( config.attachments[ i ], config.msaa_samples );
		if( target.texture.texture != 0 ) {
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, textarget, target.texture.texture, 0 );
			bufs[ i ] = GL_COLOR_ATTACHMENT0 + i;
			fb.targets[ i ] = target;

			width = target.texture.width;
			height = target.texture.height;
		}
	}
	
	FramebufferTarget target = NewFramebufferTarget( config.depth_attachment, config.msaa_samples );
	if( target.texture.texture != 0 ) {
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textarget, target.texture.texture, 0 );
		fb.depth_target = target;

		width = target.texture.width;
		height = target.texture.height;
	}

	glDrawBuffers( ARRAY_COUNT( bufs ), bufs );

	assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	assert( width > 0 && height > 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	fb.width = width;
	fb.height = height;

	return fb;
}

Framebuffer NewShadowFramebuffer( TextureArray texture_array, u32 layer ) {
	GLuint fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );

	Framebuffer fb = { };
	fb.fbo = fbo;
	fb.depth_target.clear_depth = 1.0f;

	glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_array.texture, 0, layer );

	assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	fb.width = frame_static.shadow_parameters.shadowmap_res;
	fb.height = frame_static.shadow_parameters.shadowmap_res;

	return fb;
}

void DeleteFramebuffer( Framebuffer fb ) {
	if( fb.fbo == 0 )
		return;

	glDeleteFramebuffers( 1, &fb.fbo );
	for( u32 i = 0; i < MAX_RENDER_TARGETS; i++ ) {
		DeleteTexture( fb.targets[ i ].texture );
	}
	DeleteTexture( fb.depth_target.texture );
}

#define MAX_GLSL_UNIFORM_JOINTS 100

static const char * VERTEX_SHADER_PRELUDE =
	"#define VERTEX_SHADER 1\n"
	"#define v2f out\n";

static const char * FRAGMENT_SHADER_PRELUDE =
	"#define FRAGMENT_SHADER 1\n"
	"#define v2f in\n";

static GLuint CompileShader( GLenum type, Span< const char * > srcs, Span< int > lens ) {
	const char * full_srcs[ 32 ];
	int full_lens[ 32 ];
	GLsizei n = 0;

	full_srcs[ n ] = "#version 330\n";
	full_lens[ n ] = -1;
	n++;

	full_srcs[ n ] = type == GL_VERTEX_SHADER ? VERTEX_SHADER_PRELUDE : FRAGMENT_SHADER_PRELUDE;
	full_lens[ n ] = -1;
	n++;

	full_srcs[ n ] = "#define MAX_JOINTS " STRINGIFY( MAX_GLSL_UNIFORM_JOINTS ) "\n";
	full_lens[ n ] = -1;
	n++;

	assert( n + srcs.n <= ARRAY_COUNT( full_srcs ) );

	for( size_t i = 0; i < srcs.n; i++ ) {
		full_srcs[ n ] = srcs[ i ];
		full_lens[ n ] = lens[ i ];
		n++;
	}

	GLuint shader = glCreateShader( type );
	glShaderSource( shader, n, full_srcs, full_lens );
	glCompileShader( shader );

	GLint status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );

	if( status == GL_FALSE ) {
		char buf[ 1024 ];
		glGetShaderInfoLog( shader, sizeof( buf ), NULL, buf );
		Com_Printf( S_COLOR_YELLOW "Shader compilation failed: %s\n", buf );
		glDeleteShader( shader );

		// static char src[ 65536 ];
		// glGetShaderSource( shader, sizeof( src ), NULL, src );
		// printf( "%s\n", src );

		return 0;
	}

	return shader;
}

bool NewShader( Shader * shader, Span< const char * > srcs, Span< int > lens, Span< const char * > feedback_varyings ) {
	*shader = { };
	bool feedback = feedback_varyings.n > 0;

	GLuint vs = CompileShader( GL_VERTEX_SHADER, srcs, lens );
	if( vs == 0 )
		return false;
	defer { glDeleteShader( vs ); };

	GLuint fs = 0;
	if( !feedback ) {
		fs = CompileShader( GL_FRAGMENT_SHADER, srcs, lens );
		if( fs == 0 )
			return false;
	}
	defer {
		if( fs != 0 ) {
			glDeleteShader( fs );
		}
	};

	GLuint program = glCreateProgram();
	glAttachShader( program, vs );
	if( !feedback ) {
		glAttachShader( program, fs );
	}

	glBindAttribLocation( program, VertexAttribute_Position, "a_Position" );
	glBindAttribLocation( program, VertexAttribute_Normal, "a_Normal" );
	glBindAttribLocation( program, VertexAttribute_TexCoord, "a_TexCoord" );
	glBindAttribLocation( program, VertexAttribute_Color, "a_Color" );
	glBindAttribLocation( program, VertexAttribute_JointIndices, "a_JointIndices" );
	glBindAttribLocation( program, VertexAttribute_JointWeights, "a_JointWeights" );

	glBindAttribLocation( program, VertexAttribute_ParticlePosition, "a_ParticlePosition" );
	glBindAttribLocation( program, VertexAttribute_ParticleVelocity, "a_ParticleVelocity" );
	glBindAttribLocation( program, VertexAttribute_ParticleAccelDragRest, "a_ParticleAccelDragRest" );
	glBindAttribLocation( program, VertexAttribute_ParticleUVWH, "a_ParticleUVWH" );
	glBindAttribLocation( program, VertexAttribute_ParticleStartColor, "a_ParticleStartColor" );
	glBindAttribLocation( program, VertexAttribute_ParticleEndColor, "a_ParticleEndColor" );
	glBindAttribLocation( program, VertexAttribute_ParticleSize, "a_ParticleSize" );
	glBindAttribLocation( program, VertexAttribute_ParticleAgeLifetime, "a_ParticleAgeLifetime" );
	glBindAttribLocation( program, VertexAttribute_ParticleFlags, "a_ParticleFlags" );

	if( !feedback ) {
		glBindFragDataLocation( program, 0, "f_Albedo" );
	}
	else {
		glTransformFeedbackVaryings( program, feedback_varyings.n, feedback_varyings.begin(), GL_INTERLEAVED_ATTRIBS );
	}

	glLinkProgram( program );

	GLint status;
	glGetProgramiv( program, GL_LINK_STATUS, &status );
	if( status == GL_FALSE ) {
		// GLint len;
		// glGetProgramiv( program, GL_INFO_LOG_LENGTH, &status );
		// DynamicString buf( &temp, len )
		char buf[ 1024 ];
		glGetProgramInfoLog( program, sizeof( buf ), NULL, buf );
		Com_Printf( S_COLOR_YELLOW "Shader linking failed: %s\n", buf );

		return false;
	}

	glUseProgram( program );
	shader->program = program;

	GLint count, maxlen;
	glGetProgramiv( program, GL_ACTIVE_UNIFORMS, &count );
	glGetProgramiv( program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxlen );

	size_t num_textures = 0;
	size_t num_texture_buffers = 0;
	size_t num_texture_arrays = 0;
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint size, len;
		GLenum type;
		glGetActiveUniform( program, i, sizeof( name ), &len, &size, &type, name );

		if( type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_MULTISAMPLE ) {
			if( num_textures == ARRAY_COUNT( shader->textures ) ) {
				glDeleteProgram( program );
				Com_Printf( S_COLOR_YELLOW "Too many samplers in shader\n" );
				return false;
			}

			glUniform1i( glGetUniformLocation( program, name ), num_textures );
			shader->textures[ num_textures ] = Hash64( name, len );
			num_textures++;
		}

		if( type == GL_SAMPLER_BUFFER || type == GL_INT_SAMPLER_BUFFER || type == GL_UNSIGNED_INT_SAMPLER_BUFFER ) {
			if( num_texture_buffers == ARRAY_COUNT( shader->texture_buffers ) ) {
				glDeleteProgram( program );
				Com_Printf( S_COLOR_YELLOW "Too many samplers in shader\n" );
				return false;
			}

			glUniform1i( glGetUniformLocation( program, name ), ARRAY_COUNT( &Shader::textures ) + num_texture_buffers );
			shader->texture_buffers[ num_texture_buffers ] = Hash64( name, len );
			num_texture_buffers++;
		}

		if( type == GL_SAMPLER_2D_ARRAY || type == GL_SAMPLER_2D_ARRAY_SHADOW ) {
			if( num_texture_arrays == ARRAY_COUNT( shader->texture_arrays ) ) {
				glDeleteProgram( program );
				Com_Printf( S_COLOR_YELLOW "Too many samplers in shader\n" );
				return false;
			}

			glUniform1i( glGetUniformLocation( program, name ), ARRAY_COUNT( &Shader::textures ) + ARRAY_COUNT( &Shader::texture_buffers ) + num_texture_arrays );
			shader->texture_arrays[ num_texture_arrays ] = Hash64( name, len );
			num_texture_arrays++;
		}
	}

	glGetProgramiv( program, GL_ACTIVE_UNIFORM_BLOCKS, &count );
	glGetProgramiv( program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxlen );

	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint len;
		glGetActiveUniformBlockName( program, i, sizeof( name ), &len, name );
		glUniformBlockBinding( program, i, i );
		shader->uniforms[ i ] = Hash64( name, len );
	}

	prev_pipeline.shader = NULL;
	glUseProgram( 0 );

	return true;
}

void DeleteShader( Shader shader ) {
	if( shader.program == 0 )
		return;

	if( prev_pipeline.shader != NULL && prev_pipeline.shader->program == shader.program ) {
		prev_pipeline.shader = NULL;
		glUseProgram( 0 );
	}

	glDeleteProgram( shader.program );
}

Mesh NewMesh( MeshConfig config ) {
	switch( config.primitive_type ) {
		case PrimitiveType_Triangles:
			assert( config.num_vertices % 3 == 0 );
			break;
		case PrimitiveType_TriangleStrip:
			assert( config.num_vertices >= 3 );
			break;
		case PrimitiveType_Points:
			break;
		case PrimitiveType_Lines:
			assert( config.num_vertices % 2 == 0 );
			break;
	}

	GLuint vao;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
	DebugLabel( GL_VERTEX_ARRAY, vao, config.name );

	if( config.unified_buffer.vbo == 0 ) {
		assert( config.positions.vbo != 0 );

		glBindBuffer( GL_ARRAY_BUFFER, config.positions.vbo );
		SetupAttribute( VertexAttribute_Position, config.positions_format );

		if( config.normals.vbo != 0 ) {
			glBindBuffer( GL_ARRAY_BUFFER, config.normals.vbo );
			SetupAttribute( VertexAttribute_Normal, config.normals_format );
		}

		if( config.tex_coords.vbo != 0 ) {
			glBindBuffer( GL_ARRAY_BUFFER, config.tex_coords.vbo );
			SetupAttribute( VertexAttribute_TexCoord, config.tex_coords_format );
		}

		if( config.colors.vbo != 0 ) {
			glBindBuffer( GL_ARRAY_BUFFER, config.colors.vbo );
			SetupAttribute( VertexAttribute_Color, config.colors_format );
		}

		if( config.joints.vbo != 0 ) {
			glBindBuffer( GL_ARRAY_BUFFER, config.joints.vbo );
			SetupAttribute( VertexAttribute_JointIndices, config.joints_format );
		}

		if( config.weights.vbo != 0 ) {
			glBindBuffer( GL_ARRAY_BUFFER, config.weights.vbo );
			SetupAttribute( VertexAttribute_JointWeights, config.weights_format );
		}
	}
	else {
		assert( config.stride != 0 );

		glBindBuffer( GL_ARRAY_BUFFER, config.unified_buffer.vbo );

		SetupAttribute( VertexAttribute_Position, config.positions_format, config.stride, config.positions_offset );

		if( config.normals_offset != 0 ) {
			SetupAttribute( VertexAttribute_Normal, config.normals_format, config.stride, config.normals_offset );
		}

		if( config.tex_coords_offset != 0 ) {
			SetupAttribute( VertexAttribute_TexCoord, config.tex_coords_format, config.stride, config.tex_coords_offset );
		}

		if( config.colors_offset != 0 ) {
			SetupAttribute( VertexAttribute_Color, config.colors_format, config.stride, config.colors_offset );
		}

		if( config.joints_offset != 0 ) {
			SetupAttribute( VertexAttribute_JointIndices, config.joints_format, config.stride, config.joints_offset );
		}

		if( config.weights_offset != 0 ) {
			SetupAttribute( VertexAttribute_JointWeights, config.weights_format, config.stride, config.weights_offset );
		}
	}

	if( config.indices.ebo != 0 ) {
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, config.indices.ebo );
	}

	glBindVertexArray( 0 );

	Mesh mesh = { };
	mesh.num_vertices = config.num_vertices;
	mesh.primitive_type = config.primitive_type;
	mesh.ccw_winding = config.ccw_winding;
	mesh.vao = vao;
	if( config.unified_buffer.vbo == 0 ) {
		mesh.positions = config.positions;
		mesh.normals = config.normals;
		mesh.tex_coords = config.tex_coords;
		mesh.colors = config.colors;
		mesh.joints = config.joints;
		mesh.weights = config.weights;
	}
	else {
		mesh.positions = config.unified_buffer;
	}
	mesh.indices = config.indices;
	mesh.indices_format = config.indices_format;

	return mesh;
}

void DeleteMesh( const Mesh & mesh ) {
	if( mesh.vao == 0 )
		return;

	if( mesh.positions.vbo != 0 )
		DeleteVertexBuffer( mesh.positions );
	if( mesh.normals.vbo != 0 )
		DeleteVertexBuffer( mesh.normals );
	if( mesh.tex_coords.vbo != 0 )
		DeleteVertexBuffer( mesh.tex_coords );
	if( mesh.colors.vbo != 0 )
		DeleteVertexBuffer( mesh.colors );
	if( mesh.joints.vbo != 0 )
		DeleteVertexBuffer( mesh.joints );
	if( mesh.weights.vbo != 0 )
		DeleteVertexBuffer( mesh.weights );
	if( mesh.indices.ebo != 0 )
		DeleteIndexBuffer( mesh.indices );

	glDeleteVertexArrays( 1, &mesh.vao );
}

void DeferDeleteMesh( const Mesh & mesh ) {
	deferred_mesh_deletes.add( mesh );
}

void DrawMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_vertices_override, u32 index_offset ) {
	assert( in_frame );
	assert( pipeline.pass != U8_MAX );
	assert( pipeline.shader != NULL );

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_vertices = num_vertices_override == 0 ? mesh.num_vertices : num_vertices_override;
	dc.index_offset = index_offset;
	draw_calls.add( dc );

	num_vertices_this_frame += dc.num_vertices;
}

u8 AddRenderPass( const RenderPass & pass ) {
	return checked_cast< u8 >( render_passes.add( pass ) );
}

u8 AddRenderPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer target, ClearColor clear_color, ClearDepth clear_depth ) {
	RenderPass pass;
	pass.type = RenderPass_Normal;
	pass.target = target;
	pass.name = name;
	pass.clear_color = clear_color == ClearColor_Do;
	pass.clear_depth = clear_depth == ClearDepth_Do;
	pass.tracy = tracy;
	return AddRenderPass( pass );
}

u8 AddRenderPass( const char * name, const tracy::SourceLocationData * tracy, ClearColor clear_color, ClearDepth clear_depth ) {
	Framebuffer target = { };
	return AddRenderPass( name, tracy, target, clear_color, clear_depth );
}

u8 AddUnsortedRenderPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer target ) {
	RenderPass pass;
	pass.type = RenderPass_Normal;
	pass.target = target;
	pass.name = name;
	pass.sorted = false;
	pass.tracy = tracy;
	return AddRenderPass( pass );
}

void AddBlitPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer src, Framebuffer dst, ClearColor clear_color, ClearDepth clear_depth ) {
	RenderPass pass;
	pass.type = RenderPass_Blit;
	pass.name = name;
	pass.tracy = tracy;
	pass.blit_source = src;
	pass.target = dst;
	pass.clear_color = clear_color;
	pass.clear_depth = clear_depth;
	AddRenderPass( pass );
}

void AddResolveMSAAPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer src, Framebuffer dst, ClearColor clear_color, ClearDepth clear_depth ) {
	AddBlitPass( name, tracy, src, dst, clear_color, clear_depth );
}

void UpdateParticles( const Mesh & mesh, VertexBuffer vb_in, VertexBuffer vb_out, float radius, u32 num_particles, float dt ) {
	assert( in_frame );

	PipelineState pipeline;
	pipeline.pass = frame_static.particle_update_pass;
	pipeline.shader = &shaders.particle_update;
	u32 collision = cl.map == NULL ? 0 : 1;
	pipeline.set_uniform( "u_ParticleUpdate", UploadUniformBlock( collision, radius, dt ) );
	if( collision ) {
		pipeline.set_texture_buffer( "u_NodeBuffer", cl.map->nodeBuffer );
		pipeline.set_texture_buffer( "u_LeafBuffer", cl.map->leafBuffer );
		pipeline.set_texture_buffer( "u_BrushBuffer", cl.map->brushBuffer );
		pipeline.set_texture_buffer( "u_PlaneBuffer", cl.map->planeBuffer );
	}

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_instances = num_particles;
	dc.instance_data = vb_in;
	dc.update_data = vb_out;

	draw_calls.add( dc );
}

void UpdateParticlesFeedback( const Mesh & mesh, VertexBuffer vb_in, VertexBuffer vb_out, VertexBuffer vb_feedback, float radius, u32 num_particles, float dt ) {
	assert( in_frame );

	PipelineState pipeline;
	pipeline.pass = frame_static.particle_update_pass;
	pipeline.shader = &shaders.particle_update_feedback;
	u32 collision = cl.map == NULL ? 0 : 1;
	pipeline.set_uniform( "u_ParticleUpdate", UploadUniformBlock( collision, radius, dt ) );
	if( collision ) {
		pipeline.set_texture_buffer( "u_NodeBuffer", cl.map->nodeBuffer );
		pipeline.set_texture_buffer( "u_LeafBuffer", cl.map->leafBuffer );
		pipeline.set_texture_buffer( "u_BrushBuffer", cl.map->brushBuffer );
		pipeline.set_texture_buffer( "u_PlaneBuffer", cl.map->planeBuffer );
	}

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_instances = num_particles;
	dc.instance_data = vb_in;
	dc.update_data = vb_out;
	dc.feedback_data = vb_feedback;

	draw_calls.add( dc );
}

void DrawInstancedParticles( const Mesh & mesh, VertexBuffer vb, BlendFunc blend_func, u32 num_particles ) {
	assert( in_frame );

	PipelineState pipeline;
	pipeline.pass = frame_static.transparent_pass;
	pipeline.shader = &shaders.particle;
	pipeline.blend_func = blend_func;
	pipeline.write_depth = false;
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );
	pipeline.set_uniform( "u_Fog", frame_static.fog_uniforms );
	pipeline.set_texture_array( "u_DecalAtlases", DecalAtlasTextureArray() );

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_vertices = mesh.num_vertices;
	dc.instance_data = vb;
	dc.num_instances = num_particles;

	draw_calls.add( dc );

	num_vertices_this_frame += mesh.num_vertices * num_particles;
}

void DownloadFramebuffer( void * buf ) {
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glReadPixels( 0, 0, frame_static.viewport_width, frame_static.viewport_height, GL_RGB, GL_UNSIGNED_BYTE, buf );
	prev_fbo = 0;
}

void DrawInstancedParticles( VertexBuffer vb, const Model * model, u32 num_particles ) {
	assert( in_frame );

	UniformBlock model_uniforms = UploadModelUniforms( model->transform );

	for( u32 i = 0; i < model->num_primitives; i++ ) {
		PipelineState pipeline = MaterialToPipelineState( model->primitives[ i ].material );
		pipeline.pass = frame_static.nonworld_opaque_pass;
		pipeline.shader = &shaders.particle_model;
		pipeline.write_depth = true;
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Fog", frame_static.fog_uniforms );
		pipeline.set_uniform( "u_Model", model_uniforms );

		const Model::Primitive primitive = model->primitives[ i ];
		DrawCall dc = { };
		dc.pipeline = pipeline;
		dc.instance_data = vb;

		if( primitive.num_vertices != 0 ) {
			dc.mesh = model->mesh;
			dc.num_vertices = primitive.num_vertices;
			u32 index_size = model->mesh.indices_format == IndexFormat_U16 ? sizeof( u16 ) : sizeof( u32 );
			dc.index_offset = primitive.first_index * index_size;

			num_vertices_this_frame += model->mesh.num_vertices * num_particles;
		}
		else {
			dc.mesh = primitive.mesh;
			dc.num_vertices = primitive.mesh.num_vertices;

			num_vertices_this_frame += primitive.mesh.num_vertices * num_particles;
		}

		dc.num_instances = num_particles;

		draw_calls.add( dc );
	}
}
