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
	ivec2 p = ivec2( gl_FragCoord.xy );
	ivec3 pixel = ivec3( 0, 1, -1 );

	vec4 colour_up =        sRGBToLinear( texelFetch( u_SilhouetteTexture, p + pixel.xz, 0 ) );
	vec4 colour_downleft =  sRGBToLinear( texelFetch( u_SilhouetteTexture, p + pixel.yy, 0 ) );
	vec4 colour_downright = sRGBToLinear( texelFetch( u_SilhouetteTexture, p + pixel.zy, 0 ) );

	float edgeness_x = length( colour_downright - colour_downleft );
	float edgeness_y = length( mix( colour_downleft, colour_downright, 0.5 ) - colour_up );
	float edgeness = length( vec2( edgeness_x, edgeness_y ) );

	vec4 colour = max( max( colour_downleft, colour_downright ), colour_up );

	f_Albedo = edgeness * colour;
}

#endif
