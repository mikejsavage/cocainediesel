layout( std140 ) uniform u_View {
	mat4 u_V;
	mat4 u_InverseV;
	mat4 u_P;
	mat4 u_InverseP;
	vec3 u_CameraPos;
	vec2 u_ViewportSize;
	float u_NearClip;
	int u_Samples;
	vec3 u_LightDir;
};

#ifndef INSTANCED
layout( std140 ) uniform u_Model {
	mat4 u_M;
};
#endif

layout( std140 ) uniform u_MaterialStatic {
	float u_Specular;
	float u_Shininess;
	float u_LodBias;
};

#ifndef INSTANCED
layout( std140 ) uniform u_MaterialDynamic {
	vec4 u_MaterialColor;
	vec3 u_TextureMatrix[ 2 ];
};
#endif

layout( std140 ) uniform u_ShadowMaps {
	int u_ShadowCascades;
	mat4 u_ShadowMatrix;

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
