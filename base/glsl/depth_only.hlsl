#include "../../source/client/renderer/shader_shared.h"
#include "include/skinning.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< float4x4 > u_Model;
#if SKINNED
[[vk::binding( 1, DescriptorSet_DrawCall )]] StructuredBuffer< Pose > u_Pose;
#endif

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : SV_Position;
#if SKINNED
	[[vk::location( VertexAttribute_JointIndices )]] uint4 indices : BLENDINDICES;
	[[vk::location( VertexAttribute_JointWeights )]] float4 weights : BLENDWEIGHT;
#endif
};

struct VertexOutput {
	float4 position : SV_Position;
};

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	float4 position4 = float4( input.position, 1.0f );
#if SKINNED
	float3x4 skin = SkinningMatrix( input.indices, input.weights, u_Pose[ 0 ] );
	position4 = mul( skin, position4 );
#endif
	output.position = mul( u_View[ 0 ].P * u_View[ 0 ].V * u_Model[ 0 ], position4 );
	return output;
}

void FragmentMain( VertexOutput vertex ) {
}
