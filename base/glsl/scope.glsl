#include "include/uniforms.glsl"
#include "include/common.glsl"

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

out vec4 f_Albedo;

void main() {
	const vec4 crosshair_color = vec4( 1.0, 0.0, 0.0, 1.0 );
	const vec4 crosshair_lines_color = vec4( 0.0, 0.0, 0.0, 1.0 );
	const vec4 vignette_color = vec4( 0.0, 0.0, 0.0, 1.0 );

	vec2 p = gl_FragCoord.xy - u_ViewportSize * 0.5;

	float radial_frac = length( p ) * 2.0 / min( u_ViewportSize.x, u_ViewportSize.y );
	float cross_dist = min( abs( floor( p.x ) ), abs( floor( p.y ) ) );

	float crosshair_frac = step( radial_frac, 0.035 );
	vec4 crosshair = mix( crosshair_lines_color, crosshair_color, crosshair_frac );
	crosshair.a = step( cross_dist, 1.0 - crosshair_frac );

	float vignette_frac = smoothstep( 0.375, 0.95, radial_frac );
	vec4 color = mix( crosshair, vignette_color, vignette_frac );

	f_Albedo = LinearTosRGB( color );
}

#endif
