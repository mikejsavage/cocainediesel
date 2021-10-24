#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static float RandomRadians() {
	return RandomUniformFloat( &cls.rng, 0.0f, Radians( 360.0f ) );
}

void CG_GenericExplosion( Vec3 pos, Vec3 dir, float radius ) {
	// TODO: radius is just ignored?
	ExplosionParticles( pos, dir, vec4_white );
	S_StartFixedSound( "models/bomb/explode", pos, CHAN_AUTO, 1.0f, 1.0f );
}

void CG_Dash( const SyncEntityState * state ) {
	/*
	LocalEntity *le;
	Vec3 pos, dvect, angle = { 0, 0, 0 };

	// KoFFiE: Calculate angle based on relative position of the previous origin state of the player entity
	dvect = state->origin - cg_entities[state->number].prev.origin;

	// ugly inline define -> Ignore when difference between 2 positions was less than this value.
#define IGNORE_DASH 6.0

	if( ( dvect.x > -IGNORE_DASH ) && ( dvect.x < IGNORE_DASH ) &&
		( dvect.y > -IGNORE_DASH ) && ( dvect.y < IGNORE_DASH ) ) {
		return;
	}

	angle = VecToAngles( dvect );
	pos = state->origin;
	angle.y += 270; // Adjust angle
	pos.z -= 24; // Adjust the position to ground height

	if( CG_PointContents( pos ) & MASK_WATER ) {
		return; // no smoke under water :)
	}

	le = CG_AllocModel( LE_DASH_SCALE, pos, angle, 7,
						Vec4( 1.0f, 1.0f, 1.0f, 0.2f ),
						0, 0, 0, 0,
						cgs.media.modDash,
						NULL
						);
	le->ent.scale = 0.01f;
	le->ent.axis[AXIS_UP + 2] *= 2.0f;
	*/
}

void CG_DustCircle( Vec3 pos, Vec3 dir, float radius, int count ) {
	// Vec3 dir_per1;
	// Vec3 dir_per2;
	// Vec3 dir_temp = { 0.0f, 0.0f, 0.0f };
	// int i;
	// float angle;
	//
	// if( CG_PointContents( pos ) & MASK_WATER ) {
	// 	return; // no smoke under water :)
	// }
	// PerpendicularVector( &dir_per2, dir );
	// dir_per1 = Cross( dir, dir_per2 );
	//
	// dir_per1 *= Length( dir_per1 );
	// dir_per2 *= Length( dir_per2 );
	// Normalize( dir_per1 );
	// Normalize( dir_per2 );
	//
	// for( i = 0; i < count; i++ ) {
	// 	angle = (float)( PI * 2.0f / count * i );
	// 	dir_temp = Vec3( 0.0f, 0.0f, 0.0f );
	// 	dir_temp = dir_temp + dir_per1 * ( sinf( angle ) );
	// 	dir_temp = dir_temp + dir_per2 * ( cosf( angle ) );
	//
	// 	//dir_temp = dir_temp * ( dir_temp) = Normalize( dir_temp) );
	// 	dir_temp = dir_temp * ( RandomFloat11( &cls.rng ) * 10 + radius );
	// 	CG_Explosion_Puff_2( pos, dir_temp, 10 );
	// }
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
	ZoneScoped;

	int count = Min2( damage * 3 / 2, 60 );

	float player_radius = playerbox_stand_maxs.x;
	float gib_radius = cgs.media.modGib->bounds.maxs.x;
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
	ZoneScoped;

	float dt = cls.frametime * 0.001f;

	const Model * model = cgs.media.modGib;
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
		DrawModel( model, transform, gib->color );
		DrawModelShadow( model, transform, gib->color );

		gib->origin = next_origin;
	}
}
