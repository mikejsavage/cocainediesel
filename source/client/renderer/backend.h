#pragma once

#include <string.h>

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

constexpr u32 MAX_RENDER_TARGETS = 4;

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

	TextureFormat_RGBA_Half,

	TextureFormat_BC1_sRGB,
	TextureFormat_BC3_sRGB,
	TextureFormat_BC4,
	TextureFormat_BC5,

	TextureFormat_Depth,
	TextureFormat_Shadow,
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
	VertexFormat_U8x2,
	VertexFormat_U8x2_Norm,
	VertexFormat_U8x3,
	VertexFormat_U8x3_Norm,
	VertexFormat_U8x4,
	VertexFormat_U8x4_Norm,

	VertexFormat_U16x2,
	VertexFormat_U16x2_Norm,
	VertexFormat_U16x3,
	VertexFormat_U16x3_Norm,
	VertexFormat_U16x4,
	VertexFormat_U16x4_Norm,

	VertexFormat_U32x1,

	VertexFormat_Floatx2,
	VertexFormat_Floatx3,
	VertexFormat_Floatx4,
};

enum TextureBufferFormat : u8 {
	TextureBufferFormat_U8x2,
	TextureBufferFormat_U8x4,

	TextureBufferFormat_U32,
	TextureBufferFormat_U32x2,

	TextureBufferFormat_S32x2,
	TextureBufferFormat_S32x3,

	TextureBufferFormat_Floatx4,
};

struct Texture {
	u32 texture;
	u32 width, height;
	u32 num_mipmaps;
	bool msaa;
	TextureFormat format;
};

struct TextureArray {
	u32 texture;
};

struct FramebufferTarget {
	Texture texture;
	Vec4 clear_color = Vec4( 0.0f );
	float clear_depth = 1.0f;
};

struct Framebuffer {
	u32 fbo;
	union {
		struct {
			FramebufferTarget albedo_target;
		};
		FramebufferTarget targets[ MAX_RENDER_TARGETS ] = { };
	};
	FramebufferTarget depth_target = { };
	TextureArray texture_array = { };
	u32 width, height;
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

	struct TextureArrayBinding {
		u64 name_hash;
		TextureArray ta;
	};

	struct TextureBufferBinding {
		u64 name_hash;
		TextureBuffer tb;
	};

	struct Scissor {
		u32 x, y, w, h;
	};

	UniformBinding uniforms[ ARRAY_COUNT( &Shader::uniforms ) ];
	TextureBinding textures[ ARRAY_COUNT( &Shader::textures ) ];
	TextureBufferBinding texture_buffers[ ARRAY_COUNT( &Shader::texture_buffers ) ];
	TextureArrayBinding texture_arrays[ ARRAY_COUNT( &Shader::texture_arrays ) ];
	size_t num_uniforms = 0;
	size_t num_textures = 0;
	size_t num_texture_buffers = 0;
	size_t num_texture_arrays = 0;

	u8 pass = U8_MAX;
	const Shader * shader = NULL;
	BlendFunc blend_func = BlendFunc_Disabled;
	DepthFunc depth_func = DepthFunc_Less;
	CullFace cull_face = CullFace_Back;
	Scissor scissor = { };
	bool write_depth = true;
	bool clamp_depth = false;
	bool view_weapon_depth_hack = false;
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

	void set_texture_buffer( StringHash name, TextureBuffer tb ) {
		for( size_t i = 0; i < num_texture_buffers; i++ ) {
			if( texture_buffers[ i ].name_hash == name.hash ) {
				texture_buffers[ i ].tb = tb;
				return;
			}
		}

		texture_buffers[ num_texture_buffers ].name_hash = name.hash;
		texture_buffers[ num_texture_buffers ].tb = tb;
		num_texture_buffers++;
	}

	void set_texture_array( StringHash name, TextureArray ta ) {
		for( size_t i = 0; i < num_texture_arrays; i++ ) {
			if( texture_arrays[ i ].name_hash == name.hash ) {
				texture_arrays[ i ].ta = ta;
				return;
			}
		}

		texture_arrays[ num_texture_arrays ].name_hash = name.hash;
		texture_arrays[ num_texture_arrays ].ta = ta;
		num_texture_arrays++;
	}
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

	const char * name = NULL;

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
	u32 num_mipmaps = 1;

	const void * data = NULL;

	TextureFormat format;
	TextureWrap wrap = TextureWrap_Repeat;
	TextureFilter filter = TextureFilter_Linear;

	Vec4 border_color;
};

struct TextureArrayConfig {
	u32 width = 0;
	u32 height = 0;
	u32 num_mipmaps = 1;
	u32 layers = 0;

	const void * data = NULL;

	TextureFormat format;
};

namespace tracy { struct SourceLocationData; }

enum RenderPassType {
	RenderPass_Normal,
	RenderPass_Blit,
};

struct RenderPass {
	const char * name = NULL;

	RenderPassType type;

	Framebuffer target = { };

	bool clear_color = false;
	bool clear_depth = false;

	bool sorted = true;

	Framebuffer blit_source = { };

	const tracy::SourceLocationData * tracy;
};

struct FramebufferTargetConfig {
	TextureConfig config;
	FramebufferTarget * target = NULL;
	Vec4 clear_color = Vec4( 0.0f );
	float clear_depth = 1.0f;
};

struct FramebufferConfig {
	union {
		struct {
			FramebufferTargetConfig albedo_attachment;
		};
		FramebufferTargetConfig attachments[ MAX_RENDER_TARGETS ];
	};
	FramebufferTargetConfig depth_attachment;
	int msaa_samples = 0;
};

enum ClearColor { ClearColor_Dont, ClearColor_Do };
enum ClearDepth { ClearDepth_Dont, ClearDepth_Do };

void RenderBackendInit();
void RenderBackendShutdown();

void RenderBackendBeginFrame();
void RenderBackendSubmitFrame();

u8 AddRenderPass( const RenderPass & config );
u8 AddRenderPass( const char * name, const tracy::SourceLocationData * tracy, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
u8 AddRenderPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer target, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
u8 AddUnsortedRenderPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer target = { } );
void AddBlitPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer src, Framebuffer dst, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );
void AddResolveMSAAPass( const char * name, const tracy::SourceLocationData * tracy, Framebuffer src, Framebuffer dst, ClearColor clear_color = ClearColor_Dont, ClearDepth clear_depth = ClearDepth_Dont );

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
void ReadVertexBuffer( VertexBuffer vb, void * data, u32 len, u32 offset = 0 );
void DeleteIndexBuffer( IndexBuffer ib );

template< typename T >
IndexBuffer NewIndexBuffer( Span< T > data ) {
	return NewIndexBuffer( data.ptr, data.num_bytes() );
}

TextureBuffer NewTextureBuffer( TextureBufferFormat format, u32 len );
void WriteTextureBuffer( TextureBuffer tb, const void * data, u32 size );
void DeleteTextureBuffer( TextureBuffer tb );
void DeferDeleteTextureBuffer( TextureBuffer tb );

Texture NewTexture( const TextureConfig & config );
void DeleteTexture( Texture texture );

TextureArray NewTextureArray( const TextureArrayConfig & config );
void DeleteTextureArray( TextureArray ta );

Framebuffer NewFramebuffer( const FramebufferConfig & config );
Framebuffer NewShadowFramebuffer( TextureArray texture_array, u32 layer );
void DeleteFramebuffer( Framebuffer fb );

bool NewShader( Shader * shader, Span< const char * > srcs, Span< int > lengths, Span< const char * > feedback_varyings = Span< const char * >() );
void DeleteShader( Shader shader );

Mesh NewMesh( MeshConfig config );
void DeleteMesh( const Mesh & mesh );
void DeferDeleteMesh( const Mesh & mesh );

void DrawMesh( const Mesh & mesh, const PipelineState & pipeline, u32 num_vertices_override = 0, u32 first_index = 0 );
void UpdateParticles( const Mesh & mesh, VertexBuffer vb_in, VertexBuffer vb_out, float radius, u32 num_particles, float dt );
void UpdateParticlesFeedback( const Mesh & mesh, VertexBuffer vb_in, VertexBuffer vb_out, VertexBuffer vb_feedback, float radius, u32 num_particles, float dt );
void DrawInstancedParticles( const Mesh & mesh, VertexBuffer vb, BlendFunc blend_func, u32 num_particles );
void DrawInstancedParticles( VertexBuffer vb, const Model * model, u32 num_particles );

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
