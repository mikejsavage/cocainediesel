#include "cgame/cg_local.h"

static StringHash GetPlayerSound( int entnum, PlayerSound ps ) {
	const PlayerModelMetadata * meta = GetPlayerModelMetadata( entnum );
	assert( meta != NULL );
	if( meta == NULL ) {
		Com_Printf( "Player model metadata is null\n" );
		return EMPTY_HASH;
	}
	return meta->sounds[ ps ];
}

float CG_PlayerPitch( int entnum ) {
	float basis = Length( Vec3( 1 ) );
	return 1.0f / ( Length( cg_entities[ entnum ].current.scale ) / basis );
}

void CG_PlayerSound( int entnum, PlayerSound ps, bool stop_current ) {
	StringHash sfx = GetPlayerSound( entnum, ps );

	float pitch = 1.0f;
	if( ps == PlayerSound_Death || ps == PlayerSound_Void || ps == PlayerSound_Pain25 || ps == PlayerSound_Pain50 || ps == PlayerSound_Pain75 || ps == PlayerSound_Pain100 || ps == PlayerSound_WallJump ) {
		pitch = CG_PlayerPitch( entnum );
	}

	PlaySFXConfig config = PlaySFXConfigGlobal();
	config.pitch = pitch;
	if( !ISVIEWERENTITY( entnum ) ) {
		config.spatialisation = SpatialisationMethod_Entity;
		config.ent_num = entnum;
	}

	PlayingSFXHandle handle = PlaySFX( sfx, config );

	if( stop_current ) {
		centity_t * cent = &cg_entities[ entnum ];
		StopSFX( cent->playing_body_sound );
		cent->playing_body_sound = handle;
	}
}
