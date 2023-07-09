#pragma once

#include "qcommon/types.h"
#include "client/renderer/opengl_types.h"
#include "client/renderer/metal_types.h"
#include "client/renderer/shader_shared.h"

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

struct VertexAttribute {
	VertexFormat format;
	size_t buffer;
	size_t offset;
};

enum TextureFormat : u8 {
	TextureFormat_R_U8,
	TextureFormat_R_S8,
	TextureFormat_R_UI8,
	TextureFormat_R_U16,

	TextureFormat_A_U8,

	TextureFormat_RG_Half,

	TextureFormat_RA_U8,

	TextureFormat_RGB_U8,
	TextureFormat_RGB_U8_sRGB,
	TextureFormat_RGB_Half,

	TextureFormat_RGBA_U8,
	TextureFormat_RGBA_U8_sRGB,

	TextureFormat_BC1_sRGB,
	TextureFormat_BC3_sRGB,
	TextureFormat_BC4,
	TextureFormat_BC5,

	TextureFormat_Depth,
	TextureFormat_Shadow,
};

struct VertexDescriptor {
	Optional< VertexAttribute > attributes[ VertexAttribute_Count ];
	u32 buffer_strides[ VertexAttribute_Count ];
};

struct ShaderVariant {
	const Shader * shader;
	VertexDescriptor vertex_descriptor;
};

struct Texture {
	TextureHandle handle;
	u32 width, height;
	u32 num_mipmaps;
	bool msaa;
	TextureFormat format;
};

struct GPUBufferSpan {
	GPUBuffer buffer;
	u32 offset;
	u32 size;
};

using UniformBlock = GPUBufferSpan;

struct Mesh {
	GPUBuffer vertex_buffers[ VertexAttribute_Count ];
	GPUBuffer index_buffer;
	u32 num_vertices;
	VertexDescriptor vertex_descriptor;
	IndexFormat index_format;
	bool cw_winding;
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
struct Model;
struct PipelineState;

struct GPUMaterial {
	Vec4 color;
	alignas( 16 ) Vec3 tcmod_row0;
	alignas( 16 ) Vec3 tcmod_row1;
};
