#include "include/trace.glsl"
#include "include/particles.glsl"

layout( local_size_x = 64 ) in;

layout( std140 ) uniform u_ParticleUpdate {
	int u_Collision;
	float u_Radius;
	float u_dt;
	uint new_particles;
};

layout( std430 ) readonly buffer b_ParticlesIn {
	Particle in_particles[];
};

layout( std430 ) writeonly buffer b_ParticlesOut {
	Particle out_particles[];
};

layout( std430 ) readonly buffer b_ComputeCountIn {
	uint in_num_particles;
};

layout( std430 ) coherent buffer b_ComputeCountOut {
	uint out_num_particles;
};

bool collide( inout Particle particle, float dt ) {
	if( u_Collision == 0 || ( particle.flags & ( PARTICLE_COLLISION_POINT | PARTICLE_COLLISION_SPHERE ) ) == 0 ) {
		return false;
	}

	float radius = 0.0;
	if( ( particle.flags & PARTICLE_COLLISION_SPHERE ) != 0 ) {
		float fage = particle.age / particle.lifetime;
		radius = mix( particle.start_size, particle.end_size, fage );
		radius *= u_Radius;
	}
	float asdf = 8.0;
	float prestep = min( 0.1, particle.age );

	vec4 frac = Trace( particle.position - particle.velocity * dt * prestep, particle.velocity * dt * asdf, radius );
	if( frac.w < 1.0 ) {
		particle.position += particle.velocity * frac.w * dt / asdf;
		vec3 impulse = ( 1.0 + particle.restitution ) * dot( particle.velocity, frac.xyz ) * frac.xyz;
		particle.velocity -= impulse;
		particle.position += particle.velocity * ( 1.0 - frac.w / asdf ) * dt;
		particle.angle += particle.angular_velocity * dt;

		return true;
	}
	return false;
}

Particle updateParticle( Particle particle, float dt ) {
	particle.age += dt;

	if( !collide( particle, dt ) ) {
		particle.position += particle.velocity * dt;
		particle.angle += particle.angular_velocity * dt;
	}

	particle.velocity.z += particle.acceleration * dt;
	particle.velocity -= particle.velocity * particle.drag * dt;

	return particle;
}

void main() {
	uint in_index = gl_GlobalInvocationID.x;
	if( in_index >= in_num_particles )
		return;

	Particle particle = in_particles[ in_index ];
	if( particle.age >= particle.lifetime )
		return;

	uint index = new_particles + atomicAdd( out_num_particles, 1 );
	out_particles[ index ] = updateParticle( particle, u_dt );
}
