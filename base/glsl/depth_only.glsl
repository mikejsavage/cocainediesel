#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;

#if INSTANCED
in vec4 a_ModelTransformRow0;
in vec4 a_ModelTransformRow1;
in vec4 a_ModelTransformRow2;
#endif

void main() {
#if INSTANCED
	mat4 u_M = transpose( mat4( a_ModelTransformRow0, a_ModelTransformRow1, a_ModelTransformRow2, vec4( 0.0, 0.0, 0.0, 1.0 ) ) );
#endif

	vec4 Position = a_Position;
	vec3 Normal = a_Normal;

#if SKINNED
	Skin( Position, Normal );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

void main() { }

#endif
