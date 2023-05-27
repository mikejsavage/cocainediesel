#pragma once

#include "qcommon/types.h"

enum BlendFunc : u8 {
	BlendFunc_Disabled,
	BlendFunc_Blend,
	BlendFunc_Add,
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
	Vec3 tcmod[ 2 ];
};

#include "client/renderer/opengl_types.h"
#include "client/renderer/metal_types.h"
