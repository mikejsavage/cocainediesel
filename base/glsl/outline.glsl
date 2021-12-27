#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"
#include "include/fog.glsl"

layout( std140 ) uniform u_Outline {
	vec4 u_OutlineColor;
	float u_OutlineHeight;
};

v2f vec4 v_Position;
v2f vec4 v_Color;

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;

#if INSTANCED
in vec4 a_MaterialColor;
in float a_OutlineHeight;

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

#if INSTANCED
	Position += vec4( Normal * a_OutlineHeight, 0.0 );
#else
	Position += vec4( Normal * u_OutlineHeight, 0.0 );
#endif
	Position = u_M * Position;
	v_Position = Position;
	gl_Position = u_P * u_V * Position;

#if INSTANCED
	v_Color = sRGBToLinear( a_MaterialColor );
#else
	v_Color = sRGBToLinear( u_OutlineColor );
#endif
}

#else

out vec4 f_Albedo;

void main() {
	vec4 color = v_Color;
	color.rgb = VoidFog( color.rgb, v_Position.z );
	color.a = VoidFogAlpha( color.a, v_Position.z );
	f_Albedo = color;
}

#endif
