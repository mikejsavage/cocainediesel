#pragma once

// types and constants
#ifdef __cplusplus
	#include "qcommon/types.h"
	#define shaderconst constexpr

	using f16 = u16;

	struct f16x3 { u16 x, y, z; };
	struct s16x3 { s16 x, y, z; };
	struct u16x2 { u16 x, y; };
	struct u16x4 { u16 x, y, z, w; };

	struct s16x4 { s16 x, y, z, w; };
#else
	#define shaderconst static const

	typedef uint16_t u16;
	typedef int32_t s32;
	typedef uint32_t u32;
	typedef half f16;
	typedef half3 f16x3;

	typedef float2 Vec2;
	typedef float3 Vec3;
	typedef float4 Vec4;
	typedef float3x4 Mat3x4;
	typedef float4x4 Mat4;

	typedef int16_t3 s16x3;
	typedef uint16_t2 u16x2;
	typedef uint16_t4 u16x4;

	typedef int16_t4 s16x4;

	typedef uint32_t RGBA8;
#endif

shaderconst u32 FORWARD_PLUS_TILE_SIZE = 32;
shaderconst u32 FORWARD_PLUS_TILE_CAPACITY = 50;
shaderconst u32 SKINNED_MODEL_MAX_JOINTS = 100;
shaderconst float LIGHT_CUTOFF = 0.5f;
shaderconst u32 PARTICLE_THREADGROUP_SIZE = 64;

enum ParticleFlags/* : u32*/ {
	ParticleFlag_CollisionPoint = u32( 1 ) << u32( 0 ),
	ParticleFlag_CollisionSphere = u32( 1 ) << u32( 1 ),
	ParticleFlag_Rotate = u32( 1 ) << u32( 2 ),
	ParticleFlag_Stretch = u32( 1 ) << u32( 3 ),
};

enum VertexAttributeType {
	VertexAttribute_Position,
	VertexAttribute_Normal,
	VertexAttribute_TexCoord,
	VertexAttribute_Color,
	VertexAttribute_JointIndices,
	VertexAttribute_JointWeights,

	VertexAttribute_Count
};

#ifdef __cplusplus
enum FragmentShaderOutputType {
	FragmentShaderOutput_Albedo,
	FragmentShaderOutput_CurvedSurfaceMask,

	FragmentShaderOutput_Count
};
#else
#define FragmentShaderOutput_Albedo SV_Target0
#define FragmentShaderOutput_CurvedSurfaceMask SV_Target1
#endif

enum DescriptorSetType {
	DescriptorSet_RenderPass,
	DescriptorSet_Material,
	DescriptorSet_DrawCall,

	DescriptorSet_Count
};

struct IndirectComputeArgs {
	u32 num_threadgroups_x;
	u32 num_threadgroups_y;
	u32 num_threadgroups_z;
};

struct IndirectDrawArgs {
	u32 num_vertices;
	u32 num_instances;
	u32 base_vertex;
	u32 base_instance;
};

// general
struct ViewUniforms {
	Mat3x4 V;
	Mat3x4 inverse_V;
	Mat4 P;
	Mat4 inverse_P;
	Vec3 camera_pos;
	Vec2 viewport_size;
	float near_clip;
	Vec3 sun_direction;
};

// standard.hlsl
struct MaterialProperties {
	float specular;
	float shininess;
};

struct ShadowmapUniforms {
	struct Cascade {
		float plane;
		Vec3 offset;
		Vec3 scale;
	};

	Mat4 m;
	Cascade cascades[ 4 ];
	u32 num_cascades;
};

// dynamics
struct TileCullingInputs {
	u32 rows, cols;
	u32 num_decals;
	u32 num_lights;
};

struct TileCountsUniforms {
	u32 num_decals;
	u32 num_lights;
};

struct TileIndices {
	u32 indices[ FORWARD_PLUS_TILE_CAPACITY ];
};

struct Decal {
	RGBA8 color; // note that RGBA8 has 4 byte alignment on the GPU
	s16x3 origin;
	f16 radius;
	s16x4 orientation;
	f16 height;
	u16 layer;
	u16x2 uv;
	u16x2 wh;
};

struct Light {
	RGBA8 color;
	s16x3 origin;
	f16 radius;
};

// outline.hlsl
struct OutlineUniforms {
	Vec4 color;
	float height;
};

// particles
struct Particle {
	RGBA8 start_color;
	RGBA8 end_color;
	Vec3 position;
	f16 angle;
	f16x3 velocity;
	f16 angular_velocity;
	f16 gravity;
	f16 drag;
	f16 restitution;
	u16 layer;
	u16x2 uv;
	u16x2 wh;
	u16x4 trim;
	f16 start_size;
	f16 end_size;
	f16 age;
	f16 lifetime;
	u16 flags;
};

struct NewParticlesUniforms {
	u32 num_new_particles;
	u32 clear;
};

struct ParticleUpdateUniforms {
	u32 collision;
	float dt;
	u32 num_new_particles;
};

// postprocessing
struct PostprocessPreUIUniforms {
	float vignette;
	float radial_blur;
	float exposure;
	float gamma;
	float brightness;
	float contrast;
	float saturation;
};

struct PostprocessUniforms {
	float time;
	float damage;
	float crt;
};

// text.hlsl
struct TextUniforms {
	Vec4 color;
	Vec4 border_color;
	float dSDF_dTexel;
	u32 has_border;
};
