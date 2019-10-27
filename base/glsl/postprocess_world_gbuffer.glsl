#include "include/common.glsl"
#include "include/uniforms.glsl"

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

float Threshold( float x, float t ) {
	return ( 1.0 - t ) * max( 0.0, x - t );
}

float Scharr( vec3 sample00, vec3 sample10, vec3 sample20, vec3 sample01, vec3 sample21, vec3 sample02, vec3 sample12, vec3 sample22 ) {
	float x = length( 3.0 * sample00 + 10.0 * sample01 + 3.0 * sample02 - ( 3.0 * sample20 + 10.0 * sample21 + 3.0 * sample22 ) ) * ( 1.0 / 32.0 );
	float y = length( 3.0 * sample00 + 10.0 * sample10 + 3.0 * sample20 - ( 3.0 * sample02 + 10.0 * sample12 + 3.0 * sample22 ) ) * ( 1.0 / 32.0 );
	return length( vec2( x, y ) );
}

ivec2 ClampPixelOffset( ivec2 p, int dx, int dy ) {
	return ivec2(
		clamp( p.x + dx, 0, int( u_ViewportSize.x ) - 1 ),
		clamp( p.y + dy, 0, int( u_ViewportSize.y ) - 1 )
	);
}

vec3 ViewPosition( vec2 uv ) {
	float depth = qf_texture( u_DepthTexture, uv ).r;

	vec4 clip = vec4( vec3( uv, depth ) * 2.0 - 1.0, 1.0 );
	vec4 world = u_InverseP * clip;
	return ( world / world.w ).xyz;
}

void main() {
	vec2 pixel_size = 1.0 / u_ViewportSize;
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;

	vec3 normal = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, -pixel_size.y ) ).rg );
	float depth_left = length( ViewPosition( uv + vec2( -pixel_size.x, 0.0 ) ) );
	float depth_right = length( ViewPosition( uv + vec2( +pixel_size.x, 0.0 ) ) );
	float depth = LinearizeDepth( qf_texture( u_DepthTexture, uv ).r, u_NearClip );
	float depth_up = length( ViewPosition( uv + vec2( 0.0, -pixel_size.y ) ) );
	float depth_down = length( ViewPosition( uv + vec2( 0.0, pixel_size.y ) ) );

	vec3 camera_backward = vec3( u_V[ 0 ].z, u_V[ 1 ].z, u_V[ 2 ].z );

	float dx = depth_right - depth_left;
	float dy = depth_down - depth_up;
	float depth_edgeness = length( vec2( dx, dy ) ) - 0.05 * depth / abs( dot( normalize( camera_backward ), normal ) );

#if MSAA

	float normal_edgeness = 0.0;

	for( int i = 0; i < u_Samples; i++ ) {
		ivec2 p = ivec2( gl_FragCoord.xy );

		vec3 normal00 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, -1, -1 ), i ).rg );
		vec3 normal10 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 0, -1 ), i ).rg );
		vec3 normal20 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 1, -1 ), i ).rg );

		vec3 normal01 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, -1, 0 ), i ).rg );
		vec3 normal21 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 1, 0 ), i ).rg );

		vec3 normal02 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, -1, 1 ), i ).rg );
		vec3 normal12 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 0, 1 ), i ).rg );
		vec3 normal22 = DecompressNormal( texelFetch( u_NormalTexture, ClampPixelOffset( p, 1, 1 ), i ).rg );

		float edgeness = Scharr( normal00, normal10, normal20, normal01, normal21, normal02, normal12, normal22 );
		normal_edgeness += Threshold( edgeness, 0.2 );
	}

	normal_edgeness /= u_Samples;

#else

	vec3 normal00 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, -pixel_size.y ) ).rg );
	vec3 normal10 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( 0.0, -pixel_size.y ) ).rg );
	vec3 normal20 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( pixel_size.x, -pixel_size.y ) ).rg );

	vec3 normal01 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, 0.0 ) ).rg );
	vec3 normal21 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( pixel_size.x, 0.0 ) ).rg );

	vec3 normal02 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, pixel_size.y ) ).rg );
	vec3 normal12 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( 0.0, pixel_size.y ) ).rg );
	vec3 normal22 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( pixel_size.x, pixel_size.y ) ).rg );

	float normal_edgeness = Scharr( normal00, normal10, normal20, normal01, normal21, normal02, normal12, normal22 );
	normal_edgeness = Threshold( normal_edgeness, 0.2 );

#endif

	f_Albedo = LinearTosRGB( max( depth_edgeness, normal_edgeness ) );
}

#endif
