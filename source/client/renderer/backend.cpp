#include <algorithm> // std::stable_sort
#include <new>

#include "glad/glad.h"

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

enum VertexAttribute : GLuint {
	VertexAttribute_Position,
	VertexAttribute_Normal,
	VertexAttribute_TexCoord,
	VertexAttribute_Color,
	VertexAttribute_JointIndices,
	VertexAttribute_JointWeights,

	// instance stuff
	VertexAttribute_MaterialColor,
	VertexAttribute_MaterialTextureMatrix0,
	VertexAttribute_MaterialTextureMatrix1,
	VertexAttribute_OutlineHeight,
	VertexAttribute_ModelTransformRow0,
	VertexAttribute_ModelTransformRow1,
	VertexAttribute_ModelTransformRow2,

	VertexAttribute_ParticlePosition = VertexAttribute_Color,
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

	InstanceType instance_type;
	u32 num_instances;
	GPUBuffer instance_data;
	GPUBuffer update_data;
	GPUBuffer feedback_data;
};

static NonRAIIDynamicArray< RenderPass > render_passes;
static NonRAIIDynamicArray< DrawCall > draw_calls;

static NonRAIIDynamicArray< Mesh > deferred_mesh_deletes;
static NonRAIIDynamicArray< GPUBuffer > deferred_buffer_deletes;

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

static u32 GLTypeSize( GLenum type ) {
	switch( type ) {
		case GL_UNSIGNED_BYTE: return 1;
		case GL_UNSIGNED_SHORT: return 2;
		case GL_UNSIGNED_INT:
		case GL_FLOAT:
			return 4;
	}

	assert( false );
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
			assert( false );
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
	    id == 131204
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
	assert( label != NULL );
	glObjectLabel( type, object, -1, label );
}

static void PlotVRAMUsage() {
	if( !is_public_build && GLAD_GL_NVX_gpu_memory_info != 0 ) {
		GLint total_vram;
		glGetIntegerv( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_vram );

		GLint available_vram;
		glGetIntegerv( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available_vram );

		TracyCPlot( "VRAM usage", s64( total_vram - available_vram ) );
	}
}

static GLuint DSACreateBuffer( bool ubo ) {
	GLuint buf;
	if( GLAD_GL_ARB_direct_state_access != 0 ) {
		glCreateBuffers( 1, &buf );
	}
	else {
		GLenum target = ubo ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
		glGenBuffers( 1, &buf );
		glBindBuffer( target, buf );
		glBindBuffer( target, 0 );
	}
	return buf;
}

static void DSAHacks() {
	if( GLAD_GL_ARB_direct_state_access == 0 ) {
		glCreateVertexArrays = []( GLsizei n, GLuint * vaos ) {
			assert( n == 1 );
			glGenVertexArrays( 1, vaos );
			glBindVertexArray( vaos[ 0 ] );
			glBindVertexArray( 0 );
		};
		glVertexArrayElementBuffer = []( GLuint vao, GLuint buf ) {
			glBindVertexArray( vao );
			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buf );
			glBindVertexArray( 0 );
		};
		glEnableVertexArrayAttrib = glEnableVertexArrayAttribEXT;

		glNamedBufferStorage = glNamedBufferStorageEXT;
		glNamedBufferSubData = glNamedBufferSubDataEXT;
		glGetNamedBufferSubData = glGetNamedBufferSubDataEXT;
		glMapNamedBufferRange = glMapNamedBufferRangeEXT;
		glUnmapNamedBuffer = glUnmapNamedBufferEXT;

		glCreateTextures = []( GLenum target, GLsizei n, GLuint * textures ) {
			assert( n == 1 );
			glGenTextures( 1, textures );
			glBindTexture( target, textures[ 0 ] );
			glBindTexture( target, 0 );
		};

		glCreateFramebuffers = []( GLsizei n, GLuint * fbos ) {
			assert( n == 1 );
			glGenFramebuffers( 1, fbos );
			glBindFramebuffer( GL_FRAMEBUFFER, fbos[ 0 ] );
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		};
		glCheckNamedFramebufferStatus = glCheckNamedFramebufferStatusEXT;
		glNamedFramebufferTextureLayer = glNamedFramebufferTextureLayerEXT;
		glNamedFramebufferTexture = glNamedFramebufferTextureEXT;
		glNamedFramebufferDrawBuffers = glFramebufferDrawBuffersEXT;
		glBlitNamedFramebuffer = []( GLuint src, GLuint dst, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter ) {
			GLint bound;
			glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &bound );
			assert( bound >= 0 && GLuint( bound ) == dst );

			glBindFramebuffer( GL_READ_FRAMEBUFFER, src );
			glBlitFramebuffer( srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter );
			glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		};
	}
	else if( GLAD_GL_EXT_direct_state_access == 0 ) {
		glVertexArrayVertexAttribOffsetEXT = []( GLuint vao, GLuint buffer, GLuint index, GLint num_components, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset ) {
			glVertexArrayVertexBuffer( vao, index, buffer, 0, stride );
			glVertexArrayAttribFormat( vao, index, num_components, type, normalized, offset );
		};
		glVertexArrayVertexAttribIOffsetEXT = []( GLuint vao, GLuint buffer, GLuint index, GLint num_components, GLenum type, GLsizei stride, GLintptr offset ) {
			glVertexArrayVertexBuffer( vao, index, buffer, 0, stride );
			glVertexArrayAttribIFormat( vao, index, num_components, type, offset );
		};

		glTextureStorage2DEXT = []( GLuint tex, GLenum target, GLsizei mips, GLenum format, GLsizei w, GLsizei h ) { glTextureStorage2D( tex, mips, format, w, h ); };
		glTextureStorage3DEXT = []( GLuint tex, GLenum target, GLsizei mips, GLenum format, GLsizei w, GLsizei h, GLsizei d ) { glTextureStorage3D( tex, mips, format, w, h, d ); };
		glTextureStorage2DMultisampleEXT = []( GLuint tex, GLenum target, GLsizei samples, GLenum format, GLsizei w, GLsizei h, GLboolean fixed_sample_locations ) {
			glTextureStorage2DMultisample( tex, samples, format, w, h, fixed_sample_locations );
		};
		glTextureParameterfEXT = []( GLuint tex, GLenum target, GLenum param, GLfloat value ) { glTextureParameterf( tex, param, value ); };
		glTextureParameterfvEXT = []( GLuint tex, GLenum target, GLenum param, const GLfloat * values ) { glTextureParameterfv( tex, param, values ); };
		glTextureParameteriEXT = []( GLuint tex, GLenum target, GLenum param, GLint value ) { glTextureParameteri( tex, param, value ); };
		glTextureSubImage2DEXT = []( GLuint tex, GLenum target, GLint mip, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, const void * data ) {
			glTextureSubImage2D( tex, mip, x, y, w, h, format, type, data );
		};
		glCompressedTextureSubImage2DEXT = []( GLuint tex, GLenum target, GLint mip, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLsizei n, const void * data ) {
			glCompressedTextureSubImage2D( tex, mip, x, y, w, h, format, n, data );
		};
		glCompressedTextureSubImage3DEXT = []( GLuint tex, GLenum target, GLint mip, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum format, GLsizei n, const void * data ) {
			glCompressedTextureSubImage3D( tex, mip, x, y, z, w, h, d, format, n, data );
		};
		glBindMultiTextureEXT = []( GLuint unit, GLenum target, GLuint tex ) {
			glBindTextureUnit( unit - GL_TEXTURE0, tex );
		};
	}
}

static StreamingBuffer NewStreamingBuffer( u32 len, const char * name, bool ubo );

void InitRenderBackend() {
	TracyZoneScoped;
	TracyGpuContext;

	{
		struct {
			const char * name;
			int loaded;
		} required_extensions[] = {
			{ "GL_ARB_buffer_storage", GLAD_GL_ARB_buffer_storage },
			{ "GL_ARB_clip_control", GLAD_GL_ARB_clip_control },
			{ "GL_ARB_direct_state_access/GL_EXT_direct_state_access", GLAD_GL_ARB_direct_state_access + GLAD_GL_EXT_direct_state_access },
			{ "GL_EXT_texture_compression_s3tc", GLAD_GL_EXT_texture_compression_s3tc },
			{ "GL_EXT_texture_filter_anisotropic", GLAD_GL_EXT_texture_filter_anisotropic },
			{ "GL_EXT_texture_sRGB", GLAD_GL_EXT_texture_sRGB },
			{ "GL_EXT_texture_sRGB_decode", GLAD_GL_EXT_texture_sRGB_decode },
		};

		String< 1024 > missing_extensions( "Your GPU doesn't have some required OpenGL extensions:" );
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
			Fatal( "GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS too small" );
		}
	}

	DSAHacks();

	{
		GLint context_flags;
		glGetIntegerv( GL_CONTEXT_FLAGS, &context_flags );
		if( context_flags & GL_CONTEXT_FLAG_DEBUG_BIT ) {
			Com_Printf( "Initialising OpenGL debug output\n" );

			glEnable( GL_DEBUG_OUTPUT );
			glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
			glDebugMessageCallback( ( GLDEBUGPROC ) DebugOutputCallback, NULL );
			glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE );
		}
	}

	PlotVRAMUsage();

	render_passes.init( sys_allocator );
	draw_calls.init( sys_allocator );

	deferred_mesh_deletes.init( sys_allocator );
	deferred_buffer_deletes.init( sys_allocator );

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

	for( size_t i = 0; i < ARRAY_COUNT( ubos ); i++ ) {
		TempAllocator temp = cls.frame_arena.temp();
		ubos[ i ].stream = NewStreamingBuffer( UNIFORM_BUFFER_SIZE, temp( "UBO {}", i ), true );
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

	render_passes.shutdown();
	draw_calls.shutdown();

	deferred_mesh_deletes.shutdown();
	deferred_buffer_deletes.shutdown();
}

void RenderBackendBeginFrame() {
	assert( !in_frame );
	in_frame = true;

	render_passes.clear();
	draw_calls.clear();

	deferred_mesh_deletes.clear();
	deferred_buffer_deletes.clear();

	num_vertices_this_frame = 0;

	for( UBO & ubo : ubos ) {
		StreamingBufferFrame( &ubo.stream );
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
		const Texture * prev_texture = prev_bindings.textures[ i ];

		bool found = prev_texture == NULL;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_textures; j++ ) {
				if( pipeline.textures[ j ].name_hash == name_hash ) {
					const Texture * texture = pipeline.textures[ j ].texture;
					if( texture != prev_texture ) {
						GLenum target = texture->msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
						if( prev_texture != NULL && prev_texture->msaa != texture->msaa ) {
							GLenum prev_target = prev_texture->msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
							glBindMultiTextureEXT( GL_TEXTURE0 + i, prev_target, 0 );
						}
						glBindMultiTextureEXT( GL_TEXTURE0 + i, target, texture->texture );
						prev_bindings.textures[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found && prev_texture != NULL ) {
			GLenum prev_target = prev_texture->msaa ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
			glBindMultiTextureEXT( GL_TEXTURE0 + i, prev_target, 0 );
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
						glBindMultiTextureEXT( GL_TEXTURE0 + tex_unit, GL_TEXTURE_2D_ARRAY, texture.texture );
						prev_bindings.texture_arrays[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found ) {
			glBindMultiTextureEXT( GL_TEXTURE0 + tex_unit, GL_TEXTURE_2D_ARRAY, 0 );
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

static void SetupAttribute( GLuint vao, GLuint buffer, GLuint index, VertexFormat format, u32 stride = 0, u32 offset = 0 ) {
	if( buffer == 0 )
		return;

	GLenum type;
	int num_components;
	bool integral;
	GLboolean normalized;
	VertexFormatToGL( format, &type, &num_components, &integral, &normalized, stride == 0 ? &stride : NULL );

	glEnableVertexArrayAttrib( vao, index );
	if( integral && !normalized ) {
		glVertexArrayVertexAttribIOffsetEXT( vao, buffer, index, num_components, type, stride, offset );
	}
	else {
		glVertexArrayVertexAttribOffsetEXT( vao, buffer, index, num_components, type, normalized, stride, offset );
	}
}

static void SubmitFramebufferBlit( const RenderPass & pass ) {
	Framebuffer src = pass.blit_source;
	Framebuffer target = pass.target;
	GLbitfield clear_mask = 0;
	clear_mask |= pass.clear_color ? GL_COLOR_BUFFER_BIT : 0;
	clear_mask |= pass.clear_depth ? GL_DEPTH_BUFFER_BIT : 0;
	glBlitNamedFramebuffer( src.fbo, target.fbo, 0, 0, src.width, src.height, 0, 0, target.width, target.height, clear_mask, GL_NEAREST );
}

static void SetupRenderPass( const RenderPass & pass ) {
	TracyZoneScoped;
	TracyZoneText( pass.name, strlen( pass.name ) );
#if TRACY_ENABLE
	renderpass_zone = new (renderpass_zone_memory) tracy::GpuCtxScope( pass.tracy );
#endif

	glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass.name );

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

	SetPipelineState( dc.pipeline, dc.mesh.ccw_winding );

	if( dc.instance_type == InstanceType_ComputeShader ) {
		glDispatchCompute( 1, 1, 1 );
		return;
	}

	glBindVertexArray( dc.mesh.vao );
	GLenum primitive = PrimitiveTypeToGL( dc.mesh.primitive_type );

	if( dc.instance_type == InstanceType_Particles ) {
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticlePosition, VertexFormat_Floatx4, sizeof( GPUParticle ), offsetof( GPUParticle, position ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleVelocity, VertexFormat_Floatx4, sizeof( GPUParticle ), offsetof( GPUParticle, velocity ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleAccelDragRest, VertexFormat_Floatx3, sizeof( GPUParticle ), offsetof( GPUParticle, acceleration ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleUVWH, VertexFormat_Floatx4, sizeof( GPUParticle ), offsetof( GPUParticle, uvwh ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleStartColor, VertexFormat_U8x4_Norm, sizeof( GPUParticle ), offsetof( GPUParticle, start_color ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleEndColor, VertexFormat_U8x4_Norm, sizeof( GPUParticle ), offsetof( GPUParticle, end_color ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleSize, VertexFormat_Floatx2, sizeof( GPUParticle ), offsetof( GPUParticle, start_size ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleAgeLifetime, VertexFormat_Floatx2, sizeof( GPUParticle ), offsetof( GPUParticle, age ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ParticleFlags, VertexFormat_U32x1, sizeof( GPUParticle ), offsetof( GPUParticle, flags ) );

		if( dc.update_data.buffer ) {
			glEnable( GL_RASTERIZER_DISCARD );
			glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, dc.update_data.buffer );
			if( dc.feedback_data.buffer ) {
				glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 1, dc.feedback_data.buffer );
			}

			glBeginTransformFeedback( primitive );
			GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			const void * offset = ( const void * ) uintptr_t( dc.index_offset );
			glDrawElements( primitive, dc.num_instances, type, offset );
			glEndTransformFeedback();

			glBindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0 );
			if( dc.feedback_data.buffer ) {
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
	else if( dc.instance_type == InstanceType_Model ) {
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_MaterialColor, VertexFormat_Floatx4, sizeof( GPUModelInstance ), offsetof( GPUModelInstance, material.color ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_MaterialTextureMatrix0, VertexFormat_Floatx3, sizeof( GPUModelInstance ), offsetof( GPUModelInstance, material.tcmod[ 0 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_MaterialTextureMatrix1, VertexFormat_Floatx3, sizeof( GPUModelInstance ), offsetof( GPUModelInstance, material.tcmod[ 1 ] ) );

		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow0, VertexFormat_Floatx4, sizeof( GPUModelInstance ), offsetof( GPUModelInstance, transform[ 0 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow1, VertexFormat_Floatx4, sizeof( GPUModelInstance ), offsetof( GPUModelInstance, transform[ 1 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow2, VertexFormat_Floatx4, sizeof( GPUModelInstance ), offsetof( GPUModelInstance, transform[ 2 ] ) );

		glVertexAttribDivisor( VertexAttribute_MaterialColor, 1 );
		glVertexAttribDivisor( VertexAttribute_MaterialTextureMatrix0, 1 );
		glVertexAttribDivisor( VertexAttribute_MaterialTextureMatrix1, 1 );

		glVertexAttribDivisor( VertexAttribute_ModelTransformRow0, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow1, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow2, 1 );

		GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced( primitive, dc.num_vertices, type, 0, dc.num_instances );
	}
	else if( dc.instance_type == InstanceType_ModelShadows ) {
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow0, VertexFormat_Floatx4, sizeof( GPUModelShadowsInstance ), offsetof( GPUModelShadowsInstance, transform[ 0 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow1, VertexFormat_Floatx4, sizeof( GPUModelShadowsInstance ), offsetof( GPUModelShadowsInstance, transform[ 1 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow2, VertexFormat_Floatx4, sizeof( GPUModelShadowsInstance ), offsetof( GPUModelShadowsInstance, transform[ 2 ] ) );

		glVertexAttribDivisor( VertexAttribute_ModelTransformRow0, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow1, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow2, 1 );

		GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced( primitive, dc.num_vertices, type, 0, dc.num_instances );
	}
	else if( dc.instance_type == InstanceType_ModelOutlines ) {
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_MaterialColor, VertexFormat_Floatx4, sizeof( GPUModelOutlinesInstance ), offsetof( GPUModelOutlinesInstance, color ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_OutlineHeight, VertexFormat_Floatx1, sizeof( GPUModelOutlinesInstance ), offsetof( GPUModelOutlinesInstance, height ) );

		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow0, VertexFormat_Floatx4, sizeof( GPUModelOutlinesInstance ), offsetof( GPUModelOutlinesInstance, transform[ 0 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow1, VertexFormat_Floatx4, sizeof( GPUModelOutlinesInstance ), offsetof( GPUModelOutlinesInstance, transform[ 1 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow2, VertexFormat_Floatx4, sizeof( GPUModelOutlinesInstance ), offsetof( GPUModelOutlinesInstance, transform[ 2 ] ) );

		glVertexAttribDivisor( VertexAttribute_MaterialColor, 1 );
		glVertexAttribDivisor( VertexAttribute_OutlineHeight, 1 );

		glVertexAttribDivisor( VertexAttribute_ModelTransformRow0, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow1, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow2, 1 );

		GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced( primitive, dc.num_vertices, type, 0, dc.num_instances );
	}
	else if( dc.instance_type == InstanceType_ModelSilhouette ) {
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_MaterialColor, VertexFormat_Floatx4, sizeof( GPUModelSilhouetteInstance ), offsetof( GPUModelSilhouetteInstance, color ) );

		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow0, VertexFormat_Floatx4, sizeof( GPUModelSilhouetteInstance ), offsetof( GPUModelSilhouetteInstance, transform[ 0 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow1, VertexFormat_Floatx4, sizeof( GPUModelSilhouetteInstance ), offsetof( GPUModelSilhouetteInstance, transform[ 1 ] ) );
		SetupAttribute( dc.mesh.vao, dc.instance_data.buffer, VertexAttribute_ModelTransformRow2, VertexFormat_Floatx4, sizeof( GPUModelSilhouetteInstance ), offsetof( GPUModelSilhouetteInstance, transform[ 2 ] ) );

		glVertexAttribDivisor( VertexAttribute_MaterialColor, 1 );

		glVertexAttribDivisor( VertexAttribute_ModelTransformRow0, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow1, 1 );
		glVertexAttribDivisor( VertexAttribute_ModelTransformRow2, 1 );

		GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced( primitive, dc.num_vertices, type, 0, dc.num_instances );
	}
	else if( dc.mesh.indices.buffer != 0 ) {
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
	TracyZoneScoped;

	assert( in_frame );
	assert( render_passes.size() > 0 );
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

	{
		TracyZoneScopedN( "Deferred deletes" );

		for( const Mesh & mesh : deferred_mesh_deletes ) {
			DeleteMesh( mesh );
		}
		deferred_mesh_deletes.clear();

		for( const GPUBuffer & buffer : deferred_buffer_deletes ) {
			DeleteGPUBuffer( buffer );
		}

		deferred_buffer_deletes.clear();
	}

	u32 ubo_bytes_used = 0;
	for( const UBO & ubo : ubos ) {
		ubo_bytes_used += ubo.bytes_used;
	}
	TracyCPlot( "UBO utilisation", float( ubo_bytes_used ) / float( UNIFORM_BUFFER_SIZE * ARRAY_COUNT( ubos ) ) );

	TracyCPlot( "Draw calls", s64( draw_calls.size() ) );
	TracyCPlot( "Vertices", s64( num_vertices_this_frame ) );

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

GPUBuffer NewParticleGPUBuffer( u32 n ) {
	GPUBuffer buf = { DSACreateBuffer( false ) };
	glNamedBufferStorage( buf.buffer, n * sizeof( GPUParticle ), NULL, GL_DYNAMIC_STORAGE_BIT );
	return buf;
}

GPUBuffer NewGPUBuffer( const void * data, u32 len, const char * name ) {
	GPUBuffer buf = { DSACreateBuffer( false ) };
	// TODO: probably want more control over flags than this
	glNamedBufferStorage( buf.buffer, len, data, data != NULL ? 0 : GL_DYNAMIC_STORAGE_BIT );

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

static StreamingBuffer NewStreamingBuffer( u32 len, const char * name, bool ubo ) {
	StreamingBuffer stream = { };

	if( name != NULL ) {
		stream.name = MakeSpan( CopyString( sys_allocator, name ) );
	}

	for( size_t i = 0; i < ARRAY_COUNT( stream.buffers ); i++ ) {
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		stream.buffers[ i ].buffer = DSACreateBuffer( ubo );
		glNamedBufferStorage( stream.buffers[ i ].buffer, len, NULL, flags );

		if( name != NULL ) {
			TempAllocator temp = cls.frame_arena.temp();
			DebugLabel( GL_BUFFER, stream.buffers[ i ].buffer, temp( "{} #{}", name, i ) );
		}

		stream.mappings[ i ] = ( u8 * ) glMapNamedBufferRange( stream.buffers[ i ].buffer, 0, len, flags );
	}

	return stream;
}

StreamingBuffer NewStreamingBuffer( u32 len, const char * name ) {
	return NewStreamingBuffer( len, name, false );
}

void StreamingBufferFrame( StreamingBuffer * stream ) {
	TracyZoneScoped;
	if( stream->name.ptr != NULL ) {
		TracyZoneText( stream->name.ptr, stream->name.n );
	}

	stream->fences[ stream->current ] = bit_cast< u64 >( glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 ) );
	stream->current = ( stream->current + 1 ) % ARRAY_COUNT( stream->buffers );

	if( stream->fences[ stream->current ] != 0 ) {
		GLsync sync = bit_cast< GLsync >( stream->fences[ stream->current ] );
		glClientWaitSync( sync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED );
		glDeleteSync( sync );
		stream->fences[ stream->current ] = 0;
	}
}

u8 * GetStreamingBufferMapping( StreamingBuffer stream ) {
	return stream.mappings[ stream.current ];
}

GPUBuffer GetStreamingBufferBuffer( StreamingBuffer stream ) {
	return stream.buffers[ stream.current ];
}

void DeleteStreamingBuffer( StreamingBuffer stream ) {
	for( GPUBuffer buf : stream.buffers ) {
		glUnmapNamedBuffer( buf.buffer );
		DeleteGPUBuffer( buf );
	}

	for( u64 fence : stream.fences ) {
		if( fence != 0 ) {
			glDeleteSync( bit_cast< GLsync >( fence ) );
		}
	}

	FREE( sys_allocator, stream.name.ptr );
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

	glTextureStorage2DEXT( texture.texture, target, config.num_mipmaps, internal_format, config.width, config.height );

	glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_WRAP_S, TextureWrapToGL( config.wrap ) );
	glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_WRAP_T, TextureWrapToGL( config.wrap ) );

	GLenum min_filter = GL_NONE;
	GLenum mag_filter = GL_NONE;
	TextureFilterToGL( config.filter, &min_filter, &mag_filter );
	glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_MIN_FILTER, min_filter );
	glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_MAG_FILTER, mag_filter );
	glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_MAX_LEVEL, config.num_mipmaps - 1 );

	if( config.wrap == TextureWrap_Border ) {
		glTextureParameterfvEXT( texture.texture, target, GL_TEXTURE_BORDER_COLOR, config.border_color.ptr() );
	}

	if( !CompressedTextureFormat( config.format ) ) {
		assert( config.num_mipmaps == 1 );

		if( channels == GL_RED ) {
			if( config.format == TextureFormat_A_U8 ) {
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_R, GL_ONE );
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_G, GL_ONE );
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_B, GL_ONE );
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_A, GL_RED );
			}
			else {
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_R, GL_RED );
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_G, GL_RED );
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_B, GL_RED );
				glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_A, GL_ONE );
			}
		}
		else if( channels == GL_RG && config.format == TextureFormat_RA_U8 ) {
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_R, GL_RED );
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_G, GL_RED );
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_B, GL_RED );
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_A, GL_GREEN );
		}

		if( config.data != NULL ) {
			glTextureSubImage2DEXT( texture.texture, target, 0, 0, 0,
				config.width, config.height, channels, type, config.data );
		}
	}
	else {
		assert( config.data != NULL );

		glTextureParameterfEXT( texture.texture, target, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filtering );

		if( config.format == TextureFormat_BC4 ) {
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_R, GL_ONE );
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_G, GL_ONE );
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_B, GL_ONE );
			glTextureParameteriEXT( texture.texture, target, GL_TEXTURE_SWIZZLE_A, GL_RED );
		}

		const char * cursor = ( const char * ) config.data;
		for( u32 i = 0; i < config.num_mipmaps; i++ ) {
			u32 w = config.width >> i;
			u32 h = config.height >> i;
			u32 size = ( BitsPerPixel( config.format ) * w * h ) / 8;
			assert( size < S32_MAX );

			glCompressedTextureSubImage2DEXT( texture.texture, target, i, 0, 0,
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
	glTextureStorage3DEXT( ta.texture, GL_TEXTURE_2D_ARRAY, config.num_mipmaps, internal_format, config.width, config.height, config.layers );

	glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.num_mipmaps - 1 );

	if( config.format == TextureFormat_Shadow ) {
		glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	}

	if( !CompressedTextureFormat( config.format ) ) {
		assert( config.num_mipmaps == 1 );

		if( channels == GL_RED ) {
			if( config.format == TextureFormat_A_U8 ) {
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_ONE );
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_ONE );
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_ONE );
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_RED );
			}
			else {
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED );
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_RED );
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_RED );
				glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ONE );
			}
		}
		else if( channels == GL_RG && config.format == TextureFormat_RA_U8 ) {
			glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED );
			glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_RED );
			glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_RED );
			glTextureParameteriEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_GREEN );
		}

		if( config.data != NULL ) {
			glTextureSubImage3DEXT( ta.texture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
				config.width, config.height, config.layers, channels, type, config.data );
		}
	}
	else {
		glTextureParameterfEXT( ta.texture, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filtering );

		const char * cursor = ( const char * ) config.data;
		for( u32 i = 0; i < config.num_mipmaps; i++ ) {
			u32 w = config.width >> i;
			u32 h = config.height >> i;
			u32 size = ( BitsPerPixel( config.format ) * w * h * config.layers ) / 8;
			assert( size < S32_MAX );

			glCompressedTextureSubImage3DEXT( ta.texture, GL_TEXTURE_2D_ARRAY, i, 0, 0, 0,
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

	if( config.normal_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.normal_attachment, config.msaa_samples );
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT1, texture.texture, 0 );
		bufs[ 1 ] = GL_COLOR_ATTACHMENT1;

		fb.normal_texture = texture;

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

	assert( glCheckNamedFramebufferStatus( fb.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	assert( width > 0 && height > 0 );

	fb.width = width;
	fb.height = height;

	return fb;
}

Framebuffer NewFramebuffer( Texture * albedo_texture, Texture * normal_texture, Texture * depth_texture ) {
	Framebuffer fb = { };

	glCreateFramebuffers( 1, &fb.fbo );

	u32 width = 0;
	u32 height = 0;
	GLenum bufs[ 2 ] = { GL_NONE, GL_NONE };
	if( albedo_texture != NULL ) {
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT0, albedo_texture->texture, 0 );
		bufs[ 0 ] = GL_COLOR_ATTACHMENT0;
		width = albedo_texture->width;
		height = albedo_texture->height;
	}
	if( normal_texture != NULL ) {
		glNamedFramebufferTexture( fb.fbo, GL_COLOR_ATTACHMENT1, normal_texture->texture, 0 );
		bufs[ 1 ] = GL_COLOR_ATTACHMENT1;
		width = normal_texture->width;
		height = normal_texture->height;
	}
	if( depth_texture != NULL ) {
		glNamedFramebufferTexture( fb.fbo, GL_DEPTH_ATTACHMENT, depth_texture->texture, 0 );
		bufs[ 1 ] = GL_COLOR_ATTACHMENT1;
		width = depth_texture->width;
		height = depth_texture->height;
	}
	glNamedFramebufferDrawBuffers( fb.fbo, ARRAY_COUNT( bufs ), bufs );

	assert( glCheckNamedFramebufferStatus( fb.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	assert( width > 0 && height > 0 );

	fb.width = width;
	fb.height = height;

	return fb;
}

Framebuffer NewShadowFramebuffer( TextureArray texture_array, u32 layer ) {
	Framebuffer fb = { };

	glCreateFramebuffers( 1, &fb.fbo );

	glNamedFramebufferTextureLayer( fb.fbo, GL_DEPTH_ATTACHMENT, texture_array.texture, 0, layer );

	assert( glCheckNamedFramebufferStatus( fb.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );

	fb.width = frame_static.shadow_parameters.shadowmap_res;
	fb.height = frame_static.shadow_parameters.shadowmap_res;

	return fb;
}

void DeleteFramebuffer( Framebuffer fb ) {
	if( fb.fbo == 0 )
		return;

	glDeleteFramebuffers( 1, &fb.fbo );
	DeleteTexture( fb.albedo_texture );
	DeleteTexture( fb.normal_texture );
	DeleteTexture( fb.depth_texture );
}

#define MAX_GLSL_UNIFORM_JOINTS 100

static constexpr const char * VERTEX_SHADER_PRELUDE =
	"#define VERTEX_SHADER 1\n"
	"#define v2f out\n";

static constexpr const char * FRAGMENT_SHADER_PRELUDE =
	"#define FRAGMENT_SHADER 1\n"
	"#define v2f in\n";

static GLuint CompileShader( GLenum type, Span< Span< const char > > srcs ) {
	TempAllocator temp = cls.frame_arena.temp();

	DynamicArray< const char * > src_ptrs( &temp );
	DynamicArray< int > src_lens( &temp );

	src_ptrs.add( "#version 430 core\n" );
	src_lens.add( -1 );

	if( type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER ) {
		src_ptrs.add( type == GL_VERTEX_SHADER ? VERTEX_SHADER_PRELUDE : FRAGMENT_SHADER_PRELUDE );
		src_lens.add( -1 );

		src_ptrs.add( "#define MAX_JOINTS " STRINGIFY( MAX_GLSL_UNIFORM_JOINTS ) "\n" );
		src_lens.add( -1 );
	}

	for( Span< const char > fragment : srcs ) {
		src_ptrs.add( fragment.ptr );
		src_lens.add( checked_cast< int >( fragment.n ) );
	}

	GLuint shader = glCreateShader( type );
	glShaderSource( shader, src_ptrs.size(), src_ptrs.ptr(), src_lens.ptr() );
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

		if( type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_MULTISAMPLE ) {
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

bool NewShader( Shader * shader, Span< Span< const char > > srcs, Span< const char * > feedback_varyings, bool particle_vertex_attribs ) {
	*shader = { };
	bool feedback = feedback_varyings.n > 0;

	GLuint vs = CompileShader( GL_VERTEX_SHADER, srcs );
	if( vs == 0 )
		return false;
	defer { glDeleteShader( vs ); };

	GLuint fs = 0;
	if( !feedback ) {
		fs = CompileShader( GL_FRAGMENT_SHADER, srcs );
		if( fs == 0 )
			return false;
	}
	defer {
		if( fs != 0 ) {
			glDeleteShader( fs );
		}
	};

	shader->program = glCreateProgram();
	glAttachShader( shader->program, vs );
	if( !feedback ) {
		glAttachShader( shader->program, fs );
	}

	if( particle_vertex_attribs ) {
		glBindAttribLocation( shader->program, VertexAttribute_Position, "a_Position" );
		glBindAttribLocation( shader->program, VertexAttribute_Normal, "a_Normal" );
		glBindAttribLocation( shader->program, VertexAttribute_TexCoord, "a_TexCoord" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticlePosition, "a_ParticlePosition" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleVelocity, "a_ParticleVelocity" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleAccelDragRest, "a_ParticleAccelDragRest" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleUVWH, "a_ParticleUVWH" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleStartColor, "a_ParticleStartColor" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleEndColor, "a_ParticleEndColor" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleSize, "a_ParticleSize" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleAgeLifetime, "a_ParticleAgeLifetime" );
		glBindAttribLocation( shader->program, VertexAttribute_ParticleFlags, "a_ParticleFlags" );
	}
	else {
		glBindAttribLocation( shader->program, VertexAttribute_Position, "a_Position" );
		glBindAttribLocation( shader->program, VertexAttribute_Normal, "a_Normal" );
		glBindAttribLocation( shader->program, VertexAttribute_TexCoord, "a_TexCoord" );
		glBindAttribLocation( shader->program, VertexAttribute_Color, "a_Color" );
		glBindAttribLocation( shader->program, VertexAttribute_JointIndices, "a_JointIndices" );
		glBindAttribLocation( shader->program, VertexAttribute_JointWeights, "a_JointWeights" );
		glBindAttribLocation( shader->program, VertexAttribute_MaterialColor, "a_MaterialColor" );
		glBindAttribLocation( shader->program, VertexAttribute_MaterialTextureMatrix0, "a_MaterialTextureMatrix0" );
		glBindAttribLocation( shader->program, VertexAttribute_MaterialTextureMatrix1, "a_MaterialTextureMatrix1" );
		glBindAttribLocation( shader->program, VertexAttribute_OutlineHeight, "a_OutlineHeight" );
		glBindAttribLocation( shader->program, VertexAttribute_ModelTransformRow0, "a_ModelTransformRow0" );
		glBindAttribLocation( shader->program, VertexAttribute_ModelTransformRow1, "a_ModelTransformRow1" );
		glBindAttribLocation( shader->program, VertexAttribute_ModelTransformRow2, "a_ModelTransformRow2" );
	}

	if( !feedback ) {
		glBindFragDataLocation( shader->program, 0, "f_Albedo" );
		glBindFragDataLocation( shader->program, 1, "f_Normal" );
	}
	else {
		glTransformFeedbackVaryings( shader->program, feedback_varyings.n, feedback_varyings.begin(), GL_INTERLEAVED_ATTRIBS );
	}

	return LinkShader( shader, shader->program );
}

bool NewComputeShader( Shader * shader, Span< Span< const char > > srcs ) {
	*shader = { };

	GLuint cs = CompileShader( GL_COMPUTE_SHADER, srcs );
	if( cs == 0 )
		return false;
	defer { glDeleteShader( cs ); };

	shader->program = glCreateProgram();
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
	glCreateVertexArrays( 1, &vao );
	DebugLabel( GL_VERTEX_ARRAY, vao, config.name );

	if( config.unified_buffer.buffer == 0 ) {
		SetupAttribute( vao, config.positions.buffer, VertexAttribute_Position, config.positions_format );
		SetupAttribute( vao, config.normals.buffer, VertexAttribute_Normal, config.normals_format );
		SetupAttribute( vao, config.tex_coords.buffer, VertexAttribute_TexCoord, config.tex_coords_format );
		SetupAttribute( vao, config.colors.buffer, VertexAttribute_Color, config.colors_format );
		SetupAttribute( vao, config.joints.buffer, VertexAttribute_JointIndices, config.joints_format );
		SetupAttribute( vao, config.weights.buffer, VertexAttribute_JointWeights, config.weights_format );
	}
	else {
		assert( config.stride != 0 );

		GLuint buffer = config.unified_buffer.buffer;
		SetupAttribute( vao, buffer, VertexAttribute_Position, config.positions_format, config.stride, config.positions_offset );
		SetupAttribute( vao, buffer, VertexAttribute_Normal, config.normals_format, config.stride, config.normals_offset );
		SetupAttribute( vao, buffer, VertexAttribute_TexCoord, config.tex_coords_format, config.stride, config.tex_coords_offset );
		SetupAttribute( vao, buffer, VertexAttribute_Color, config.colors_format, config.stride, config.colors_offset );
		SetupAttribute( vao, buffer, VertexAttribute_JointIndices, config.joints_format, config.stride, config.joints_offset );
		SetupAttribute( vao, buffer, VertexAttribute_JointWeights, config.weights_format, config.stride, config.weights_offset );
	}

	glVertexArrayElementBuffer( vao, config.indices.buffer );

	Mesh mesh = { };
	mesh.num_vertices = config.num_vertices;
	mesh.primitive_type = config.primitive_type;
	mesh.ccw_winding = config.ccw_winding;
	mesh.vao = vao;
	if( config.unified_buffer.buffer == 0 ) {
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

	DeleteGPUBuffer( mesh.positions );
	DeleteGPUBuffer( mesh.normals );
	DeleteGPUBuffer( mesh.tex_coords );
	DeleteGPUBuffer( mesh.colors );
	DeleteGPUBuffer( mesh.joints );
	DeleteGPUBuffer( mesh.weights );
	DeleteGPUBuffer( mesh.indices );

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

void DrawInstancedMesh( const Mesh & mesh, const PipelineState & pipeline, GPUBuffer instance_data, u32 num_instances, InstanceType instance_type, u32 num_vertices_override, u32 index_offset ) {
	assert( in_frame );
	assert( pipeline.pass != U8_MAX );
	assert( pipeline.shader != NULL );

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_vertices = num_vertices_override == 0 ? mesh.num_vertices : num_vertices_override;
	dc.index_offset = index_offset;
	dc.instance_type = instance_type;
	dc.instance_data = instance_data;
	dc.num_instances = num_instances;
	draw_calls.add( dc );

	num_vertices_this_frame += dc.num_vertices * num_instances;
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

void UpdateParticles( const Mesh & mesh, GPUBuffer vb_in, GPUBuffer vb_out, float radius, u32 num_particles, float dt ) {
	assert( in_frame );

	PipelineState pipeline;
	pipeline.pass = frame_static.particle_update_pass;
	pipeline.shader = &shaders.particle_update;
	u32 collision = cl.map == NULL ? 0 : 1;
	pipeline.set_uniform( "u_ParticleUpdate", UploadUniformBlock( collision, radius, dt ) );
	if( collision ) {
		pipeline.set_buffer( "b_BSPNodeLinks", cl.map->nodeBuffer );
		pipeline.set_buffer( "b_BSPLeaves", cl.map->leafBuffer );
		pipeline.set_buffer( "b_BSPBrushes", cl.map->brushBuffer );
		pipeline.set_buffer( "b_BSPPlanes", cl.map->planeBuffer );
	}

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.instance_type = InstanceType_Particles;
	dc.num_instances = num_particles;
	dc.instance_data = vb_in;
	dc.update_data = vb_out;

	draw_calls.add( dc );
}

void UpdateParticlesFeedback( const Mesh & mesh, GPUBuffer vb_in, GPUBuffer vb_out, GPUBuffer vb_feedback, float radius, u32 num_particles, float dt ) {
	assert( in_frame );

	PipelineState pipeline;
	pipeline.pass = frame_static.particle_update_pass;
	pipeline.shader = &shaders.particle_update_feedback;
	u32 collision = cl.map == NULL ? 0 : 1;
	pipeline.set_uniform( "u_ParticleUpdate", UploadUniformBlock( collision, radius, dt ) );
	if( collision ) {
		pipeline.set_buffer( "b_BSPNodeLinks", cl.map->nodeBuffer );
		pipeline.set_buffer( "b_BSPLeaves", cl.map->leafBuffer );
		pipeline.set_buffer( "b_BSPBrushes", cl.map->brushBuffer );
		pipeline.set_buffer( "b_BSPPlanes", cl.map->planeBuffer );
	}

	DrawCall dc = { };
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_instances = num_particles;
	dc.instance_type = InstanceType_Particles;
	dc.instance_data = vb_in;
	dc.update_data = vb_out;
	dc.feedback_data = vb_feedback;

	draw_calls.add( dc );
}

void DrawInstancedParticles( const Mesh & mesh, GPUBuffer vb, BlendFunc blend_func, u32 num_particles ) {
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
	dc.instance_type = InstanceType_Particles;
	dc.instance_data = vb;
	dc.num_instances = num_particles;

	draw_calls.add( dc );

	num_vertices_this_frame += mesh.num_vertices * num_particles;
}

void DrawInstancedParticles( GPUBuffer vb, const Model * model, u32 num_particles ) {
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

		dc.instance_type = InstanceType_Particles;
		dc.num_instances = num_particles;

		draw_calls.add( dc );
	}
}

void DispatchCompute( const PipelineState & pipeline, u32 x, u32 y, u32 z ) {
	DrawCall dc = { };
	dc.pipeline = pipeline;
	dc.instance_type = InstanceType_ComputeShader;
	draw_calls.add( dc );
}

void DownloadFramebuffer( void * buf ) {
	glReadPixels( 0, 0, frame_static.viewport_width, frame_static.viewport_height, GL_RGB, GL_UNSIGNED_BYTE, buf );
}
