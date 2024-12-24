#include "../../source/client/renderer/shader_shared.h"

[[vk::binding( 0 )]] StructuredBuffer< NewParticlesUniforms > b_ParticleUpdate;
[[vk::binding( 1 )]] RWStructuredBuffer< uint32_t > b_NextComputeCount;
[[vk::binding( 2 )]] RWStructuredBuffer< uint32_t > b_ComputeCount;
[[vk::binding( 3 )]] RWStructuredBuffer< DispatchComputeIndirectArguments > b_ComputeIndirect;
[[vk::binding( 4 )]] RWStructuredBuffer< DrawArraysIndirectArguments > b_DrawIndirect;

[numthreads( 1, 1, 1 )]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex ) {
	if( b_ParticleUpdate[ 0 ].clear ) {
		b_ComputeCount[ 0 ] = 0;
	}
	else {
		b_ComputeCount[ 0 ] += b_ParticleUpdate[ 0 ].num_new_particles;
	}

	b_NextComputeCount[ 0 ] = 0;
	b_ComputeIndirect[ 0 ].num_groups_x = b_ComputeCount[ 0 ] / 64 + 1;
	b_DrawIndirect[ 0 ].num_instances = b_ComputeCount[ 0 ];
}
