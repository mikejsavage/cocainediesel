#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;

struct DrawCallPushConstants {
	vk::BufferPointer< Float3x4 > model_transform;
	vk::BufferPointer< float4 > silhouette_color;
#ifdef SKINNED
	vk::BufferPointer< Float3x4 > pose;
#endif
};
[[vk::push_constant]] DrawCallPushConstants u_DrawCall;

#include "include/skinning.hlsl"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : POSITION;
#ifdef SKINNED
	[[vk::location( VertexAttribute_JointIndices )]] uint4 indices : BLENDINDICES;
	[[vk::location( VertexAttribute_JointWeights )]] float4 weights : BLENDWEIGHT;
#endif
};

float4 VertexMain( VertexInput input ) : SV_Position {
	float4 position4 = float4( input.position, 1.0f );
#ifdef SKINNED
	position4 = mul34( SkinningMatrix( u_DrawCall.pose, input.indices, input.weights ), position4 );
#endif
	return mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, mul34( u_DrawCall.model_transform.Get().m, position4 ) ) );
}

float4 FragmentMain() : FragmentShaderOutput_Albedo {
	return u_DrawCall.silhouette_color.Get();
}
