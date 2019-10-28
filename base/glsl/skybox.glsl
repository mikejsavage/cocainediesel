#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/dither.glsl"

qf_varying vec3 v_Position;

layout( std140 ) uniform u_Sky {
	vec3 u_EquatorColor;
	vec3 u_PoleColor;
};

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	v_Position = a_Position.xyz;
	gl_Position = u_P * u_V * vec4( a_Position );
}

#else

out vec3 f_Albedo;

void main() {
	float elevation = acos( normalize( v_Position ).z ) / ( M_PI * 0.5 );
	f_Albedo = LinearTosRGB( mix( u_EquatorColor, u_PoleColor, elevation ) + Dither() );
}

#endif
