#include "../../../source/client/renderer/shader_shared.h"

// mat4x3 is what we actually want but even in std430 that gets aligned like
// vec4[4] so we can't use it. do this funny looking swizzle instead, which you
// can derive by checking renderdoc's ssbo viewer
struct AffineTransform {
	mat3x4 m;
};

layout( std140, set = DescriptorSet_RenderPass ) uniform u_View {
	AffineTransform u_V;
	AffineTransform u_InverseV;
	mat4 u_P;
	mat4 u_InverseP;
	vec3 u_CameraPos;
	vec2 u_ViewportSize;
	float u_NearClip;
	int u_Samples;
	vec3 u_LightDir;
};

#ifndef INSTANCED
layout( std140, set = DescriptorSet_DrawCall ) uniform u_Model {
	AffineTransform u_M;
};
#endif

layout( std140, set = DescriptorSet_Material ) uniform u_MaterialStatic {
	float u_Specular;
	float u_Shininess;
	float u_LodBias;
};

#ifndef INSTANCED
layout( std140, set = DescriptorSet_DrawCall ) uniform u_MaterialDynamic {
	vec4 u_MaterialColor;
};
#endif

layout( std140, set = DescriptorSet_RenderPass ) uniform u_ShadowMaps {
	int u_ShadowCascades;
	AffineTransform u_ShadowMatrix;

	float u_CascadePlaneA;
	float u_CascadePlaneB;
	float u_CascadePlaneC;
	float u_CascadePlaneD;

	vec3 u_CascadeOffsetA;
	vec3 u_CascadeOffsetB;
	vec3 u_CascadeOffsetC;
	vec3 u_CascadeOffsetD;

	vec3 u_CascadeScaleA;
	vec3 u_CascadeScaleB;
	vec3 u_CascadeScaleC;
	vec3 u_CascadeScaleD;
};
