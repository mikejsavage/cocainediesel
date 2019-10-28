#include "include/uniforms.glsl"
#include "include/common.glsl"

uniform sampler2D u_BaseTexture;

qf_varying vec2 v_TexCoord;

#if VERTEX_SHADER

in vec4 a_Position;
in vec2 a_TexCoord;

void main() {
	gl_Position = a_Position;
	v_TexCoord = a_TexCoord;
}

#else

#include "include/kawase.glsl"

out vec4 f_Albedo;

void main() {
	float alpha = qf_texture( u_BaseTexture, v_TexCoord ).a;
	// TODO: chec last param
	f_Albedo = vec4( KawaseBlurFilter( u_BaseTexture, v_TexCoord, u_TextureSize, 1.0 / u_TextureSize.x ), alpha );
}

#endif
