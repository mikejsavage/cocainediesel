#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/time.h"
#include "client/assets.h"
#include "client/audio/api.h"
#include "client/renderer/material.h"
#include "cgame/cg_local.h"

#include "nanosort/nanosort.hpp"

struct Spray {
	Vec3 origin;
	Quaternion orientation;
	float radius;
	StringHash material;
	Time expiration;
};

static constexpr Time SPRAY_LIFETIME = Seconds( 60 );

static BoundedDynamicArray< StringHash, 4096 > spray_assets;
static Spray sprays[ 1024 ];
static size_t sprays_head;
static size_t num_sprays;

void InitSprays() {
	spray_assets.clear();

	for( Span< const char > path : AssetPaths() ) {
		bool ext_ok = EndsWith( path, ".png" ) || EndsWith( path, ".jpg" ) || EndsWith( path, ".dds" );
		if( !StartsWith( path, "textures/sprays/" ) || !ext_ok )
			continue;

		const Material * material = FindMaterial( StringHash( Hash64( StripExtension( path ) ) ) );
		if( !material->decal ) {
			Com_GGPrint( S_COLOR_YELLOW "Spray {} needs a decal material", path );
			continue;
		}

		[[maybe_unused]] bool ok = spray_assets.add( StringHash( StripExtension( path ) ) );
		Assert( ok );
	}

	nanosort( spray_assets.begin(), spray_assets.end(), []( StringHash a, StringHash b ) {
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
	spray.material = spray_assets.size() == 0 ? StringHash( "" ) : RandomElement( &rng, spray_assets.span() );
	spray.radius = RandomUniformFloat( &rng, 32.0f, 48.0f ) * scale;
	spray.expiration = cls.game_time + SPRAY_LIFETIME;

	Vec3 left = Cross( normal, up );
	Vec3 decal_up = Normalize( Cross( left, normal ) );

	Quaternion random_rotation = QuaternionFromAxisAndRadians( Vec3( 1.0f, 0.0f, 0.0f ), RandomFloat11( &rng ) * Radians( 10.0f ) );
	spray.orientation = BasisToQuaternion( normal, -left, decal_up ) * random_rotation;

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
		if( sprays[ sprays_head % ARRAY_COUNT( sprays ) ].expiration >= cls.game_time )
			break;
		sprays_head++;
		num_sprays--;
	}

	for( size_t i = 0; i < num_sprays; i++ ) {
		const Spray * spray = &sprays[ ( sprays_head + i ) % ARRAY_COUNT( sprays ) ];
		DrawDecal( spray->origin, spray->orientation, spray->radius, spray->material, white.vec4, 0.0f );
	}
}
