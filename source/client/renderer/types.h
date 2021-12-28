#pragma once

#include "qcommon/types.h"

enum BlendFunc : u8 {
	BlendFunc_Disabled,
	BlendFunc_Blend,
	BlendFunc_Add,
};

enum PrimitiveType : u8 {
	PrimitiveType_Triangles,
	PrimitiveType_TriangleStrip,
	PrimitiveType_Points,
	PrimitiveType_Lines,
};

enum IndexFormat : u8 {
	IndexFormat_U16,
	IndexFormat_U32,
};

struct Shader {
	u32 program;
	u64 uniforms[ 8 ];
	u64 textures[ 4 ];
	u64 texture_arrays[ 2 ];
	u64 texture_buffers[ 8 ];
	u64 buffers[ 8 ];
};

struct GPUBuffer {
	u32 buffer;
};

using VertexBuffer = GPUBuffer;
using IndexBuffer = GPUBuffer;

struct TextureBuffer {
	u32 tbo;
	u32 texture;
};

struct UniformBlock {
	u32 ubo;
	u32 offset;
	u32 size;
};

struct Mesh {
	u32 num_vertices;
	PrimitiveType primitive_type;
	u32 vao;
	GPUBuffer positions;
	GPUBuffer normals;
	GPUBuffer tex_coords;
	GPUBuffer colors;
	GPUBuffer joints;
	GPUBuffer weights;
	GPUBuffer indices;
	IndexFormat indices_format;
	bool ccw_winding;
};

struct GPUParticle {
	Vec3 position;
	float angle;
	Vec3 velocity;
	float rotation_speed;
	float acceleration;
	float drag;
	float restitution;
	Vec4 uvwh;
	RGBA8 start_color;
	RGBA8 end_color;
	float start_size;
	float end_size;
	float age;
	float lifetime;
	u32 flags;
};

struct GPUParticleFeedback {
	Vec3 position_normal;
	RGB8 color;
	u8 parm;
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

enum InstanceType {
	InstanceType_None,
	InstanceType_Particles,
	InstanceType_Model,
	InstanceType_ModelShadows,
	InstanceType_ModelOutlines,
	InstanceType_ModelSilhouette,
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
