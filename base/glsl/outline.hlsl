#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;

struct DrawCallPushConstants {
	vk::BufferPointer< Float3x4 > model_transform;
	vk::BufferPointer< OutlineUniforms > outline;
#ifdef SKINNED
	vk::BufferPointer< Float3x4 > pose;
#endif
};
[[vk::push_constant]] DrawCallPushConstants u_DrawCall;

#include "include/fog.hlsl"
#include "include/skinning.hlsl"

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
	float3x4 skin = SkinningMatrix( u_DrawCall.pose, input.indices, input.weights );
	position4 = mul34( skin, position4 );
	normal = mul( Adjugate( skin ), normal );
#endif

	VertexOutput output;
	output.world_position = mul34( u_DrawCall.model_transform.Get().m, position4 ).xyz + normal * u_DrawCall.outline.Get().height;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( output.world_position, 1.0f ) ) );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	return float4( VoidFog( u_DrawCall.outline.Get().color.rgb, v.world_position.z ), VoidFogAlpha( u_DrawCall.outline.Get().color.a, v.world_position.z ) );
}
