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
in vec3 a_ParticleOrientation;
in vec3 a_ParticleAVelocity;
in vec4 a_ParticleStartColor;
in vec4 a_ParticleEndColor;
in float a_ParticleStartSize;
in float a_ParticleEndSize;
in float a_ParticleAge;
in float a_ParticleLifetime;

// output vbo
out vec3 v_ParticlePosition;
out vec3 v_ParticleVelocity;
out vec3 v_ParticleOrientation;
out vec3 v_ParticleAVelocity;
out uint v_ParticleStartColor;
out uint v_ParticleEndColor;
out float v_ParticleStartSize;
out float v_ParticleEndSize;
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

bool collide() {
	if ( u_Collision == 0 ) {
		return false;
	}

	float radius = 0.0;
	if( u_Collision == 2 ) {
		float fage = a_ParticleAge / a_ParticleLifetime;
		radius = mix( a_ParticleStartSize, a_ParticleEndSize, fage );
		radius *= u_Radius;
	}
	float restitution = 0.8;
	float asdf = 8.0;
	
	vec4 frac = Trace( a_ParticlePosition - a_ParticleVelocity * u_dt * 0.1, a_ParticleVelocity * u_dt * asdf, radius );
	if ( frac.w < 1.0 ) {
		v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * frac.w * u_dt / asdf;
		v_ParticleOrientation = a_ParticleOrientation + a_ParticleAVelocity * frac.w * u_dt / asdf;

		vec3 impulse = ( 1.0 + restitution ) * dot( a_ParticleVelocity, frac.xyz ) * frac.xyz;

		v_ParticleVelocity = a_ParticleVelocity - impulse;
		v_ParticleAVelocity = a_ParticleAVelocity;

#if FEEDBACK
		v_Feedback |= FEEDBACK_COLLISION;
		v_FeedbackPosition = v_ParticlePosition;
		v_FeedbackNormal = normalize( frac.xyz );
#endif

		v_ParticlePosition += v_ParticleVelocity * ( 1.0 - frac.w / asdf ) * u_dt;
		v_ParticleOrientation += v_ParticleAVelocity * ( 1.0 - frac.w / asdf ) * u_dt;

		return true;
	}
	return false;
}

void main() {
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

	if ( !collide() ) {
		v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * u_dt;
		v_ParticleOrientation = a_ParticleOrientation + a_ParticleAVelocity * u_dt;

		v_ParticleVelocity = a_ParticleVelocity;
		v_ParticleAVelocity = a_ParticleAVelocity;
	}

	v_ParticleVelocity += u_Acceleration * u_dt;
	v_ParticleStartColor = colorPack( a_ParticleStartColor );
	v_ParticleEndColor = colorPack( a_ParticleEndColor );
	v_ParticleStartSize = a_ParticleStartSize;
	v_ParticleEndSize = a_ParticleEndSize;
	v_ParticleLifetime = a_ParticleLifetime;
}
