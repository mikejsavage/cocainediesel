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

	TextureFormat_RA_U8,

	TextureFormat_RGBA_U8,
	TextureFormat_RGBA_U8_sRGB,

	TextureFormat_BC1_sRGB,
	TextureFormat_BC3_sRGB,
	TextureFormat_BC4,
	TextureFormat_BC5,

	TextureFormat_Depth,
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

struct Transform {
	Quaternion rotation;
	Vec3 translation;
	float scale;
};

struct MatrixPalettes {
	Span< Mat3x4 > node_transforms;
	Span< Mat3x4 > skinning_matrices;
};

struct Font;
struct Material;
struct ModelRenderData;
struct DrawModelConfig;
struct PipelineState;

struct GPUMaterial {
	Vec4 color;
	alignas( 16 ) Vec3 tcmod_row0;
	alignas( 16 ) Vec3 tcmod_row1;
};
