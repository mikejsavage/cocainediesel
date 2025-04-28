#include "include/common.slang"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1, DescriptorSet_RenderPass )]] StructuredBuffer< float > u_Time;
[[vk::binding( 2, DescriptorSet_RenderPass )]] Texture2D< float4 > u_RGBNoiseTexture;
[[vk::binding( 3, DescriptorSet_RenderPass )]] Texture2D< float4 > u_BlueNoiseTexture;
[[vk::binding( 4, DescriptorSet_RenderPass )]] SamplerState u_StandardSampler;

#include "include/dither.slang"
#include "include/fog.slang"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float4 position : POSITION;
};

struct VertexOutput {
	float3 world_position : POSITION;
	float4 position : SV_Position;
};

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	output.world_position = input.position.xyz;
	output.position = mul( u_View[ 0 ].P, mul34( u_View[ 0 ].V, input.position ) );
	return output;
}

float value( float2 p ) {
	float2 f = floor( p );
	float2 s = ( p - f );
	s *= s * ( 3.0f - 2.0f * s );
	return u_RGBNoiseTexture.Sample( u_StandardSampler, ( f + s - 0.5f ) / 256.0f ).r;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	float2 uv = v.world_position.xy / v.world_position.z;

	float3 cloud_color = 0.01f;
	float3 sky_color = 0.06f;
	float3 sun_color = 1.0f;

	float2 wind = float2( 0.3f, -0.3f );
	float iterations = 4.0f;

	// sun
	float sun_fract = max( 0.0f, dot( normalize( v.world_position.xyz ), -u_View[ 0 ].sun_direction ) );
	sun_fract = pow( sun_fract, 66.6f ) * 4.0f;
	sky_color = lerp( sky_color, sun_color, sun_fract );

	// clouds
	float2 h = 0.0f;
	float a = 1.0f;
	float s = 1.0f;
	for( float i = 0.0; i < iterations; i++ ) {
		a *= 2.2f;
		s *= 2.0f;
		float2 v_uv = uv * s;
		v_uv += uv * i * 0.5f; // parallax
		v_uv += wind * u_Time[ 0 ]; // movement
		v_uv += h.x * a / s * float2( 4.0f, 7.0f ); // warping
		h += float2( value( v_uv ), 1.0f ) / a;
	}
	float g = -h.x / h.y;
	float n = smoothstep( 0.0f, 0.5f, v.world_position.z );
	float m = smoothstep( 30.0f, 0.0f, length( uv ) );

	float3 color = lerp( sky_color, cloud_color, exp( g ) * n * m );
	color = VoidFog( color, v.position.xy );
	return float4( color + Dither( v.position.xy ), 1.0f );
}
