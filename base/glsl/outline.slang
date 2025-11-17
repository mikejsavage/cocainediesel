#include "include/common.slang"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_ModelTransform;
[[vk::binding( 1, DescriptorSet_DrawCall )]] StructuredBuffer< OutlineUniforms > u_Outline;
#ifdef SKINNED
[[vk::binding( 2, DescriptorSet_DrawCall )]] StructuredBuffer< float3x4 > u_Pose;
#endif

#include "include/fog.slang"
#include "include/skinning.slang"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : POSITION;
	[[vk::location( VertexAttribute_Normal )]] float3 normal : NORMAL;
#ifdef SKINNED
	[[vk::location( VertexAttribute_JointIndices )]] uint4 indices : BLENDINDICES;
	[[vk::location( VertexAttribute_JointWeights )]] float4 weights : BLENDWEIGHT;
#endif
};

struct VertexOutput {
	float3 world_position : POSITION;
	float4 position : SV_Position;
};

VertexOutput VertexMain( VertexInput input ) {
	float4 position4 = float4( input.position, 1.0f );
	float3 normal = input.normal;

#ifdef SKINNED
	float3x4 skin = SkinningMatrix( u_Pose, input.indices, input.weights );
	position4 = mul34( skin, position4 );
	normal = mul( Adjugate( skin ), normal );
#endif

	VertexOutput output;
	output.world_position = mul34( u_ModelTransform[ 0 ], position4 ).xyz + normal * u_Outline[ 0 ].height;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( output.world_position, 1.0f ) ) );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	return float4( VoidFog( u_Outline[ 0 ].color.rgb, v.world_position.z ), VoidFogAlpha( u_Outline[ 0 ].color.a, v.world_position.z ) );
}
