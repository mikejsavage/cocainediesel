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

#include "g_local.h"
#include "qalgo/rng.h"

void SP_info_player_start( edict_t *self ) {
	G_DropSpawnpointToFloor( self );
}

void SP_info_player_deathmatch( edict_t *self ) {
	G_DropSpawnpointToFloor( self );
}

void SP_post_match_camera( edict_t *ent ) { }

//=======================================================================
//
// SelectSpawnPoints
//
//=======================================================================


/*
* PlayersRangeFromSpot
*
* Returns the distance to the nearest player from the given spot
*/
float PlayersRangeFromSpot( edict_t *spot, int ignore_team ) {
	edict_t *player;
	float bestplayerdistance;
	int n;
	float playerdistance;

	bestplayerdistance = 9999999;

	for( n = 1; n <= gs.maxclients; n++ ) {
		player = &game.edicts[n];

		if( !player->r.inuse ) {
			continue;
		}
		if( player->r.solid == SOLID_NOT ) {
			continue;
		}
		if( ( ignore_team && ignore_team == player->s.team ) || player->s.team == TEAM_SPECTATOR ) {
			continue;
		}

		playerdistance = DistanceFast( spot->s.origin, player->s.origin );

		if( playerdistance < bestplayerdistance ) {
			bestplayerdistance = playerdistance;
		}
	}

	return bestplayerdistance;
}

static edict_t *G_FindPostMatchCamera( void ) {
	edict_t * ent = G_Find( NULL, FOFS( classname ), "post_match_camera" );
	if( ent != NULL )
		return ent;

	return G_Find( NULL, FOFS( classname ), "info_player_intermission" );
}

/*
* G_OffsetSpawnPoint - use a grid of player boxes to offset the spawn point
*/
bool G_OffsetSpawnPoint( vec3_t origin, const vec3_t box_mins, const vec3_t box_maxs, float radius, bool checkground ) {
	trace_t trace;
	vec3_t virtualorigin;
	vec3_t absmins, absmaxs;
	float playerbox_rowwidth;
	float playerbox_columnwidth;
	int rows, columns;
	int i, j;
	RNG rng = new_rng( rand(), 0 );
	int mask_spawn = MASK_PLAYERSOLID | ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_TELEPORTER | CONTENTS_JUMPPAD | CONTENTS_BODY | CONTENTS_NODROP );
	int playersFound = 0, worldfound = 0, nofloorfound = 0, badclusterfound = 0;

	// check box clusters
	int cluster;
	int num_leafs;
	int leafs[8];

	if( radius <= box_maxs[0] - box_mins[0] ) {
		return true;
	}

	if( checkground ) { // drop the point to ground (scripts can accept any entity now)
		VectorCopy( origin, virtualorigin );
		virtualorigin[2] -= 1024;

		G_Trace( &trace, origin, box_mins, box_maxs, virtualorigin, NULL, MASK_PLAYERSOLID );
		if( trace.fraction == 1.0f ) {
			checkground = false;
		} else if( trace.endpos[2] + 8.0f < origin[2] ) {
			VectorCopy( trace.endpos, origin );
			origin[2] += 8.0f;
		}
	}

	playerbox_rowwidth = 2 + box_maxs[0] - box_mins[0];
	playerbox_columnwidth = 2 + box_maxs[1] - box_mins[1];

	rows = radius / playerbox_rowwidth;
	columns = radius / playerbox_columnwidth;

	// no, we won't just do a while, let's go safe and just check as many times as
	// positions in the grid. If we didn't found a spawnpoint by then, we let it telefrag.
	for( i = 0; i < ( rows * columns ); i++ ) {
		int row = random_uniform( &rng, -rows, rows + 1 );
		int column = random_uniform( &rng, -columns, columns + 1 );

		VectorSet( virtualorigin, origin[0] + ( row * playerbox_rowwidth ),
				   origin[1] + ( column * playerbox_columnwidth ),
				   origin[2] );

		VectorAdd( virtualorigin, box_mins, absmins );
		VectorAdd( virtualorigin, box_maxs, absmaxs );
		for( j = 0; j < 2; j++ ) {
			absmaxs[j] += 1;
			absmins[j] -= 1;
		}

		//check if position is inside world

		// check if valid cluster
		cluster = -1; // fix a warning
		num_leafs = trap_CM_BoxLeafnums( absmins, absmaxs, leafs, 8, NULL );
		for( j = 0; j < num_leafs; j++ ) {
			cluster = trap_CM_LeafCluster( leafs[j] );
			if( cluster == -1 ) {
				break;
			}
		}

		if( cluster == -1 ) {
			badclusterfound++;
			continue;
		}

		// one more trace is needed, only checking if some part of the world is on the
		// way from spawnpoint to the virtual position
		trap_CM_TransformedBoxTrace( &trace, origin, virtualorigin, box_mins, box_maxs, NULL, MASK_PLAYERSOLID, NULL, NULL );
		if( trace.fraction != 1.0f ) {
			continue;
		}

		// check if anything solid is on player's way

		G_Trace( &trace, vec3_origin, absmins, absmaxs, vec3_origin, world, mask_spawn );
		if( trace.startsolid || trace.allsolid || trace.ent != -1 ) {
			if( trace.ent == 0 ) {
				worldfound++;
			} else if( trace.ent < gs.maxclients ) {
				playersFound++;
			}
			continue;
		}

		// one more check before accepting this spawn: there's ground at our feet?
		if( checkground ) { // if floating item flag is not set
			vec3_t origin_from, origin_to;
			VectorCopy( virtualorigin, origin_from );
			origin_from[2] += box_mins[2] + 1;
			VectorCopy( origin_from, origin_to );
			origin_to[2] -= 32;

			// use point trace instead of box trace to avoid small glitches that can't support the player but will stop the trace
			G_Trace( &trace, origin_from, vec3_origin, vec3_origin, origin_to, NULL, MASK_PLAYERSOLID );
			if( trace.startsolid || trace.allsolid || trace.fraction == 1.0f ) { // full run means no ground
				nofloorfound++;
				continue;
			}
		}

		VectorCopy( virtualorigin, origin );
		return true;
	}

	//G_Printf( "Warning: couldn't find a safe spawnpoint (blocked by players:%i world:%i nofloor:%i badcluster:%i)\n", playersFound, worldfound, nofloorfound, badclusterfound );
	return false;
}


/*
* SelectSpawnPoint
*
* Chooses a player start, deathmatch start, etc
*/
void SelectSpawnPoint( edict_t *ent, edict_t **spawnpoint, vec3_t origin, vec3_t angles ) {
	edict_t *spot = NULL;

	if( GS_MatchState() >= MATCH_STATE_POSTMATCH ) {
		spot = G_FindPostMatchCamera();
	} else {
		spot = GT_asCallSelectSpawnPoint( ent );
	}

	// find a single player start spot
	if( !spot ) {
		spot = G_Find( spot, FOFS( classname ), "info_player_start" );
		if( !spot ) {
			spot = G_Find( spot, FOFS( classname ), "team_CTF_alphaspawn" );
			if( !spot ) {
				spot = G_Find( spot, FOFS( classname ), "team_CTF_betaspawn" );
			}
			if( !spot ) {
				spot = world;
			}
		}
	}

	*spawnpoint = spot;
	VectorCopy( spot->s.origin, origin );
	VectorCopy( spot->s.angles, angles );

	if( !Q_stricmp( spot->classname, "info_player_intermission" ) ) {
		// if it has a target, look towards it
		if( spot->target ) {
			vec3_t dir;
			edict_t *target;

			target = G_PickTarget( spot->target );
			if( target ) {
				VectorSubtract( target->s.origin, origin, dir );
				VecToAngles( dir, angles );
			}
		}
	}

	// SPAWN TELEFRAGGING PROTECTION.
	if( ent->r.solid == SOLID_YES && ( level.gametype.spawnpointRadius > ( playerbox_stand_maxs[0] - playerbox_stand_mins[0] ) ) ) {
		G_OffsetSpawnPoint( origin, playerbox_stand_mins, playerbox_stand_maxs, level.gametype.spawnpointRadius, ( spot->spawnflags & 1 ) == 0 );
	}
}
