#include "include/uniforms.glsl"
#include "include/common.glsl"

qf_varying vec2 v_TexCoord;
qf_varying vec4 v_Color;

#if VERTEX_SHADER

in vec2 a_Position;
in vec2 a_TexCoord;

in vec3 a_ParticlePosition;
in float a_ParticleScale;
in float a_ParticleT;
in vec4 a_ParticleColor;

uniform sampler2D u_GradientTexture;

layout( std140 ) uniform u_GradientMaterial {
	float u_GradientHalfPixel;
};

void main() {
	float uv = mix( u_GradientHalfPixel, a_ParticleT, 1.0 - u_GradientHalfPixel );
	v_Color = sRGBToLinear( a_ParticleColor ) * qf_texture( u_GradientTexture, vec2( uv, 0.5 ) );
	v_TexCoord = a_TexCoord;

	vec3 camera_right = vec3( u_V[ 0 ].x, u_V[ 1 ].x, u_V[ 2 ].x );
	vec3 camera_up = vec3( u_V[ 0 ].y, u_V[ 1 ].y, u_V[ 2 ].y );

	vec3 right = a_Position.x * a_ParticleScale * camera_right;
	vec3 up = a_Position.y * a_ParticleScale * camera_up;
	vec3 position = right + up + a_ParticlePosition;

	gl_Position = u_P * u_V * vec4( position, 1.0 );
}

#else

uniform sampler2D u_BaseTexture;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	f_Albedo = LinearTosRGB( qf_texture( u_BaseTexture, v_TexCoord ) * v_Color );
}

#endif
