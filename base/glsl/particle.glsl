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
in vec3 a_ParticleOrientation;
in vec3 a_ParticleAVelocity;
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

vec3 rotate_euler( vec3 euler, vec3 v ) {
  float sp = sin( euler.x );
  float cp = cos( euler.x );
  float sy = sin( euler.y );
  float cy = cos( euler.y );
  float sr = sin( euler.z );
  float cr = cos( euler.z );

  float t1 = -sr * sp;
  float t2 = cr * sp;
  return mat3(
    cp * cy, cp * sy, -sp,
    t1 * cy + cr * sy, t1 * sy - cr * cy, -sr * cp,
    t2 * cy + sr * sy, t2 * sy - sr * cy, cr * cp
  ) * v;
}

void main() {
	float fage = a_ParticleAge / a_ParticleLifetime;

	float uv = mix( u_GradientHalfPixel, fage, 1.0 - u_GradientHalfPixel );
	vec4 gradColor = texture( u_GradientTexture, vec2( uv, 0.5 ) );

	v_Color = sRGBToLinear( a_ParticleColor + a_ParticleDColor * a_ParticleAge ) * gradColor;
	// v_Color.a = 1-fage;
	v_TexCoord = a_TexCoord;
	float scale = a_ParticleSize + a_ParticleDSize * a_ParticleAge;

#if MODEL
	vec3 position = a_ParticlePosition + rotate_euler( a_ParticleOrientation, ( u_M * vec4( a_Position * scale, 1.0 ) ).xyz );

	gl_Position = u_P * u_V * vec4( position, 1.0 );
#else
	// stretched billboards based on
	// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/emittedparticleVS.hlsl
	vec3 view_velocity = ( u_V * vec4( a_ParticleVelocity * 0.01, 0.0 ) ).xyz;
	float angle = atan( view_velocity.x, -view_velocity.y );
	float ca = cos( angle );
	float sa = sin( angle );
	mat2 rot = mat2( ca, sa, -sa, ca );
	vec3 quadPos = vec3( scale * rot * a_Position, 0.0 );
	vec3 stretch = dot( quadPos, view_velocity ) * view_velocity;
	quadPos += normalize( stretch ) * clamp( length( stretch ), 1.0, scale );
	gl_Position = u_P * ( u_V * vec4( a_ParticlePosition, 1.0 ) + vec4( quadPos, 0.0 ) );
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
