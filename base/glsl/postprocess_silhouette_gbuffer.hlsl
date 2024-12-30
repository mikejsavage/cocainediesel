#include "include/common.hlsl"

[[vk::binding( 0, DescriptorSet_RenderPass )]] Texture2D< float4 > u_SilhouetteTexture;

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
	int2 p = int2( v.position.xy );
	int3 pixel = int3( 0, 1, -1 );

	float4 colour_up =        sRGBToLinear( ClampedTextureLoad( u_SilhouetteTexture, p + pixel.xz ) );
	float4 colour_downleft =  sRGBToLinear( ClampedTextureLoad( u_SilhouetteTexture, p + pixel.yy ) );
	float4 colour_downright = sRGBToLinear( ClampedTextureLoad( u_SilhouetteTexture, p + pixel.zy ) );

	float edgeness_x = length( colour_downright - colour_downleft );
	float edgeness_y = length( lerp( colour_downleft, colour_downright, 0.5f ) - colour_up );
	float edgeness = length( float2( edgeness_x, edgeness_y ) );

	float4 colour = max( max( colour_downleft, colour_downright ), colour_up );

	return edgeness * colour;
}
