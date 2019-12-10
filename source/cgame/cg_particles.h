#pragma once

#include "qcommon/types.h"
#include "client/renderer/renderer.h"

struct ParticleChunk {
	alignas( 16 ) float t[ 4 ];
	alignas( 16 ) float lifetime[ 4 ];

	alignas( 16 ) float position_x[ 4 ];
	alignas( 16 ) float position_y[ 4 ];
	alignas( 16 ) float position_z[ 4 ];

	alignas( 16 ) float velocity_x[ 4 ];
	alignas( 16 ) float velocity_y[ 4 ];
	alignas( 16 ) float velocity_z[ 4 ];

	alignas( 16 ) float dvelocity[ 4 ];

	alignas( 16 ) float color_r[ 4 ];
	alignas( 16 ) float color_g[ 4 ];
	alignas( 16 ) float color_b[ 4 ];
	alignas( 16 ) float color_a[ 4 ];

	alignas( 16 ) float dcolor_r[ 4 ];
	alignas( 16 ) float dcolor_g[ 4 ];
	alignas( 16 ) float dcolor_b[ 4 ];
	alignas( 16 ) float dcolor_a[ 4 ];

	alignas( 16 ) float size[ 4 ];
	alignas( 16 ) float dsize[ 4 ];
};

enum EasingFunction {
	EasingFunction_Linear,
	EasingFunction_Quadratic,
	EasingFunction_Cubic,
	EasingFunction_QuadraticEaseIn,
	EasingFunction_QuadraticEaseOut,
};

struct ParticleSystem {
	Span< ParticleChunk > chunks;
	size_t num_particles;

	VertexBuffer vb;
	GPUParticle * vb_memory;
	Mesh mesh;

	EasingFunction color_easing;
	EasingFunction size_easing;

	BlendFunc blend_func;
	const Material * material;
	const Material * gradient;
	Vec3 acceleration;
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

struct ParticleEmitter {
	Vec3 position;
	RandomDistribution3D position_distribution;

	bool use_cone_direction;
	ConeDistribution direction_cone;

	float start_speed;
	float end_speed;
	RandomDistribution speed_distribution;

	Vec4 start_color;
	Vec3 end_color;
	RandomDistribution red_distribution;
	RandomDistribution green_distribution;
	RandomDistribution blue_distribution;
	RandomDistribution alpha_distribution;

	float start_size, end_size;
	RandomDistribution size_distribution;

	float lifetime;
	RandomDistribution lifetime_distribution;

	float emission_rate;
	float n;
};

void InitParticles();
void ShutdownParticles();

ParticleSystem NewParticleSystem( Allocator * a, size_t n, const Material * material );
void DeleteParticleSystem( Allocator * a, ParticleSystem ps );

void EmitParticles( ParticleSystem * ps, const ParticleEmitter & emitter );

void DrawParticles();

void InitParticleEditor();
void ShutdownParticleEditor();

void ResetParticleEditor();
void DrawParticleEditor();
