#include "qcommon/base.h"
#include "client/assets.h"
#include "client/audio/api.h"
#include "client/renderer/material.h"
#include "cgame/cg_local.h"

#include "nanosort/nanosort.hpp"

struct Spray {
	Vec3 origin;
	Vec3 normal;
	float radius;
	float angle;
	StringHash material;
	s64 spawn_time;
};

static StringHash spray_assets[ 4096 ];
static size_t num_spray_assets;

static constexpr s64 SPRAY_LIFETIME = 60000;

static Spray sprays[ 1024 ];
static size_t sprays_head;
static size_t num_sprays;

void InitSprays() {
	num_spray_assets = 0;

	for( Span< const char > path : AssetPaths() ) {
		bool ext_ok = EndsWith( path, ".png" ) || EndsWith( path, ".jpg" ) || EndsWith( path, ".dds" );
		if( !StartsWith( path, "textures/sprays/" ) || !ext_ok )
			continue;

		const Material * material = FindMaterial( StringHash( Hash64( StripExtension( path ) ) ) );
		if( !material->decal ) {
			Com_GGPrint( S_COLOR_YELLOW "Spray {} needs a decal material", path );
			continue;
		}

		Assert( num_spray_assets < ARRAY_COUNT( spray_assets ) );

		spray_assets[ num_spray_assets ] = StringHash( StripExtension( path ) );
		num_spray_assets++;
	}

	nanosort( spray_assets, spray_assets + num_spray_assets, []( StringHash a, StringHash b ) {
		return a.hash < b.hash;
	} );

	sprays_head = 0;
	num_sprays = 0;
}

void AddSpray( Vec3 origin, Vec3 normal, EulerDegrees3 angles, float scale, u64 entropy ) {
	RNG rng = NewRNG( entropy, 0 );

	Vec3 forward, up;
	AngleVectors( angles, &forward, NULL, &up );

	Spray spray;
	spray.origin = origin;
	spray.normal = normal;
	spray.material = num_spray_assets == 0 ? StringHash( "" ) : RandomElement( &rng, spray_assets, num_spray_assets );
	spray.radius = RandomUniformFloat( &rng, 32.0f, 48.0f ) * scale;
	spray.spawn_time = cls.gametime;

	Vec3 left = Cross( normal, up );
	Vec3 decal_up = Normalize( Cross( left, normal ) );

	Vec3 tangent, bitangent;
	OrthonormalBasis( normal, &tangent, &bitangent );

	spray.angle = -atan2f( Dot( decal_up, tangent ), Dot( decal_up, bitangent ) );
	spray.angle += RandomFloat11( &rng ) * Radians( 10.0f );

	sprays[ ( sprays_head + num_sprays ) % ARRAY_COUNT( sprays ) ] = spray;

	if( num_sprays == ARRAY_COUNT( sprays ) ) {
		sprays_head++;
	}
	else {
		num_sprays++;
	}

	PlaySFX( "sounds/spray/spray", PlaySFXConfigPosition( origin ) );
	DoVisualEffect( "vfx/spray", origin - forward * 8.0f, forward );
}

void DrawSprays() {
	while( num_sprays > 0 ) {
		if( sprays[ sprays_head % ARRAY_COUNT( sprays ) ].spawn_time + SPRAY_LIFETIME >= cls.gametime )
			break;
		sprays_head++;
		num_sprays--;
	}

	for( size_t i = 0; i < num_sprays; i++ ) {
		const Spray * spray = &sprays[ ( sprays_head + i ) % ARRAY_COUNT( sprays ) ];
		DrawDecal( spray->origin, spray->normal, spray->radius, spray->angle, spray->material, white.vec4, 0.0f );
	}
}
