#include "client/client.h"
#include "cgame/cg_local.h"

ParticleSystem NewParticleSystem( Allocator * a, size_t n, Texture texture, Vec3 acceleration ) {
	ParticleSystem ps;
	size_t num_chunks = AlignPow2( n, size_t( 4 ) ) / 4;
	ps.chunks = ALLOC_SPAN( a, ParticleChunk, num_chunks );
	ps.num_particles = 0;

	ps.texture = texture;

	ps.vb = NewParticleVertexBuffer( n );
	ps.vb_memory = ALLOC_MANY( a, GPUParticle, n );

	{
		constexpr Vec2 verts[] = {
			Vec2( -0.5f, -0.5f ),
			Vec2( 0.5f, -0.5f ),
			Vec2( -0.5f, 0.5f ),
			Vec2( 0.5f, 0.5f ),
		};

		Vec2 half_pixel = 0.5f / Vec2( texture.width, texture.height );
		Vec2 uvs[] = {
			half_pixel,
			Vec2( 1.0f - half_pixel.x, half_pixel.y ),
			Vec2( half_pixel.x, 1.0f - half_pixel.y ),
			1.0f - half_pixel,
		};

		constexpr u16 indices[] = { 0, 1, 2, 3 };

		MeshConfig mesh_config;
		mesh_config.positions = NewVertexBuffer( verts, sizeof( verts ) );
		mesh_config.positions_format = VertexFormat_Floatx2;
		mesh_config.tex_coords = NewVertexBuffer( uvs, sizeof( uvs ) );
		mesh_config.indices = NewIndexBuffer( indices, sizeof( indices ) );
		mesh_config.num_vertices = ARRAY_COUNT( indices );
		mesh_config.primitive_type = PrimitiveType_TriangleStrip;

		ps.mesh = NewMesh( mesh_config );
	}

	return ps;
}

void DeleteParticleSystem( Allocator * a, ParticleSystem ps ) {
	FREE( a, ps.chunks.ptr );
	FREE( a, ps.vb_memory );
	DeleteVertexBuffer( ps.vb );
	DeleteMesh( ps.mesh );
}

void InitParticles() {
	Vec3 gravity = Vec3( 0, 0, -GRAVITY );
	cgs.ions = NewParticleSystem( sys_allocator, 8192, FindTexture( "$particle" ), Vec3( 0 ) );
	cgs.sparks = NewParticleSystem( sys_allocator, 8192, FindTexture( "$particle" ), gravity );
	cgs.smoke = NewParticleSystem( sys_allocator, 1024, FindTexture( "gfx/misc/cartoon_smokepuff3" ), Vec3( 0 ) );
}

void ShutdownParticles() {
	DeleteParticleSystem( sys_allocator, cgs.ions );
	DeleteParticleSystem( sys_allocator, cgs.sparks );
	DeleteParticleSystem( sys_allocator, cgs.smoke );
}

static void UpdateParticleChunk( ParticleChunk * chunk, Vec3 acceleration, float dt ) {
	for( int i = 0; i < 4; i++ ) {
		chunk->velocity_x[ i ] += acceleration.x * dt;
		chunk->velocity_y[ i ] += acceleration.y * dt;
		chunk->velocity_z[ i ] += acceleration.z * dt;

		chunk->position_x[ i ] += chunk->velocity_x[ i ] * dt;
		chunk->position_y[ i ] += chunk->velocity_y[ i ] * dt;
		chunk->position_z[ i ] += chunk->velocity_z[ i ] * dt;

		chunk->color_a[ i ] += chunk->dalpha[ i ] * dt;
	}
}

void UpdateParticleSystem( ParticleSystem * ps, float dt ) {
	{
		ZoneScopedN( "Update particles" );
		size_t active_chunks = AlignPow2( ps->num_particles, size_t( 4 ) ) / 4;
		for( size_t i = 0; i < active_chunks; i++ ) {
			UpdateParticleChunk( &ps->chunks[ i ], ps->acceleration, dt );
		}
	}

	ZoneScopedN( "Delete expired particles" );

	// delete expired particles
	for( size_t i = 0; i < ps->num_particles; i++ ) {
		ParticleChunk & chunk = ps->chunks[ i / 4 ];
		size_t chunk_offset = i % 4;

		if( chunk.color_a[ chunk_offset ] <= 0.0f ) {
			ps->num_particles--;
			i--;

			ParticleChunk & swap_chunk = ps->chunks[ ps->num_particles / 4 ];
			size_t swap_offset = ps->num_particles % 4;

			Swap2( &chunk.position_x[ chunk_offset ], &swap_chunk.position_x[ swap_offset ] );
			Swap2( &chunk.position_y[ chunk_offset ], &swap_chunk.position_y[ swap_offset ] );
			Swap2( &chunk.position_z[ chunk_offset ], &swap_chunk.position_z[ swap_offset ] );

			Swap2( &chunk.velocity_x[ chunk_offset ], &swap_chunk.velocity_x[ swap_offset ] );
			Swap2( &chunk.velocity_y[ chunk_offset ], &swap_chunk.velocity_y[ swap_offset ] );
			Swap2( &chunk.velocity_z[ chunk_offset ], &swap_chunk.velocity_z[ swap_offset ] );

			Swap2( &chunk.color_r[ chunk_offset ], &swap_chunk.color_r[ swap_offset ] );
			Swap2( &chunk.color_g[ chunk_offset ], &swap_chunk.color_g[ swap_offset ] );
			Swap2( &chunk.color_b[ chunk_offset ], &swap_chunk.color_b[ swap_offset ] );
			Swap2( &chunk.color_a[ chunk_offset ], &swap_chunk.color_a[ swap_offset ] );

			Swap2( &chunk.dalpha[ chunk_offset ], &swap_chunk.dalpha[ swap_offset ] );

			Swap2( &chunk.size[ chunk_offset ], &swap_chunk.size[ swap_offset ] );
		}
	}
}

void DrawParticleSystem( ParticleSystem * ps ) {
	if( ps->num_particles == 0 )
		return;

	ZoneScoped;

	size_t active_chunks = AlignPow2( ps->num_particles, size_t( 4 ) ) / 4;
	for( size_t i = 0; i < active_chunks; i++ ) {
		const ParticleChunk & chunk = ps->chunks[ i ];
		for( int j = 0; j < 4; j++ ) {
			ps->vb_memory[ i * 4 + j ].position = Vec3( chunk.position_x[ j ], chunk.position_y[ j ], chunk.position_z[ j ] );
			ps->vb_memory[ i * 4 + j ].scale = chunk.size[ j ];
			Vec4 color = Vec4( chunk.color_r[ j ], chunk.color_g[ j ], chunk.color_b[ j ], chunk.color_a[ j ] );
			ps->vb_memory[ i * 4 + j ].color = RGBA8( Clamp01( color ) );
		}
	}
	
	WriteVertexBuffer( ps->vb, ps->vb_memory, ps->num_particles * sizeof( GPUParticle ) );

	DrawInstancedParticles( ps->mesh, ps->vb, ps->texture, ps->num_particles );
}

void DrawParticles() {
	float dt = cg.frameTime / 1000.0f;
	UpdateParticleSystem( &cgs.ions, dt );
	UpdateParticleSystem( &cgs.sparks, dt );
	UpdateParticleSystem( &cgs.smoke, dt );
	DrawParticleSystem( &cgs.ions );
	DrawParticleSystem( &cgs.sparks );
	DrawParticleSystem( &cgs.smoke );
}

void EmitParticle( ParticleSystem * ps, Vec3 position, Vec3 velocity, Vec4 color, float size, float lifetime ) {
	if( ps->num_particles == ps->chunks.n * 4 )
		return;

	ParticleChunk & chunk = ps->chunks[ ps->num_particles / 4 ];
	size_t i = ps->num_particles % 4;

	chunk.position_x[ i ] = position.x;
	chunk.position_y[ i ] = position.y;
	chunk.position_z[ i ] = position.z;

	chunk.velocity_x[ i ] = velocity.x;
	chunk.velocity_y[ i ] = velocity.y;
	chunk.velocity_z[ i ] = velocity.z;

	chunk.color_r[ i ] = color.x;
	chunk.color_g[ i ] = color.y;
	chunk.color_b[ i ] = color.z;
	chunk.color_a[ i ] = color.w;

	chunk.dalpha[ i ] = -color.w / lifetime;

	chunk.size[ i ] = size;

	ps->num_particles++;
}

static Vec3 UniformSampleSphere( RNG * rng ) {
	float z = random_float11( rng );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * float( M_PI ) * random_float01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

static Vec3 UniformSampleInsideSphere( RNG * rng ) {
	Vec3 p = UniformSampleSphere( rng );
	float r = powf( random_float01( rng ), 1.0f / 3.0f );
	return p * r;
}

static Vec2 UniformSampleDisk( RNG * rng ) {
	float theta = random_float01( rng ) * 2.0f * float( M_PI );
	float r = sqrtf( random_float01( rng ) );
	return Vec2( r * cosf( theta ), r * sinf( theta ) );
}

static float SampleNormalDistribution( RNG * rng, float mean, float sigma ) {
	static float spare;
	static bool hasSpare = false;

	if( hasSpare ) {
		hasSpare = false;
		return spare * sigma + mean;
	}

	float u, v, s;
	do {
		u = random_float11( rng );
		v = random_float11( rng );
		s = u * u + v * v;
	} while ( s >= 1.0f || s == 0.0f );

	s = sqrtf( -2.0f * logf( s ) / s );
	spare = v * s;
	hasSpare = true;

	return mean + sigma * u * s;
}

static float SampleRandomDistribution( RNG * rng, RandomDistribution dist ) {
	if( dist.type == RandomDistributionType_Uniform ) {
		return random_uniform_float( rng, dist.uniform.lo, dist.uniform.hi );
	}

	return SampleNormalDistribution( rng, dist.normal.mean, dist.normal.sigma );
}

static void EmitParticle( ParticleSystem * ps, const ParticleEmitter & emitter, float t ) {
	Vec3 position = emitter.position;

	switch( emitter.position_dist_shape ) {
		case DistributionShape_Sphere: {
			position += UniformSampleInsideSphere( &cls.rng ) * emitter.position_sphere.radius;
		} break;

		case DistributionShape_Disk: {
			Vec2 p = UniformSampleDisk( &cls.rng );
			position += emitter.position_disk.radius * Vec3( p, 0.0f );
			// TODO: emitter.position_disk.normal;
		} break;

		case DistributionShape_Line: {
			position = Lerp( position, t, emitter.position_line.end );
		} break;
	}

	// TODO
	Vec3 velocity = emitter.velocity + UniformSampleInsideSphere( &cls.rng ) * emitter.velocity_cone.radius;

	Vec4 color = emitter.color;
	color.x += SampleRandomDistribution( &cls.rng, emitter.red_distribution );
	color.y += SampleRandomDistribution( &cls.rng, emitter.green_distribution );
	color.z += SampleRandomDistribution( &cls.rng, emitter.blue_distribution );
	color.w += SampleRandomDistribution( &cls.rng, emitter.alpha_distribution );

	float size = emitter.size + SampleRandomDistribution( &cls.rng, emitter.size_distribution );

	float lifetime = emitter.lifetime + SampleRandomDistribution( &cls.rng, emitter.lifetime_distribution );

	EmitParticle( ps, position, velocity, color, size, lifetime );
}

void EmitParticles( ParticleSystem * ps, const ParticleEmitter & emitter ) {
	float dt = cg.frameTime / 1000.0f;

	float p = emitter.emission_rate > 0 ? emitter.emission_rate * dt : emitter.n;
	u32 n = u32( p );
	float remaining_p = p - n;

	for( u32 i = 0; i < n; i++ ) {
		float t = float( i ) / ( p + 1.0f );
		EmitParticle( ps, emitter, t );
	}

	if( random_p( &cls.rng, remaining_p ) ) {
		EmitParticle( ps, emitter, p / ( p + 1.0f ) );
	}
}
