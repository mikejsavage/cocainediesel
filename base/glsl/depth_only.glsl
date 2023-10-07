#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

#if INSTANCED
layout( std430 ) readonly buffer b_Instances {
	AffineTransform instances[];
};
#endif

#ifdef MULTIDRAW
struct Instance {
	AffineTransform denormalize;
	AffineTransform transform;
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};
#endif

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;
layout( location = VertexAttribute_Normal ) in vec3 a_Normal;

void main() {
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;

#if INSTANCED
	mat4 u_M = AffineToMat4( instances[ gl_InstanceID ] );
#endif

#if MULTIDRAW
	mat4 u_M = AffineToMat4( instances[ gl_DrawID ].transform );
	Position = AffineToMat4( instances[ gl_DrawID ].denormalize ) * Position;
#endif

#if SKINNED
	Skin( Position, Normal );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

void main() { }

#endif
