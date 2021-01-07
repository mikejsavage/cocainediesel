#include "include/uniforms.glsl"
#include "include/common.glsl"

uniform sampler2D u_SilhouetteTexture;

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

	vec4 colour_up = texture( u_SilhouetteTexture, uv + vec2( 0.0, pixel_size.y ) );
	vec4 colour_downleft = texture( u_SilhouetteTexture, uv + vec2( -pixel_size.x, -pixel_size.y ) );
	vec4 colour_downright = texture( u_SilhouetteTexture, uv + vec2( pixel_size.x, -pixel_size.y ) );

	float edgeness_x = length( colour_downright - colour_downleft );
	float edgeness_y = length( mix( colour_downleft, colour_downright, 0.5 ) - colour_up );
	float edgeness = length( vec2( edgeness_x, edgeness_y ) );

	vec4 colour = max( max( colour_downleft, colour_downright ), colour_up );

	f_Albedo = edgeness * colour;
}

#endif
