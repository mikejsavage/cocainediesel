#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/trace.glsl"

layout( std140 ) uniform u_ParticleUpdate {
	int u_Collision;
	float u_Radius;
	float u_dt;
};

// input vbo
in vec3 a_ParticlePosition;
in vec3 a_ParticleVelocity;
in vec3 a_ParticleAccelDragRest;
in vec4 a_ParticleUVWH;
in vec4 a_ParticleStartColor;
in vec4 a_ParticleEndColor;
in vec2 a_ParticleSize;
in vec2 a_ParticleAgeLifetime;
in uint a_ParticleFlags;

// output vbo
out vec3 v_ParticlePosition;
out vec3 v_ParticleVelocity;
out vec3 v_ParticleAccelDragRest;
out vec4 v_ParticleUVWH;
out uint v_ParticleStartColor;
out uint v_ParticleEndColor;
out vec2 v_ParticleSize;
out vec2 v_ParticleAgeLifetime;
out uint v_ParticleFlags;

#if FEEDBACK
	out uint v_Feedback;
	out vec3 v_FeedbackPosition;
	out vec3 v_FeedbackNormal;
#endif

// must match source
#define PARTICLE_COLLISION_POINT 1u
#define PARTICLE_COLLISION_SPHERE 2u
#define PARTICLE_ROTATE 4u
#define PARTICLE_STRETCH 8u

#define FEEDBACK_NONE 0u
#define FEEDBACK_AGE 1u
#define FEEDBACK_COLLISION 2u

uint colorPack( vec4 color ) {
	uvec4 bytes = uvec4( color * 255.0 );
	return (bytes.a << 24) | (bytes.b << 16) | (bytes.g << 8) | (bytes.r);
}

bool collide() {
	if ( u_Collision == 0 || ( a_ParticleFlags & ( PARTICLE_COLLISION_POINT | PARTICLE_COLLISION_SPHERE ) ) == 0u ) {
		return false;
	}

	float radius = 0.0;
	if( ( a_ParticleFlags & PARTICLE_COLLISION_SPHERE ) != 0u ) {
		float fage = a_ParticleAgeLifetime.x / a_ParticleAgeLifetime.y;
		radius = mix( a_ParticleSize.x, a_ParticleSize.y, fage );
		radius *= u_Radius;
	}
	float restitution = a_ParticleAccelDragRest.z;
	float asdf = 8.0;
	
	vec4 frac = Trace( a_ParticlePosition - a_ParticleVelocity * u_dt * 0.1, a_ParticleVelocity * u_dt * asdf, radius );
	if ( frac.w < 1.0 ) {
		v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * frac.w * u_dt / asdf;

		vec3 impulse = ( 1.0 + restitution ) * dot( a_ParticleVelocity, frac.xyz ) * frac.xyz;

		v_ParticleVelocity = a_ParticleVelocity - impulse;

#if FEEDBACK
		v_Feedback |= FEEDBACK_COLLISION;
		v_FeedbackPosition = v_ParticlePosition;
		v_FeedbackNormal = normalize( frac.xyz );
#endif

		v_ParticlePosition += v_ParticleVelocity * ( 1.0 - frac.w / asdf ) * u_dt;

		return true;
	}
	return false;
}

void main() {
	v_ParticleAgeLifetime.x = a_ParticleAgeLifetime.x + u_dt;

#if FEEDBACK
	v_Feedback = FEEDBACK_NONE;
	v_FeedbackPosition = vec3( 0.0 );
	v_FeedbackNormal = vec3( 0.0 );
	if ( v_ParticleAgeLifetime.x >= a_ParticleAgeLifetime.y ) {
		v_Feedback |= FEEDBACK_AGE;
		v_FeedbackPosition = a_ParticlePosition;
		v_FeedbackNormal = normalize( a_ParticleVelocity );
	}
#endif

	if ( !collide() ) {
		v_ParticlePosition = a_ParticlePosition + a_ParticleVelocity * u_dt;

		v_ParticleVelocity = a_ParticleVelocity;
	}

	v_ParticleVelocity.z += a_ParticleAccelDragRest.x * u_dt;
	v_ParticleVelocity -= v_ParticleVelocity * a_ParticleAccelDragRest.y * u_dt;
	v_ParticleAccelDragRest = a_ParticleAccelDragRest;
	v_ParticleUVWH = a_ParticleUVWH;
	v_ParticleStartColor = colorPack( a_ParticleStartColor );
	v_ParticleEndColor = colorPack( a_ParticleEndColor );
	v_ParticleSize = a_ParticleSize;
	v_ParticleAgeLifetime.y = a_ParticleAgeLifetime.y;
	v_ParticleFlags = a_ParticleFlags;
}
