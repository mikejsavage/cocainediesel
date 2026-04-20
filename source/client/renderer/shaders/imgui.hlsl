#include "include/common.hlsl"
#include "include/standard_material.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;

struct ImDrawVert {
	[[vk::location( VertexAttribute_Position )]] float2 position : POSITION;
	[[vk::location( VertexAttribute_Color )]] float4 color : COLOR;
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
};

struct VertexOutput {
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

VertexOutput VertexMain( ImDrawVert input ) {
	VertexOutput output;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( input.position, 1.0f ) ) );
	output.uv = input.uv;
	output.color = sRGBToLinear( input.color );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	return u_Texture.SampleBias( u_Sampler, v.uv, u_MaterialProperties[ 0 ].lod_bias ) * v.color;
}
