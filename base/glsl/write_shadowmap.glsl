#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;

void main() {
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;

#if SKINNED
	Skin( Position, Normal );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

out vec4 f_Albedo;

void main() {
	f_Albedo = vec4( 1.0 );
}

#endif
