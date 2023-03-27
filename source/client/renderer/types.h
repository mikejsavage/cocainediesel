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

	VertexAttribute_Count,

	// instance stuff
	VertexAttribute_MaterialColor,
	VertexAttribute_MaterialTextureMatrix0,
	VertexAttribute_MaterialTextureMatrix1,
	VertexAttribute_OutlineHeight,
	VertexAttribute_ModelTransformRow0,
	VertexAttribute_ModelTransformRow1,
	VertexAttribute_ModelTransformRow2,
};

struct Shader {
	u32 program;
	u64 uniforms[ 8 ];
	u64 textures[ 4 ];
	u64 texture_arrays[ 2 ];
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

struct GPUParticle {
	Vec3 position;
	float angle;
	Vec3 velocity;
	float angular_velocity;
	float acceleration;
	float drag;
	float restitution;
	float PADDING;
	Vec4 uvwh;
	RGBA8 start_color;
	RGBA8 end_color;
	float start_size;
	float end_size;
	float age;
	float lifetime;
	u32 flags;
	u32 PADDING2;
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

enum InstanceType {
	InstanceType_None,
	InstanceType_Particles,
	InstanceType_Model,
	InstanceType_ModelShadows,
	InstanceType_ModelOutlines,
	InstanceType_ModelSilhouette,

	InstanceType_ComputeShader,
	InstanceType_ComputeShaderIndirect,
};

struct GPUMaterial {
	Vec4 color;
	Vec3 tcmod[ 2 ];
};

struct GPUModelInstance {
	GPUMaterial material;
	Vec4 transform[ 3 ];
};

struct GPUModelShadowsInstance {
	Vec4 transform[ 3 ];
};

struct GPUModelOutlinesInstance {
	Vec4 transform[ 3 ];
	Vec4 color;
	float height;
};

struct GPUModelSilhouetteInstance {
	Vec4 transform[ 3 ];
	Vec4 color;
};
