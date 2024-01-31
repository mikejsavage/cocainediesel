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
	AffineTransform u_M = instances[ gl_InstanceID ];
#endif

	vec4 Position = a_Position;

#if SKINNED
	Skin( Position );
#endif

	gl_Position = u_P * AffineToMat4( u_V ) * AffineToMat4( u_M ) * Position;
}

#else

void main() { }

#endif
