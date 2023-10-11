#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

#if INSTANCED
layout( std430 ) readonly buffer b_Instances {
	AffineTransform instances[];
};
#endif

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;

void main() {
#if INSTANCED
	mat4 u_M = AffineToMat4( instances[ gl_InstanceID ] );
#endif

	vec4 Position = a_Position;

#if SKINNED
	Skin( Position );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

void main() { }

#endif
