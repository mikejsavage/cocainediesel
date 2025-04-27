#pragma once

#ifdef __cplusplus
  #include "qcommon/types.h"
  #define shaderconst constexpr
#else
  #define shaderconst static const
  typedef uint32_t u32;
  typedef float2 Vec2;
  typedef float3 Vec3;
  typedef float4 Vec4;
  typedef float3x4 Mat3x4;
  typedef float4x4 Mat4;
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

struct TextUniforms {
	Vec4 color;
	Vec4 border_color;
	float dSDF_dTexel;
	u32 has_border;
};

struct ViewUniforms {
	Mat3x4 V;
	Mat3x4 inverse_V;
	Mat4 P;
	Mat4 inverse_P;
	Vec3 camera_pos;
	Vec2 viewport_size;
	float near_clip;
	u32 msaa_samples;
	Vec3 sun_direction;
};

struct ShadowmapUniforms {
	struct Cascade {
		float plane;
		Vec3 offset;
		Vec3 scale;
	};

	Mat3x4 VP;
	Cascade cascades[ 4 ];
	u32 num_cascades;
};

struct MaterialProperties {
	float specular = 0.0f;
	float shininess = 64.0f;
	float lod_bias = 0.0f;
};

struct Particle {
	Vec3 position;
	float angle;
	Vec3 velocity;
	float angular_velocity;
	float acceleration;
	float drag;
	float restitution;
	float PADDING;
	Vec4 uvwh;
	Vec4 trim;
	RGBA8 start_color;
	RGBA8 end_color;
	float start_size;
	float end_size;
	float age;
	float lifetime;
	u32 flags;
	u32 PADDING2;
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

// TODO: use half floats etc
struct Decal {
	Vec3 origin_orientation_xyz; // floor( origin ) + ( orientation.xyz * 0.49 + 0.5 )
	float radius_orientation_w; // floor( radius ) + ( orientation.w * 0.49 + 0.5 )
	Vec4 color_uvwh_height; // vec4( u + layer, v + floor( r * 255 ) + floor( height ) * 256, w + floor( g * 255 ), h + floor( b * 255 ) )
	// NOTE(msc): uvwh should all be < 1.0
};

struct Light {
	Vec3 origin_color; // floor( origin ) + ( color * 0.9 )
	float radius;
};

struct OutlineUniforms {
	Vec4 color;
	float height;
};

struct DispatchComputeIndirectArguments {
	u32 num_groups_x;
	u32 num_groups_y;
	u32 num_groups_z;
};

struct DrawArraysIndirectArguments {
	u32 count;
	u32 num_instances;
	u32 base_vertex;
	u32 base_instance;
};

struct PostprocessUniforms {
	float time;
	float damage;
	float crt;
	float brightness;
	float contrast;
};

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
