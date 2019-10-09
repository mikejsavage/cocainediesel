#include "include/common.glsl"
#include "include/uniforms.glsl"

uniform sampler2D u_TeammateTexture;

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

out vec4 f_Albedo;

void main() {
	vec2 pixel_size = 1.0 / u_ViewportSize;
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;

	vec4 colour_up = qf_texture( u_TeammateTexture, uv + vec2( 0.0, pixel_size.y ) );
	vec4 colour_down = qf_texture( u_TeammateTexture, uv + vec2( 0.0, -pixel_size.y ) );
	vec4 colour_left = qf_texture( u_TeammateTexture, uv + vec2( -pixel_size.x, 0.0 ) );
	vec4 colour_right = qf_texture( u_TeammateTexture, uv + vec2( pixel_size.x, 0.0 ) );

	float edgeness_x = length( colour_right - colour_left );
	float edgeness_y = length( colour_down - colour_up );
	float edgeness = length( vec2( edgeness_x, edgeness_y ) );

	vec4 colour = max( max( colour_left, colour_right ), max( colour_up, colour_down ) );

	f_Albedo = edgeness * colour;
}

#endif
