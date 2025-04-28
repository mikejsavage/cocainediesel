#include "include/common.slang"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1, DescriptorSet_RenderPass )]] StructuredBuffer< float4 > u_OutlineColor;

#ifdef MSAA
[[vk::binding( 2, DescriptorSet_RenderPass )]] Texture2DMS< float > u_DepthTexture;
[[vk::binding( 3, DescriptorSet_RenderPass )]] Texture2DMS< uint > u_CurvedSurfaceMask;
#else
[[vk::binding( 2, DescriptorSet_RenderPass )]] Texture2D< float > u_DepthTexture;
[[vk::binding( 3, DescriptorSet_RenderPass )]] Texture2D< uint > u_CurvedSurfaceMask;
#endif

#include "include/fog.slang"

struct VertexInput {
	[[vk::location( VertexAttribute_Position )]] float3 position : POSITION;
};

struct VertexOutput {
	float4 position : SV_Position;
};

VertexOutput VertexMain( VertexInput input ) {
	VertexOutput output;
	output.position = float4( input.position, 1.0f );
	return output;
}

float LinearizeDepth( float ndc ) {
	return u_View[ 0 ].near_clip / ( 1.0f - ndc );
}

float EdgeDetect( float center, float up, float down_left, float down_right, float epsilon ) {
	float delta = 4.0f * center - 2.0f * up - down_left - down_right;
	return smoothstep( 0.0f, epsilon, abs( delta ) );
}

#ifdef MSAA
float4 FragmentMain( VertexInput input, uint sample_index : SV_SampleIndex ) : FragmentShaderOutput_Albedo {
#define SAMPLE_INDEX_LAST_ARG , sample_index
#else
float4 FragmentMain( VertexInput input ) : FragmentShaderOutput_Albedo {
#define SAMPLE_INDEX_LAST_ARG
#endif

	int2 p = int2( input.position.xy );

	float depth =            ClampedTextureLoad( u_DepthTexture, p SAMPLE_INDEX_LAST_ARG );
	float depth_up =         ClampedTextureLoad( u_DepthTexture, p + int2( +0, -1 ) SAMPLE_INDEX_LAST_ARG );
	float depth_down_right = ClampedTextureLoad( u_DepthTexture, p + int2( +1, +1 ) SAMPLE_INDEX_LAST_ARG );
	float depth_down_left  = ClampedTextureLoad( u_DepthTexture, p + int2( -1, +1 ) SAMPLE_INDEX_LAST_ARG );

#ifdef MSAA
	uint mask = u_CurvedSurfaceMask.Load( p, sample_index );
#else
	uint mask = u_CurvedSurfaceMask.Load( int3( p, 0 ) );
#endif
	float epsilon = ( mask & MASK_CURVED ) == MASK_CURVED ? 0.005f : 0.00001f;
	float edgeness = EdgeDetect( depth, depth_up, depth_down_left, depth_down_right, epsilon );
	float avg_depth = 0.25f * ( depth + depth_up + depth_down_left + depth_down_right );

	float2 clamping = clamp( u_View[ 0 ].viewport_size - abs( u_View[ 0 ].viewport_size - input.position.xy * 2.0f ), 1.0f, 2.0f ) - 1.0f;
	edgeness *= min( clamping.x, clamping.y );
	if( edgeness < 0.1f ) {
		discard;
	}
	edgeness = FogAlpha( edgeness, LinearizeDepth( avg_depth ) );
	edgeness = VoidFogAlpha( edgeness, input.position.xy, avg_depth );
	return edgeness * u_OutlineColor[ 0 ];
}
