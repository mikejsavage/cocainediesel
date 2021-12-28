#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

constexpr u32 MAX_PARTICLE_SYSTEMS = 512;
constexpr u32 MAX_PARTICLE_EMITTERS = 512;
constexpr u32 MAX_PARTICLE_EMITTER_EVENTS = 8;
constexpr u32 MAX_PARTICLE_EMITTER_MATERIALS = 16;

constexpr u32 MAX_DECAL_EMITTERS = 512;
constexpr u32 MAX_DECAL_EMITTER_MATERIALS = 8;

constexpr u32 MAX_DLIGHT_EMITTERS = 512;

constexpr u32 MAX_VISUAL_EFFECT_GROUPS = 512;
constexpr u32 MAX_VISUAL_EFFECTS = 16;

enum EasingFunction {
	EasingFunction_Linear,
	EasingFunction_Quadratic,
	EasingFunction_Cubic,
	EasingFunction_QuadraticEaseIn,
	EasingFunction_QuadraticEaseOut,
};

enum RandomDistribution3DType : u8 {
	RandomDistribution3DType_Sphere,
	RandomDistribution3DType_Disk,
	RandomDistribution3DType_Line,
};

struct SphereDistribution {
	float radius;
};

struct ConeDistribution {
	Vec3 normal;
	float theta;
};

struct DiskDistribution {
	Vec3 normal;
	float radius;
};

struct LineDistribution {
	Vec3 end;
};

enum RandomDistributionType : u8 {
	RandomDistributionType_Uniform,
	RandomDistributionType_Normal,
};

struct RandomDistribution {
	u8 type;
	union {
		float uniform;
		float sigma;
	};
};

struct RandomDistribution3D {
	u8 type;
	union {
		SphereDistribution sphere;
		DiskDistribution disk;
		LineDistribution line;
	};
};

enum ParticleCollisionType : u8 {
	ParticleCollisionType_None,
	ParticleCollisionType_Point,
	ParticleCollisionType_Sphere,
};

struct ParticleEvents {
	u8 num_events;
	StringHash events[ MAX_PARTICLE_EMITTER_EVENTS ];
};

struct ParticleSystem {
	size_t max_particles;

	const Model * model;

	BlendFunc blend_func;
	float radius;

	ParticleEvents on_collision;
	ParticleEvents on_age;
	ParticleEvents on_frame;

	// dynamic stuff
	bool initialized;

	size_t num_particles;
	size_t new_particles;
	Span< GPUParticle > particles;
	bool feedback;
	Span< GPUParticleFeedback > particles_feedback;
	Span< u32 > gpu_instances;
	Span< s64 > gpu_instances_time;

	GPUBuffer ibo;
	GPUBuffer vb;
	GPUBuffer vb2;
	GPUBuffer vb_feedback;

	Mesh mesh;
	Mesh update_mesh;
};

enum VisualEffectType : u8 {
	VisualEffectType_Particles,
	VisualEffectType_Decal,
	VisualEffectType_DynamicLight,
};

struct VisualEffect {
	u8 type;
	u64 hash;
};

struct VisualEffectGroup {
	VisualEffect effects[ MAX_VISUAL_EFFECTS ];
	u8 num_effects;
};

enum ParticleEmitterPositionType : u8 {
	ParticleEmitterPosition_Sphere,
	ParticleEmitterPosition_Disk,
	ParticleEmitterPosition_Line,
};

struct ParticleEmitterPosition {
	u8 type;
	Vec3 origin;
	union {
		Vec3 normal;
		Vec3 end;
	};
	float theta;
	float radius;
	RandomDistribution surface_offset;
	float surface_theta;
};

struct ParticleEmitter {
	u64 particle_system;

	u8 num_materials;
	StringHash materials[ MAX_PARTICLE_EMITTER_MATERIALS ];
	StringHash model;

	BlendFunc blend_func = BlendFunc_Add;

	ParticleEmitterPosition position;

	float acceleration;
	float drag = 0.0f;
	float restitution = 0.8f;

	float speed;
	RandomDistribution speed_distribution;

	float angle;
	RandomDistribution angle_distribution;

	float rotation;
	RandomDistribution rotation_distribution;

	Vec4 start_color = Vec4( 1.0f ), end_color = Vec4( 1.0f );
	RandomDistribution red_distribution, green_distribution, blue_distribution, alpha_distribution;
	bool color_override;

	float start_size = 16.0f, end_size = 16.0f;
	RandomDistribution size_distribution;

	float lifetime = 1.0f;
	RandomDistribution lifetime_distribution;

	float count;
	float emission;

	u32 flags;

	bool feedback;
	ParticleEvents on_collision;
	ParticleEvents on_age;
	ParticleEvents on_frame;
};

struct DecalEmitter {
	u8 num_materials;
	StringHash materials[ MAX_DECAL_EMITTER_MATERIALS ];

	Vec4 color = Vec4( 1.0f );
	RandomDistribution red_distribution, green_distribution, blue_distribution, alpha_distribution;
	bool color_override;

	float size = 32.0f;
	RandomDistribution size_distribution;

	float lifetime = 30.0f;
	RandomDistribution lifetime_distribution;

	float height = 0.0f;
};

struct DynamicLightEmitter {
	Vec4 color = Vec4( 1.0f );
	RandomDistribution red_distribution, green_distribution, blue_distribution, alpha_distribution;
	bool color_override;

	float intensity = 3200.0f;
	RandomDistribution intensity_distribution;

	float lifetime = 5.0f;
	RandomDistribution lifetime_distribution;
};

void InitVisualEffects();
void HotloadVisualEffects();
void ShutdownVisualEffects();

ParticleEmitterPosition ParticleEmitterSphere( Vec3 origin, Vec3 normal, float theta = 180.0f, float radius = 0.0f );
ParticleEmitterPosition ParticleEmitterSphere( Vec3 origin, float radius = 0.0f );
ParticleEmitterPosition ParticleEmitterDisk( Vec3 origin, Vec3 normal, float radius = 0.0f );
ParticleEmitterPosition ParticleEmitterLine( Vec3 origin, Vec3 end, float radius = 0.0f );

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 start_color );
void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count );

void DoVisualEffect( const char * name, Vec3 origin, Vec3 normal = Vec3( 0.0f, 0.0f, 1.0f ), float count = 1.0f, Vec4 color = Vec4( 1.0f ), float decal_lifetime_scale = 1.0f );
void DoVisualEffect( StringHash name, Vec3 origin, Vec3 normal = Vec3( 0.0f, 0.0f, 1.0f ), float count = 1.0f, Vec4 color = Vec4( 1.0f ), float decal_lifetime_scale = 1.0f );

void DrawParticles();
void ClearParticles();

// void InitParticleMenuEffect();
// void ShutdownParticleEditor();

// void ResetParticleMenuEffect();
// void ResetParticleEditor();

void DrawParticleMenuEffect();
// void DrawParticleEditor();
