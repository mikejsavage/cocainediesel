#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

layout( std140 ) uniform u_Silhouette {
	vec4 u_SilhouetteColor;
};

#if INSTANCED
flat v2f vec4 v_SilhouetteColor;
#endif

#if VERTEX_SHADER

in vec4 a_Position;

#if INSTANCED
in vec4 a_MaterialColor;

in vec4 a_ModelTransformRow0;
in vec4 a_ModelTransformRow1;
in vec4 a_ModelTransformRow2;
#endif

void main() {
#if INSTANCED
	mat4 u_M = transpose( mat4( a_ModelTransformRow0, a_ModelTransformRow1, a_ModelTransformRow2, vec4( 0.0, 0.0, 0.0, 1.0 ) ) );
	v_SilhouetteColor = a_MaterialColor;
#endif
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
#if INSTANCED
	f_Albedo = v_SilhouetteColor;
#else
	f_Albedo = u_SilhouetteColor;
#endif
}

#endif
