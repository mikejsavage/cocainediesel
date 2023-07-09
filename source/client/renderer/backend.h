#pragma once

#include <string.h>

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;

enum CullFace : u8 {
	CullFace_Back,
	CullFace_Front,
	CullFace_Disabled,
};

enum DepthFunc : u8 {
	DepthFunc_Less,
	DepthFunc_Equal,
	DepthFunc_Always,
	DepthFunc_Disabled, // also disables writing
};

enum TextureWrap : u8 {
	TextureWrap_Repeat,
	TextureWrap_Clamp,
	TextureWrap_Mirror,
	TextureWrap_Border,
};

enum TextureFilter : u8 {
	TextureFilter_Linear,
	TextureFilter_Point,
};

struct BindGroupDescriptor {
	struct UniformBinding {
		u64 name_hash;
		GPUBufferSpan span;
	};

	struct TextureBinding {
		u64 name_hash;
		const Texture * texture;
	};

	struct BufferBinding {
		u64 name_hash;
		GPUBufferSpan span;
	};

	UniformBinding uniforms[ ARRAY_COUNT( &Shader::uniforms ) ];
	TextureBinding textures[ ARRAY_COUNT( &Shader::textures ) ];
	BufferBinding buffers[ ARRAY_COUNT( &Shader::buffers ) ];
	size_t num_uniforms = 0;
	size_t num_textures = 0;
	size_t num_buffers = 0;
};

struct BindGroup {
	BindGroupDescriptor lol;
};

struct RenderTarget {
	u32 fbo;
	Texture color_attachments[ FragmentShaderOutput_Count ];
	Texture depth_attachment;
	u32 width, height;
};

struct StreamingBuffer {
	GPUBuffer buffer;
	void * ptr;
	u32 size;
};

struct PipelineState {
	struct UniformBinding {
		u64 name_hash;
		UniformBlock block;
	};

	struct TextureBinding {
		u64 name_hash;
		const Texture * texture;
	};

	struct BufferBinding {
		u64 name_hash;
		GPUBuffer buffer;
		u32 offset;
		u32 size;
	};

	struct Scissor {
		u32 x, y, w, h;
	};

	UniformBinding uniforms[ ARRAY_COUNT( &Shader::uniforms ) ];
	TextureBinding textures[ ARRAY_COUNT( &Shader::textures ) ];
	BufferBinding buffers[ ARRAY_COUNT( &Shader::buffers ) ];
	size_t num_uniforms = 0;
	size_t num_textures = 0;
	size_t num_buffers = 0;

	u8 pass = U8_MAX;
	const Shader * shader = NULL;
	BlendFunc blend_func = BlendFunc_Disabled;
	DepthFunc depth_func = DepthFunc_Less;
	CullFace cull_face = CullFace_Back;
	Optional< Scissor > scissor = NONE;
	bool write_depth = true;
	bool clamp_depth = false;
	bool view_weapon_depth_hack = false;
	bool wireframe = false;

	void bind_uniform( StringHash name, UniformBlock block );
	void bind_texture( StringHash name, const Texture * texture );
	void bind_buffer( StringHash name, GPUBuffer buffer, u32 offset = 0, u32 size = 0 );
	void bind_streaming_buffer( StringHash name, StreamingBuffer stream );
};

struct MeshConfig {
	const char * name;

	VertexDescriptor vertex_descriptor;
	GPUBuffer vertex_buffers[ VertexAttribute_Count ];

	IndexFormat index_format;
	GPUBuffer index_buffer;
	u32 num_vertices;

	bool cw_winding;

	static constexpr VertexFormat default_attribute_formats[] = {
		VertexFormat_Floatx3,
		VertexFormat_Floatx3,
		VertexFormat_Floatx2,
		VertexFormat_U8x4_Norm,
		VertexFormat_U16x4,
		VertexFormat_Floatx4,
	};

	void set_attribute( VertexAttributeType type, GPUBuffer buffer, Optional< VertexFormat > format = NONE ) {
		VertexAttribute attribute = { };
		attribute.format = format.exists ? format.value : default_attribute_formats[ type ];
		attribute.buffer = type;
		vertex_descriptor.attributes[ type ] = attribute;
		vertex_buffers[ type ] = buffer;
	}

	void set_attribute( VertexAttributeType type, size_t buffer, size_t offset ) {
		VertexAttribute attribute = { };
		attribute.format = default_attribute_formats[ type ];
		attribute.buffer = buffer;
		attribute.offset = offset;
		vertex_descriptor.attributes[ type ] = attribute;
	}
};

struct TextureConfig {
	TextureFormat format;
	u32 width = 0;
	u32 height = 0;
	u32 num_layers = 0;
	u32 num_mipmaps = 1;
	int msaa_samples = 0;

	const void * data = NULL;

	TextureWrap wrap = TextureWrap_Repeat;
	TextureFilter filter = TextureFilter_Linear;

	Vec4 border_color;
};

namespace tracy { struct SourceLocationData; }

enum RenderPassType {
	RenderPass_Normal,
	RenderPass_Blit,
};

struct RenderPass {
	RenderPassType type;

	RenderTarget target = { };

	bool barrier = false;

	bool clear_color = false;
	Vec4 color = Vec4( 0 );

	bool clear_depth = false;
	float depth = 1.0f;

	bool sorted = true;

	RenderTarget blit_source = { };

	const tracy::SourceLocationData * tracy;
};

struct RenderTargetConfig {
	struct Attachment {
		Texture texture;
		Optional< u32 > layer;
	};

	Optional< Attachment > color_attachments[ FragmentShaderOutput_Count ] = { };
	Optional< Attachment > depth_attachment = NONE;
};

enum ClearColor { ClearColor_Dont, ClearColor_Do };
enum ClearDepth { ClearDepth_Dont, ClearDepth_Do };

void InitRenderBackend();
void ShutdownRenderBackend();

void RenderBackendBeginFrame();
void RenderBackendSubmitFrame();

u8 AddRenderPass( const tracy::SourceLocationData * tracy, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
u8 AddRenderPass( const tracy::SourceLocationData * tracy, RenderTarget target, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
u8 AddUnsortedRenderPass( const tracy::SourceLocationData * tracy, RenderTarget target = { } );
u8 AddBarrierRenderPass( const tracy::SourceLocationData * tracy, RenderTarget target = { } );
void AddBlitPass( const tracy::SourceLocationData * tracy, RenderTarget src, RenderTarget dst, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
void AddResolveMSAAPass( const tracy::SourceLocationData * tracy, RenderTarget src, RenderTarget dst, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );

UniformBlock UploadUniforms( const void * data, size_t size );

GPUBuffer NewGPUBuffer( const void * data, u32 size, const char * name = NULL );
GPUBuffer NewGPUBuffer( u32 size, const char * name = NULL );
void WriteGPUBuffer( GPUBuffer buf, const void * data, u32 size, u32 offset = 0 );
void DeleteGPUBuffer( GPUBuffer buf );
void DeferDeleteGPUBuffer( GPUBuffer buf );

StreamingBuffer NewStreamingBuffer( u32 size, const char * name = NULL );
void * GetStreamingBufferMemory( StreamingBuffer stream );
void FlushStreamingBuffer( StreamingBuffer stream, u32 offset, u32 length );
void DeleteStreamingBuffer( StreamingBuffer buf );
void DeferDeleteStreamingBuffer( StreamingBuffer buf );

template< typename T >
void WriteAndFlushStreamingBuffer( StreamingBuffer stream, const T * data, size_t n, size_t offset = 0 ) {
	memcpy( ( ( u8 * ) GetStreamingBufferMemory( stream ) ) + offset * sizeof( T ), data, n * sizeof( T ) );
	FlushStreamingBuffer( stream, offset * sizeof( T ), n * sizeof( T ) );
}

template< typename T >
GPUBuffer NewGPUBuffer( Span< T > data, const char * name = NULL ) {
	return NewGPUBuffer( data.ptr, data.num_bytes(), name );
}

template< typename T >
void WriteGPUBuffer( GPUBuffer buf, Span< T > data, u32 offset = 0 ) {
	WriteGPUBuffer( buf, data.ptr, data.num_bytes(), offset );
}

Texture NewTexture( const TextureConfig & config );
void DeleteTexture( Texture texture );

RenderTarget NewRenderTarget( const RenderTargetConfig & config );
void DeleteRenderTarget( RenderTarget rt );
void DeleteRenderTargetAndTextures( RenderTarget rt );

bool NewShader( Shader * shader, const char * src, const char * name );
bool NewComputeShader( Shader * shader, const char * src, const char * name );
void DeleteShader( Shader shader );

Mesh NewMesh( const MeshConfig & config );
void DeleteMesh( const Mesh & mesh );
void DeferDeleteMesh( const Mesh & mesh );

void DrawMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_vertices_override = 0, u32 first_index = 0, u32 base_vertex = 0 );
void DrawInstancedMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_instances, u32 num_vertices_override = 0, u32 first_index = 0, u32 base_vertex = 0 );
void DrawMeshIndirect( const Mesh & mesh, const PipelineState & pipeline, GPUBuffer indirect );
void DispatchCompute( const PipelineState & pipeline, u32 x, u32 y, u32 z );
void DispatchComputeIndirect( const PipelineState & pipeline, GPUBuffer indirect );

void DownloadFramebuffer( void * buf );

template< typename T > constexpr size_t Std140Alignment();
template<> constexpr size_t Std140Alignment< s32 >() { return sizeof( s32 ); }
template<> constexpr size_t Std140Alignment< u32 >() { return sizeof( u32 ); }
template<> constexpr size_t Std140Alignment< float >() { return sizeof( float ); }
template<> constexpr size_t Std140Alignment< Vec2 >() { return sizeof( Vec2 ); }
template<> constexpr size_t Std140Alignment< Vec3 >() { return sizeof( Vec4 ); } // !
template<> constexpr size_t Std140Alignment< Vec4 >() { return sizeof( Vec4 ); }
template<> constexpr size_t Std140Alignment< Mat4 >() { return sizeof( Vec4 ); } // !

template< typename T >
constexpr size_t Std140Size( size_t size ) {
	return sizeof( T ) + AlignPow2( size, Std140Alignment< T >() );
}

template< typename S, typename T, typename... Rest >
constexpr size_t Std140Size( size_t size ) {
	return Std140Size< T, Rest... >( sizeof( S ) + AlignPow2( size, Std140Alignment< S >() ) );
}

inline void SerializeUniforms( char * buf, size_t len ) { }

template< typename T, typename... Rest >
void SerializeUniforms( char * buf, size_t len, const T & first, const Rest & ... rest ) {
	len = AlignPow2( len, Std140Alignment< T >() );
	memcpy( buf + len, &first, sizeof( first ) );
	SerializeUniforms( buf, len + sizeof( first ), rest... );
}

template< typename... Rest >
UniformBlock UploadUniformBlock( Rest... rest ) {
	// assign to constexpr variable to break the build if it
	// stops being constexpr, instead of switching to VLA
	constexpr size_t buf_size = Std140Size< Rest... >( 0 );
	char buf[ buf_size ] = { };
	SerializeUniforms( buf, 0, rest... );
	return UploadUniforms( buf, sizeof( buf ) );
}
