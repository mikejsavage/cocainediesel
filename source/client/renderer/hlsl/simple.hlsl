#include "include/common.hlsl"
#include "include/standard_material.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;

struct ImDrawVert {
	[[vk::location( VertexAttribute_Position )]] float2 position : POSITION;
	[[vk::location( VertexAttribute_Color )]] float4 color : COLOR;
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
};

struct VFXVertex {
	[[vk::location( VertexAttribute_Position )]] float3 position : POSITION;
	[[vk::location( VertexAttribute_Color )]] float4 color : COLOR;
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
};

struct VertexOutput {
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

#ifdef IMGUI
typedef ImDrawVert VertexInput;
#else
typedef VFXVertex VertexInput;
#endif

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	float3 position3;
#ifdef IMGUI
	position3 = float3( input.position, 0.0f );
#else
	position3 = input.position;
#endif
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( position3, 1.0f ) ) );
	output.uv = input.uv;
	output.color = sRGBToLinear( input.color );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
#if IMGUI
	return u_Texture.SampleBias( u_Sampler, v.uv, -1.0f ) * v.color;
#else
	return u_Texture.Sample( u_Sampler, v.uv ) * v.color;
#endif
}
