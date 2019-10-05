#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

#if VERTEX_SHADER

in vec4 a_Position;
in vec2 a_TexCoord;

void main() {
	gl_Position = a_Position;
	v_TexCoord = a_TexCoord;
}

#else

uniform sampler2D u_BaseTexture;

const float W = 0.90; // the white point

#if APPLY_HDR

uniform float u_HDRGamma;
uniform float u_HDRExposure;

vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	vec3 s = ((x*(a*x+b))/(x*(c*x+d)+e));
	return s;
}

vec3 ToneMap(vec3 c) {
	return ACESFilm(c / 2.0) / ACESFilm(vec3(W) / 2.0);
}

#endif

out vec4 f_Albedo;

void main() {
	vec4 texel = qf_texture(u_BaseTexture, v_TexCoord);
	vec3 color = texel.rgb;

#if APPLY_HDR
	color = ToneMap(color * u_HDRExposure);
#endif

#if APPLY_SRGB2LINEAR

#if APPLY_HDR
	color = pow(color, vec3(1.0 / u_HDRGamma));
#else
	color = pow(color, vec3(1.0 / 2.2));
#endif

#endif

	f_Albedo = vec4(color, texel.a);
}

#endif
