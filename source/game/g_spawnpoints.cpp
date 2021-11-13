/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "game/g_local.h"
#include "qcommon/cmodel.h"
#include "qcommon/rng.h"

void SP_post_match_camera( edict_t *ent ) { }

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

void SelectSpawnPoint( edict_t * ent, edict_t ** spawnpoint, Vec3 * origin, Vec3 * angles ) {
	edict_t * spot;

	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		spot = G_Find( NULL, &edict_t::classname, "post_match_camera" );
	}
	else {
		spot = GT_CallSelectSpawnPoint( ent );
	}

	if( spot == NULL ) {
		spot = world;
	}

	*spawnpoint = spot;
	*origin = spot->s.origin;
	*angles = spot->s.angles;
}
