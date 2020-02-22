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
	vec2 p = gl_FragCoord.xy;
	vec2 mid = u_ViewportSize * 0.5f;

	float radial_frac = distance( p, mid ) / min( mid.x, mid.y );
	float cross_dist = min( distance( floor( p.x ), floor( mid.x ) ), distance( floor( p.y ), floor( mid.y ) ) );

	if( radial_frac < 0.03 ) {
		if( floor( p.x ) == floor( mid.x ) || floor( p.y ) == floor( mid.y ) ) {
			f_Albedo = vec4( 1.0, 0.0, 0.0, 1.0 );
		}
	}
	else {

		if( cross_dist <= 1.0 ) {
			f_Albedo = vec4( 0.0, 0.0, 0.0, 1.0 );
		}
		else {
			float frac = Unlerp( 0.7, radial_frac, 0.8 );
			f_Albedo = LinearTosRGB( mix( vec4( 0.0, 0.0, 0.0, 0.0 ), vec4( 0.0, 0.0, 0.0, 1.0 ), frac ) );
		}
	}
}

#endif
