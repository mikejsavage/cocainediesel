#include <algorithm> // std::stable_sort
#include <new>

#include "glad/glad.h"

#include "tracy/Tracy.hpp"
#include "tracy/TracyOpenGL.hpp"

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/string.h"
#include "client/renderer/renderer.h"

#include "cgame/cg_local.h"

template< typename S, typename T >
struct SameType { static constexpr bool value = false; };
template< typename T >
struct SameType< T, T > { static constexpr bool value = true; };

STATIC_ASSERT( ( SameType< u32, GLuint >::value ) );

static const u32 UNIFORM_BUFFER_SIZE = 64 * 1024;

enum DrawCallType {
	DrawCallType_Normal,
	DrawCallType_Indirect,
	DrawCallType_Compute,
	DrawCallType_IndirectCompute,
};

struct DrawCall {
	DrawCallType type;
	PipelineState pipeline;
	Mesh mesh;
	u32 num_vertices;
	u32 num_instances;
	u32 first_index;

	u32 dispatch_size[ 3 ];
	GPUBuffer indirect;
};

static GLsync fences[ 3 ];
static u64 frame_counter;

STATIC_ASSERT( ARRAY_COUNT( fences ) == ARRAY_COUNT( &StreamingBuffer::buffers ) );
STATIC_ASSERT( ARRAY_COUNT( fences ) == ARRAY_COUNT( &StreamingBuffer::mappings ) );

static NonRAIIDynamicArray< RenderPass > render_passes;
static NonRAIIDynamicArray< DrawCall > draw_calls;

static NonRAIIDynamicArray< Mesh > deferred_mesh_deletes;
static NonRAIIDynamicArray< GPUBuffer > deferred_buffer_deletes;
static NonRAIIDynamicArray< StreamingBuffer > deferred_streaming_buffer_deletes;

#if TRACY_ENABLE
alignas( tracy::GpuCtxScope ) static char renderpass_zone_memory[ sizeof( tracy::GpuCtxScope ) ];
static tracy::GpuCtxScope * renderpass_zone;
#endif

static u32 num_vertices_this_frame;

static bool in_frame;

struct UBO {
	StreamingBuffer stream;
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
	GPUBuffer buffers[ ARRAY_COUNT( &Shader::uniforms ) ] = { };
	const Texture * textures[ ARRAY_COUNT( &Shader::textures ) ] = { };
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

	Assert( false );
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
		case TextureFormat_R_UI8:
			*internal = GL_R8UI;
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

	Assert( false );
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

	Assert( false );
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

	Assert( false );
}

static u32 GLTypeSize( GLenum type ) {
	switch( type ) {
		case GL_UNSIGNED_BYTE: return 1;
		case GL_UNSIGNED_SHORT: return 2;
		case GL_UNSIGNED_INT:
		case GL_FLOAT:
			return 4;
	}

	Assert( false );
	return 0;
}

static void VertexFormatToGL( VertexFormat format, GLenum * type, int * num_components, bool * integral, GLboolean * normalized, u32 * stride ) {
	*integral = false;
	*normalized = false;

	switch( format ) {
		case VertexFormat_U8x2:
		case VertexFormat_U8x2_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 2;
			*integral = true;
			*normalized = format == VertexFormat_U8x2_Norm;
			break;
		case VertexFormat_U8x3:
		case VertexFormat_U8x3_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 3;
			*integral = true;
			*normalized = format == VertexFormat_U8x3_Norm;
			break;
		case VertexFormat_U8x4:
		case VertexFormat_U8x4_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 4;
			*integral = true;
			*normalized = format == VertexFormat_U8x4_Norm;
			break;

		case VertexFormat_U16x2:
		case VertexFormat_U16x2_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 2;
			*integral = true;
			*normalized = format == VertexFormat_U16x2_Norm;
			break;
		case VertexFormat_U16x3:
		case VertexFormat_U16x3_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 3;
			*integral = true;
			*normalized = format == VertexFormat_U16x3_Norm;
			break;
		case VertexFormat_U16x4:
		case VertexFormat_U16x4_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 4;
			*integral = true;
			*normalized = format == VertexFormat_U16x4_Norm;
			break;

		case VertexFormat_U32x1:
			*type = GL_UNSIGNED_INT;
			*num_components = 1;
			*integral = true;
			break;

		case VertexFormat_Floatx1:
			*type = GL_FLOAT;
			*num_components = 1;
			break;
		case VertexFormat_Floatx2:
			*type = GL_FLOAT;
			*num_components = 2;
			break;
		case VertexFormat_Floatx3:
			*type = GL_FLOAT;
			*num_components = 3;
			break;
		case VertexFormat_Floatx4:
			*type = GL_FLOAT;
			*num_components = 4;
			break;

		default:
			Assert( false );
	}

	if( stride != NULL ) {
		*stride = GLTypeSize( *type ) * *num_components;
	}
}

static const char * DebugTypeString( GLenum type ) {
	switch( type ) {
		case GL_DEBUG_TYPE_ERROR:
			return "error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			return "deprecated";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			return "undefined";
		case GL_DEBUG_TYPE_PORTABILITY:
			return "nonportable";
		case GL_DEBUG_TYPE_PERFORMANCE:
			return "performance";
		case GL_DEBUG_TYPE_OTHER:
			return "other";
		default:
			return "idk";
	}
}

static const char * DebugSeverityString( GLenum severity ) {
	switch( severity ) {
		case GL_DEBUG_SEVERITY_LOW:
			return S_COLOR_GREEN "low" S_COLOR_WHITE;
		case GL_DEBUG_SEVERITY_MEDIUM:
			return S_COLOR_YELLOW "medium" S_COLOR_WHITE;
		case GL_DEBUG_SEVERITY_HIGH:
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
	    source == 33352 || // shader compilation errors
	    id == 131169 ||
	    id == 131185 ||
	    id == 131218 ||
	    id == 131204 ||
	  	id == 131184 // TODO(msc): idk what this is but it's spamming my console, cheers
	) {
		return;
	}

	if( severity == GL_DEBUG_SEVERITY_NOTIFICATION ) {
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

static void DebugLabel( GLenum type, GLuint object, const char * label ) {
	Assert( label != NULL );
	glObjectLabel( type, object, -1, label );
}

static void PlotVRAMUsage() {
	if( !is_public_build && GLAD_GL_NVX_gpu_memory_info != 0 ) {
		GLint total_vram;
		glGetIntegerv( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_vram );

		GLint available_vram;
		glGetIntegerv( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available_vram );

		TracyPlotSample( "VRAM usage", s64( total_vram - available_vram ) );
	}
}

static void RunDeferredDeletes() {
	TracyZoneScoped;

	for( const Mesh & mesh : deferred_mesh_deletes ) {
		DeleteMesh( mesh );
	}
	deferred_mesh_deletes.clear();

	for( const GPUBuffer & buffer : deferred_buffer_deletes ) {
		DeleteGPUBuffer( buffer );
	}
	deferred_buffer_deletes.clear();

	for( const StreamingBuffer & stream : deferred_streaming_buffer_deletes ) {
		DeleteStreamingBuffer( stream );
	}
	deferred_streaming_buffer_deletes.clear();
}

void InitRenderBackend() {
	TracyZoneScoped;
	TracyGpuContext;

	{
		struct {
			const char * name;
			int loaded;
		} required_extensions[] = {
			{ "GL_EXT_texture_compression_s3tc", GLAD_GL_EXT_texture_compression_s3tc },
			{ "GL_EXT_texture_filter_anisotropic", GLAD_GL_EXT_texture_filter_anisotropic },
			{ "GL_EXT_texture_sRGB", GLAD_GL_EXT_texture_sRGB },
			{ "GL_EXT_texture_sRGB_decode", GLAD_GL_EXT_texture_sRGB_decode },
		};

		String< 1024 > missing_extensions( "Your GPU is insane and doesn't have some required OpenGL extensions:" );
		bool any_missing = false;
		for( auto ext : required_extensions ) {
			if( ext.loaded == 0 ) {
				missing_extensions.append( " {}", ext.name );
				any_missing = true;
			}
		}

		if( any_missing ) {
			Fatal( "%s", missing_extensions.c_str() );
		}

		GLint vert_buffers;
		glGetIntegerv( GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &vert_buffers );
		if( vert_buffers >= 0 && size_t( vert_buffers ) < ARRAY_COUNT( &Shader::buffers ) ) {
			Fatal( "Your GPU is too old, GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS is too small" );
		}
	}

	{
		GLint context_flags;
		glGetIntegerv( GL_CONTEXT_FLAGS, &context_flags );
		if( context_flags & GL_CONTEXT_FLAG_DEBUG_BIT ) {
			Com_Printf( "Initialising OpenGL debug output\n" );

			glEnable( GL_DEBUG_OUTPUT );
			glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
			glDebugMessageCallback( DebugOutputCallback, NULL );
			glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE );
		}
	}

	PlotVRAMUsage();

	for( GLsync & fence : fences ) {
		fence = 0;
	}
	frame_counter = 0;

	render_passes.init( sys_allocator );
	draw_calls.init( sys_allocator );

	deferred_mesh_deletes.init( sys_allocator );
	deferred_buffer_deletes.init( sys_allocator );
	deferred_streaming_buffer_deletes.init( sys_allocator );

	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );

	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	glDisable( GL_BLEND );

	glEnable( GL_FRAMEBUFFER_SRGB );
	glDisable( GL_DITHER );

	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	GLint alignment;
	glGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment );
	ubo_offset_alignment = checked_cast< u32 >( alignment );

	glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_filtering );

	GLint max_ubo_size;
	glGetIntegerv( GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size );
	Assert( max_ubo_size >= s32( UNIFORM_BUFFER_SIZE ) );

	for( size_t i = 0; i < ARRAY_COUNT( ubos ); i++ ) {
		TempAllocator temp = cls.frame_arena.temp();
		ubos[ i ].stream = NewStreamingBuffer( UNIFORM_BUFFER_SIZE, temp( "UBO {}", i ) );
	}

	in_frame = false;

	prev_pipeline = PipelineState();
	prev_fbo = 0;
	prev_viewport_width = 0;
	prev_viewport_height = 0;
}

void ShutdownRenderBackend() {
	for( UBO ubo : ubos ) {
		DeleteStreamingBuffer( ubo.stream );
	}

	RunDeferredDeletes();

	for( GLsync fence : fences ) {
		if( fence != 0 ) {
			glDeleteSync( fence );
		}
	}

	render_passes.shutdown();
	draw_calls.shutdown();

	deferred_mesh_deletes.shutdown();
	deferred_buffer_deletes.shutdown();
	deferred_streaming_buffer_deletes.shutdown();
}

void RenderBackendBeginFrame() {
	TracyZoneScoped;

	Assert( !in_frame );
	in_frame = true;

	render_passes.clear();
	draw_calls.clear();

	if( fences[ frame_counter % ARRAY_COUNT( fences ) ] != 0 ) {
		TracyZoneScopedN( "Wait on frame fence" );
		glClientWaitSync( fences[ frame_counter % ARRAY_COUNT( fences ) ], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED );
		glDeleteSync( fences[ frame_counter % ARRAY_COUNT( fences ) ] );
	}

	num_vertices_this_frame = 0;

	for( UBO & ubo : ubos ) {
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

static void SetPipelineState( PipelineState pipeline, bool cw_winding ) {
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
		const Texture * prev_texture = prev_bindings.textures[ i ];

		bool found = prev_texture == NULL;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_textures; j++ ) {
				if( pipeline.textures[ j ].name_hash == name_hash ) {
					const Texture * texture = pipeline.textures[ j ].texture;
					if( texture != prev_texture ) {
						if( prev_texture != NULL && prev_texture->msaa != texture->msaa ) {
							glBindTextureUnit( i, 0 );
						}
						glBindTextureUnit( i, texture->texture );
						prev_bindings.textures[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found && prev_texture != NULL ) {
			glBindTextureUnit( i, 0 );
			prev_bindings.textures[ i ] = NULL;
		}
	}

	// buffers
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->buffers ); i++ ) {
		u64 name_hash = pipeline.shader->buffers[ i ];
		GPUBuffer prev_buffer = prev_bindings.buffers[ i ];

		bool should_unbind = prev_buffer.buffer != 0;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_buffers; j++ ) {
				if( pipeline.buffers[ j ].name_hash == name_hash ) {
					GPUBuffer buffer = pipeline.buffers[ j ].buffer;
					if( buffer.buffer != prev_buffer.buffer ) {
						glBindBufferBase( GL_SHADER_STORAGE_BUFFER, i, buffer.buffer );
						prev_bindings.buffers[ i ] = buffer;
					}
					should_unbind = false;
					break;
				}
			}
		}

		if( should_unbind ) {
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, i, 0 );
			prev_bindings.buffers[ i ] = { };
		}
	}

	// texture arrays
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->texture_arrays ); i++ ) {
		u64 name_hash = pipeline.shader->texture_arrays[ i ];
		GLenum tex_unit = ARRAY_COUNT( pipeline.shader->textures ) + i;
		TextureArray prev_texture = prev_bindings.texture_arrays[ i ];

		bool found = prev_texture.texture == 0;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_texture_arrays; j++ ) {
				if( pipeline.texture_arrays[ j ].name_hash == name_hash ) {
					TextureArray texture = pipeline.texture_arrays[ j ].ta;
					if( texture.texture != prev_texture.texture ) {
						glBindTextureUnit( tex_unit, texture.texture );
						prev_bindings.texture_arrays[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found ) {
			glBindTextureUnit( tex_unit, 0 );
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
	if( pipeline.cull_face != CullFace_Disabled && cw_winding ) {
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
		}
		else {
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

static void SubmitFramebufferBlit( const RenderPass & pass ) {
	Framebuffer src = pass.blit_source;
	Framebuffer target = pass.target;
	GLbitfield clear_mask = 0;
	clear_mask |= pass.clear_color ? GL_COLOR_BUFFER_BIT : 0;
	clear_mask |= pass.clear_depth ? GL_DEPTH_BUFFER_BIT : 0;
	glBlitNamedFramebuffer( src.fbo, target.fbo, 0, 0, src.width, src.height, 0, 0, target.width, target.height, clear_mask, GL_NEAREST );
}

#if !TRACY_ENABLE
namespace tracy {
struct SourceLocationData {
	const char * name;
	const char * function;
	const char * file;
	uint32_t line;
	uint32_t color;
};
}
#endif

static void SetupRenderPass( const RenderPass & pass ) {
	TracyZoneScoped;
	TracyZoneText( pass.tracy->name, strlen( pass.tracy->name ) );
#if TRACY_ENABLE
	renderpass_zone = new (renderpass_zone_memory) tracy::GpuCtxScope( pass.tracy );
#endif

	glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass.tracy->name );

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

	if( pass.barrier ) {
		glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
	}

	GLbitfield clear_mask = 0;
	clear_mask |= pass.clear_color ? GL_COLOR_BUFFER_BIT : 0;
	clear_mask |= pass.clear_depth ? GL_DEPTH_BUFFER_BIT : 0;
	if( clear_mask != 0 ) {
		PipelineState::Scissor scissor = prev_pipeline.scissor;
		if( scissor.x != 0 || scissor.y != 0 || scissor.w != 0 || scissor.h != 0 ) {
			glDisable( GL_SCISSOR_TEST );
			prev_pipeline.scissor = { };
		}

		if( pass.clear_color ) {
			glClearColor( pass.color.x, pass.color.y, pass.color.z, pass.color.w );
		}

		if( pass.clear_depth ) {
			if( !prev_pipeline.write_depth ) {
				glDepthMask( GL_TRUE );
				prev_pipeline.write_depth = true;
			}
			glClearDepth( pass.depth );
		}

		glClear( clear_mask );
	}
}

static void FinishRenderPass() {
	glPopDebugGroup();

#if TRACY_ENABLE
	renderpass_zone->~GpuCtxScope();
#endif
}

static void SubmitDrawCall( const DrawCall & dc ) {
	TracyZoneScoped;
	TracyGpuZone( "Draw call" );

	if( dc.pipeline.shader->program == 0 )
		return;

	SetPipelineState( dc.pipeline, dc.mesh.cw_winding );

	if( dc.type == DrawCallType_Compute ) {
		glDispatchCompute( dc.dispatch_size[ 0 ], dc.dispatch_size[ 1 ], dc.dispatch_size[ 2 ] );
		return;
	}

	if( dc.type == DrawCallType_IndirectCompute ) {
		glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, dc.indirect.buffer );
		glDispatchComputeIndirect( 0 );
		glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, 0 );
		return;
	}

	GLenum gl_index_format = dc.mesh.index_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	glBindVertexArray( dc.mesh.vao );

	if( dc.type == DrawCallType_Normal ) {
		if( dc.mesh.index_buffer.buffer != 0 ) {
			size_t index_size = dc.mesh.index_format == IndexFormat_U16 ? sizeof( u16 ) : sizeof( u32 );
			const void * offset = ( const void * ) ( uintptr_t( dc.first_index ) * index_size );
			glDrawElementsInstanced( GL_TRIANGLES, dc.num_vertices, gl_index_format, offset, dc.num_instances );
		}
		else {
			glDrawArraysInstanced( GL_TRIANGLES, dc.first_index, dc.num_vertices, dc.num_instances );
		}
	}
	else if( dc.type == DrawCallType_Indirect ) {
		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, dc.indirect.buffer );
		if( dc.mesh.index_buffer.buffer != 0 ) {
			glDrawElementsIndirect( GL_TRIANGLES, gl_index_format, 0 );
		}
		else {
			glDrawArraysIndirect( GL_TRIANGLES, 0 );
		}
		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
	}

	glBindVertexArray( 0 );
}

void RenderBackendSubmitFrame() {
	TracyZoneScoped;

	Assert( in_frame );
	Assert( render_passes.size() > 0 );
	in_frame = false;

	{
		TracyZoneScopedN( "Sort draw calls" );
		std::stable_sort( draw_calls.begin(), draw_calls.end(), SortDrawCall );
	}

	SetupRenderPass( render_passes[ 0 ] );
	u8 pass_idx = 0;

	{
		TracyZoneScopedN( "Submit draw calls" );
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

	RunDeferredDeletes();

	fences[ frame_counter % ARRAY_COUNT( fences ) ] = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	frame_counter++;

	u32 ubo_bytes_used = 0;
	for( const UBO & ubo : ubos ) {
		ubo_bytes_used += ubo.bytes_used;
	}
	TracyPlotSample( "UBO utilisation", float( ubo_bytes_used ) / float( UNIFORM_BUFFER_SIZE * ARRAY_COUNT( ubos ) ) );

	TracyPlotSample( "Draw calls", s64( draw_calls.size() ) );
	TracyPlotSample( "Vertices", s64( num_vertices_this_frame ) );

	TracyGpuCollect;
}

UniformBlock UploadUniforms( const void * data, size_t size ) {
	Assert( in_frame );

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
	block.ubo = GetStreamingBufferBuffer( ubo->stream ).buffer;
	block.offset = offset;
	block.size = AlignPow2( checked_cast< u32 >( size ), u32( 16 ) );

	// memset so we don't leave any gaps. good for write combined memory!
	u8 * mapping = GetStreamingBufferMapping( ubo->stream );
	memset( mapping + ubo->bytes_used, 0, offset - ubo->bytes_used );
	memcpy( mapping + offset, data, size );
	ubo->bytes_used = offset + size;

	return block;
}

void ReadGPUBuffer( GPUBuffer buf, void * data, u32 len, u32 offset ) {
	glGetNamedBufferSubData( buf.buffer, offset, len, data );
}

GPUBuffer NewGPUBuffer( const void * data, u32 len, const char * name ) {
	GPUBuffer buf = { };
	glCreateBuffers( 1, &buf.buffer );
	glNamedBufferStorage( buf.buffer, len, data, GL_DYNAMIC_STORAGE_BIT );

	if( name != NULL ) {
		DebugLabel( GL_BUFFER, buf.buffer, name );
	}

	return buf;
}

GPUBuffer NewGPUBuffer( u32 len, const char * name ) {
	return NewGPUBuffer( NULL, len, name );
}

void WriteGPUBuffer( GPUBuffer buf, const void * data, u32 len, u32 offset ) {
	glNamedBufferSubData( buf.buffer, offset, len, data );
}

void DeleteGPUBuffer( GPUBuffer buf ) {
	if( buf.buffer == 0 )
		return;
	glDeleteBuffers( 1, &buf.buffer );
}

void DeferDeleteGPUBuffer( GPUBuffer buf ) {
	deferred_buffer_deletes.add( buf );
}

StreamingBuffer NewStreamingBuffer( u32 len, const char * name ) {
	StreamingBuffer stream = { };

	for( size_t i = 0; i < ARRAY_COUNT( stream.buffers ); i++ ) {
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glCreateBuffers( 1, &stream.buffers[ i ].buffer );
		glNamedBufferStorage( stream.buffers[ i ].buffer, len, NULL, flags );

		if( name != NULL ) {
			TempAllocator temp = cls.frame_arena.temp();
			DebugLabel( GL_BUFFER, stream.buffers[ i ].buffer, temp( "{} #{}", name, i ) );
		}

		stream.mappings[ i ] = ( u8 * ) glMapNamedBufferRange( stream.buffers[ i ].buffer, 0, len, flags );
	}

	return stream;
}

u8 * GetStreamingBufferMapping( StreamingBuffer stream ) {
	return stream.mappings[ frame_counter % ARRAY_COUNT( stream.mappings ) ];
}

GPUBuffer GetStreamingBufferBuffer( StreamingBuffer stream ) {
	return stream.buffers[ frame_counter % ARRAY_COUNT( stream.buffers ) ];
}

void DeleteStreamingBuffer( StreamingBuffer stream ) {
	for( GPUBuffer buf : stream.buffers ) {
		glUnmapNamedBuffer( buf.buffer );
		DeleteGPUBuffer( buf );
	}
}

void DeferDeleteStreamingBuffer( StreamingBuffer stream ) {
	deferred_streaming_buffer_deletes.add( stream );
}

static Texture NewTextureSamples( TextureConfig config, int msaa_samples ) {
	Texture texture = { };
	texture.width = config.width;
	texture.height = config.height;
	texture.num_mipmaps = config.num_mipmaps;
	texture.msaa = msaa_samples > 1;
	texture.format = config.format;

	GLenum target = msaa_samples == 0 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
	glCreateTextures( target, 1, &texture.texture );

	GLenum internal_format, channels, type;
	TextureFormatToGL( config.format, &internal_format, &channels, &type );

	if( msaa_samples != 0 ) {
		glTextureStorage2DMultisample( texture.texture, msaa_samples,
			internal_format, config.width, config.height, GL_TRUE );
		return texture;
	}

	glTextureStorage2D( texture.texture, config.num_mipmaps, internal_format, config.width, config.height );

	glTextureParameteri( texture.texture, GL_TEXTURE_WRAP_S, TextureWrapToGL( config.wrap ) );
	glTextureParameteri( texture.texture, GL_TEXTURE_WRAP_T, TextureWrapToGL( config.wrap ) );

	GLenum min_filter = GL_NONE;
	GLenum mag_filter = GL_NONE;
	TextureFilterToGL( config.filter, &min_filter, &mag_filter );
	glTextureParameteri( texture.texture, GL_TEXTURE_MIN_FILTER, min_filter );
	glTextureParameteri( texture.texture, GL_TEXTURE_MAG_FILTER, mag_filter );
	glTextureParameteri( texture.texture, GL_TEXTURE_MAX_LEVEL, config.num_mipmaps - 1 );
	glTextureParameterf( texture.texture, GL_TEXTURE_LOD_BIAS, -1.0f );

	if( config.wrap == TextureWrap_Border ) {
		glTextureParameterfv( texture.texture, GL_TEXTURE_BORDER_COLOR, config.border_color.ptr() );
	}

	if( !CompressedTextureFormat( config.format ) ) {
		Assert( config.num_mipmaps == 1 );

		if( channels == GL_RED ) {
			if( config.format == TextureFormat_A_U8 ) {
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_R, GL_ONE );
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_G, GL_ONE );
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_B, GL_ONE );
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_A, GL_RED );
			}
			else {
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_R, GL_RED );
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_G, GL_RED );
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_B, GL_RED );
				glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_A, GL_ONE );
			}
		}
		else if( channels == GL_RG && config.format == TextureFormat_RA_U8 ) {
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_R, GL_RED );
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_G, GL_RED );
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_B, GL_RED );
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_A, GL_GREEN );
		}

		if( config.data != NULL ) {
			glTextureSubImage2D( texture.texture, 0, 0, 0,
				config.width, config.height, channels, type, config.data );
		}
	}
	else {
		Assert( config.data != NULL );

		glTextureParameterf( texture.texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filtering );

		if( config.format == TextureFormat_BC4 ) {
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_R, GL_ONE );
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_G, GL_ONE );
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_B, GL_ONE );
			glTextureParameteri( texture.texture, GL_TEXTURE_SWIZZLE_A, GL_RED );
		}

		const char * cursor = ( const char * ) config.data;
		for( u32 i = 0; i < config.num_mipmaps; i++ ) {
			u32 w = config.width >> i;
			u32 h = config.height >> i;
			u32 size = ( BitsPerPixel( config.format ) * w * h ) / 8;
			Assert( size < S32_MAX );

			glCompressedTextureSubImage2D( texture.texture, i, 0, 0,
				w, h, internal_format, size, cursor );

			cursor += size;
		}
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

	glCreateTextures( GL_TEXTURE_2D_ARRAY, 1, &ta.texture );

	GLenum internal_format, channels, type;
	TextureFormatToGL( config.format, &internal_format, &channels, &type );
	glTextureStorage3D( ta.texture, config.num_mipmaps, internal_format, config.width, config.height, config.layers );

	glTextureParameteri( ta.texture, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTextureParameteri( ta.texture, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTextureParameteri( ta.texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTextureParameteri( ta.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTextureParameteri( ta.texture, GL_TEXTURE_MAX_LEVEL, config.num_mipmaps - 1 );

	if( config.format == TextureFormat_Shadow ) {
		glTextureParameteri( ta.texture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		glTextureParameteri( ta.texture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	}

	if( !CompressedTextureFormat( config.format ) ) {
		Assert( config.num_mipmaps == 1 );

		if( channels == GL_RED ) {
			if( config.format == TextureFormat_A_U8 ) {
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_R, GL_ONE );
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_G, GL_ONE );
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_B, GL_ONE );
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_A, GL_RED );
			}
			else {
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_R, GL_RED );
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_G, GL_RED );
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_B, GL_RED );
				glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_A, GL_ONE );
			}
		}
		else if( channels == GL_RG && config.format == TextureFormat_RA_U8 ) {
			glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_R, GL_RED );
			glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_G, GL_RED );
			glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_B, GL_RED );
			glTextureParameteri( ta.texture, GL_TEXTURE_SWIZZLE_A, GL_GREEN );
		}

		if( config.data != NULL ) {
			glTextureSubImage3D( ta.texture, 0, 0, 0, 0,
				config.width, config.height, config.layers, channels, type, config.data );
		}
	}
	else {
		glTextureParameterf( ta.texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filtering );

		const char * cursor = ( const char * ) config.data;
		for( u32 i = 0; i < config.num_mipmaps; i++ ) {
			u32 w = config.width >> i;
			u32 h = config.height >> i;
			u32 size = ( BitsPerPixel( config.format ) * w * h * config.layers ) / 8;
			Assert( size < S32_MAX );

			glCompressedTextureSubImage3D( ta.texture, i, 0, 0, 0,
				w, h, config.layers, internal_format, size, cursor );

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

Framebuffer NewFramebuffer( const FramebufferConfig & config ) {
	Framebuffer fb = { };

	glCreateFramebuffers( 1, &fb.fbo );

	u32 width = 0;
	u32 height = 0;
	GLenum bufs[ 2 ] = { GL_NONE, GL_NONE };

	if( config.albedo_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.albedo_attachment, config.msaa_samples );
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT0, texture.texture, 0 );
		bufs[ 0 ] = GL_COLOR_ATTACHMENT0;

		fb.albedo_texture = texture;

		width = texture.width;
		height = texture.height;
	}

	if( config.mask_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.mask_attachment, config.msaa_samples );
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT1, texture.texture, 0 );
		bufs[ 1 ] = GL_COLOR_ATTACHMENT1;

		fb.mask_texture = texture;

		width = texture.width;
		height = texture.height;
	}

	if( config.depth_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.depth_attachment, config.msaa_samples );
		glNamedFramebufferTexture( fb.fbo, GL_DEPTH_ATTACHMENT, texture.texture, 0 );

		fb.depth_texture = texture;

		width = texture.width;
		height = texture.height;
	}

	glNamedFramebufferDrawBuffers( fb.fbo, ARRAY_COUNT( bufs ), bufs );

	Assert( glCheckNamedFramebufferStatus( fb.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	Assert( width > 0 && height > 0 );

	fb.width = width;
	fb.height = height;

	return fb;
}

Framebuffer NewFramebuffer( Texture * albedo_texture, Texture * mask_texture, Texture * depth_texture ) {
	Framebuffer fb = { };

	glCreateFramebuffers( 1, &fb.fbo );

	u32 width = 0;
	u32 height = 0;
	GLenum bufs[ 2 ] = { GL_NONE, GL_NONE };
	if( albedo_texture != NULL ) {
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT0, albedo_texture->texture, 0 );
		fb.albedo_texture = *albedo_texture;
		bufs[ 0 ] = GL_COLOR_ATTACHMENT0;
		width = albedo_texture->width;
		height = albedo_texture->height;
	}
	if( mask_texture != NULL ) {
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT1, mask_texture->texture, 0 );
		fb.mask_texture = *mask_texture;
		bufs[ 1 ] = GL_COLOR_ATTACHMENT1;
		width = mask_texture->width;
		height = mask_texture->height;
	}
	if( depth_texture != NULL ) {
		glNamedFramebufferTexture( fb.fbo, GL_DEPTH_ATTACHMENT, depth_texture->texture, 0 );
		fb.depth_texture = *depth_texture;
		width = depth_texture->width;
		height = depth_texture->height;
	}
	glNamedFramebufferDrawBuffers( fb.fbo, ARRAY_COUNT( bufs ), bufs );

	Assert( glCheckNamedFramebufferStatus( fb.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	Assert( width > 0 && height > 0 );

	fb.width = width;
	fb.height = height;

	return fb;
}

Framebuffer NewShadowFramebuffer( TextureArray texture_array, u32 layer ) {
	Framebuffer fb = { };

	glCreateFramebuffers( 1, &fb.fbo );

	glNamedFramebufferTextureLayer( fb.fbo, GL_DEPTH_ATTACHMENT, texture_array.texture, 0, layer );

	Assert( glCheckNamedFramebufferStatus( fb.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );

	fb.width = frame_static.shadow_parameters.shadowmap_res;
	fb.height = frame_static.shadow_parameters.shadowmap_res;

	return fb;
}

void DeleteFramebuffer( Framebuffer fb ) {
	if( fb.fbo == 0 )
		return;

	glDeleteFramebuffers( 1, &fb.fbo );
	DeleteTexture( fb.albedo_texture );
	DeleteTexture( fb.mask_texture );
	DeleteTexture( fb.depth_texture );
}

static GLuint CompileShader( GLenum type, const char * body, const char * name ) {
	TempAllocator temp = cls.frame_arena.temp();

	DynamicString src( &temp, "#version 450 core\n" );
	if( type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER ) {
		constexpr const char * vertex_shader_prelude =
			"#define VERTEX_SHADER 1\n"
			"#define v2f out\n";
		constexpr const char * fragment_shader_prelude =
			"#define FRAGMENT_SHADER 1\n"
			"#define v2f in\n";

		src += type == GL_VERTEX_SHADER ? vertex_shader_prelude : fragment_shader_prelude;
	}

	src += body;

	GLuint shader = glCreateShader( type );
	const char * nice_api = src.c_str();
	glShaderSource( shader, 1, &nice_api, NULL );
	glCompileShader( shader );

	GLint status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );

	if( status == GL_FALSE ) {
		char buf[ 1024 ];
		glGetShaderInfoLog( shader, sizeof( buf ), NULL, buf );
		Com_Printf( S_COLOR_YELLOW "Shader compilation failed %s: %s\n", name, buf );
		glDeleteShader( shader );

		// static char src[ 65536 ];
		// glGetShaderSource( shader, sizeof( src ), NULL, src );
		// Com_Printf( "%s\n", src );

		return 0;
	}

	return shader;
}

static bool LinkShader( Shader * shader, GLuint program ) {
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

	GLint count;
	glGetProgramiv( program, GL_ACTIVE_UNIFORMS, &count );

	size_t num_textures = 0;
	size_t num_texture_arrays = 0;
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint size, len;
		GLenum type;
		glGetActiveUniform( program, i, sizeof( name ), &len, &size, &type, name );

		if( type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_MULTISAMPLE || type == GL_UNSIGNED_INT_SAMPLER_2D || type == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE ) {
			if( num_textures == ARRAY_COUNT( shader->textures ) ) {
				glDeleteProgram( program );
				Com_Printf( S_COLOR_YELLOW "Too many samplers in shader\n" );
				return false;
			}

			glProgramUniform1i( program, glGetUniformLocation( program, name ), num_textures );
			shader->textures[ num_textures ] = Hash64( name, len );
			num_textures++;
		}

		if( type == GL_SAMPLER_2D_ARRAY || type == GL_SAMPLER_2D_ARRAY_SHADOW ) {
			if( num_texture_arrays == ARRAY_COUNT( shader->texture_arrays ) ) {
				glDeleteProgram( program );
				Com_Printf( S_COLOR_YELLOW "Too many samplers in shader\n" );
				return false;
			}

			glProgramUniform1i( program, glGetUniformLocation( program, name ), ARRAY_COUNT( &Shader::textures ) + num_texture_arrays );
			shader->texture_arrays[ num_texture_arrays ] = Hash64( name, len );
			num_texture_arrays++;
		}
	}

	glGetProgramInterfaceiv( program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &count );
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint len;
		glGetProgramResourceName( program, GL_UNIFORM_BLOCK, i, sizeof( name ), &len, name );
		glUniformBlockBinding( program, i, i );
		shader->uniforms[ i ] = Hash64( name, len );
	}

	glGetProgramInterfaceiv( program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count );
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint len;
		glGetProgramResourceName( program, GL_SHADER_STORAGE_BLOCK, i, sizeof( name ), &len, name );
		glShaderStorageBlockBinding( program, i, i );
		shader->buffers[ i ] = Hash64( name, len );
	}

	return true;
}

bool NewShader( Shader * shader, const char * src, const char * name ) {
	TempAllocator temp = cls.frame_arena.temp();

	*shader = { };

	const char * vertex_shader_name = temp( "{} [VS]", name );
	GLuint vertex_shader = CompileShader( GL_VERTEX_SHADER, src, vertex_shader_name );
	if( vertex_shader == 0 )
		return false;
	DebugLabel( GL_SHADER, vertex_shader, vertex_shader_name );
	defer { glDeleteShader( vertex_shader ); };

	const char * fragment_shader_name = temp( "{} [FS]", name );
	GLuint fragment_shader = CompileShader( GL_FRAGMENT_SHADER, src, fragment_shader_name );
	if( fragment_shader == 0 )
		return false;
	DebugLabel( GL_SHADER, fragment_shader, fragment_shader_name );
	defer { glDeleteShader( fragment_shader ); };

	shader->program = glCreateProgram();
	DebugLabel( GL_PROGRAM, shader->program, name );
	glAttachShader( shader->program, vertex_shader );
	glAttachShader( shader->program, fragment_shader );

	glBindAttribLocation( shader->program, VertexAttribute_Position, "a_Position" );
	glBindAttribLocation( shader->program, VertexAttribute_Normal, "a_Normal" );
	glBindAttribLocation( shader->program, VertexAttribute_TexCoord, "a_TexCoord" );
	glBindAttribLocation( shader->program, VertexAttribute_Color, "a_Color" );
	glBindAttribLocation( shader->program, VertexAttribute_JointIndices, "a_JointIndices" );
	glBindAttribLocation( shader->program, VertexAttribute_JointWeights, "a_JointWeights" );

	glBindFragDataLocation( shader->program, 0, "f_Albedo" );
	glBindFragDataLocation( shader->program, 1, "f_Mask" );

	return LinkShader( shader, shader->program );
}

bool NewComputeShader( Shader * shader, const char * src, const char * name ) {
	TempAllocator temp = cls.frame_arena.temp();

	*shader = { };

	GLuint cs = CompileShader( GL_COMPUTE_SHADER, src, name );
	if( cs == 0 )
		return false;
	DebugLabel( GL_SHADER, cs, temp( "{} [CS]", name ) );
	defer { glDeleteShader( cs ); };

	shader->program = glCreateProgram();
	DebugLabel( GL_PROGRAM, shader->program, name );
	glAttachShader( shader->program, cs );

	return LinkShader( shader, shader->program );
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

Mesh NewMesh( const MeshConfig & config ) {
	GLuint vao;
	glCreateVertexArrays( 1, &vao );
	DebugLabel( GL_VERTEX_ARRAY, vao, config.name );

	for( size_t i = 0; i < ARRAY_COUNT( config.vertex_descriptor.attributes ); i++ ) {
		if( !config.vertex_descriptor.attributes[ i ].exists )
			continue;

		const VertexAttribute & attribute = config.vertex_descriptor.attributes[ i ].value;

		GLenum type;
		int num_components;
		bool integral;
		GLboolean normalized;
		u32 stride = config.vertex_descriptor.buffer_strides[ attribute.buffer ];
		VertexFormatToGL( attribute.format, &type, &num_components, &integral, &normalized, stride == 0 ? &stride : NULL );

		glEnableVertexArrayAttrib( vao, i );
		glVertexArrayVertexBuffer( vao, i, config.vertex_buffers[ attribute.buffer ].buffer, 0, stride );

		if( integral && !normalized ) {
			/*
			 * wintel driver ignores the type and treats everything as u32
			 * non-DSA call works fine so fall back to that here
			 *
			 * see also https://doc.magnum.graphics/magnum/opengl-workarounds.html
			 *
			 * glVertexArrayAttribIFormat( vao, attribute, num_components, type, attribute.offset );
			 */

			glBindVertexArray( vao );
			glVertexAttribIFormat( i, num_components, type, attribute.offset );
			glBindVertexArray( 0 );
		}
		else {
			glVertexArrayAttribFormat( vao, i, num_components, type, normalized, attribute.offset );
		}
	}

	if( config.index_buffer.buffer != 0 ) {
		glVertexArrayElementBuffer( vao, config.index_buffer.buffer );
	}

	Mesh mesh = { };
	mesh.vao = vao;
	mesh.index_format = config.index_format;
	mesh.num_vertices = config.num_vertices;
	mesh.cw_winding = config.cw_winding;

	for( size_t i = 0; i < ARRAY_COUNT( mesh.vertex_buffers ); i++ ) {
		mesh.vertex_buffers[ i ] = config.vertex_buffers[ i ];
	}
	mesh.index_buffer = config.index_buffer;

	return mesh;
}

void DeleteMesh( const Mesh & mesh ) {
	if( mesh.vao == 0 )
		return;

	for( GPUBuffer buffer : mesh.vertex_buffers ) {
		DeleteGPUBuffer( buffer );
	}
	DeleteGPUBuffer( mesh.index_buffer );

	glDeleteVertexArrays( 1, &mesh.vao );
}

void DeferDeleteMesh( const Mesh & mesh ) {
	deferred_mesh_deletes.add( mesh );
}

void DrawMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_vertices_override, u32 first_index ) {
	Assert( in_frame );
	Assert( pipeline.pass != U8_MAX );
	Assert( pipeline.shader != NULL );

	DrawCall dc = { };
	dc.type = DrawCallType_Normal;
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_vertices = num_vertices_override == 0 ? mesh.num_vertices : num_vertices_override;
	dc.num_instances = 1;
	dc.first_index = first_index;
	draw_calls.add( dc );

	num_vertices_this_frame += dc.num_vertices;
}

void DrawInstancedMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_instances, u32 num_vertices_override, u32 first_index ) {
	Assert( in_frame );
	Assert( pipeline.pass != U8_MAX );
	Assert( pipeline.shader != NULL );

	DrawCall dc = { };
	dc.type = DrawCallType_Normal;
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_vertices = num_vertices_override == 0 ? mesh.num_vertices : num_vertices_override;
	dc.first_index = first_index;
	dc.num_instances = num_instances;
	draw_calls.add( dc );

	num_vertices_this_frame += dc.num_vertices * num_instances;
}

static u8 AddRenderPass( const RenderPass & pass ) {
	return checked_cast< u8 >( render_passes.add( pass ) );
}

u8 AddRenderPass( const tracy::SourceLocationData * tracy, Framebuffer target, ClearColor clear_color, ClearDepth clear_depth ) {
	RenderPass pass;
	pass.type = RenderPass_Normal;
	pass.target = target;
	pass.clear_color = clear_color == ClearColor_Do;
	pass.clear_depth = clear_depth == ClearDepth_Do;
	pass.tracy = tracy;
	return AddRenderPass( pass );
}

u8 AddRenderPass( const tracy::SourceLocationData * tracy, ClearColor clear_color, ClearDepth clear_depth ) {
	Framebuffer target = { };
	return AddRenderPass( tracy, target, clear_color, clear_depth );
}

u8 AddBarrierRenderPass( const tracy::SourceLocationData * tracy, Framebuffer target ) {
	RenderPass pass;
	pass.type = RenderPass_Normal;
	pass.target = target;
	pass.barrier = true;
	pass.tracy = tracy;
	return AddRenderPass( pass );
}

u8 AddUnsortedRenderPass( const tracy::SourceLocationData * tracy, Framebuffer target ) {
	RenderPass pass;
	pass.type = RenderPass_Normal;
	pass.target = target;
	pass.sorted = false;
	pass.tracy = tracy;
	return AddRenderPass( pass );
}

void AddBlitPass( const tracy::SourceLocationData * tracy, Framebuffer src, Framebuffer dst, ClearColor clear_color, ClearDepth clear_depth ) {
	RenderPass pass;
	pass.type = RenderPass_Blit;
	pass.tracy = tracy;
	pass.blit_source = src;
	pass.target = dst;
	pass.clear_color = clear_color;
	pass.clear_depth = clear_depth;
	AddRenderPass( pass );
}

void AddResolveMSAAPass( const tracy::SourceLocationData * tracy, Framebuffer src, Framebuffer dst, ClearColor clear_color, ClearDepth clear_depth ) {
	AddBlitPass( tracy, src, dst, clear_color, clear_depth );
}

void DispatchCompute( const PipelineState & pipeline, u32 x, u32 y, u32 z ) {
	DrawCall dc = { };
	dc.type = DrawCallType_Compute;
	dc.pipeline = pipeline;
	dc.dispatch_size[ 0 ] = x;
	dc.dispatch_size[ 1 ] = y;
	dc.dispatch_size[ 2 ] = z;
	draw_calls.add( dc );
}

void DispatchComputeIndirect( const PipelineState & pipeline, GPUBuffer indirect ) {
	DrawCall dc = { };
	dc.type = DrawCallType_IndirectCompute;
	dc.pipeline = pipeline;
	dc.indirect = indirect;
	draw_calls.add( dc );
}

void DrawInstancedParticles( const Mesh & mesh, const PipelineState & pipeline, GPUBuffer indirect ) {
	DrawCall dc = { };
	dc.type = DrawCallType_Indirect;
	dc.pipeline = pipeline;
	dc.mesh = mesh;
	dc.indirect = indirect;
	draw_calls.add( dc );
}

void DownloadFramebuffer( void * buf ) {
	glReadPixels( 0, 0, frame_static.viewport_width, frame_static.viewport_height, GL_RGB, GL_UNSIGNED_BYTE, buf );
}
