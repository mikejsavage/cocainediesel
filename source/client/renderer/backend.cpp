#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/string.h"
#include "client/renderer/renderer.h"

#include "cgame/cg_local.h"

#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include "glfw3/GLFW/glfw3.h"

#include "nanosort/nanosort.hpp"

#include "tracy/tracy/Tracy.hpp"
#include "tracy/tracy/TracyOpenGL.hpp"

#include <new>

STATIC_ASSERT( ( SameType< u32, GLuint > ) );

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
	u32 base_vertex;

	u32 dispatch_size[ 3 ];
	GPUBuffer indirect;
};

struct RenderPass {
	RenderPassConfig config;
	NonRAIIDynamicArray< DrawCall > draws;
};

static GLuint vao;
static GLsync fences[ MAX_FRAMES_IN_FLIGHT ];

static NonRAIIDynamicArray< RenderPass > render_passes;
static u8 num_render_passes;

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
static VertexDescriptor prev_vertex_descriptor;
static GLuint prev_fbo;
static u32 prev_viewport_width;
static u32 prev_viewport_height;

static struct {
	UniformBlock uniforms[ ARRAY_COUNT( &Shader::uniforms ) ] = { };
	PipelineState::BufferBinding buffers[ ARRAY_COUNT( &Shader::uniforms ) ] = { };
	const Texture * textures[ ARRAY_COUNT( &Shader::textures ) ] = { };
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

		case TextureFormat_RA_U8:
			*internal = GL_RG8;
			*channels = GL_RG;
			*type = GL_UNSIGNED_BYTE;
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
			*internal = GL_DEPTH_COMPONENT24;
			*channels = GL_DEPTH_COMPONENT;
			*type = GL_FLOAT;
			return;
	}

	Assert( false );
}

static GLenum SamplerWrapToGL( SamplerWrap wrap ) {
	switch( wrap ) {
		case SamplerWrap_Repeat:
			return GL_REPEAT;
		case SamplerWrap_Clamp:
			return GL_CLAMP_TO_EDGE;
	}

	Assert( false );
	return GL_INVALID_ENUM;
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
}

static u32 VertexFormatDefaultStride( VertexFormat format ) {
	GLenum type;
	int num_components;
	bool integral;
	GLboolean normalized;
	VertexFormatToGL( format, &type, &num_components, &integral, &normalized );

	u32 component_size = 0;
	if( type == GL_UNSIGNED_BYTE ) {
		component_size = 1;
	}
	else if( type == GL_UNSIGNED_SHORT ) {
		component_size = 2;
	}
	else if( type == GL_FLOAT ) {
		component_size = 4;
	}

	return component_size * num_components;
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

static void DebugLabel( GLenum type, GLuint object, Span< const char > label ) {
	Assert( label.ptr != NULL );
	glObjectLabel( type, object, checked_cast< GLsizei >( label.n ), label.ptr );
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

	{
		TracyZoneScopedN( "Load OpenGL" );
		if( gladLoadGLLoader( GLADloadproc( glfwGetProcAddress ) ) != 1 ) {
			Fatal( "Couldn't load GL" );
		}
	}

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

	render_passes.init( sys_allocator );
	num_render_passes = 0;

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

	glCreateVertexArrays( 1, &vao );
	DebugLabel( GL_VERTEX_ARRAY, vao, "Global VAO" );
	glBindVertexArray( vao );

	GLint max_ubo_size;
	glGetIntegerv( GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size );
	Assert( max_ubo_size >= s32( UNIFORM_BUFFER_SIZE ) );

	for( size_t i = 0; i < ARRAY_COUNT( ubos ); i++ ) {
		TempAllocator temp = cls.frame_arena.temp();
		ubos[ i ].stream = NewStreamingBuffer( UNIFORM_BUFFER_SIZE, temp.sv( "UBO {}", i ) );
	}

	in_frame = false;

	prev_pipeline = PipelineState();
	prev_vertex_descriptor = { };
	prev_fbo = 0;
	prev_viewport_width = 0;
	prev_viewport_height = 0;
}

void FlushRenderBackend() {
	TracyZoneScoped;
	glFinish();
}

void ShutdownRenderBackend() {
	for( UBO ubo : ubos ) {
		DeleteStreamingBuffer( ubo.stream );
	}

	glBindVertexArray( 0 );
	glDeleteVertexArrays( 1, &vao );

	RunDeferredDeletes();

	for( GLsync fence : fences ) {
		if( fence != 0 ) {
			glDeleteSync( fence );
		}
	}

	for( RenderPass & pass : render_passes ) {
		pass.draws.shutdown();
	}
	render_passes.shutdown();

	deferred_mesh_deletes.shutdown();
	deferred_buffer_deletes.shutdown();
	deferred_streaming_buffer_deletes.shutdown();
}

void RenderBackendBeginFrame() {
	TracyZoneScoped;

	Assert( !in_frame );
	in_frame = true;

	num_render_passes = 0;

	size_t fence_id = FrameSlot();
	if( fences[ fence_id ] != 0 ) {
		TracyZoneScopedN( "Wait on frame fence" );
		glClientWaitSync( fences[ fence_id ], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED );
		glDeleteSync( fences[ fence_id ] );
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

static bool operator==( PipelineState::Scissor a, PipelineState::Scissor b ) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void PipelineState::bind_uniform( StringHash name, UniformBlock block ) {
	for( size_t i = 0; i < num_uniforms; i++ ) {
		if( uniforms[ i ].name_hash == name.hash ) {
			uniforms[ i ].block = block;
			return;
		}
	}

	Assert( num_uniforms < ARRAY_COUNT( uniforms ) );
	uniforms[ num_uniforms ].name_hash = name.hash;
	uniforms[ num_uniforms ].block = block;
	num_uniforms++;
}

void PipelineState::bind_texture_and_sampler( StringHash name, const Texture * texture, SamplerType sampler ) {
	for( size_t i = 0; i < num_textures; i++ ) {
		if( textures[ i ].name_hash == name.hash ) {
			textures[ i ].texture = texture;
			textures[ i ].sampler = sampler;
			return;
		}
	}

	Assert( num_textures < ARRAY_COUNT( textures ) );
	textures[ num_textures ].name_hash = name.hash;
	textures[ num_textures ].texture = texture;
	textures[ num_textures ].sampler = sampler;
	num_textures++;
}

void PipelineState::bind_buffer( StringHash name, GPUBuffer buffer, u32 offset, u32 size ) {
	for( size_t i = 0; i < num_buffers; i++ ) {
		if( buffers[ i ].name_hash == name.hash ) {
			buffers[ i ].buffer = buffer;
			buffers[ i ].offset = offset;
			return;
		}
	}

	Assert( num_buffers < ARRAY_COUNT( buffers ) );
	buffers[ num_buffers ].name_hash = name.hash;
	buffers[ num_buffers ].buffer = buffer;
	buffers[ num_buffers ].offset = offset;
	buffers[ num_buffers ].size = size;
	num_buffers++;
}

void PipelineState::bind_streaming_buffer( StringHash name, StreamingBuffer stream ) {
	u32 offset = stream.size * FrameSlot();
	bind_buffer( name, stream.buffer, offset, stream.size );
}

static bool operator==( const VertexAttribute & lhs, const VertexAttribute & rhs ) {
	return lhs.format == rhs.format && lhs.buffer == rhs.buffer && lhs.offset == rhs.offset;
}

static void SetPipelineState( const PipelineState & pipeline, bool cw_winding ) {
	TracyZoneScoped;
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
						if( prev_texture != NULL && prev_texture->msaa_samples != texture->msaa_samples ) {
							glBindTextureUnit( i, 0 );
							glBindSampler( i, 0 );
						}
						glBindTextureUnit( i, texture->texture );
						glBindSampler( i, GetSampler( pipeline.textures[ j ].sampler ).sampler );
						prev_bindings.textures[ i ] = texture;
					}
					found = true;
					break;
				}
			}
		}

		if( !found && prev_texture != NULL ) {
			glBindTextureUnit( i, 0 );
			glBindSampler( i, 0 );
			prev_bindings.textures[ i ] = NULL;
		}
	}

	// buffers
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->buffers ); i++ ) {
		u64 name_hash = pipeline.shader->buffers[ i ];
		PipelineState::BufferBinding prev = prev_bindings.buffers[ i ];

		bool should_unbind = prev.buffer.buffer != 0;
		if( name_hash != 0 ) {
			for( size_t j = 0; j < pipeline.num_buffers; j++ ) {
				const PipelineState::BufferBinding & binding = pipeline.buffers[ j ];
				if( binding.name_hash == name_hash ) {
					if( binding.buffer.buffer != prev.buffer.buffer || binding.offset != prev.offset || binding.size != prev.size ) {
						if( binding.offset == 0 && binding.size == 0 ) {
							glBindBufferBase( GL_SHADER_STORAGE_BUFFER, i, binding.buffer.buffer );
						}
						else {
							glBindBufferRange( GL_SHADER_STORAGE_BUFFER, i, binding.buffer.buffer, binding.offset, binding.size );
						}
						prev_bindings.buffers[ i ] = binding;
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
	CullFace cull_face = pipeline.cull_face;
	if( cull_face != CullFace_Disabled && cw_winding ) {
		cull_face = cull_face == CullFace_Front ? CullFace_Back : CullFace_Front;
	}

	if( cull_face != prev_pipeline.cull_face ) {
		if( cull_face == CullFace_Disabled ) {
			glDisable( GL_CULL_FACE );
		}
		else {
			if( prev_pipeline.cull_face == CullFace_Disabled ) {
				glEnable( GL_CULL_FACE );
			}
			glCullFace( cull_face == CullFace_Front ? GL_FRONT : GL_BACK );
		}
	}

	// scissor
	if( pipeline.scissor != prev_pipeline.scissor ) {
		if( !pipeline.scissor.exists ) {
			glDisable( GL_SCISSOR_TEST );
		}
		else {
			if( !prev_pipeline.scissor.exists ) {
				glEnable( GL_SCISSOR_TEST );
			}
			PipelineState::Scissor s = pipeline.scissor.value;
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
	prev_pipeline.cull_face = cull_face;
}

static void BindVertexDescriptorAndBuffers( const Mesh & mesh ) {
	TracyZoneScoped;

	const VertexDescriptor & vertex_descriptor = mesh.vertex_descriptor;

	// vertex descriptor
	for( size_t i = 0; i < ARRAY_COUNT( vertex_descriptor.attributes ); i++ ) {
		const Optional< VertexAttribute > & opt_prev_attr = prev_vertex_descriptor.attributes[ i ];
		const Optional< VertexAttribute > & opt_attr = vertex_descriptor.attributes[ i ];
		if( opt_attr == opt_prev_attr )
			continue;

		if( opt_prev_attr.exists != opt_attr.exists ) {
			if( opt_attr.exists ) {
				glEnableVertexArrayAttrib( vao, i );
			}
			else {
				glDisableVertexArrayAttrib( vao, i );
			}
		}

		if( opt_attr.exists ) {
			const VertexAttribute & attr = opt_attr.value;

			GLenum type;
			int num_components;
			bool integral;
			GLboolean normalized;
			VertexFormatToGL( attr.format, &type, &num_components, &integral, &normalized );

			if( integral && !normalized ) {
				/*
				 * wintel driver ignores the type and treats everything as u32
				 * non-DSA call works fine so fall back to that here
				 *
				 * see also https://doc.magnum.graphics/magnum/opengl-workarounds.html
				 *
				 * glVertexArrayAttribIFormat( vao, attr, num_components, type, attr.offset );
				 */

				glVertexAttribIFormat( i, num_components, type, attr.offset );
			}
			else {
				glVertexArrayAttribFormat( vao, i, num_components, type, normalized, attr.offset );
			}
		}
	}

	prev_vertex_descriptor = vertex_descriptor;

	// vertex buffers
	for( size_t i = 0; i < ARRAY_COUNT( mesh.vertex_descriptor.attributes ); i++ ) {
		if( !mesh.vertex_descriptor.attributes[ i ].exists )
			continue;

		const VertexAttribute & attribute = mesh.vertex_descriptor.attributes[ i ].value;
		u32 stride = mesh.vertex_descriptor.buffer_strides[ attribute.buffer ];
		if( stride == 0 ) {
			stride = VertexFormatDefaultStride( attribute.format );
		}
		glVertexArrayVertexBuffer( vao, i, mesh.vertex_buffers[ attribute.buffer ].buffer, 0, stride );
	}

	// index buffer
	glVertexArrayElementBuffer( vao, mesh.index_buffer.buffer );
}

static bool SortDrawCall( const DrawCall & a, const DrawCall & b ) {
	return a.pipeline.shader < b.pipeline.shader;
}

static void SubmitFramebufferBlit( const RenderPassConfig & pass ) {
	RenderTarget src = pass.blit_source;
	RenderTarget target = pass.target;
	glBlitNamedFramebuffer( src.fbo, target.fbo, 0, 0, src.width, src.height, 0, 0, target.width, target.height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST );
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

static void SetupRenderPass( const RenderPassConfig & pass ) {
	TracyZoneScoped;
	TracyZoneText( pass.tracy->name, strlen( pass.tracy->name ) );
#if TRACY_ENABLE
	renderpass_zone = new (renderpass_zone_memory) tracy::GpuCtxScope( pass.tracy, true );
#endif

	glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass.tracy->name );

	const RenderTarget & fb = pass.target;
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

	for( size_t i = 0; i < ARRAY_COUNT( pass.clear_color ); i++ ) {
		const Optional< Vec4 > & clear_color = pass.clear_color[ i ];
		if( !clear_color.exists || pass.target.color_attachments[ i ].texture == 0 )
			continue;
		glClearNamedFramebufferfv( pass.target.fbo, GL_COLOR, i, clear_color.value.ptr() );
	}

	if( pass.clear_depth.exists && pass.target.depth_attachment.texture != 0 ) {
		glClearNamedFramebufferfv( pass.target.fbo, GL_DEPTH, 0, &pass.clear_depth.value );
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
		TracyZoneScopedN( "Compute command" );
		glDispatchCompute( dc.dispatch_size[ 0 ], dc.dispatch_size[ 1 ], dc.dispatch_size[ 2 ] );
		return;
	}

	if( dc.type == DrawCallType_IndirectCompute ) {
		TracyZoneScopedN( "Compute command" );
		glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, dc.indirect.buffer );
		glDispatchComputeIndirect( 0 );
		glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, 0 );
		return;
	}

	BindVertexDescriptorAndBuffers( dc.mesh );

	GLenum gl_index_format = dc.mesh.index_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

	{
		TracyZoneScopedN( "Draw command" );

		if( dc.type == DrawCallType_Normal ) {
			if( dc.mesh.index_buffer.buffer != 0 ) {
				size_t index_size = dc.mesh.index_format == IndexFormat_U16 ? sizeof( u16 ) : sizeof( u32 );
				const void * offset = ( const void * ) ( uintptr_t( dc.first_index ) * index_size );
				glDrawElementsInstancedBaseVertex( GL_TRIANGLES, dc.num_vertices, gl_index_format, offset, dc.num_instances, dc.base_vertex );
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
	}

	{
		TracyZoneScopedN( "Unbind buffers" );

		// unbind buffers
		for( size_t i = 0; i < ARRAY_COUNT( dc.mesh.vertex_buffers ); i++ ) {
			glVertexArrayVertexBuffer( vao, i, 0, 0, 0 );
		}

		glVertexArrayElementBuffer( vao, 0 );
	}
}

void RenderBackendSubmitFrame() {
	TracyZoneScoped;

	Assert( in_frame );
	Assert( render_passes.size() > 0 );
	in_frame = false;

	size_t num_draw_calls_this_frame = 0;
	for( u8 i = 0; i < num_render_passes; i++ ) {
		RenderPass & pass = render_passes[ i ];
		SetupRenderPass( pass.config );

		if( pass.config.sorted ) {
			TracyZoneScopedN( "Sort draw calls" );
			nanosort( pass.draws.begin(), pass.draws.end(), SortDrawCall );
		}

		{
			TracyZoneScopedN( "Submit draw calls" );
			for( const DrawCall & draw : pass.draws ) {
				SubmitDrawCall( draw );
				num_draw_calls_this_frame++;
			}
		}

		FinishRenderPass();
	}

	{
		// OBS captures the game with glBlitFramebuffer which gets
		// nuked by scissor, so turn it off at the end of every frame
		PipelineState no_scissor_test = prev_pipeline;
		no_scissor_test.scissor = NONE;
		SetPipelineState( no_scissor_test, true );
	}

	RunDeferredDeletes();

	fences[ FrameSlot() ] = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );

	u32 ubo_bytes_used = 0;
	for( const UBO & ubo : ubos ) {
		ubo_bytes_used += ubo.bytes_used;
	}
	TracyPlotSample( "UBO utilisation", float( ubo_bytes_used ) / float( UNIFORM_BUFFER_SIZE * ARRAY_COUNT( ubos ) ) );

	TracyPlotSample( "Draw calls", s64( num_draw_calls_this_frame ) );
	TracyPlotSample( "Vertices", s64( num_vertices_this_frame ) );

	TracyGpuCollect;
}

UniformBlock UploadUniforms( const void * data, size_t size ) {
	Assert( in_frame );

	UBO * ubo = NULL;
	u32 offset = 0;

	for( size_t i = 0; i < ARRAY_COUNT( ubos ); i++ ) {
		offset = checked_cast< u32 >( AlignPow2( ubos[ i ].bytes_used, ubo_offset_alignment ) );
		if( UNIFORM_BUFFER_SIZE - offset >= size ) {
			ubo = &ubos[ i ];
			break;
		}
	}

	if( ubo == NULL )
		Fatal( "Ran out of UBO space" );

	UniformBlock block;
	block.ubo = ubo->stream.buffer.buffer;
	block.offset = offset + ubo->stream.size * FrameSlot();
	block.size = checked_cast< u32 >( AlignPow2( size, 16 ) );

	u8 * mapping = ( u8 * ) GetStreamingBufferMemory( ubo->stream );
	memcpy( mapping + offset, data, size );
	ubo->bytes_used = offset + size;

	return block;
}

static GPUBuffer NewGPUBuffer( const void * data, u32 size, bool coherent, Span< const char > name ) {
	GLbitfield flags = coherent ? GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT : 0;
	GPUBuffer buf = { };
	glCreateBuffers( 1, &buf.buffer );
	glNamedBufferStorage( buf.buffer, size, data, flags );

	if( name.ptr != NULL ) {
		DebugLabel( GL_BUFFER, buf.buffer, name );
	}

	return buf;
}

GPUBuffer NewGPUBuffer( const void * data, u32 size, Span< const char > name ) {
	return NewGPUBuffer( data, size, false, name );
}

GPUBuffer NewGPUBuffer( u32 size, Span< const char > name ) {
	return NewGPUBuffer( NULL, size, name );
}

void DeleteGPUBuffer( GPUBuffer buf ) {
	if( buf.buffer == 0 )
		return;
	glDeleteBuffers( 1, &buf.buffer );
}

void DeferDeleteGPUBuffer( GPUBuffer buf ) {
	deferred_buffer_deletes.add( buf );
}

StreamingBuffer NewStreamingBuffer( u32 size, Span< const char > name ) {
	StreamingBuffer stream = { };
	stream.buffer = NewGPUBuffer( NULL, size * MAX_FRAMES_IN_FLIGHT, true, name );
	stream.ptr = glMapNamedBufferRange( stream.buffer.buffer, 0, size * MAX_FRAMES_IN_FLIGHT, GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT );
	stream.size = size;

	if( name.ptr != NULL ) {
		DebugLabel( GL_BUFFER, stream.buffer.buffer, name );
	}

	return stream;
}

void * GetStreamingBufferMemory( StreamingBuffer stream ) {
	return ( ( char * ) stream.ptr ) + stream.size * FrameSlot();
}

void DeleteStreamingBuffer( StreamingBuffer stream ) {
	glUnmapNamedBuffer( stream.buffer.buffer );
	DeleteGPUBuffer( stream.buffer );
}

void DeferDeleteStreamingBuffer( StreamingBuffer stream ) {
	deferred_streaming_buffer_deletes.add( stream );
}

Sampler NewSampler( const SamplerConfig & config ) {
	Sampler sampler;
	glCreateSamplers( 1, &sampler.sampler );

	glSamplerParameteri( sampler.sampler, GL_TEXTURE_WRAP_S, SamplerWrapToGL( config.wrap ) );
	glSamplerParameteri( sampler.sampler, GL_TEXTURE_WRAP_T, SamplerWrapToGL( config.wrap ) );

	GLenum min_filter = config.filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
	GLenum mag_filter = config.filter ? GL_LINEAR : GL_NEAREST;
	glSamplerParameteri( sampler.sampler, GL_TEXTURE_MIN_FILTER, min_filter );
	glSamplerParameteri( sampler.sampler, GL_TEXTURE_MAG_FILTER, mag_filter );

	glSamplerParameterf( sampler.sampler, GL_TEXTURE_LOD_BIAS, config.lod_bias );

	if( config.shadowmap_sampler ) {
		glSamplerParameteri( sampler.sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		glSamplerParameteri( sampler.sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	}

	return sampler;
}

void DeleteSampler( Sampler sampler ) {
	if( sampler.sampler == 0 )
		return;
	glDeleteSamplers( 1, &sampler.sampler );
}

Texture NewTexture( const TextureConfig & config ) {
	Texture texture = { };
	texture.width = config.width;
	texture.height = config.height;
	texture.num_layers = config.num_layers;
	texture.num_mipmaps = config.num_mipmaps;
	texture.msaa_samples = config.msaa_samples;
	texture.format = config.format;

	GLenum target;
	if( config.msaa_samples == 0 ) {
		target = config.num_layers == 0 ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY;
	}
	else {
		target = config.num_layers == 0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
	}
	glCreateTextures( target, 1, &texture.texture );

	GLenum internal_format, channels, type;
	TextureFormatToGL( config.format, &internal_format, &channels, &type );

	if( config.msaa_samples != 0 ) {
		Assert( config.data == NULL );
		if( config.num_layers == 0 ) {
			glTextureStorage2DMultisample( texture.texture, config.msaa_samples,
				internal_format, config.width, config.height, GL_TRUE );
		}
		else {
			glTextureStorage3DMultisample( texture.texture, config.msaa_samples,
				internal_format, config.width, config.height, config.num_layers, GL_TRUE );
		}
		return texture;
	}

	if( config.num_layers == 0 ) {
		glTextureStorage2D( texture.texture, config.num_mipmaps, internal_format, config.width, config.height );
	}
	else {
		glTextureStorage3D( texture.texture, config.num_mipmaps, internal_format, config.width, config.height, config.num_layers );
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
			if( config.num_layers == 0 ) {
				glTextureSubImage2D( texture.texture, 0, 0, 0,
					config.width, config.height, channels, type, config.data );
			}
			else {
				glTextureSubImage3D( texture.texture, 0, 0, 0, 0,
					config.width, config.height, config.num_layers, channels, type, config.data );
			}
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
			u32 layers = Max2( u32( 1 ), config.num_layers );
			u32 size = ( BitsPerPixel( config.format ) * w * h * layers ) / 8;
			Assert( size < S32_MAX );

			if( config.num_layers == 0 ) {
				glCompressedTextureSubImage2D( texture.texture, i, 0, 0,
					w, h, internal_format, size, cursor );
			}
			else {
				glCompressedTextureSubImage3D( texture.texture, i, 0, 0, 0,
					w, h, config.num_layers, internal_format, size, cursor );
			}

			cursor += size;
		}
	}

	return texture;
}

void DeleteTexture( Texture texture ) {
	if( texture.texture == 0 )
		return;
	glDeleteTextures( 1, &texture.texture );
}

static void AddRenderTargetAttachment( GLuint fbo, const RenderTargetConfig::Attachment & config, GLenum attachment, u32 * width, u32 * height ) {
	Assert( config.texture.texture != 0 );
	Assert( ( config.texture.num_layers != 0 ) == config.layer.exists );

	if( config.texture.num_layers == 0 ) {
		glNamedFramebufferTexture( fbo, attachment, config.texture.texture, 0 );
	}
	else {
		glNamedFramebufferTextureLayer( fbo, attachment, config.texture.texture, 0, config.layer.value );
	}

	Assert( *width == 0 || config.texture.width == *width );
	Assert( *height == 0 || config.texture.height == *height );
	*width = config.texture.width;
	*height = config.texture.height;
}

RenderTarget NewRenderTarget( const RenderTargetConfig & config ) {
	RenderTarget rt = { };

	glCreateFramebuffers( 1, &rt.fbo );

	u32 width = 0;
	u32 height = 0;

	GLenum opengl_sucks[ FragmentShaderOutput_Count ] = { };

	for( size_t i = 0; i < ARRAY_COUNT( config.color_attachments ); i++ ) {
		const Optional< RenderTargetConfig::Attachment > & attachment = config.color_attachments[ i ];
		if( !attachment.exists )
			continue;
		AddRenderTargetAttachment( rt.fbo, attachment.value, GL_COLOR_ATTACHMENT0 + i, &width, &height );
		rt.color_attachments[ i ] = attachment.value.texture;
		opengl_sucks[ i ] = GL_COLOR_ATTACHMENT0 + i;
	}

	if( config.depth_attachment.exists ) {
		AddRenderTargetAttachment( rt.fbo, config.depth_attachment.value, GL_DEPTH_ATTACHMENT, &width, &height );
		rt.depth_attachment = config.depth_attachment.value.texture;
	}

	glNamedFramebufferDrawBuffers( rt.fbo, ARRAY_COUNT( opengl_sucks ), opengl_sucks );

	Assert( glCheckNamedFramebufferStatus( rt.fbo, GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	Assert( width > 0 && height > 0 );
	rt.width = width;
	rt.height = height;

	return rt;
}

void DeleteRenderTarget( RenderTarget rt ) {
	if( rt.fbo == 0 )
		return;
	glDeleteFramebuffers( 1, &rt.fbo );
}

void DeleteRenderTargetAndTextures( RenderTarget rt ) {
	DeleteRenderTarget( rt );
	for( Texture texture : rt.color_attachments ) {
		DeleteTexture( texture );
	}
	DeleteTexture( rt.depth_attachment );
}

static GLuint CompileShader( GLenum type, Span< const char > body, Span< const char > name ) {
	TempAllocator temp = cls.frame_arena.temp();

	DynamicString src( &temp, "#version 450 core\n" );
	if( type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER ) {
		constexpr Span< const char > vertex_shader_prelude =
			"#define VERTEX_SHADER 1\n"
			"#define v2f out\n";
		constexpr Span< const char > fragment_shader_prelude =
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
		Com_GGPrint( S_COLOR_YELLOW "Shader compilation failed {}: {}", name, buf );
		glDeleteShader( shader );

		// static char src[ 65536 ];
		// glGetShaderSource( shader, sizeof( src ), NULL, src );
		// Com_Printf( "%s\n", src );

		return 0;
	}

	return shader;
}

static bool LinkShader( Shader * shader, GLuint program, Span< const char > shader_name ) {
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
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint size, len;
		GLenum type;
		glGetActiveUniform( program, i, sizeof( name ), &len, &size, &type, name );

		bool is_texture = false;
		is_texture = is_texture || type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_MULTISAMPLE;
		is_texture = is_texture || type == GL_UNSIGNED_INT_SAMPLER_2D || type == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE;
		is_texture = is_texture || type == GL_SAMPLER_2D_ARRAY || type == GL_SAMPLER_2D_ARRAY_SHADOW;

		if( is_texture ) {
			if( num_textures == ARRAY_COUNT( shader->textures ) ) {
				glDeleteProgram( program );
				Com_GGPrint( S_COLOR_YELLOW "Too many textures in shader {}", shader_name );
				return false;
			}

			glProgramUniform1i( program, glGetUniformLocation( program, name ), num_textures );
			shader->textures[ num_textures ] = Hash64( name, len );
			num_textures++;
		}
	}

	glGetProgramInterfaceiv( program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &count );
	if( count > checked_cast< GLint >( ARRAY_COUNT( shader->uniforms ) ) ) {
		glDeleteProgram( program );
		Com_GGPrint( S_COLOR_YELLOW "Too many uniforms in shader {}", shader_name );
		return false;
	}
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint len;
		glGetProgramResourceName( program, GL_UNIFORM_BLOCK, i, sizeof( name ), &len, name );
		glUniformBlockBinding( program, i, i );
		shader->uniforms[ i ] = Hash64( name, len );
	}

	glGetProgramInterfaceiv( program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count );
	if( count > checked_cast< GLint >( ARRAY_COUNT( shader->uniforms ) ) ) {
		glDeleteProgram( program );
		Com_GGPrint( S_COLOR_YELLOW "Too many buffers in shader {}", shader_name );
		return false;
	}
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint len;
		glGetProgramResourceName( program, GL_SHADER_STORAGE_BLOCK, i, sizeof( name ), &len, name );
		glShaderStorageBlockBinding( program, i, i );
		shader->buffers[ i ] = Hash64( name, len );
	}

	return true;
}

bool NewShader( Shader * shader, Span< const char > src, Span< const char > name ) {
	TempAllocator temp = cls.frame_arena.temp();

	*shader = { };

	Span< const char > vertex_shader_name = temp.sv( "{} [VS]", name );
	GLuint vertex_shader = CompileShader( GL_VERTEX_SHADER, src, vertex_shader_name );
	if( vertex_shader == 0 )
		return false;
	DebugLabel( GL_SHADER, vertex_shader, vertex_shader_name );
	defer { glDeleteShader( vertex_shader ); };

	Span< const char > fragment_shader_name = temp.sv( "{} [FS]", name );
	GLuint fragment_shader = CompileShader( GL_FRAGMENT_SHADER, src, fragment_shader_name );
	if( fragment_shader == 0 )
		return false;
	DebugLabel( GL_SHADER, fragment_shader, fragment_shader_name );
	defer { glDeleteShader( fragment_shader ); };

	shader->program = glCreateProgram();
	DebugLabel( GL_PROGRAM, shader->program, name );
	glAttachShader( shader->program, vertex_shader );
	glAttachShader( shader->program, fragment_shader );

	return LinkShader( shader, shader->program, name );
}

bool NewComputeShader( Shader * shader, Span< const char > src, Span< const char > name ) {
	TempAllocator temp = cls.frame_arena.temp();

	*shader = { };

	GLuint cs = CompileShader( GL_COMPUTE_SHADER, src, name );
	if( cs == 0 )
		return false;
	DebugLabel( GL_SHADER, cs, temp.sv( "{} [CS]", name ) );
	defer { glDeleteShader( cs ); };

	shader->program = glCreateProgram();
	DebugLabel( GL_PROGRAM, shader->program, name );
	glAttachShader( shader->program, cs );

	return LinkShader( shader, shader->program, name );
}

void DeleteShader( Shader shader ) {
	if( shader.program == 0 )
		return;

	if( prev_pipeline.shader != NULL && prev_pipeline.shader->program == shader.program ) {
		prev_pipeline.shader = { };
		glUseProgram( 0 );
	}

	glDeleteProgram( shader.program );
}

Mesh NewMesh( const MeshConfig & config ) {
	Mesh mesh = { };
	mesh.vertex_descriptor = config.vertex_descriptor;
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
	for( GPUBuffer buffer : mesh.vertex_buffers ) {
		DeleteGPUBuffer( buffer );
	}
	DeleteGPUBuffer( mesh.index_buffer );
}

void DeferDeleteMesh( const Mesh & mesh ) {
	deferred_mesh_deletes.add( mesh );
}

u8 AddRenderPass( const RenderPassConfig & config ) {
	if( num_render_passes >= render_passes.size() ) {
		num_render_passes = render_passes.add( RenderPass() );
		render_passes[ num_render_passes ].draws.init( sys_allocator );
	}

	render_passes[ num_render_passes ].config = config;
	render_passes[ num_render_passes ].draws.clear();
	num_render_passes++;

	return num_render_passes - 1;
}

u8 AddRenderPass( const tracy::SourceLocationData * tracy, RenderTarget target, Optional< Vec4 > clear_color, Optional< float > clear_depth ) {
	RenderPassConfig pass;
	pass.target = target;
	for( Optional< Vec4 > & color : pass.clear_color ) {
		color = clear_color;
	}
	pass.clear_depth = clear_depth;
	pass.tracy = tracy;
	return AddRenderPass( pass );
}

u8 AddRenderPass( const tracy::SourceLocationData * tracy, Optional< Vec4 > clear_color, Optional< float > clear_depth ) {
	RenderTarget target = { };
	return AddRenderPass( tracy, target, clear_color, clear_depth );
}

void DrawMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_vertices_override, u32 first_index, u32 base_vertex ) {
	DrawInstancedMesh( mesh, pipeline, 1, num_vertices_override, first_index, base_vertex );
}

void DrawInstancedMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_instances, u32 num_vertices_override, u32 first_index, u32 base_vertex ) {
	Assert( in_frame );
	Assert( pipeline.pass != U8_MAX );
	Assert( pipeline.shader != NULL );

	DrawCall dc = { };
	dc.type = DrawCallType_Normal;
	dc.mesh = mesh;
	dc.pipeline = pipeline;
	dc.num_vertices = num_vertices_override == 0 ? mesh.num_vertices : num_vertices_override;
	dc.first_index = first_index;
	dc.base_vertex = base_vertex;
	dc.num_instances = num_instances;
	render_passes[ pipeline.pass ].draws.add( dc );

	num_vertices_this_frame += dc.num_vertices * num_instances;
}

void DrawMeshIndirect( const Mesh & mesh, const PipelineState & pipeline, GPUBuffer indirect ) {
	DrawCall dc = { };
	dc.type = DrawCallType_Indirect;
	dc.pipeline = pipeline;
	dc.mesh = mesh;
	dc.indirect = indirect;
	render_passes[ pipeline.pass ].draws.add( dc );
}

void DispatchCompute( const PipelineState & pipeline, u32 x, u32 y, u32 z ) {
	DrawCall dc = { };
	dc.type = DrawCallType_Compute;
	dc.pipeline = pipeline;
	dc.dispatch_size[ 0 ] = x;
	dc.dispatch_size[ 1 ] = y;
	dc.dispatch_size[ 2 ] = z;
	render_passes[ pipeline.pass ].draws.add( dc );
}

void DispatchComputeIndirect( const PipelineState & pipeline, GPUBuffer indirect ) {
	DrawCall dc = { };
	dc.type = DrawCallType_IndirectCompute;
	dc.pipeline = pipeline;
	dc.indirect = indirect;
	render_passes[ pipeline.pass ].draws.add( dc );
}

void DownloadFramebuffer( void * buf ) {
	glReadPixels( 0, 0, frame_static.viewport_width, frame_static.viewport_height, GL_RGB, GL_UNSIGNED_BYTE, buf );
}
