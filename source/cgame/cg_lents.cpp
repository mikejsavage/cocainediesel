#include "cgame/cg_local.h"
#include "qcommon/array.h"
#include "qcommon/time.h"
#include "client/audio/api.h"
#include "client/renderer/renderer.h"

static float RandomRadians() {
	return RandomUniformFloat( &cls.rng, 0.0f, Radians( 360.0f ) );
}

struct Gib {
	Vec3 origin;
	Vec3 velocity;
	float scale;
	float lifetime;
	Vec4 color;
};

static BoundedDynamicArray< Gib, 512 > gibs;

void InitGibs() {
	gibs.clear();
}

void SpawnGibs( Vec3 origin, Vec3 velocity, int damage, Vec4 color ) {
	TracyZoneScoped;

	int count = Min2( damage * 3 / 2, 60 );

	float player_radius = playerbox_stand.maxs.x;

	const GLTFRenderData * model = FindGLTFRenderData( "models/gibs/gib" );
	if( model == NULL )
		return;

	float gib_radius = Max2( Max2( model->bounds.maxs.x, model->bounds.maxs.y ), model->bounds.maxs.z );

	constexpr float epsilon = 0.1f;
	float radius = player_radius - gib_radius - epsilon;

	for( int i = 0; i < count; i++ ) {
		Optional< Gib * > gib = gibs.add();
		if( !gib.exists )
			break;

		Vec3 dir = Vec3( UniformSampleInsideCircle( &cls.rng ), 0.0f );
		dir.z = RandomFloat01( &cls.rng );

		*gib.value = Gib {
			.origin = origin + dir * radius,
			.velocity = velocity * 0.5f + dir * Length( velocity ) * 0.5f,
			.scale = RandomUniformFloat( &cls.rng, 0.5f, 1.0f ),
			.lifetime = 10.0f,
			.color = color,
		};
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
			AddPersistentDecal( pos, QuaternionFromNormalAndRadians( normal, RandomRadians() ), scale * 64.0f, RandomElement( &cls.rng, decals ), color, Seconds( 30 ), 10.0f );
		}
	}
}

void DrawGibs() {
	TracyZoneScoped;

	float dt = cls.frametime * 0.001f;

	const GLTFRenderData * model = FindGLTFRenderData( "models/gibs/gib" );
	Vec3 gravity = Vec3( 0, 0, -GRAVITY );

	for( size_t i = 0; i < gibs.size(); i++ ) {
		Gib * gib = &gibs[ i ];

		gib->velocity += gravity * dt;
		Vec3 next_origin = gib->origin + gib->velocity * dt;

		float size = 0.5f * gib->scale;
		MinMax3 bounds = model->bounds * size;

		trace_t trace = CG_Trace( gib->origin, bounds, next_origin, -1, Solid_World );

		if( trace.GotNowhere() ) {
			gib->lifetime = 0;
		}
		else if( trace.HitSomething() ) {
			gib->lifetime = 0;

			GibImpact( trace.endpos, trace.normal, gib->color, gib->scale );
		}

		gib->lifetime -= dt;
		if( gib->lifetime <= 0 ) {
			gibs.remove_swap( i );
			i--;
			continue;
		}

		Mat3x4 transform = Mat4Translation( gib->origin ) * Mat4Scale( size );
		DrawModelConfig config = { };
		config.draw_model.enabled = true;
		config.draw_shadows.enabled = true;
		DrawGLTFModel( config, model, transform, gib->color );

		gib->origin = next_origin;
	}
}
