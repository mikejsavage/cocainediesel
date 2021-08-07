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

//=======================================================================
//
// SelectSpawnPoints
//
//=======================================================================

static edict_t * G_FindPostMatchCamera() {
	edict_t * ent = G_Find( NULL, &edict_t::classname, "post_match_camera" );
	if( ent != NULL )
		return ent;

	return G_Find( NULL, &edict_t::classname, "info_player_intermission" );
}

void SelectSpawnPoint( edict_t * ent, edict_t ** spawnpoint, Vec3 * origin, Vec3 * angles ) {
	edict_t * spot;
	bool cam = false;

	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		spot = G_FindPostMatchCamera();
		cam = spot != NULL;
	}
	else {
		spot = GT_asCallSelectSpawnPoint( ent );
	}

	if( spot == NULL ) {
		spot = world;
	}

	*spawnpoint = spot;
	*origin = spot->s.origin;
	*angles = spot->s.angles;

	// drop to floor
	trace_t trace;
	Vec3 start = *origin + Vec3( 0.0f, 0.0f, 16.0f );
	Vec3 end = *origin - Vec3( 0.0f, 0.0f, 512.0f );

	G_Trace( &trace, start, playerbox_stand_mins, playerbox_stand_maxs, end, ent, MASK_PLAYERSOLID );

	*origin = trace.endpos + trace.plane.normal;
}

//==================================================
// CLIENTS SPAWN QUEUE
//==================================================

#define REINFORCEMENT_WAVE_DELAY        15  // seconds
#define REINFORCEMENT_WAVE_MAXCOUNT     16

typedef struct
{
	int list[MAX_CLIENTS];
	int head;
	int start;
	int system;
	int wave_time;
	int wave_maxcount;
	bool spectate_team;
	int64_t nextWaveTime;
} g_teamspawnqueue_t;

g_teamspawnqueue_t g_spawnQueues[GS_MAX_TEAMS];

void G_SpawnQueue_SetTeamSpawnsystem( int team, int spawnsystem, int wave_time, int wave_maxcount, bool spectate_team ) {
	g_teamspawnqueue_t *queue;

	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[team];
	if( wave_time && wave_time != queue->wave_time ) {
		queue->nextWaveTime = level.time + RandomUniform( &svs.rng, 0, wave_time * 1000 );
	}

	queue->system = spawnsystem;
	queue->wave_time = wave_time;
	queue->wave_maxcount = wave_maxcount;
	if( spawnsystem != SPAWNSYSTEM_INSTANT ) {
		queue->spectate_team = spectate_team;
	} else {
		queue->spectate_team = false;
	}
}

void G_SpawnQueue_Init() {
	int spawnsystem, team;
	cvar_t *g_spawnsystem;
	cvar_t *g_spawnsystem_wave_time;
	cvar_t *g_spawnsystem_wave_maxcount;

	g_spawnsystem = Cvar_Get( "g_spawnsystem", va( "%i", SPAWNSYSTEM_INSTANT ), CVAR_DEVELOPER );
	g_spawnsystem_wave_time = Cvar_Get( "g_spawnsystem_wave_time", va( "%i", REINFORCEMENT_WAVE_DELAY ), CVAR_ARCHIVE );
	g_spawnsystem_wave_maxcount = Cvar_Get( "g_spawnsystem_wave_maxcount", va( "%i", REINFORCEMENT_WAVE_MAXCOUNT ), CVAR_ARCHIVE );

	memset( g_spawnQueues, 0, sizeof( g_spawnQueues ) );
	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ )
		memset( &g_spawnQueues[team].list, -1, sizeof( g_spawnQueues[team].list ) );

	spawnsystem = Clamp( int( SPAWNSYSTEM_INSTANT ), g_spawnsystem->integer, int( SPAWNSYSTEM_HOLD ) );
	if( spawnsystem != g_spawnsystem->integer ) {
		Cvar_ForceSet( "g_spawnsystem", va( "%i", spawnsystem ) );
	}

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		if( team == TEAM_SPECTATOR ) {
			G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );
		} else {
			G_SpawnQueue_SetTeamSpawnsystem( team, spawnsystem, g_spawnsystem_wave_time->integer, g_spawnsystem_wave_maxcount->integer, true );
		}
	}
}

int G_SpawnQueue_NextRespawnTime( int team ) {
	int time;

	if( g_spawnQueues[team].system != SPAWNSYSTEM_WAVES ) {
		return 0;
	}

	if( g_spawnQueues[team].nextWaveTime < level.time ) {
		return 0;
	}

	time = (int)( g_spawnQueues[team].nextWaveTime - level.time );
	return ( time > 0 ) ? time : 0;
}

void G_SpawnQueue_RemoveClient( edict_t *ent ) {
	g_teamspawnqueue_t *queue;
	int i, team;

	if( !ent->r.client ) {
		return;
	}

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		queue = &g_spawnQueues[team];
		for( i = queue->start; i < queue->head; i++ ) {
			if( queue->list[i % MAX_CLIENTS] == ENTNUM( ent ) ) {
				queue->list[i % MAX_CLIENTS] = -1;
			}
		}
	}
}

void G_SpawnQueue_ResetTeamQueue( int team ) {
	g_teamspawnqueue_t *queue;

	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[team];
	memset( &queue->list, -1, sizeof( queue->list ) );
	queue->head = queue->start = 0;
	queue->nextWaveTime = 0;
}

int G_SpawnQueue_GetSystem( int team ) {
	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return SPAWNSYSTEM_INSTANT;
	}

	return g_spawnQueues[team].system;
}

void G_SpawnQueue_ReleaseTeamQueue( int team ) {
	g_teamspawnqueue_t *queue;
	edict_t *ent;
	int count;

	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[team];

	if( queue->start >= queue->head ) {
		return;
	}

	bool ghost = team == TEAM_SPECTATOR;

	// try to spawn them
	for( count = 0; ( queue->start < queue->head ) && ( count < server_gs.maxclients ); queue->start++, count++ ) {
		if( queue->list[queue->start % MAX_CLIENTS] <= 0 || queue->list[queue->start % MAX_CLIENTS] > server_gs.maxclients ) {
			continue;
		}

		ent = &game.edicts[queue->list[queue->start % MAX_CLIENTS]];

		G_ClientRespawn( ent, ghost );

		// when spawning inside spectator team bring up the chase camera
		if( team == TEAM_SPECTATOR && !ent->r.client->resp.chase.active ) {
			G_ChasePlayer( ent, NULL, false, 0 );
		}
	}
}

void G_SpawnQueue_AddClient( edict_t *ent ) {
	g_teamspawnqueue_t *queue;
	int i;

	if( !ent || !ent->r.client ) {
		return;
	}

	if( ENTNUM( ent ) <= 0 || ENTNUM( ent ) > server_gs.maxclients ) {
		return;
	}

	if( ent->r.client->team < TEAM_SPECTATOR || ent->r.client->team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[ent->r.client->team];

	for( i = queue->start; i < queue->head; i++ ) {
		if( queue->list[i % MAX_CLIENTS] == ENTNUM( ent ) ) {
			return;
		}
	}

	G_SpawnQueue_RemoveClient( ent );
	queue->list[queue->head % MAX_CLIENTS] = ENTNUM( ent );
	queue->head++;

	if( queue->spectate_team ) {
		G_ChasePlayer( ent, NULL, true, 0 );
	}
}

void G_SpawnQueue_Think() {
	int team, maxCount, count;
	g_teamspawnqueue_t *queue;
	edict_t *ent;

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		queue = &g_spawnQueues[team];

		// if the system is limited, set limits
		maxCount = MAX_CLIENTS;

		switch( queue->system ) {
			case SPAWNSYSTEM_INSTANT:
			default:
				break;

			case SPAWNSYSTEM_WAVES:
				if( queue->nextWaveTime > level.time ) {
					maxCount = 0;
				} else {
					maxCount = ( queue->wave_maxcount < 1 ) ? server_gs.maxclients : queue->wave_maxcount; // max count per reinforcement wave
					queue->nextWaveTime = level.time + ( queue->wave_time * 1000 );
				}
				break;

			case SPAWNSYSTEM_HOLD:
				maxCount = 0; // players wait to be spawned elsewhere
				break;
		}

		if( maxCount <= 0 ) {
			continue;
		}

		if( queue->start >= queue->head ) {
			continue;
		}

		bool ghost = team == TEAM_SPECTATOR;

		// try to spawn them
		for( count = 0; ( queue->start < queue->head ) && ( count < maxCount ); queue->start++, count++ ) {
			if( queue->list[queue->start % MAX_CLIENTS] <= 0 || queue->list[queue->start % MAX_CLIENTS] > server_gs.maxclients ) {
				continue;
			}

			ent = &game.edicts[queue->list[queue->start % MAX_CLIENTS]];

			G_ClientRespawn( ent, ghost );

			// when spawning inside spectator team bring up the chase camera
			if( team == TEAM_SPECTATOR && !ent->r.client->resp.chase.active ) {
				G_ChasePlayer( ent, NULL, false, 0 );
			}
		}
	}
}
