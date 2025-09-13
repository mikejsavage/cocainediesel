#include "game/g_local.h"

void SP_post_match_camera( edict_t * ent, const spawn_temp_t * st ) {
}

void DropSpawnToFloor( edict_t * ent ) {
	Vec3 start = ent->s.origin + Vec3::Z( 16.0f );
	Vec3 end = ent->s.origin - Vec3::Z( 1024.0f );

	trace_t trace = G_Trace( start, playerbox_stand, end, ent, Solid_World );

	if( trace.GotNowhere() ) {
		Com_GGPrint( "Spawn at {} is inside entity {}, removing...", ent->s.origin, trace.ent );
		G_FreeEdict( ent );
		return;
	}

	// move it 1 unit away from the plane
	ent->s.origin = trace.endpos + trace.normal;
}

const edict_t * SelectSpawnPoint( const edict_t * ent ) {
	const edict_t * spot = GT_CallSelectSpawnPoint( ent );
	return spot == NULL ? world : spot;
}
