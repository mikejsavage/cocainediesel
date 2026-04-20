#include "../shader_shared.h"

[[vk::binding( 0 )]] StructuredBuffer< Particle > b_PrevParticles;
[[vk::binding( 1 )]] RWStructuredBuffer< Particle > b_NextParticles;
[[vk::binding( 2 )]] StructuredBuffer< Particle > b_NewParticles;
[[vk::binding( 3 )]] StructuredBuffer< uint32_t > b_PrevParticleCount;
[[vk::binding( 4 )]] RWStructuredBuffer< uint32_t > b_NextParticleCount;
[[vk::binding( 5 )]] StructuredBuffer< ParticleUpdateUniforms > b_UpdateParams;

Particle SimulateParticle( Particle old_particle, float dt ) {
	Particle particle = old_particle;
	particle.age += dt;
	particle.position += particle.velocity * dt;
	particle.angle += particle.angular_velocity * dt;
	particle.velocity.z += particle.gravity * dt;
	particle.velocity -= particle.velocity * particle.drag * dt;
	return particle;
}

[numthreads( PARTICLE_THREADGROUP_SIZE, 1, 1 )]
void ComputeMain( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex ) {
	uint32_t in_index = DTid.x;

	// this is a little silly but it's easier than scheduling a copy
	// command in the right place. num_new_particles is generally very
	// small so this doesn't need to be optimal
	/* uint32_t num_threads = ( ( b_PrevParticleCount[ 0 ] + PARTICLE_THREADGROUP_SIZE - 1 ) / PARTICLE_THREADGROUP_SIZE ) * PARTICLE_THREADGROUP_SIZE; */
	/* uint32_t new_particles_per_thread = ( b_UpdateParams[ 0 ].num_new_particles + num_threads - 1 ) / num_threads; */
	/* for( uint32_t i = 0; i < new_particles_per_thread; i++ ) { */
	/* 	uint32_t idx = DTid.x * new_particles_per_thread + i; */
	/* 	if( idx < b_UpdateParams[ 0 ].num_new_particles ) { */
	/* 		b_NextParticles[ idx ] = b_NewParticles[ idx ]; */
	/* 	} */
	/* } */

	// this is easier than issuing a separate copy command
	if( all( DTid == uint3( 0, 0, 0 ) ) ) {
		for( uint32_t i = 0; i < b_UpdateParams[ 0 ].num_new_particles; i++ ) {
			b_NextParticles[ i ] = b_NewParticles[ i ];
		}
	}

	if( in_index >= b_PrevParticleCount[ 0 ] )
		return;

	Particle particle = b_PrevParticles[ in_index ];
	if( particle.age >= particle.lifetime )
		return;

	// TODO: this doesn't include num_new_particles, the setup indirect shader combines the two into the indirect args
	// there is no point doing it like this we should scalarise and write the indirect args here
	// TODO: if we keep the scalar new particle loop that should also get moved to the end if we scalarise
	// TODO: can do 1 atomic per wave instead with something like:
	// uint active_bits = WaveActiveCountBits( in_index < b_PrevParticleCount[ 0 ] );
	// uint32_t out_index;
	// if( WaveIsFirstLane() ) {
	//     InterlockedAdd( b_NextParticleCount[ 0 ], active_bits, out_index );
    // }
	// if( in_index >= b_PrevParticleCount[ 0 ] )
	//     return;
	// out_index = WaveReadFirstLane( out_index, 0 ) + WaveGetLaneIndex();
	uint32_t out_index;
	InterlockedAdd( b_NextParticleCount[ 0 ], 1, out_index );
	b_NextParticles[ out_index + b_UpdateParams[ 0 ].num_new_particles ] = SimulateParticle( particle, b_UpdateParams[ 0 ].dt );
}
