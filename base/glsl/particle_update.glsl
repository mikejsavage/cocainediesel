#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/trace.glsl"

layout( std140 ) uniform u_ParticleUpdate {
	int u_Collision;
	float u_Radius;
	vec3 u_Acceleration;
	float u_dt;
};

// input vbo
in vec3 a_ParticlePosition;
in vec3 a_ParticleVelocity;
in vec4 a_ParticleColor;
in vec4 a_ParticleDColor;
in float a_ParticleSize;
in float a_ParticleDSize;
in float a_ParticleAge;
in float a_ParticleLifetime;

// output vbo
out vec3 v_ParticlePosition;
out vec3 v_ParticleVelocity;
out uint v_ParticleColor;
out uint v_ParticleDColor;
out float v_ParticleSize;
out float v_ParticleDSize;
out float v_ParticleAge;
out float v_ParticleLifetime;

#if FEEDBACK
	out uint v_Feedback;
	out vec3 v_FeedbackPosition;
	out vec3 v_FeedbackNormal;
#endif

#define FEEDBACK_NONE 0u
#define FEEDBACK_AGE 1u
#define FEEDBACK_COLLISION 2u

uint colorPack( vec4 color ) {
	uvec4 bytes = uvec4( color * 255.0 );
	return (bytes.a << 24) | (bytes.b << 16) | (bytes.g << 8) | (bytes.r);
}

void main() {
	vec4 frac = vec4( 1.0 );
	float radius = 0.0;
	if( u_Collision == 2 ) {
		radius = a_ParticleSize + a_ParticleAge * a_ParticleDSize;
		radius *= u_Radius;
	}
	v_ParticleAge = a_ParticleAge + u_dt;

#if FEEDBACK
	v_Feedback = FEEDBACK_NONE;
	v_FeedbackPosition = vec3( 0.0 );
	v_FeedbackNormal = vec3( 0.0 );
	if ( v_ParticleAge >= a_ParticleLifetime ) {
		v_Feedback |= FEEDBACK_AGE;
		v_FeedbackPosition = a_ParticlePosition;
		v_FeedbackNormal = normalize( a_ParticleVelocity );
	}
#endif

	if ( u_Collision != 0 ) {
		frac = Trace( a_ParticlePosition - a_ParticleVelocity * u_dt * 0.1, a_ParticleVelocity * u_dt * 8.0, radius );
		if ( frac.w < 1.0 ) {
			v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * frac.w * u_dt / 8.0;
			v_ParticleVelocity = a_ParticleVelocity - 2.0 * dot( frac.xyz, a_ParticleVelocity ) * frac.xyz * 0.8;

#if FEEDBACK
			v_Feedback |= FEEDBACK_COLLISION;
			v_FeedbackPosition = v_ParticlePosition;
			v_FeedbackNormal = normalize( frac.xyz );
#endif

			v_ParticlePosition += v_ParticleVelocity * ( 1.0 - frac.w ) * u_dt;
		} else {
			v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * u_dt;
			v_ParticleVelocity = a_ParticleVelocity;
		}
	} else {
		v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * u_dt;
		v_ParticleVelocity = a_ParticleVelocity;
	}
	v_ParticleVelocity += u_Acceleration * u_dt;

	v_ParticleColor = colorPack( a_ParticleColor );
	v_ParticleDColor = colorPack( a_ParticleDColor );
	v_ParticleSize = a_ParticleSize;
	v_ParticleDSize = a_ParticleDSize;
	v_ParticleLifetime = a_ParticleLifetime;
}