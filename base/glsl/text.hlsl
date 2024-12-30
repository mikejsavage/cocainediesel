#include "../../source/client/renderer/shader_shared.h"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float4 position : SV_Position;
	[[vk::location( VertexAttribute_TexCoord )]] float2 uv : TEXCOORD0;
};

struct VertexOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
#include "include/standard_material.hlsl"
[[vk::binding( 0, DescriptorSet_DrawCall )]] StructuredBuffer< TextUniforms > u_Text;

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	output.position = mul( u_View[ 0 ].P * u_View[ 0 ].V, input.position );
	output.uv = input.uv;
	return output;
}

float Median( float3 v ) {
	return max( min( v.x, v.y ), min( max( v.x, v.y ), v.z ) );
}

float LinearStep( float lo, float hi, float x ) {
	return ( clamp( x, lo, hi ) - lo ) / ( hi - lo );
}

float4 SampleMSDF( float2 uv, float half_pixel_size ) {
	float d = 2.0 * Median( u_Texture.Sample( u_Sampler, uv ).rgb ) - 1.0f; // rescale to [-1,1], positive being inside

	if( u_Text[ 0 ].has_border != 0 ) {
		float border_amount = LinearStep( -half_pixel_size, half_pixel_size, d );
		float4 color = lerp( u_Text[ 0 ].border_color, u_Text[ 0 ].color, border_amount );

		float alpha = LinearStep( -3.0f * half_pixel_size, -half_pixel_size, d );
		return float4( color.rgb, color.a * alpha );
	}

	float alpha = LinearStep( -half_pixel_size, half_pixel_size, d );
	return float4( u_Text[ 0 ].color.rgb, u_Text[ 0 ].color.a * alpha );
}

#if DEPTH_ONLY
void FragmentMain( VertexOutput v ) {
#else
float4 FragmentMain( VertexOutput v ) : SV_Target0 {
#endif
	float2 fw = fwidth( v.uv );
	float2 texture_size;
	u_Texture.GetDimensions( texture_size.x, texture_size.y );
	float half_pixel_size = 0.5f * u_Text[ 0 ].dSDF_dTexel * dot( fw, texture_size );

	float supersample_offset = 0.35355f; // rsqrt( 2 ) / 2
	float2 ssx = float2( supersample_offset * fw.x, 0.0f );
	float2 ssy = float2( 0.0f, supersample_offset * fw.y );

	float4 color = SampleMSDF( v.uv, half_pixel_size );
	color += 0.5f * SampleMSDF( v.uv - ssx, half_pixel_size );
	color += 0.5f * SampleMSDF( v.uv + ssx, half_pixel_size );
	color += 0.5f * SampleMSDF( v.uv - ssy, half_pixel_size );
	color += 0.5f * SampleMSDF( v.uv + ssy, half_pixel_size );
	color *= 1.0f / 3.0f;

#if DEPTH_ONLY
	if( color.a <= 0.5f ) {
		discard;
	}
#else
	return color;
#endif
}
