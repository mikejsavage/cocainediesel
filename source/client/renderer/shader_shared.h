#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

#if __cplusplus
  #include "qcommon/types.h"
  #define shaderconst constexpr
#else
  #define shaderconst static const
  #define s32 int32_t
  #define u32 uint32_t
  #define Vec2 float2
  #define Vec3 float3
  #define Vec4 float4
  #define Mat3x4 float3x4
  #define Mat4 float4x4
#endif

shaderconst u32 FORWARD_PLUS_TILE_SIZE = 32;
shaderconst u32 FORWARD_PLUS_TILE_CAPACITY = 50;
shaderconst u32 SKINNED_MODEL_MAX_JOINTS = 100;
shaderconst float DLIGHT_CUTOFF = 0.5f;
shaderconst u32 PARTICLE_THREADGROUP_SIZE = 64;

enum ParticleFlags : u32 {
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

enum FragmentShaderOutputType {
	FragmentShaderOutput_Albedo,
	FragmentShaderOutput_CurvedSurfaceMask,

	FragmentShaderOutput_Count
};

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
	s32 has_border;
};

struct ViewUniforms {
	Mat3x4 V;
	Mat3x4 inverse_V;
	Mat4 P;
	Mat4 inverse_P;
	Vec3 camera_pos;
	Vec2 viewport_size;
	float near_clip;
	int msaa_samples;
	Vec3 light_direction;
};

struct ShadowMapUniforms {
	struct Cascade {
		float plane;
		Vec3 offset;
		Vec3 scale;
	};

	Mat3x4 shadow_view_projection;
	Cascade cascades[ 4 ];
	s32 num_cascades;
};

struct MaterialStaticUniforms {
	float specular;
	float shininess;
	float lod_bias;
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
	u32 start_color;
	u32 end_color;
	float start_size;
	float end_size;
	float age;
	float lifetime;
	u32 flags;
	u32 PADDING2;
};

struct ParticleUpdateUniforms {
	u32 collision;
	float dt;
	u32 num_new_particles;
};


struct OutlineUniforms {
	Vec4 color;
	float height;
};

#endif // header guard
