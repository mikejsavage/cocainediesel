#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;
qf_varying vec4 v_Color;

#if VERTEX_SHADER

in vec2 a_Position;
in vec2 a_TexCoord;

in vec3 a_ParticlePosition;
in float a_ParticleScale;
in vec4 a_ParticleColor;

void main() {
	v_Color = sRGBToLinear( a_ParticleColor );
	v_TexCoord = a_TexCoord;

	vec3 Position = vec3( a_Position, 0.0 ) * a_ParticleScale + a_ParticlePosition;

	gl_Position = u_P * u_V * vec4( Position, 1.0 );
}

#else

uniform sampler2D u_BaseTexture;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	f_Albedo = LinearTosRGB( qf_texture( u_BaseTexture, v_TexCoord ) * v_Color );
}

#endif
