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

void main() {
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;

#if SKINNED
	Skin( Position, Normal );
#endif

	Position += vec4( Normal * u_OutlineHeight, 0.0 );
	Position = u_M * Position;
	v_Position = Position;
	gl_Position = u_P * u_V * Position;

	v_Color = sRGBToLinear( u_OutlineColor );
}

#else

out vec4 f_Albedo;

void main() {
	vec4 color = v_Color;
	color.rgb = VoidFog( color.rgb, v_Position.z );
	color.a = VoidFogAlpha( color.a, v_Position.z );
	f_Albedo = LinearTosRGB( color );
}

#endif
