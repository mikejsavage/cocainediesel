#include <math.h>

#include "qcommon/base.h"
#include "cgame/cg_local.h"

struct Spray {
	Vec3 origin;
	Vec3 normal;
	float radius;
	float angle;
	StringHash material;
	s64 spawn_time;
};

constexpr static s64 SPRAY_DURATION = 60000;

static Spray sprays[ 1024 ];
static size_t sprays_head;
static size_t num_sprays;

void InitSprays() {
	sprays_head = 0;
	num_sprays = 0;
}

// must match the GLSL OrthonormalBasis
static void OrthonormalBasis( Vec3 v, Vec3 * tangent, Vec3 * bitangent ) {
	float s = copysignf( 1.0f, v.z );
	float a = -1.0f / ( s + v.z );
	float b = v.x * v.y * a;

	*tangent = Vec3( 1.0f + s * v.x * v.x * a, s * b, -s * v.x );
	*bitangent = Vec3( b, s + v.y * v.y * a, -v.y );
}

void AddSpray( Vec3 origin, Vec3 normal, Vec3 angles, StringHash material ) {
	Vec3 forward, up;
	AngleVectors( angles, &forward, NULL, &up );

	Spray spray;
	spray.origin = origin;
	spray.normal = normal;
	spray.material = material;
	spray.radius = random_uniform_float( &cls.rng, 32.0f, 48.0f );
	spray.spawn_time = cls.gametime;

	Vec3 left = Cross( normal, up );
	Vec3 decal_up = Normalize( Cross( left, normal ) );

	Vec3 tangent, bitangent;
	OrthonormalBasis( normal, &tangent, &bitangent );

	spray.angle = -atan2( Dot( decal_up, tangent ), Dot( decal_up, bitangent ) );
	spray.angle += random_float11( &cls.rng ) * Radians( 10.0f );

	sprays[ ( sprays_head + num_sprays ) % ARRAY_COUNT( sprays ) ] = spray;

	if( num_sprays == ARRAY_COUNT( sprays ) ) {
		sprays_head++;
	}
	else {
		num_sprays++;
	}

	{
		ParticleEmitter emitter = { };
		// TODO: we don't have player eye position so fake it
		emitter.position = origin - forward * 64.0f;

		emitter.use_cone_direction = true;
		emitter.direction_cone.normal = forward;
		emitter.direction_cone.theta = 20.0f;

		emitter.start_speed = 500.0f;
		emitter.end_speed = 250.0f;

		emitter.start_color = vec4_white;
		emitter.end_color = vec4_white.xyz();

		emitter.start_size = 4.0f;
		emitter.end_size = 4.0f;
		emitter.size_distribution.type = RandomDistributionType_Uniform;
		emitter.size_distribution.uniform = 4.0f;

		emitter.lifetime = 0.10f;
		emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
		emitter.lifetime_distribution.uniform = 0.05f;

		emitter.n = 64;

		EmitParticles( &cgs.sparks, emitter );
	}

}

void DrawSprays() {
	while( num_sprays > 0 ) {
		if( sprays[ sprays_head % ARRAY_COUNT( sprays ) ].spawn_time + SPRAY_DURATION >= cls.gametime )
			break;
		sprays_head++;
		num_sprays--;
	}

	for( size_t i = 0; i < num_sprays; i++ ) {
		const Spray * spray = &sprays[ ( sprays_head + i ) % ARRAY_COUNT( sprays ) ];
		DrawDecal( spray->origin, spray->normal, spray->radius, spray->angle, spray->material, vec4_white );
	}
}
