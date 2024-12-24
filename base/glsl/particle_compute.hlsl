#include "../../source/client/renderer/shader_shared.h"

[[vk::binding( 0 )]] StructuredBuffer< Particle > b_ParticlesIn;
[[vk::binding( 1 )]] RWStructuredBuffer< Particle > b_ParticlesOut;
[[vk::binding( 2 )]] StructuredBuffer< Particle > b_NewParticles;
[[vk::binding( 3 )]] StructuredBuffer< uint32_t > b_ComputeCountIn;
[[vk::binding( 4 )]] RWStructuredBuffer< uint32_t > b_ComputeCountOut;
[[vk::binding( 5 )]] StructuredBuffer< ParticleUpdateUniforms > b_UpdateStep;

Particle SimulateParticle( Particle old_particle, float dt ) {
	Particle particle = old_particle;
	particle.age += dt;

	/* if( !collide( particle, dt ) ) { */
		particle.position += particle.velocity * dt;
		particle.angle += particle.angular_velocity * dt;
	/* } */

	particle.velocity.z += particle.acceleration * dt;
	particle.velocity -= particle.velocity * particle.drag * dt;

	return particle;
}

[numthreads( PARTICLE_THREADGROUP_SIZE, 1, 1 )]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex ) {
	uint32_t in_index = DTid.x;

	// this is a little silly but it's easier than scheduling a copy
	// command in the right place. num_new_particles is generally very
	// small so this doesn't need to be optimal
	uint32_t num_threads = ( ( b_ComputeCountIn[ 0 ] + PARTICLE_THREADGROUP_SIZE - 1 ) / PARTICLE_THREADGROUP_SIZE ) * PARTICLE_THREADGROUP_SIZE;
	uint32_t new_particles_per_thread = ( b_UpdateStep[ 0 ].num_new_particles + num_threads - 1 ) / num_threads;
	for( uint32_t i = 0; i < new_particles_per_thread; i++ ) {
		uint32_t idx = DTid.x * new_particles_per_thread + i;
		if( idx < b_ComputeCountIn[ 0 ] ) {
			b_ParticlesOut[ idx ] = b_NewParticles[ idx ];
		}
	}

	if( in_index >= b_ComputeCountIn[ 0 ] )
		return;

	Particle particle = b_ParticlesIn[ in_index ];
	if( particle.age >= particle.lifetime )
		return;

	uint32_t out_index;
	InterlockedAdd( b_ComputeCountOut[ 0 ], 1, out_index );
	b_ParticlesOut[ out_index ] = SimulateParticle( particle, b_UpdateStep[ 0 ].dt );
}
