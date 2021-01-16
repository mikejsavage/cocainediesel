#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/fog.glsl"

layout( std140 ) uniform u_Outline {
	vec4 u_OutlineColor;
};

#if MSAA
uniform sampler2DMS u_DepthTexture;
#else
uniform sampler2D u_DepthTexture;
#endif

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

out vec4 f_Albedo;

float edgeDetect( float center, float up, float down_left, float down_right ) {
	float delta = 4.0 * center - 2.0 * up - down_left - down_right;
	vec2 clamping = clamp( u_ViewportSize - abs( u_ViewportSize - gl_FragCoord.xy * 2.0 ), 1.0, 2.0 ) - 1.0;
	delta *= min( clamping.x, clamping.y );
	return smoothstep( 0.0, 0.00001, abs( delta ) );
}

void main() {
#if MSAA
	ivec2 p = ivec2( gl_FragCoord.xy );
	ivec2 pixel_right = ivec2( 1, 0 );
	ivec2 pixel_up = ivec2( 0, 1 );

	float edgeness = 0.0;
	float avg_depth = 0.0;
	for( int i = 0; i < u_Samples; i++ ) {
		float depth =            texelFetch( u_DepthTexture, p, i ).r;
		float depth_up =         texelFetch( u_DepthTexture, p + pixel_up, i ).r;
		float depth_down_left =  texelFetch( u_DepthTexture, p - pixel_up - pixel_right, i ).r;
		float depth_down_right = texelFetch( u_DepthTexture, p - pixel_up + pixel_right, i ).r;
		edgeness += edgeDetect( depth, depth_up, depth_down_left, depth_down_right );
		avg_depth += depth + depth_up + depth_down_left + depth_down_right;
	}
	edgeness /= u_Samples;
	avg_depth /= u_Samples * 4.0;
	edgeness = FogAlpha( edgeness, LinearizeDepth( avg_depth ) );
	edgeness = VoidFogAlpha( edgeness, gl_FragCoord.xy, avg_depth );
#else
	vec2 pixel_size = 1.0 / u_ViewportSize;
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;
	vec2 pixel_right = vec2( pixel_size.x, 0.0 );
	vec2 pixel_up = vec2( 0.0, pixel_size.y );

	float depth =            texture( u_DepthTexture, uv ).r;
	float depth_up =         texture( u_DepthTexture, uv + pixel_up ).r;
	float depth_down_left =  texture( u_DepthTexture, uv - pixel_up - pixel_right ).r;
	float depth_down_right = texture( u_DepthTexture, uv - pixel_up + pixel_right ).r;
	float edgeness = edgeDetect( depth, depth_up, depth_down_left, depth_down_right );
	float avg_depth = ( depth + depth_up + depth_down_left + depth_down_right ) / 4.0;
	edgeness = FogAlpha( edgeness, LinearizeDepth( avg_depth ) );
	edgeness = VoidFogAlpha( edgeness, gl_FragCoord.xy, avg_depth );
#endif
	f_Albedo = LinearTosRGB( edgeness * u_OutlineColor );
}

#endif
