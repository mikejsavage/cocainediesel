layout( std140 ) uniform u_View {
	mat4 u_V;
	mat4 u_P;
	vec3 u_CameraPos;
	float u_NearClip;
};

layout( std140 ) uniform u_Model {
	mat4 u_M;
	vec4 u_ModelColor;
};

layout( std140 ) uniform u_Material {
	vec4 u_TextureMatrix[ 2 ];
	vec2 u_TextureSize;
	float u_AlphaCutoff;
};

uniform vec2 u_BlendMix;

#define TextureMatrix2x3Mul(m2x3,tc) (vec2(dot((m2x3)[0].xy, (tc)), dot((m2x3)[0].zw, (tc))) + (m2x3)[1].xy)
