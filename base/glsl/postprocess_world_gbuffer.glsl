#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/fog.glsl"

float LinearizeDepth( float ndc ) {
	return u_NearClip / ( 1.0 - ndc );
}

layout( std140 ) uniform u_Outline {
	vec4 u_OutlineColor;
};

#if MSAA
uniform sampler2DMS u_DepthTexture;
uniform usampler2DMS u_CurvedSurfaceMask;
#else
uniform sampler2D u_DepthTexture;
uniform usampler2D u_CurvedSurfaceMask;
#endif

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

float edgeDetect( float center, float up, float down_left, float down_right, float epsilon ) {
	float delta = 4.0 * center - 2.0 * up - down_left - down_right;
	return smoothstep( 0.0, epsilon, abs( delta ) );
}

void main() {
	ivec2 p = ivec2( gl_FragCoord.xy );
	ivec3 pixel = ivec3( 0, 1, -1 );

	float depth =            ClampedTexelFetch( u_DepthTexture, p, gl_SampleID ).r;
	float depth_up =         ClampedTexelFetch( u_DepthTexture, p + pixel.xz, gl_SampleID ).r;
	float depth_down_left =  ClampedTexelFetch( u_DepthTexture, p + pixel.yy, gl_SampleID ).r;
	float depth_down_right = ClampedTexelFetch( u_DepthTexture, p + pixel.zy, gl_SampleID ).r;

	uint mask = texelFetch( u_CurvedSurfaceMask, p, gl_SampleID ).r;
	float epsilon = ( mask & MASK_CURVED ) == MASK_CURVED ? 0.005 : 0.00001;
	float edgeness = edgeDetect( depth, depth_up, depth_down_left, depth_down_right, epsilon );
	float avg_depth = 0.25 * ( depth + depth_up + depth_down_left + depth_down_right );

	vec2 clamping = clamp( u_ViewportSize - abs( u_ViewportSize - gl_FragCoord.xy * 2.0 ), 1.0, 2.0 ) - 1.0;
	edgeness *= min( clamping.x, clamping.y );
	if( edgeness < 0.1 ) {
		discard;
	}
	edgeness = FogAlpha( edgeness, LinearizeDepth( avg_depth ) );
	edgeness = VoidFogAlpha( edgeness, gl_FragCoord.xy, avg_depth );
	f_Albedo = edgeness * u_OutlineColor;
}

#endif
