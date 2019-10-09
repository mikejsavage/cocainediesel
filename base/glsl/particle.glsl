#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;
qf_varying vec4 v_Color;

#if VERTEX_SHADER

in vec3 a_Position;
in vec4 a_Color;
in vec2 a_TexCoord;

void main() {
	v_Color = sRGBToLinear( a_Color );
	v_TexCoord = a_TexCoord;
	gl_Position = u_P * u_V * vec4( a_Position, 1.0 );
}

#else

uniform sampler2D u_BaseTexture;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	f_Albedo = LinearTosRGB( qf_texture( u_BaseTexture, v_TexCoord ) * v_Color );
}

#endif
