#include "include/uniforms.glsl"
#include "include/common.glsl"

v2f vec2 v_TexCoord;
v2f vec4 v_Color;

#if VERTEX_SHADER

#if MODEL
	in vec3 a_Position;
#else
	in vec2 a_Position;
#endif
in vec2 a_TexCoord;

in vec3 a_ParticlePosition;
in vec3 a_ParticleVelocity;
in float a_ParticleDVelocity;
in vec4 a_ParticleColor;
in vec4 a_ParticleDColor;
in float a_ParticleSize;
in float a_ParticleDSize;
in float a_ParticleAge;
in float a_ParticleLifetime;

uniform sampler2D u_GradientTexture;

layout( std140 ) uniform u_GradientMaterial {
	float u_GradientHalfPixel;
};

void main() {
	float fage = a_ParticleAge / a_ParticleLifetime;

	float uv = mix( u_GradientHalfPixel, fage, 1.0 - u_GradientHalfPixel );
	vec4 gradColor = texture( u_GradientTexture, vec2( uv, 0.5 ) );

	v_Color = sRGBToLinear( a_ParticleColor + a_ParticleDColor * a_ParticleAge ) * gradColor;
	// v_Color.a = 1-fage;
	v_TexCoord = a_TexCoord;
	float scale = a_ParticleSize + a_ParticleDSize * a_ParticleAge;

#if MODEL
	vec3 position = a_ParticlePosition + ( u_M * vec4( a_Position * scale, 1.0 ) ).xyz;

	gl_Position = u_P * u_V * vec4( position, 1.0 );
#else
	vec3 camera_right = vec3( u_V[ 0 ].x, u_V[ 1 ].x, u_V[ 2 ].x );
	vec3 camera_up = vec3( u_V[ 0 ].y, u_V[ 1 ].y, u_V[ 2 ].y );

	vec3 right = a_Position.x * scale * camera_right;
	vec3 up = a_Position.y * scale * camera_up;
	vec3 position = right + up + a_ParticlePosition;

	gl_Position = u_P * u_V * vec4( position, 1.0 );
#endif
}

#else

uniform sampler2D u_BaseTexture;

out vec4 f_Albedo;

void main() {
	// TODO: soft particles
	f_Albedo = LinearTosRGB( texture( u_BaseTexture, v_TexCoord ) * v_Color );
}

#endif
