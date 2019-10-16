#pragma once

#include "qcommon/types.h"
#include "client/renderer/renderer.h"

struct ParticleChunk {
	alignas( 16 ) float position_x[ 4 ];
	alignas( 16 ) float position_y[ 4 ];
	alignas( 16 ) float position_z[ 4 ];

	alignas( 16 ) float velocity_x[ 4 ];
	alignas( 16 ) float velocity_y[ 4 ];
	alignas( 16 ) float velocity_z[ 4 ];

	alignas( 16 ) float color_r[ 4 ];
	alignas( 16 ) float color_g[ 4 ];
	alignas( 16 ) float color_b[ 4 ];
	alignas( 16 ) float color_a[ 4 ];

	alignas( 16 ) float dalpha[ 4 ];

	alignas( 16 ) float size[ 4 ];
};

struct ParticleSystem {
	Span< ParticleChunk > chunks;
	size_t num_particles;

	VertexBuffer vb;
	GPUParticle * vb_memory;
	Mesh mesh;

	BlendFunc blend_func;
	Texture texture;
	Vec3 acceleration;
};

enum DistributionShape {
	DistributionShape_Sphere,
	DistributionShape_Disk,
	DistributionShape_Line,
};

struct SphereDistribution {
	float radius;
};

struct ConeDistribution {
	Vec3 normal;
	float radius;
	float theta;
};

struct DiskDistribution {
	Vec3 normal;
	float radius;
};

struct LineDistribution {
	Vec3 end;
};

enum RandomDistributionType {
	RandomDistributionType_Uniform,
	RandomDistributionType_Normal,
};

struct RandomDistribution {
	RandomDistributionType type;
	union {
		float uniform;
		float sigma;
	};
};

struct ParticleEmitter {
	Vec3 position;
	DistributionShape position_dist_shape;
	SphereDistribution position_sphere;
	DiskDistribution position_disk;
	LineDistribution position_line;

	Vec3 velocity;
	ConeDistribution velocity_cone;

	Vec4 color;
	RandomDistribution red_distribution;
	RandomDistribution green_distribution;
	RandomDistribution blue_distribution;
	RandomDistribution alpha_distribution;

	float size;
	RandomDistribution size_distribution;

	float lifetime;
	RandomDistribution lifetime_distribution;
	MinMax1 lifetime_range;
	
	float emission_rate;
	float n;
};

void InitParticles();
void ShutdownParticles();

ParticleSystem NewParticleSystem( Allocator * a, size_t n, Texture texture, Vec3 acceleration, bool blend );
void DeleteParticleSystem( Allocator * a, ParticleSystem ps );

void EmitParticle( ParticleSystem * ps, Vec3 position, Vec3 velocity, Vec4 color, float size, float lifetime );
void EmitParticles( ParticleSystem * ps, const ParticleEmitter & emitter );

void DrawParticles();

void InitParticleEditor();
void ShutdownParticleEditor();

void ResetParticleEditor();
void DrawParticleEditor();
