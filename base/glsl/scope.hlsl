#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : SV_Position;
};

struct VertexOutput {
	float4 position : SV_Position;
};

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	output.position = float4( input.position, 1.0f );
	return output;
}

float4 FragmentMain( VertexOutput v ) : FragmentShaderOutput_Albedo {
	const float4 crosshair_color = float4( 1.0f, 0.0f, 0.0f, 1.0f );
	const float4 crosshair_lines_color = float4( 0.0f, 0.0f, 0.0f, 1.0f );
	const float4 vignette_color = float4( 0.0f, 0.0f, 0.0f, 1.0f );

	float2 p = v.position.xy - u_View[ 0 ].viewport_size * 0.5f;

	float radial_frac = length( p ) * 2.0f / min( u_View[ 0 ].viewport_size.x, u_View[ 0 ].viewport_size.y );
	float cross_dist = min( abs( floor( p.x ) ), abs( floor( p.y ) ) );

	float crosshair_frac = step( radial_frac, 0.035f );
	float4 crosshair = lerp( crosshair_lines_color, crosshair_color, crosshair_frac );
	crosshair.a = step( cross_dist, 1.0f - crosshair_frac );

	float vignette_frac = smoothstep( 0.375f, 0.95f, radial_frac );
	return lerp( crosshair, vignette_color, vignette_frac );
}
