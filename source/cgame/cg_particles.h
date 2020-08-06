#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

constexpr u32 MAX_PARTICLE_SYSTEMS = 512;
constexpr u32 MAX_PARTICLE_EMITTERS = 512;
constexpr u32 MAX_PARTICLE_EMITTER_EVENTS = 8;

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

enum ParticleSystemEventType : u8 {
	ParticleSystemEventType_None,
	ParticleSystemEventType_Despawn,
	ParticleSystemEventType_Emitter,
	ParticleSystemEventType_Decal,
};

struct ParticleSystemEvent {
	u8 type;
	StringHash event_name;
};

struct ParticleSystemEvents {
	u8 num_events;
	ParticleSystemEvent events[ MAX_PARTICLE_EMITTER_EVENTS ];
};

struct ParticleSystem {
	size_t max_particles;

	const Material * material;
	const Model * model;

	const Material * gradient;

	BlendFunc blend_func;
	Vec3 acceleration;
	ParticleCollisionType collision;
	float radius;

	ParticleSystemEvents on_collision;
	ParticleSystemEvents on_age;

	// dynamic stuff
	bool initialized;

	size_t num_particles;
	size_t new_particles;
	Span< GPUParticle > particles;
	bool feedback;
	Span< GPUParticleFeedback > particles_feedback;
	Span< u32 > gpu_instances;
	Span< s64 > gpu_instances_time;

	IndexBuffer ibo;
	VertexBuffer vb;
	VertexBuffer vb2;
	VertexBuffer vb_feedback;

	Mesh mesh;
	Mesh update_mesh;
};

struct ParticleEmitter {
	StringHash pSystem;

	float speed;
	RandomDistribution speed_distribution;

	Vec4 start_color, end_color;
	RandomDistribution red_distribution, green_distribution, blue_distribution, alpha_distribution;

	float start_size, end_size;
	RandomDistribution size_distribution;

	float lifetime;
	RandomDistribution lifetime_distribution;

	float count;
	float emission;
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

void InitParticles();
void HotloadParticles();
void ShutdownParticles();

ParticleSystem * FindParticleSystem( StringHash name );
ParticleSystem * FindParticleSystem( const char * name );
ParticleEmitter * FindParticleEmitter( StringHash name );
ParticleEmitter * FindParticleEmitter( const char * name );

ParticleEmitterPosition ParticleEmitterSphere( Vec3 origin, Vec3 normal, float theta = 180.0f, float radius = 0.0f );
ParticleEmitterPosition ParticleEmitterDisk( Vec3 origin, Vec3 normal, float radius = 0.0f );
ParticleEmitterPosition ParticleEmitterLine( Vec3 origin, Vec3 end, float radius = 0.0f );

void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count, Vec4 color );
void EmitParticles( ParticleEmitter * emitter, ParticleEmitterPosition pos, float count );

void DrawParticles();

// void InitParticleMenuEffect();
// void ShutdownParticleEditor();

// void ResetParticleMenuEffect();
// void ResetParticleEditor();

// void DrawParticleMenuEffect();
// void DrawParticleEditor();
