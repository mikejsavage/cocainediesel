#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

#if INSTANCED
struct Instance {
	mat4 transform;
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};
#endif

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;

void main() {
#if INSTANCED
	mat4 u_M = instances[ gl_InstanceID ].transform;
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
