#include "include/common.slang"
#include "include/standard_material.slang"

// TODO: probably just make this 2 shaders

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;

// must match ImDrawVert
struct ImGuiVertexInput {
	[[vk::location( VertexAttribute_Position )]] float2 position : POSITION;
	[[vk::location( VertexAttribute_Color )]] float4 color : COLOR;
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
};

// must match VFXVertex
struct VFXVertexInput {
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
typedef ImGuiVertexInput VertexInput;
#else
typedef VFXVertexInput VertexInput;
#endif

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, float4( input.position, 1.0f ) ) );
	output.uv = input.uv;
	output.color = sRGBToLinear( input.color );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	return u_Texture.SampleBias( u_Sampler, v.uv, u_MaterialProperties[ 0 ].lod_bias ) * v.color;
}
