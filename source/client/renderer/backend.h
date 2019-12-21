#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

constexpr size_t SHADER_MAX_TEXTURES = 4;
constexpr size_t SHADER_MAX_UNIFORM_BLOCKS = 8;

enum BlendFunc : u8 {
	BlendFunc_Disabled,
	BlendFunc_Blend,
	BlendFunc_Add,
};

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

enum PrimitiveType : u8 {
	PrimitiveType_Triangles,
	PrimitiveType_TriangleStrip,
	PrimitiveType_Points,
	PrimitiveType_Lines,
};

enum TextureFormat : u8 {
	TextureFormat_R_U8,
	TextureFormat_R_S8,
	TextureFormat_R_U16,

	TextureFormat_A_U8,

	TextureFormat_RG_Half,

	TextureFormat_RA_U8,

	TextureFormat_RGB_U8,
	TextureFormat_RGB_U8_sRGB,
	TextureFormat_RGB_Half,

	TextureFormat_RGBA_U8,
	TextureFormat_RGBA_U8_sRGB,

	TextureFormat_Depth,
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

enum VertexFormat : u8 {
	VertexFormat_U8x4,
	VertexFormat_U8x4_Norm,

	VertexFormat_U16x4,
	VertexFormat_U16x4_Norm,

	VertexFormat_Halfx2,
	VertexFormat_Halfx3,
	VertexFormat_Halfx4,

	VertexFormat_Floatx1,
	VertexFormat_Floatx2,
	VertexFormat_Floatx3,
	VertexFormat_Floatx4,
};

enum IndexFormat : u8 {
	IndexFormat_U16,
	IndexFormat_U32,
};

struct VertexBuffer {
	u32 vbo;
};

struct IndexBuffer {
	u32 ebo;
};

struct Texture {
	u32 texture;
	u32 width, height;
	bool msaa;
	TextureFormat format;
};

struct SamplerObject {
	u32 sampler;
};

struct Shader {
	u32 program;
	u64 uniforms[ SHADER_MAX_UNIFORM_BLOCKS ];
	u64 textures[ SHADER_MAX_TEXTURES ];
};

struct Framebuffer {
	u32 fbo;
	Texture albedo_texture;
	Texture normal_texture;
	Texture depth_texture;
	u32 width, height;
};

struct UniformBlock {
	u32 ubo;
	u32 offset;
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

	struct Scissor {
		u32 x, y, w, h;
	};

	UniformBinding uniforms[ SHADER_MAX_UNIFORM_BLOCKS ];
	TextureBinding textures[ SHADER_MAX_TEXTURES ];
	size_t num_uniforms = 0;
	size_t num_textures = 0;

	u8 pass = U8_MAX;
	const Shader * shader = NULL;
	BlendFunc blend_func = BlendFunc_Disabled;
	DepthFunc depth_func = DepthFunc_Less;
	CullFace cull_face = CullFace_Back;
	Scissor scissor = { };
	bool write_depth = true;
	bool disable_color_writes = false;
	bool wireframe = false;

	void set_uniform( StringHash name, UniformBlock block ) {
		for( size_t i = 0; i < num_uniforms; i++ ) {
			if( uniforms[ i ].name_hash == name.hash ) {
				uniforms[ i ].block = block;
				return;
			}
		}

		uniforms[ num_uniforms ].name_hash = name.hash;
		uniforms[ num_uniforms ].block = block;
		num_uniforms++;
	}

	void set_texture( StringHash name, const Texture * texture ) {
		for( size_t i = 0; i < num_textures; i++ ) {
			if( textures[ i ].name_hash == name.hash ) {
				textures[ i ].texture = texture;
				return;
			}
		}

		textures[ num_textures ].name_hash = name.hash;
		textures[ num_textures ].texture = texture;
		num_textures++;
	}
};

struct Mesh {
	u32 num_vertices;
	PrimitiveType primitive_type;
	u32 vao;
	VertexBuffer positions;
	VertexBuffer normals;
	VertexBuffer tex_coords;
	VertexBuffer colors;
	VertexBuffer joints;
	VertexBuffer weights;
	IndexBuffer indices;
	IndexFormat indices_format;
	bool ccw_winding;
};

struct GPUParticle {
	Vec3 position;
	float scale;
	float t;
	RGBA8 color;
};

struct MeshConfig {
	MeshConfig() {
		positions = { };
		normals = { };
		tex_coords = { };
		colors = { };
		joints = { };
		weights = { };
	}

	VertexBuffer unified_buffer = { };
	u32 stride = 0;

	union {
		struct {
			VertexBuffer positions;
			VertexBuffer normals;
			VertexBuffer tex_coords;
			VertexBuffer colors;
			VertexBuffer joints;
			VertexBuffer weights;
		};
		struct {
			u32 positions_offset;
			u32 normals_offset;
			u32 tex_coords_offset;
			u32 colors_offset;
			u32 joints_offset;
			u32 weights_offset;
		};
	};

	VertexFormat positions_format = VertexFormat_Floatx3;
	VertexFormat normals_format = VertexFormat_Floatx3;
	VertexFormat tex_coords_format = VertexFormat_Floatx2;
	VertexFormat colors_format = VertexFormat_U8x4_Norm;
	VertexFormat joints_format = VertexFormat_U16x4;
	VertexFormat weights_format = VertexFormat_Floatx4;
	IndexFormat indices_format = IndexFormat_U16;

	IndexBuffer indices = { };
	u32 num_vertices = 0;

	PrimitiveType primitive_type = PrimitiveType_Triangles;
	bool ccw_winding = true;
};

struct TextureConfig {
	u32 width = 0;
	u32 height = 0;

	const void * data = NULL;

	TextureFormat format;
	TextureWrap wrap = TextureWrap_Repeat;
	TextureFilter filter = TextureFilter_Linear;

	Vec4 border_color;
};

struct SamplerConfig {
	TextureWrap wrap = TextureWrap_Repeat;
	TextureFilter filter = TextureFilter_Linear;
	Vec4 border_color;
	// swizzle
};

struct RenderPass {
	const char * name = NULL;

	Framebuffer target = { };

	bool clear_color = false;
	Vec4 color = Vec4( 0 );

	bool clear_depth = false;
	float depth = 1.0f;

	bool sorted = true;

	Framebuffer msaa_source = { };
};

struct FramebufferConfig {
	TextureConfig albedo_attachment = { };
	TextureConfig normal_attachment = { };
	TextureConfig depth_attachment = { };
	int msaa_samples = 0;
};

enum ClearColor { ClearColor_Dont, ClearColor_Do };
enum ClearDepth { ClearDepth_Dont, ClearDepth_Do };

void RenderBackendInit();
void RenderBackendShutdown();

void RenderBackendBeginFrame();
void RenderBackendSubmitFrame();

u8 AddRenderPass( const RenderPass & config );
u8 AddRenderPass( const char * name, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
u8 AddRenderPass( const char * name, Framebuffer target, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
u8 AddUnsortedRenderPass( const char * name );
void AddResolveMSAAPass( Framebuffer fb );

u32 renderer_num_draw_calls();
u32 renderer_num_vertices();

UniformBlock UploadUniforms( const void * data, size_t size );

VertexBuffer NewVertexBuffer( const void * data, u32 len );
VertexBuffer NewVertexBuffer( u32 len );
void WriteVertexBuffer( VertexBuffer vb, const void * data, u32 size, u32 offset = 0 );
void DeleteVertexBuffer( VertexBuffer vb );

template< typename T >
VertexBuffer NewVertexBuffer( Span< T > data ) {
	return NewVertexBuffer( data.ptr, data.num_bytes() );
}

VertexBuffer NewParticleVertexBuffer( u32 n );

IndexBuffer NewIndexBuffer( const void * data, u32 len );
IndexBuffer NewIndexBuffer( u32 len );
void WriteIndexBuffer( IndexBuffer ib, const void * data, u32 size, u32 offset = 0 );
void DeleteIndexBuffer( IndexBuffer ib );

template< typename T >
IndexBuffer NewIndexBuffer( Span< T > data ) {
	return NewIndexBuffer( data.ptr, data.num_bytes() );
}

Texture NewTexture( const TextureConfig & config );
void UpdateTexture( Texture texture, int x, int y, int w, int h, const void * data );
void DeleteTexture( Texture texture );

SamplerObject NewSampler( const SamplerConfig & config );
void DeleteSampler( SamplerObject sampler );

Framebuffer NewFramebuffer( const FramebufferConfig & config );
void DeleteFramebuffer( Framebuffer fb );

bool NewShader( Shader * shader, Span< const char * > srcs, Span< int > lengths );
void DeleteShader( Shader shader );

Mesh NewMesh( MeshConfig config );
void DeleteMesh( const Mesh & mesh );
void DeferDeleteMesh( const Mesh & mesh );

void DrawMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_vertices_override = 0, u32 first_index = 0 );
void DrawInstancedParticles( const Mesh & mesh, VertexBuffer vb, const Material * material, const Material * gradient, BlendFunc blend_func, u32 num_particles );

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
	char buf[ buf_size ];
	memset( buf, 0, sizeof( buf ) );
	SerializeUniforms( buf, 0, rest... );
	return UploadUniforms( buf, sizeof( buf ) );
}
