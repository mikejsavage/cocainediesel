#include <algorithm> // std::stable_sort

#include "glad/glad.h"

#include "tracy/TracyOpenGL.hpp"

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "client/renderer/renderer.h"

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
	VertexAttribute_ParticleScale,
	VertexAttribute_ParticleT,
	VertexAttribute_ParticleColor,
};

static const u32 UNIFORM_BUFFER_SIZE = 64 * 1024;

struct DrawCall {
	PipelineState pipeline;
	Mesh mesh;
	u32 num_vertices;
	u32 index_offset;

	u32 num_instances;
	VertexBuffer instance_data;
};

static DynamicArray< RenderPass > render_passes( NO_INIT );
static DynamicArray< DrawCall > draw_calls( NO_INIT );
static DynamicArray< Mesh > deferred_deletes( NO_INIT );

static u32 num_vertices_this_frame;

static bool in_frame;

struct UBO {
	GLuint ubo;
	u8 * buffer;
	u32 bytes_used;
};

static UBO ubos[ 16 ]; // 1MB of uniform space
static u32 ubo_offset_alignment;

static PipelineState prev_pipeline;
static GLuint prev_fbo;
static u32 prev_viewport_width;
static u32 prev_viewport_height;

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

		case TextureFormat_Depth:
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

static GLenum TextureFilterToGL( TextureFilter filter ) {
	switch( filter ) {
		case TextureFilter_Linear:
			return GL_LINEAR;
		case TextureFilter_Point:
			return GL_NEAREST;
	}

	assert( false );
	return GL_INVALID_ENUM;
}

static void VertexFormatToGL( VertexFormat format, GLenum * type, int * num_components, bool * integral, GLboolean * normalized ) {
	*integral = false;
	*normalized = false;

	switch( format ) {
		case VertexFormat_U8x4:
		case VertexFormat_U8x4_Norm:
			*type = GL_UNSIGNED_BYTE;
			*num_components = 4;
			*integral = true;
			*normalized = format == VertexFormat_U8x4_Norm;
			return;

		case VertexFormat_U16x4:
		case VertexFormat_U16x4_Norm:
			*type = GL_UNSIGNED_SHORT;
			*num_components = 4;
			*integral = true;
			*normalized = format == VertexFormat_U16x4_Norm;
			return;

		case VertexFormat_Halfx2:
			*type = GL_HALF_FLOAT;
			*num_components = 2;
			return;
		case VertexFormat_Halfx3:
			*type = GL_HALF_FLOAT;
			*num_components = 3;
			return;
		case VertexFormat_Halfx4:
			*type = GL_HALF_FLOAT;
			*num_components = 4;
			return;

		case VertexFormat_Floatx1:
			*type = GL_FLOAT;
			*num_components = 1;
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

void RenderBackendInit() {
	TracyGpuContext;

	render_passes.init( sys_allocator );
	draw_calls.init( sys_allocator );
	deferred_deletes.init( sys_allocator );

	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );

	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	glDisable( GL_BLEND );

	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	GLint alignment;
	glGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment );
	ubo_offset_alignment = checked_cast< u32 >( alignment );

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
	deferred_deletes.shutdown();
}

void RenderBackendBeginFrame() {
	assert( !in_frame );
	in_frame = true;

	render_passes.clear();
	draw_calls.clear();
	deferred_deletes.clear();

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
	// TODO: maybe these shouldn't always get rebound
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->uniforms ); i++ ) {
		bool found = false;
		for( size_t j = 0; j < pipeline.num_uniforms; j++ ) {
			UniformBlock block = pipeline.uniforms[ j ].block;
			if( pipeline.uniforms[ j ].name_hash == pipeline.shader->uniforms[ i ] && block.size > 0 ) {
				glBindBufferRange( GL_UNIFORM_BUFFER, i, block.ubo, block.offset, block.size );
				found = true;
				break;
			}
		}

		if( !found ) {
			glBindBufferBase( GL_UNIFORM_BUFFER, i, 0 );
		}
	}

	// textures
	for( size_t i = 0; i < ARRAY_COUNT( pipeline.shader->textures ); i++ ) {
		glActiveTexture( GL_TEXTURE0 + i );

		bool found = false;
		for( size_t j = 0; j < pipeline.num_textures; j++ ) {
			if( pipeline.textures[ j ].name_hash == pipeline.shader->textures[ i ] ) {
				GLenum target = pipeline.textures[ j ].texture->msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
				GLenum other_target = pipeline.textures[ j ].texture->msaa ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
				glBindTexture( other_target, 0 );
				glBindTexture( target, pipeline.textures[ j ].texture->texture );
				found = true;
				break;
			}
		}

		if( !found ) {
			glBindTexture( GL_TEXTURE_2D, 0 );
			glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		}
	}

	// depth writing
	if( pipeline.write_depth != prev_pipeline.write_depth ) {
		glDepthMask( pipeline.write_depth ? GL_TRUE : GL_FALSE );
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

static void SetupRenderPass( const RenderPass & pass ) {
	ZoneScoped;
	ZoneText( pass.name, strlen( pass.name ) );
	TracyGpuZone( "Setup render pass" );

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

static void SubmitDrawCall( const DrawCall & dc ) {
	ZoneScoped;
	TracyGpuZone( "Draw call" );

	SetPipelineState( dc.pipeline, dc.mesh.ccw_winding );

	glBindVertexArray( dc.mesh.vao );
	GLenum primitive = PrimitiveTypeToGL( dc.mesh.primitive_type );

	if( dc.num_instances != 0 ) {
		glBindBuffer( GL_ARRAY_BUFFER, dc.instance_data.vbo );

		SetupAttribute( VertexAttribute_ParticlePosition, VertexFormat_Floatx3, sizeof( GPUParticle ), offsetof( GPUParticle, position ) );
		glVertexAttribDivisor( VertexAttribute_ParticlePosition, 1 );
		SetupAttribute( VertexAttribute_ParticleScale, VertexFormat_Floatx1, sizeof( GPUParticle ), offsetof( GPUParticle, scale ) );
		glVertexAttribDivisor( VertexAttribute_ParticleScale, 1 );
		SetupAttribute( VertexAttribute_ParticleT, VertexFormat_Floatx1, sizeof( GPUParticle ), offsetof( GPUParticle, t ) );
		glVertexAttribDivisor( VertexAttribute_ParticleT, 1 );
		SetupAttribute( VertexAttribute_ParticleColor, VertexFormat_U8x4_Norm, sizeof( GPUParticle ), offsetof( GPUParticle, color ) );
		glVertexAttribDivisor( VertexAttribute_ParticleColor, 1 );

		GLenum type = dc.mesh.indices_format == IndexFormat_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstanced( primitive, dc.num_vertices, type, 0, dc.num_instances );
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

static void SubmitResolveMSAA( Framebuffer fb ) {
	assert( fb.width == frame_static.viewport_width && fb.height == frame_static.viewport_height );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, fb.fbo );
	glBlitFramebuffer( 0, 0, fb.width, fb.height, 0, 0, fb.width, fb.height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
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
				if( GLAD_GL_KHR_debug != 0 )
					glPopDebugGroup();
				pass_idx++;
				SetupRenderPass( render_passes[ pass_idx ] );
			}

			if( dc.pipeline.shader == NULL ) {
				SubmitResolveMSAA( render_passes[ dc.pipeline.pass ].msaa_source );
			}
			else {
				SubmitDrawCall( dc );
			}
		}
	}

	if( GLAD_GL_KHR_debug != 0 )
		glPopDebugGroup();

	while( pass_idx < render_passes.size() - 1 ) {
		pass_idx++;
		SetupRenderPass( render_passes[ pass_idx ] );
		if( GLAD_GL_KHR_debug != 0 )
			glPopDebugGroup();
	}

	{
		ZoneScopedN( "Deferred deletes" );
		for( const Mesh & mesh : deferred_deletes ) {
			DeleteMesh( mesh );
		}
	}

	u32 ubo_bytes_used = 0;
	for( const UBO & ubo : ubos ) {
		ubo_bytes_used += ubo.bytes_used;
	}
	TracyPlot( "UBO utilisation", float( ubo_bytes_used ) / float( UNIFORM_BUFFER_SIZE * ARRAY_COUNT( ubos ) ) );

	TracyGpuCollect;
}

u32 renderer_num_draw_calls() {
	return draw_calls.size();
}

u32 renderer_num_vertices() {
	return num_vertices_this_frame;
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
		Com_Error( ERR_FATAL, "Ran out of UBO space" );

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
	glBufferData( GL_ARRAY_BUFFER, len, NULL, GL_STREAM_DRAW );

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

static Texture NewTextureSamples( TextureConfig config, int msaa_samples ) {
	Texture texture;
	texture.width = config.width;
	texture.height = config.height;
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

		GLenum filter = TextureFilterToGL( config.filter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );

		if( config.wrap == TextureWrap_Border ) {
			glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, ( GLfloat * ) &config.border_color );
		}

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
		glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, msaa_samples,
			internal_format, config.width, config.height, GL_TRUE );
	}

	return texture;
}

Texture NewTexture( const TextureConfig & config ) {
	return NewTextureSamples( config, 0 );
}

void UpdateTexture( Texture texture, int x, int y, int w, int h, const void * data ) {
	GLenum internal_format, channels, type;
	TextureFormatToGL( texture.format, &internal_format, &channels, &type );
	glBindTexture( GL_TEXTURE_2D, texture.texture );
	glTexSubImage2D( GL_TEXTURE_2D, 0, x, y, w, h, channels, type, data );
}

void DeleteTexture( Texture texture ) {
	glDeleteTextures( 1, &texture.texture );
}

SamplerObject NewSampler( const SamplerConfig & config ) {
	SamplerObject sampler;
	glGenSamplers( 1, &sampler.sampler );
	// glSamplerParameteri( sampler.sampler, GL_TEXTURE_WRAP_S, GL_REPEAT );
	// glSamplerParameteri( sampler.sampler, GL_TEXTURE_WRAP_T, GL_REPEAT );
	// glSamplerParameteri( sampler.sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	// glSamplerParameteri( sampler.sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	return sampler;

}

void DeleteSampler( SamplerObject sampler ) {
	glDeleteSamplers( 1, &sampler.sampler );
}

Framebuffer NewFramebuffer( const FramebufferConfig & config ) {
	GLuint fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );

	Framebuffer fb = { };
	fb.fbo = fbo;

	u32 width = 0;
	u32 height = 0;
	GLenum target = config.msaa_samples <= 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
	GLenum bufs[ 2 ] = { GL_NONE, GL_NONE };

	if( config.albedo_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.albedo_attachment, config.msaa_samples );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, texture.texture, 0 );
		bufs[ 0 ] = GL_COLOR_ATTACHMENT0;

		fb.albedo_texture = texture;

		width = texture.width;
		height = texture.height;
	}

	if( config.normal_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.normal_attachment, config.msaa_samples );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, target, texture.texture, 0 );
		bufs[ 1 ] = GL_COLOR_ATTACHMENT1;

		fb.normal_texture = texture;

		width = texture.width;
		height = texture.height;
	}

	if( config.depth_attachment.width != 0 ) {
		Texture texture = NewTextureSamples( config.depth_attachment, config.msaa_samples );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, texture.texture, 0 );

		fb.depth_texture = texture;

		width = texture.width;
		height = texture.height;
	}

	glDrawBuffers( ARRAY_COUNT( bufs ), bufs );

	assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
	assert( width > 0 && height > 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	fb.width = width;
	fb.height = height;

	return fb;
}

void DeleteFramebuffer( Framebuffer fb ) {
	if( fb.fbo == 0 )
		return;

	glDeleteFramebuffers( 1, &fb.fbo );

	if( fb.albedo_texture.texture != 0 )
		DeleteTexture( fb.albedo_texture );
	if( fb.normal_texture.texture != 0 )
		DeleteTexture( fb.normal_texture );
	if( fb.depth_texture.texture != 0 )
		DeleteTexture( fb.depth_texture );
}

#define MAX_GLSL_UNIFORM_JOINTS 100

static const char * VERTEX_SHADER_PRELUDE =
	"#define VERTEX_SHADER 1\n"
	"#define qf_attribute in\n"
	"#define qf_varying out\n";

static const char * FRAGMENT_SHADER_PRELUDE =
	"#define FRAGMENT_SHADER 1\n"
	"#define qf_varying in\n";

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

	full_srcs[ n ] = "#define qf_texture texture\n";
	full_lens[ n ] = -1;
	n++;

	full_srcs[ n ] = "#define MAX_JOINTS " STR_TOSTR( MAX_GLSL_UNIFORM_JOINTS ) "\n";
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

bool NewShader( Shader * shader, Span< const char * > srcs, Span< int > lens ) {
	*shader = { };

	GLuint vs = CompileShader( GL_VERTEX_SHADER, srcs, lens );
	GLuint fs = CompileShader( GL_FRAGMENT_SHADER, srcs, lens );

	if( vs == 0 || fs == 0 ) {
		if( vs != 0 )
			glDeleteShader( vs );
		if( fs != 0 )
			glDeleteShader( fs );
		return false;
	}

	GLuint program = glCreateProgram();
	glAttachShader( program, vs );
	glAttachShader( program, fs );

	glBindAttribLocation( program, VertexAttribute_Position, "a_Position" );
	glBindAttribLocation( program, VertexAttribute_Normal, "a_Normal" );
	glBindAttribLocation( program, VertexAttribute_TexCoord, "a_TexCoord" );
	glBindAttribLocation( program, VertexAttribute_Color, "a_Color" );
	glBindAttribLocation( program, VertexAttribute_JointIndices, "a_JointIndices" );
	glBindAttribLocation( program, VertexAttribute_JointWeights, "a_JointWeights" );

	glBindAttribLocation( program, VertexAttribute_ParticlePosition, "a_ParticlePosition" );
	glBindAttribLocation( program, VertexAttribute_ParticleScale, "a_ParticleScale" );
	glBindAttribLocation( program, VertexAttribute_ParticleT, "a_ParticleT" );
	glBindAttribLocation( program, VertexAttribute_ParticleColor, "a_ParticleColor" );

	glBindFragDataLocation( program, 0, "f_Albedo" );
	glBindFragDataLocation( program, 1, "f_Normal" );

	glLinkProgram( program );

	glDeleteShader( vs );
	glDeleteShader( fs );

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
	for( GLint i = 0; i < count; i++ ) {
		char name[ 128 ];
		GLint size, len;
		GLenum type;
		glGetActiveUniform( program, i, sizeof( name ), &len, &size, &type, name );

		if( type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_MULTISAMPLE ) {
			glUniform1i( glGetUniformLocation( program, name ), num_textures );
			shader->textures[ num_textures ] = Hash64( name, len );
			num_textures++;
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

u8 AddRenderPass( const RenderPass & pass ) {
	return checked_cast< u8 >( render_passes.add( pass ) );
}

u8 AddRenderPass( const char * name, Framebuffer target, ClearColor clear_color, ClearDepth clear_depth ) {
	RenderPass pass;
	pass.target = target;
	pass.name = name;
	pass.clear_color = clear_color == ClearColor_Do;
	pass.clear_depth = clear_depth == ClearDepth_Do;
	return AddRenderPass( pass );
}

u8 AddRenderPass( const char * name, ClearColor clear_color, ClearDepth clear_depth ) {
	Framebuffer target = { };
	return AddRenderPass( name, target, clear_color, clear_depth );
}

u8 AddUnsortedRenderPass( const char * name ) {
	RenderPass pass;
	pass.name = name;
	pass.sorted = false;
	return AddRenderPass( pass );
}

void AddResolveMSAAPass( Framebuffer fb ) {
	RenderPass pass;
	pass.name = "Resolve MSAA";
	pass.msaa_source = fb;

	PipelineState dummy;
	dummy.pass = AddRenderPass( pass );
	dummy.shader = NULL;

	DrawCall dc = { };
	dc.pipeline = dummy;
	draw_calls.add( dc );
}

void DeferDeleteMesh( const Mesh & mesh ) {
	deferred_deletes.add( mesh );
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

	num_vertices_this_frame += mesh.num_vertices;
}

void DrawInstancedParticles( const Mesh & mesh, VertexBuffer vb, const Material * material, const Material * gradient, BlendFunc blend_func, u32 num_particles ) {
	assert( in_frame );

	PipelineState pipeline;
	pipeline.pass = frame_static.transparent_pass;
	pipeline.shader = &shaders.particle;
	pipeline.blend_func = blend_func;
	pipeline.write_depth = false;
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );
	pipeline.set_uniform( "u_GradientMaterial", UploadUniformBlock( 0.5f / gradient->texture->width ) );
	pipeline.set_texture( "u_BaseTexture", material->texture );
	pipeline.set_texture( "u_GradientTexture", gradient->texture );

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
	glReadPixels( 0, 0, frame_static.viewport_width, frame_static.viewport_height, GL_RGB, GL_UNSIGNED_BYTE, buf );
}
