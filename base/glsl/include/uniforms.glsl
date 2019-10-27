layout( std140 ) uniform u_View {
	mat4 u_V;
	mat4 u_InverseV;
	mat4 u_P;
	mat4 u_InverseP;
	vec3 u_CameraPos;
	vec2 u_ViewportSize;
	float u_NearClip;
	int u_Samples;
};

layout( std140 ) uniform u_Model {
	mat4 u_M;
};

layout( std140 ) uniform u_Material {
	vec4 u_MaterialColor;
	vec3 u_TextureMatrix[ 2 ];
	vec2 u_TextureSize;
	float u_AlphaCutoff;
};

uniform vec2 u_BlendMix;
