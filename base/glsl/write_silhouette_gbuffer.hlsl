#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_ModelTransform;
[[vk::binding( 1, DescriptorSet_DrawCall )]] StructuredBuffer< float4 > u_SilhouetteColor;
#if SKINNED
[[vk::binding( 2, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_Pose;
#endif

#include "include/skinning.hlsl"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : SV_Position;
#if SKINNED
	[[vk::location( VertexAttribute_JointIndices )]] uint4 indices : BLENDINDICES;
	[[vk::location( VertexAttribute_JointWeights )]] float4 weights : BLENDWEIGHT;
#endif
};

float4 VertexMain( VertexInput input ) : SV_Position {
	float4 position4 = float4( input.position, 1.0f );

#if SKINNED
	float3x4 skin = SkinningMatrix( input.indices, input.weights );
	position4 = mul34( skin, position4 );
#endif

	return mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, mul34( u_ModelTransform[ 0 ], position4 ) ) );
}

float4 FragmentMain() : FragmentShaderOutput_Albedo {
	return u_SilhouetteColor[ 0 ];
}