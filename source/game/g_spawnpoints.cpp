#include "game/g_local.h"
#include "qcommon/rng.h"

void SP_post_match_camera( edict_t * ent, const spawn_temp_t * st ) { }

void DropSpawnToFloor( edict_t * ent ) {
	Vec3 mins = playerbox_stand_mins;
	Vec3 maxs = playerbox_stand_maxs;

	Vec3 start = ent->s.origin + Vec3( 0.0f, 0.0f, 16.0f );
	Vec3 end = ent->s.origin - Vec3( 0.0f, 0.0f, 1024.0f );

	trace_t tr;
	G_Trace( &tr, start, mins, maxs, end, ent, MASK_SOLID );

	if( tr.startsolid ) {
		Com_GGPrint( "Spawn starts inside solid, removing..." );
		G_FreeEdict( ent );
		return;
	}

	// move it 1 unit away from the plane
	ent->s.origin = tr.endpos + tr.plane.normal;
}

void SelectSpawnPoint( const edict_t * ent, const edict_t ** spawnpoint, Vec3 * origin, Vec3 * angles ) {
	const edict_t * spot = GT_CallSelectSpawnPoint( ent );
	if( spot == NULL ) {
		spot = world;
	}

	*spawnpoint = spot;
	*origin = spot->s.origin;
	*angles = spot->s.angles;
}
