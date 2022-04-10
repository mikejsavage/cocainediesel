#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static float RandomRadians() {
	return RandomUniformFloat( &cls.rng, 0.0f, Radians( 360.0f ) );
}

void CG_GenericExplosion( Vec3 pos, Vec3 dir, float radius ) {
	// TODO: radius is just ignored?
	DoVisualEffect( "vfx/explosion", pos, dir, 1.0f, vec4_white );
	S_StartFixedSound( "models/bomb/explode", pos, 1.0f, 1.0f );
}

struct Gib {
	Vec3 origin;
	Vec3 velocity;
	float scale;
	float lifetime;
	Vec4 color;
};

static Gib gibs[ 512 ];
static u32 num_gibs;

void InitGibs() {
	num_gibs = 0;
}

void SpawnGibs( Vec3 origin, Vec3 velocity, int damage, Vec4 color ) {
	TracyZoneScoped;

	int count = Min2( damage * 3 / 2, 60 );

	float player_radius = playerbox_stand_maxs.x;

	const Model * model = FindModel( "models/gibs/gib" );
	float gib_radius = model->bounds.maxs.x;

	constexpr float epsilon = 0.1f;
	float radius = player_radius - gib_radius - epsilon;

	for( int i = 0; i < count; i++ ) {
		if( num_gibs == ARRAY_COUNT( gibs ) )
			break;

		Gib * gib = &gibs[ num_gibs ];
		num_gibs++;

		Vec3 dir = Vec3( UniformSampleInsideCircle( &cls.rng ), 0.0f );
		gib->origin = origin + dir * radius;

		dir.z = RandomFloat01( &cls.rng );
		gib->velocity = velocity * 0.5f + dir * Length( velocity ) * 0.5f;

		gib->scale = RandomUniformFloat( &cls.rng, 0.5f, 1.0f );
		gib->lifetime = 10.0f;
		gib->color = color;
	}
}

static void GibImpact( Vec3 pos, Vec3 normal, Vec4 color, float scale ) {
	DoVisualEffect( "vfx/blood", pos, normal, 1.0f, color );

	{
		constexpr StringHash decals[] = {
			"textures/blood_decals/blood1",
			"textures/blood_decals/blood2",
			"textures/blood_decals/blood3",
			"textures/blood_decals/blood4",
			"textures/blood_decals/blood5",
			"textures/blood_decals/blood6",
			"textures/blood_decals/blood7",
			"textures/blood_decals/blood8",
			"textures/blood_decals/blood9",
			"textures/blood_decals/blood10",
			"textures/blood_decals/blood11",
		};

		if( Probability( &cls.rng, 0.25f ) ) {
			AddPersistentDecal( pos, normal, scale * 64.0f, RandomRadians(), RandomElement( &cls.rng, decals ), color, 30000, 10.0f );
		}
	}
}

void DrawGibs() {
	TracyZoneScoped;

	float dt = cls.frametime * 0.001f;

	const Model * model = FindModel( "models/gibs/gib" );
	Vec3 gravity = Vec3( 0, 0, -GRAVITY );

	for( u32 i = 0; i < num_gibs; i++ ) {
		Gib * gib = &gibs[ i ];

		gib->velocity += gravity * dt;
		Vec3 next_origin = gib->origin + gib->velocity * dt;

		float size = 0.5f * gib->scale;
		MinMax3 bounds = model->bounds * size;

		trace_t trace;
		CG_Trace( &trace, gib->origin, bounds.mins, bounds.maxs, next_origin, 0, MASK_SOLID );

		if( trace.startsolid ) {
			gib->lifetime = 0;
		}
		else if( trace.fraction != 1.0f ) {
			gib->lifetime = 0;

			GibImpact( trace.endpos, trace.plane.normal, gib->color, gib->scale );
		}

		gib->lifetime -= dt;
		if( gib->lifetime <= 0 ) {
			num_gibs--;
			i--;
			Swap2( gib, &gibs[ num_gibs ] );
			continue;
		}

		Mat4 transform = Mat4Translation( gib->origin ) * Mat4Scale( size );
		DrawModelConfig config = { };
		config.draw_model.enabled = true;
		config.draw_shadows.enabled = true;
		DrawModel( config, model, transform, gib->color );

		gib->origin = next_origin;
	}
}
