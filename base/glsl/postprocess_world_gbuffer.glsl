#include "include/uniforms.glsl"
#include "include/common.glsl"

#if MSAA
uniform sampler2DMS u_DepthTexture;
uniform sampler2DMS u_NormalTexture;
#else
uniform sampler2D u_DepthTexture;
uniform sampler2D u_NormalTexture;
#endif

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

out float f_Albedo;

#define USE_NORMAL 0
#define DEPTH_SCALE 10000.0
#define DEPTH_COMPENSATE 0.01
#define NORMAL_SCALE 0.5

void main() {
	vec2 pixel_size = 1.0 / u_ViewportSize;
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;

#if MSAA
	ivec2 p = ivec2( gl_FragCoord.xy );
	ivec2 pixel_right = ivec2( 1, 0 );
	ivec2 pixel_up = ivec2( 0, 1 );

	float edgeness = 0.0;
	for( int i = 0; i < u_Samples; i++ ) {
		// depth discontinuity edges
		float depth =       texelFetch( u_DepthTexture, p, i ).r;
		float depth_right = texelFetch( u_DepthTexture, p + pixel_right, i ).r;
		float depth_left =  texelFetch( u_DepthTexture, p - pixel_right, i ).r;
		float depth_up =    texelFetch( u_DepthTexture, p + pixel_up, i ).r;
		float depth_down =  texelFetch( u_DepthTexture, p - pixel_up, i ).r;
		float depth_hori = 2.0 * depth - depth_right - depth_left;
		float depth_vert = 2.0 * depth - depth_up - depth_down;
		depth_hori *= clamp( u_ViewportSize.x - abs( u_ViewportSize.x - gl_FragCoord.x * 2.0 ), 1.0, 2.0 ) - 1.0;
		depth_vert *= clamp( u_ViewportSize.y - abs( u_ViewportSize.y - gl_FragCoord.y * 2.0 ), 1.0, 2.0 ) - 1.0;

		float depth_edgeness = DEPTH_SCALE * sqrt( depth_hori * depth_hori + depth_vert * depth_vert );
		// TODO: depth compensation, probably a better value to use here..
		depth_edgeness -= DEPTH_COMPENSATE * depth * depth;

		float sample_edgeness = depth_edgeness;

#if USE_NORMAL
		// normal discontinuity edges
		vec2 normal =       texelFetch( u_NormalTexture, p, i ).rg;
		vec2 normal_right = texelFetch( u_NormalTexture, p + pixel_right, i ).rg;
		vec2 normal_left =  texelFetch( u_NormalTexture, p - pixel_right, i ).rg;
		vec2 normal_up =    texelFetch( u_NormalTexture, p + pixel_up, i ).rg;
		vec2 normal_down =  texelFetch( u_NormalTexture, p - pixel_up, i ).rg;
		float normal_hori = length( 2.0 * normal - normal_right - normal_left );
		float normal_vert = length( 2.0 * normal - normal_up - normal_down );
		normal_hori *= clamp( u_ViewportSize.x - abs( u_ViewportSize.x - gl_FragCoord.x * 2.0 ), 1.0, 2.0 ) - 1.0;
		normal_vert *= clamp( u_ViewportSize.y - abs( u_ViewportSize.y - gl_FragCoord.y * 2.0 ), 1.0, 2.0 ) - 1.0;

		float normal_edgeness = NORMAL_SCALE * sqrt( normal_hori * normal_hori + normal_vert * normal_vert );

		sample_edgeness = max( sample_edgeness, normal_edgeness );
#endif
		edgeness += clamp( sample_edgeness, 0.0, 1.0 );
	}
	edgeness /= u_Samples;
#else
	vec2 pixel_right = vec2( pixel_size.x, 0.0 );
	vec2 pixel_up = vec2( 0.0, pixel_size.y );

	// depth discontinuity edges
	float depth =       texture( u_DepthTexture, uv ).r;
	float depth_right = texture( u_DepthTexture, uv + pixel_right ).r;
	float depth_left =  texture( u_DepthTexture, uv - pixel_right ).r;
	float depth_up =    texture( u_DepthTexture, uv + pixel_up ).r;
	float depth_down =  texture( u_DepthTexture, uv - pixel_up ).r;
	float depth_hori = 2.0 * depth - depth_right - depth_left;
	float depth_vert = 2.0 * depth - depth_up - depth_down;
	depth_hori *= clamp( u_ViewportSize.x - abs( u_ViewportSize.x - gl_FragCoord.x * 2.0 ), 1.0, 2.0 ) - 1.0;
	depth_vert *= clamp( u_ViewportSize.y - abs( u_ViewportSize.y - gl_FragCoord.y * 2.0 ), 1.0, 2.0 ) - 1.0;

	float depth_edgeness = DEPTH_SCALE * sqrt( depth_hori * depth_hori + depth_vert * depth_vert );
	// TODO: depth compensation, probably a better value to use here..
	depth_edgeness -= DEPTH_COMPENSATE * depth * depth;

	float edgeness = depth_edgeness;

#if USE_NORMAL
	// normal discontinuity edges
	vec2 normal =       texture( u_NormalTexture, uv ).rg;
	vec2 normal_right = texture( u_NormalTexture, uv + pixel_right ).rg;
	vec2 normal_left =  texture( u_NormalTexture, uv - pixel_right ).rg;
	vec2 normal_up =    texture( u_NormalTexture, uv + pixel_up ).rg;
	vec2 normal_down =  texture( u_NormalTexture, uv - pixel_up ).rg;
	float normal_hori = length( 2.0 * normal - normal_right - normal_left );
	float normal_vert = length( 2.0 * normal - normal_up - normal_down );
	normal_hori *= clamp( u_ViewportSize.x - abs( u_ViewportSize.x - gl_FragCoord.x * 2.0 ), 1.0, 2.0 ) - 1.0;
	normal_vert *= clamp( u_ViewportSize.y - abs( u_ViewportSize.y - gl_FragCoord.y * 2.0 ), 1.0, 2.0 ) - 1.0;

	float normal_edgeness = NORMAL_SCALE * sqrt( normal_hori * normal_hori + normal_vert * normal_vert );

	edgeness = max( edgeness, normal_edgeness );
#endif

#endif

	f_Albedo = LinearTosRGB( edgeness );
}

#endif
