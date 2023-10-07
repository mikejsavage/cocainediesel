#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

layout( std140 ) uniform u_Silhouette {
	vec4 u_SilhouetteColor;
};

#if INSTANCED
struct Instance {
	AffineTransform transform;
	vec4 color;
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};

v2f flat int v_Instance;
#endif

#ifdef MULTIDRAW
struct Instance {
	AffineTransform denormalize;
	AffineTransform transform;
	vec4 color;
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};

v2f flat int v_DrawID;
#endif

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;

void main() {
	vec4 Position = a_Position;
	vec3 NormalDontCare = vec3( 0 );

#if INSTANCED
	mat4 u_M = AffineToMat4( instances[ gl_InstanceID ].transform );
	v_Instance = gl_InstanceID;
#elif MULTIDRAW
	mat4 u_M = AffineToMat4( instances[ gl_DrawID ].transform );
	Position = AffineToMat4( instances[ gl_DrawID ].denormalize ) * Position;
	v_DrawID = gl_DrawID;
#endif

#if SKINNED
	Skin( Position, NormalDontCare );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

void main() {
#if INSTANCED
	f_Albedo = instances[ v_Instance ].color;
#elif MULTIDRAW
	f_Albedo = instances[ v_DrawID ].color;
#else
	f_Albedo = u_SilhouetteColor;
#endif
}

#endif
