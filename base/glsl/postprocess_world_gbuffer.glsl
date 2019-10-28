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

#if MSAA
ivec2 ClampPixelOffset( ivec2 p, int dx, int dy ) {
	return ivec2(
		clamp( p.x + dx, 0, int( u_ViewportSize.x ) - 1 ),
		clamp( p.y + dy, 0, int( u_ViewportSize.y ) - 1 )
	);
}
#endif

float MagicKernel( float right, float left, float up, float down, float min_bias, float max_bias ) {
	float max_magic = max( max( right, left ), max( up, down ) ) + max_bias;
	float min_magic = min( min( right, left ), min( up, down ) ) + min_bias;
	return ( max_magic - min_magic ) * ( max_magic + min_magic );
}

void main() {
	vec2 pixel_size = 1.0 / u_ViewportSize;
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;

#if MSAA
	ivec2 p = ivec2( gl_FragCoord.xy );

	float edgeness = 0.0;
	for( int i = 0; i < u_Samples; i++ ) {
		// normal discontinuity edges
		vec3 normal = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 0, 0 ), i ).rg );
		float normal_right = dot( DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 1, 0 ), i ).rg ), normal );
		float normal_left =  dot( DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, -1, 0 ), i ).rg ), normal );
		float normal_up =    dot( DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 0, 1 ), i ).rg ), normal );
		float normal_down =  dot( DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 0, -1 ), i ).rg ), normal );

		float normal_edgeness = MagicKernel( normal_right, normal_left, normal_up, normal_down, 0.0, -0.05 );

		// depth discontinuity edges
		float depth = LinearizeDepth( texelFetch( u_DepthTexture, ClampPixelOffset( p, 0, 0 ), i ).r );
		float depth_right = depth - LinearizeDepth( texelFetch( u_DepthTexture, ClampPixelOffset( p, 1, 0 ), i ).r );
		float depth_left =  depth - LinearizeDepth( texelFetch( u_DepthTexture, ClampPixelOffset( p, -1, 0 ), i ).r );
		float depth_up =    depth - LinearizeDepth( texelFetch( u_DepthTexture, ClampPixelOffset( p, 0, 1 ), i ).r );
		float depth_down =  depth - LinearizeDepth( texelFetch( u_DepthTexture, ClampPixelOffset( p, 0, -1 ), i ).r );

		float depth_edgeness = MagicKernel( depth_right, depth_left, depth_up, depth_down, -0.2, 0.0 );

		edgeness += max( depth_edgeness, normal_edgeness );
	}
	edgeness /= u_Samples;
#else
	vec2 pixel_right = vec2( pixel_size.x, 0.0 );
	vec2 pixel_up = vec2( 0.0, pixel_size.y );

	// normal discontinuity edges
	vec3 normal = DecompressNormal( qf_texture( u_NormalTexture, uv ).rg );
	float normal_right = dot( DecompressNormal( qf_texture( u_NormalTexture, uv + pixel_right ).rg ), normal );
	float normal_left =  dot( DecompressNormal( qf_texture( u_NormalTexture, uv - pixel_right ).rg ), normal );
	float normal_up =    dot( DecompressNormal( qf_texture( u_NormalTexture, uv + pixel_up ).rg ), normal );
	float normal_down =  dot( DecompressNormal( qf_texture( u_NormalTexture, uv - pixel_up ).rg ), normal );

	float normal_edgeness = MagicKernel( normal_right, normal_left, normal_up, normal_down, 0.0, -0.05 );

	// depth discontinuity edges
	float depth = LinearizeDepth( qf_texture( u_DepthTexture, uv ).r );
	float depth_right = depth - LinearizeDepth( qf_texture( u_DepthTexture, uv + pixel_right ).r );
	float depth_left =  depth - LinearizeDepth( qf_texture( u_DepthTexture, uv - pixel_right ).r );
	float depth_up =    depth - LinearizeDepth( qf_texture( u_DepthTexture, uv + pixel_up ).r );
	float depth_down =  depth - LinearizeDepth( qf_texture( u_DepthTexture, uv - pixel_up ).r );

	float depth_edgeness = MagicKernel( depth_right, depth_left, depth_up, depth_down, -0.2, 0.0 );

	float edgeness = max( depth_edgeness, normal_edgeness );
#endif

	f_Albedo = LinearTosRGB( edgeness );
}

#endif
