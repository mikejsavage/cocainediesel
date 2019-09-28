#include "include/common.glsl"
#include "include/uniforms.glsl"

uniform sampler2D u_DepthTexture;
uniform sampler2D u_NormalTexture;

#ifdef VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

out float f_Albedo;

void main() {
	vec2 pixel_size = 1.0 / u_TextureSize;
	vec2 uv = gl_FragCoord.xy / u_TextureSize;

	float depth_left = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( -pixel_size.x, 0.0 ) ).r, u_NearClip );
	float depth_right = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( +pixel_size.x, 0.0 ) ).r, u_NearClip );
	float depth_up = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( 0.0, -pixel_size.y ) ).r, u_NearClip );
	float depth_down = LinearizeDepth( qf_texture( u_DepthTexture, uv + vec2( 0.0, pixel_size.y ) ).r, u_NearClip );

	float depth_edgeness = 0.0;
	depth_edgeness += max( 0.0, abs( depth_right - depth_left ) - 0.05 );
	depth_edgeness += max( 0.0, abs( depth_up - depth_down ) - 0.05 );

	vec3 normal_left = qf_texture( u_NormalTexture, uv + vec2( -pixel_size.x, 0.0 ) ).rgb;
	vec3 normal_right = qf_texture( u_NormalTexture, uv + vec2( +pixel_size.x, 0.0 ) ).rgb;
	vec3 normal_down = qf_texture( u_NormalTexture, uv + vec2( 0.0, -pixel_size.y ) ).rgb;
	vec3 normal_up = qf_texture( u_NormalTexture, uv + vec2( 0.0, pixel_size.y ) ).rgb;

	float normal_edgeness = 0.0;
	normal_edgeness += max( 0.0, 1.0 - dot( normal_left, normal_right ) );
	normal_edgeness += max( 0.0, 1.0 - dot( normal_up, normal_down ) );
	normal_edgeness *= 0.5;

	/* f_Albedo = max( depth_edgeness, normal_edgeness ); */
	f_Albedo = LinearTosRGB( normal_edgeness );
}

#endif
