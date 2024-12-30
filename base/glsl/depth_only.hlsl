#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_Model;
#if SKINNED
[[vk::binding( 1, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_Pose;
#endif

#include "include/skinning.hlsl"

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
	float3x4 skin = SkinningMatrix( input.indices, input.weights );
	position4 = mul34( skin, position4 );
#endif
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, mul34( u_Model[ 0 ], position4 ) ) );
	return output;
}

void FragmentMain( VertexOutput vertex ) {
}
