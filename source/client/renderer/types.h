#pragma once

#include "qcommon/types.h"

enum BlendFunc : u8 {
	BlendFunc_Disabled,
	BlendFunc_Blend,
	BlendFunc_Add,
};

enum VertexFormat : u8 {
	VertexFormat_U8x2,
	VertexFormat_U8x2_Norm,
	VertexFormat_U8x3,
	VertexFormat_U8x3_Norm,
	VertexFormat_U8x4,
	VertexFormat_U8x4_Norm,

	VertexFormat_U10x3_U2x1_Norm,

	VertexFormat_U16x2,
	VertexFormat_U16x2_Norm,
	VertexFormat_U16x3,
	VertexFormat_U16x3_Norm,
	VertexFormat_U16x4,
	VertexFormat_U16x4_Norm,

	VertexFormat_Floatx2,
	VertexFormat_Floatx3,
	VertexFormat_Floatx4,
};

enum IndexFormat : u8 {
	IndexFormat_U16,
	IndexFormat_U32,
};

enum VertexAttributeType : u32 {
	VertexAttribute_Position,
	VertexAttribute_Normal,
	VertexAttribute_TexCoord,
	VertexAttribute_Color,
	VertexAttribute_JointIndices,
	VertexAttribute_JointWeights,

	VertexAttribute_Count
};

enum FragmentShaderOutputType : u32 {
	FragmentShaderOutput_Albedo,
	FragmentShaderOutput_CurvedSurfaceMask,

	FragmentShaderOutput_Count
};

struct VertexAttribute {
	VertexFormat format;
	size_t buffer;
	size_t offset;
};

struct VertexDescriptor {
	Optional< VertexAttribute > attributes[ VertexAttribute_Count ];
	u32 buffer_strides[ VertexAttribute_Count ];
};

struct Shader {
	u32 program;
	u64 uniforms[ 8 ];
	u64 textures[ 4 ];
	u64 buffers[ 8 ];
};

struct ShaderVariant {
	const Shader * shader;
	VertexDescriptor vertex_descriptor;
};

struct GPUBuffer {
	u32 buffer;
};

struct UniformBlock {
	u32 ubo;
	u32 offset;
	u32 size;
};

enum SamplerWrap : u8 {
	SamplerWrap_Repeat,
	SamplerWrap_Clamp,
};

struct Sampler {
	u32 sampler;
};

enum SamplerType : u8 {
	Sampler_Standard,
	Sampler_Clamp,
	Sampler_Unfiltered,
	Sampler_Shadowmap,

	Sampler_Count
};

enum TextureFormat : u8 {
	TextureFormat_R_U8,
	TextureFormat_R_S8,
	TextureFormat_R_UI8,

	TextureFormat_A_U8,

	TextureFormat_RG_Half,

	TextureFormat_RA_U8,

	TextureFormat_RGBA_U8,
	TextureFormat_RGBA_U8_sRGB,

	TextureFormat_BC1_sRGB,
	TextureFormat_BC3_sRGB,
	TextureFormat_BC4,
	TextureFormat_BC5,

	TextureFormat_Depth,
	TextureFormat_Shadow,
};

struct Texture {
	u32 texture;
	u32 width, height;
	u32 num_layers;
	u32 num_mipmaps;
	int msaa_samples;
	TextureFormat format;
};

struct Mesh {
	GPUBuffer vertex_buffers[ VertexAttribute_Count ];
	GPUBuffer index_buffer;
	u32 num_vertices;
	VertexDescriptor vertex_descriptor;
	IndexFormat index_format;
	bool cw_winding;
};

struct StaticMesh {
	bool skinned;
	u32 base_vertex;
	u32 first_index;

	u32 skinned_base_vertex;
	u32 skinned_first_index;

	u32 num_indices;
	Mat4 transform;
};

struct TRS {
	Quaternion rotation;
	Vec3 translation;
	float scale;
};

struct MatrixPalettes {
	Span< Mat4 > node_transforms;
	Span< Mat4 > skinning_matrices;
};

struct Font;
struct Material;
struct ModelRenderData;
struct DrawModelConfig;
struct PipelineState;

struct U8_Vec2;
struct U8_Vec4;
struct U16_Vec2;
struct U16_Vec3;
struct U16_Vec4;
struct U10_U2_Vec4;

struct U8_Vec2 {
	u8 x, y;
	U8_Vec2() = default;
	constexpr U8_Vec2( u8 x_, u8 y_ ) : x( x_ ), y( y_ ) { }
};

struct U8_Vec4 {
	u8 x, y, z, w;
	U8_Vec4() = default;
	constexpr U8_Vec4( u8 x_, u8 y_, u8 z_, u8 w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }
	constexpr U8_Vec4( U16_Vec4 );
};

struct U16_Vec2 {
	u16 x, y;
	U16_Vec2() = default;
	constexpr U16_Vec2( u16 x_, u16 y_ ) : x( x_ ), y( y_ ) { }
	constexpr U16_Vec2( Vec2 v );
	constexpr U16_Vec2( U8_Vec2 v );
};

struct U16_Vec3 {
	u16 x, y, z;
	U16_Vec3() = default;
	constexpr U16_Vec3( u16 x_, u16 y_, u16 z_ ) : x( x_ ), y( y_ ), z( z_ ) { }
	constexpr U16_Vec3( Vec3 v, MinMax3 bounds );
};

struct U16_Vec4 {
	u16 x, y, z, w;
	U16_Vec4() = default;
	constexpr U16_Vec4( u16 x_, u16 y_, u16 z_, u16 w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }
	constexpr U16_Vec4( Vec4 v );
	constexpr U16_Vec4( U8_Vec4 v );
};

struct U10_U2_Vec4 {
	s32 x : 10;
	s32 y : 10;
	s32 z : 10;
	s32 w : 2;
	U10_U2_Vec4() = default;
	constexpr U10_U2_Vec4( Vec3 v );
};




constexpr U8_Vec4::U8_Vec4( U16_Vec4 v ) {
	x = v.x;
	y = v.y;
	z = v.z;
	w = v.w;
}

constexpr U16_Vec2::U16_Vec2( Vec2 v ) {
	x = u16( Clamp01( v.x ) * float( U16_MAX ) );
	y = u16( Clamp01( v.y ) * float( U16_MAX ) );
}
constexpr U16_Vec2::U16_Vec2( U8_Vec2 v ) {
	x = u16( v.x ) * 2;
	y = u16( v.y ) * 2;
}

constexpr U16_Vec4::U16_Vec4( U8_Vec4 v ) {
	x = u16( v.x ) * 2;
	y = u16( v.y ) * 2;
	z = u16( v.z ) * 2;
	w = u16( v.w ) * 2;
}
constexpr U16_Vec4::U16_Vec4( Vec4 v ) {
	Assert( v.x + v.y + v.z + v.w < 1.01f );
	x = u16( v.x * float( U16_MAX ) + 0.5f );
	y = u16( v.y * float( U16_MAX ) + 0.5f );
	z = u16( v.z * float( U16_MAX ) + 0.5f );
	w = u16( v.w * float( U16_MAX ) + 0.5f );
}

constexpr U16_Vec3::U16_Vec3( Vec3 v, MinMax3 bounds ) {
	x = u16( ( v.x - bounds.mins.x ) / ( bounds.maxs.x - bounds.mins.x ) * float( U16_MAX ) );
	y = u16( ( v.y - bounds.mins.y ) / ( bounds.maxs.y - bounds.mins.y ) * float( U16_MAX ) );
	z = u16( ( v.z - bounds.mins.z ) / ( bounds.maxs.z - bounds.mins.z ) * float( U16_MAX ) );
}

constexpr U10_U2_Vec4::U10_U2_Vec4( Vec3 v ) {
	x = s32( v.x * float( ( 1 << 9 ) - 1 ) + 0.5f );
	y = s32( v.y * float( ( 1 << 9 ) - 1 ) + 0.5f );
	z = s32( v.z * float( ( 1 << 9 ) - 1 ) + 0.5f );
	w = 0;
}



enum DrawType {
	DrawType_Model,
	DrawType_Shadows,
	DrawType_Outlines,
	DrawType_Silhouette,
};

struct MultiDrawElement {
	u32 num_vertices;
	u32 num_instances;
	u32 first_index;
	s32 base_vertex;
	u32 base_instance;
};

struct GPUMaterial {
	float specular;
	float shininess;
	float lod_bias;
	alignas( 16 ) Vec4 color;
	alignas( 16 ) Vec3 tcmod_row0;
	alignas( 16 ) Vec3 tcmod_row1;
};

struct GPUSkinningInstance {
	u32 offset;
};

struct GPUModelInstance {
	Mat3x4 denormalize;
	Mat3x4 transform;
	GPUMaterial material;
};

struct GPUModelShadowsInstance {
	Mat3x4 denormalize;
	Mat3x4 transform;
};

struct GPUModelOutlinesInstance {
	Mat3x4 denormalize;
	Mat3x4 transform;
	Vec4 color;
	float height;
};

struct GPUModelSilhouetteInstance {
	Mat3x4 denormalize;
	Mat3x4 transform;
	Vec4 color;
};
