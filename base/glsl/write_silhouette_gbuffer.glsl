#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	vec4 Position = a_Position;
	vec3 NormalDontCare = vec3( 0 );

#if SKINNED
	Skin( Position, NormalDontCare );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

out vec4 f_Albedo;

void main() {
	f_Albedo = u_MaterialColor;
}

#endif
