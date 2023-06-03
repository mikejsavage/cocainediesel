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

enum FragmentShaderOutputType : u32 {
	FragmentShaderOutput_Albedo,
	FragmentShaderOutput_CurvedSurfaceMask,

	FragmentShaderOutput_Count
};

struct Shader {
	u32 program;
	u64 uniforms[ 8 ];
	u64 textures[ 4 ];
	u64 buffers[ 8 ];
};

struct GPUBuffer {
	u32 buffer;
};

struct UniformBlock {
	u32 ubo;
	u32 offset;
	u32 size;
};

struct Mesh {
	u32 vao;
	GPUBuffer vertex_buffers[ VertexAttribute_Count ];
	GPUBuffer index_buffer;

	IndexFormat index_format;
	u32 num_vertices;
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
	Vec3 tcmod[ 2 ];
};
