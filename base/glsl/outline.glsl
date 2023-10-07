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

layout( location = VertexAttribute_Position ) in vec4 a_Position;
layout( location = VertexAttribute_Normal ) in vec3 a_Normal;

#if INSTANCED
struct Instance {
	AffineTransform transform;
	vec4 outline_color;
	float outline_height;
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};
#endif

#ifdef MULTIDRAW
struct Instance {
	AffineTransform denormalize;
	AffineTransform transform;
	vec4 outline_color;
	float outline_height;
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};
#endif

void main() {
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;

#if INSTANCED
	mat4 u_M = AffineToMat4( instances[ gl_InstanceID ].transform );
#elif MULTIDRAW
	mat4 u_M = AffineToMat4( instances[ gl_DrawID ].transform );
	Position = AffineToMat4( instances[ gl_DrawID ].denormalize ) * Position;
#endif

#if SKINNED
	Skin( Position, Normal );
#endif

#if INSTANCED
	Position += vec4( Normal * instances[ gl_InstanceID ].outline_height, 0.0 );
#elif MULTIDRAW
	Position += vec4( Normal * instances[ gl_DrawID ].outline_height, 0.0 );
#else
	Position += vec4( Normal * u_OutlineHeight, 0.0 );
#endif
	Position = u_M * Position;
	v_Position = Position;
	gl_Position = u_P * u_V * Position;

#if INSTANCED
	v_Color = sRGBToLinear( instances[ gl_InstanceID ].outline_color );
#elif MULTIDRAW
	v_Color = sRGBToLinear( instances[ gl_DrawID ].outline_color );
#else
	v_Color = sRGBToLinear( u_OutlineColor );
#endif
}

#else

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

void main() {
	vec4 color = v_Color;
	color.rgb = VoidFog( color.rgb, v_Position.z );
	color.a = VoidFogAlpha( color.a, v_Position.z );
	f_Albedo = color;
}

#endif
