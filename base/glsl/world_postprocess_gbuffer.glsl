#include "include/common.glsl"
#include "include/uniforms.glsl"

uniform sampler2D u_DepthTexture;
uniform sampler2D u_NormalTexture;

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

void main() {
	vec2 pixel_size = 1.0 / u_TextureSize;
	vec2 uv = gl_FragCoord.xy / u_TextureSize;

#if 0
	float depth_left = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( -pixel_size.x, 0.0 ) ).r, u_NearClip );
	float depth_right = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( +pixel_size.x, 0.0 ) ).r, u_NearClip );
	float depth_up = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( 0.0, -pixel_size.y ) ).r, u_NearClip );
	float depth_down = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( 0.0, pixel_size.y ) ).r, u_NearClip );

	vec3 normal = qf_texture( u_NormalTexture, uv ).rgb;

	vec3 camera_forward = vec3( -u_V[ 0 ].z, -u_V[ 1 ].z, -u_V[ 2 ].z );
	float asdf = abs( dot( normal, camera_forward ) );
	float depth_edgeness = 0.0;
	depth_edgeness += max( 0.0, abs( depth_right - depth_left ) - mix( 15.0, 5.0, asdf ) );
	depth_edgeness += max( 0.0, abs( depth_up - depth_down ) - mix( 15.0, 5.0, asdf ) );
	f_Albedo = LinearTosRGB( depth_edgeness );
#endif

	vec3 normal00 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, -pixel_size.y ) ).ra );
	vec3 normal10 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( 0.0, -pixel_size.y ) ).ra );
	vec3 normal20 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( pixel_size.x, -pixel_size.y ) ).ra );

	vec3 normal01 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, 0.0 ) ).ra );
	vec3 normal21 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( pixel_size.x, 0.0 ) ).ra );

	vec3 normal02 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, pixel_size.y ) ).ra );
	vec3 normal12 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( 0.0, pixel_size.y ) ).ra );
	vec3 normal22 = DecompressNormal( qf_texture( u_NormalTexture, uv + vec2( pixel_size.x, pixel_size.y ) ).ra );

	float normal_edgeness_x = length( 3.0 * normal00 + 10.0 * normal01 + 3.0 * normal02 - ( 3.0 * normal20 + 10.0 * normal21 + 3.0 * normal22 ) ) * ( 1.0 / 32.0 );
	float normal_edgeness_y = length( 3.0 * normal00 + 10.0 * normal10 + 3.0 * normal20 - ( 3.0 * normal02 + 10.0 * normal12 + 3.0 * normal22 ) ) * ( 1.0 / 32.0 );
	normal_edgeness_x = Threshold( normal_edgeness_x, 0.2 );
	normal_edgeness_y = Threshold( normal_edgeness_y, 0.2 );
	float normal_edgeness = length( vec2( normal_edgeness_x, normal_edgeness_y ) );

	/* f_Albedo = max( depth_edgeness, normal_edgeness ); */
	f_Albedo = LinearTosRGB( normal_edgeness );
}

#endif
