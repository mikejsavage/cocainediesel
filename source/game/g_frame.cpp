/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.


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

void G_Timeout_Reset() {
	server_gs.gameState.paused = false;
	level.timeout = { };
}

/*
* G_Timeout_Update
*
* Updates the timeout struct and informs clients about the status of the pause
*/
static void G_Timeout_Update( unsigned int msec ) {
	static int timeout_printtime = 0;
	static int timeout_last_endtime = 0;

	if( !server_gs.gameState.paused ) {
		return;
	}

	game.frametime = 0;

	if( timeout_last_endtime != level.timeout.endtime ) { // force print when endtime is changed
		timeout_printtime = 0;
		timeout_last_endtime = level.timeout.endtime;
	}

	level.timeout.time += msec;
	if( level.timeout.endtime && level.timeout.time >= level.timeout.endtime ) {
		level.timeout.time = 0;
		level.timeout.caller = Team_None;
		server_gs.gameState.paused = false;

		timeout_printtime = 0;
		timeout_last_endtime = -1;

		G_PrintMsg( NULL, "Match resumed\n" );
	} else if( timeout_printtime == 0 || level.timeout.time - timeout_printtime >= 1000 ) {
		if( level.timeout.endtime ) {
			int seconds_left = (int)( ( level.timeout.endtime - level.timeout.time ) / 1000.0 + 0.5 );

			if( seconds_left == ( TIMEIN_TIME * 2 ) / 1000 ) {
				G_AnnouncerSound( NULL, StringHash( "sounds/announcer/ready" ), Team_Count, false, NULL );
			} else if( seconds_left >= 1 && seconds_left <= 3 ) {
				constexpr StringHash countdown[] = {
					"sounds/announcer/1",
					"sounds/announcer/2",
					"sounds/announcer/3",
				};
				G_AnnouncerSound( NULL, countdown[ seconds_left + 1 ], Team_Count, false, NULL );
			}

			G_CenterPrintMsg( NULL, "Match will resume in %i %s", seconds_left, seconds_left == 1 ? "second" : "seconds" );
		} else {
			G_CenterPrintMsg( NULL, "Match paused" );
		}

		timeout_printtime = level.timeout.time;
	}
}

static void G_UpdateClientScoreboard( edict_t * ent ) {
	const score_stats_t * stats = G_ClientGetStats( ent );
	SyncScoreboardPlayer * player = &server_gs.gameState.players[ PLAYERNUM( ent ) ];

	SafeStrCpy( player->name, ent->r.client->name, sizeof( player->name ) );
	player->ping = svs.clients[ NUM_FOR_EDICT( ent ) - 1 ].ping;
	player->score = stats->score;
	player->kills = stats->kills;
	player->ready = stats->ready;
	player->carrier = ent->r.client->ps.carrying_bomb;
	player->alive = !( G_IsDead( ent ) || G_ISGHOSTING( ent ) );
}

/*
* G_CheckCvars
* Check for cvars that have been modified and need the game to be updated
*/
void G_CheckCvars() {
	if( g_antilag_maxtimedelta->modified ) {
		if( g_antilag_maxtimedelta->integer < 0 ) {
			Cvar_SetInteger( "g_antilag_maxtimedelta", Abs( g_antilag_maxtimedelta->integer ) );
		}
		g_antilag_maxtimedelta->modified = false;
		g_antilag_timenudge->modified = true;
	}

	if( g_antilag_timenudge->modified ) {
		if( g_antilag_timenudge->integer > g_antilag_maxtimedelta->integer ) {
			Cvar_SetInteger( "g_antilag_timenudge", g_antilag_maxtimedelta->integer );
		} else if( g_antilag_timenudge->integer < -g_antilag_maxtimedelta->integer ) {
			Cvar_SetInteger( "g_antilag_timenudge", -g_antilag_maxtimedelta->integer );
		}
		g_antilag_timenudge->modified = false;
	}

	if( g_warmup_timelimit->modified ) {
		// if we are inside timelimit period, update the endtime
		if( server_gs.gameState.match_state == MatchState_Warmup ) {
			server_gs.gameState.match_duration = (int64_t)Abs( 60.0f * 1000 * g_warmup_timelimit->integer );
		}
		g_warmup_timelimit->modified = false;
	}
}

//===================================================================
//		SNAP FRAMES
//===================================================================

void G_SnapClients() {
	// calc the player views now that all pushing and damage has been added
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = game.edicts + 1 + i;
		if( !ent->r.inuse || !ent->r.client ) {
			continue;
		}

		G_Client_InactivityRemove( ent->r.client );
		G_UpdateClientScoreboard( ent );
		G_ClientEndSnapFrame( ent );
	}

	G_EndServerFrames_UpdateChaseCam();
}

/*
* G_EdictsAddSnapEffects
* add effects based on accumulated info along the server frame
*/
static void G_SnapEntities() {
	for( int i = 0; i < game.numentities; i++ ) {
		edict_t * ent = &game.edicts[ i ];
		if( !ent->r.inuse || ( ent->s.svflags & SVF_NOCLIENT ) ) {
			continue;
		}

		if( ent->s.type == ET_PLAYER || ent->s.type == ET_CORPSE ) {
			// this is pretty hackish
			ent->s.origin2 = ent->velocity;
		}
	}
}

// backup entitiy sounds in timeout
static StringHash entity_sound_backup[MAX_EDICTS];

/*
* G_ClearSnap
* We just run G_SnapFrame, the server just sent the snap to the clients,
* it's now time to clean up snap specific data to start the next snap from clean.
*/
void G_ClearSnap() {
	svs.realtime = Sys_Milliseconds(); // level.time etc. might not be real time

	// clear gametype's clock override
	server_gs.gameState.clock_override = 0;

	// clear all events in the snap
	for( edict_t * ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( ISEVENTENTITY( &ent->s ) ) { // events do not persist after a snapshot
			G_FreeEdict( ent );
			continue;
		}

		// events only last for a single message
		memset( ent->s.events, 0, sizeof( ent->s.events ) );
		ent->numEvents = 0;
		ent->eventPriority[0] = ent->eventPriority[1] = false;
		ent->s.teleported = false; // remove teleported bit.

		if( server_gs.gameState.paused ) {
			ent->s.sound = entity_sound_backup[ENTNUM( ent )];
		}
	}

	// recover some info, let players respawn and finally clear the snap structures
	for( size_t i = 1; i <= MAX_CLIENTS; i++ ) {
		edict_t * ent = &game.edicts[ i ];
		if( !server_gs.gameState.paused ) {
			// copy origin to old origin ( this old_origin is for snaps )
			G_CheckClientRespawnClick( ent );
		}

		// clear the snap temp info
		if( ent->r.client && PF_GetClientState( PLAYERNUM( ent ) ) >= CS_SPAWNED ) {
			memset( &ent->r.client->snap, 0, sizeof( ent->r.client->snap ) );
		}
	}
}

/*
* G_SnapFrame
* It's time to send a new snap, so set the world up for sending
*/
void G_SnapFrame() {
	TracyZoneScoped;

	edict_t *ent;
	svs.realtime = Sys_Milliseconds(); // level.time etc. might not be real time

	Cvar_ForceSet( "g_needpass", StrEqual( sv_password->value, "" ) ? "0" : "1" );

	// exit level
	if( level.exitNow ) {
		G_ExitLevel();
		return;
	}

	// finish snap
	G_SnapClients(); // build the playerstate_t structures for all players
	G_SnapEntities(); // add effects based on accumulated info along the frame

	// set entity bits (prepare entities for being sent in the snap)
	for( ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		Assert( ent->s.number == ENTNUM( ent ) );

		// temporary filter (Q2 system to ensure reliability)
		// ignore ents without visible models unless they have an effect
		if( !ent->r.inuse ) {
			ent->s.svflags |= SVF_NOCLIENT;
			continue;
		}

		if( server_gs.gameState.paused ) {
			// when in timeout, we don't send entity sounds
			entity_sound_backup[ENTNUM( ent )] = ent->s.sound;
			ent->s.sound = EMPTY_HASH;
		}
	}
}

//===================================================================
//		WORLD FRAMES
//===================================================================

static void G_RunEntities() {
	TracyZoneScoped;

	edict_t *ent;

	for( ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( !ent->r.inuse ) {
			continue;
		}
		if( ISEVENTENTITY( &ent->s ) ) {
			continue; // events do not think
		}
		level.current_entity = ent;

		// if the ground entity moved, make sure we are still on it
		if( !ent->r.client ) {
			if( ent->groundentity ) {
				G_CheckGround( ent );
			}
		}

		G_RunEntity( ent );
	}
}

static void G_RunClients() {
	TracyZoneScoped;

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t *ent = game.edicts + 1 + i;
		if( !ent->r.inuse )
			continue;

		G_ClientThink( ent );
	}
}

void G_RunFrame( unsigned int msec ) {
	TracyZoneScoped;

	G_CheckCvars();

	game.frametime = msec;
	G_Timeout_Update( msec );

	G_CallVotes_Think();

	if( server_gs.gameState.paused ) {
		unsigned int serverTimeDelta = svs.gametime - game.prevServerTime;
		// freeze match clock and linear projectiles
		server_gs.gameState.match_state_start_time += serverTimeDelta;
		for( edict_t *ent = game.edicts + server_gs.maxclients; ENTNUM( ent ) < game.numentities; ent++ ) {
			if( ent->s.linearMovement ) {
				ent->s.linearMovementTimeStamp += serverTimeDelta;
			}
		}

		G_RunClients();
		G_RunGametype();
		return;
	}

	// reset warmup clock if not enough players
	if( server_gs.gameState.match_state == MatchState_Warmup ) {
		server_gs.gameState.match_state_start_time = svs.gametime;
	}

	level.time += msec;

	// run the world
	G_RunClients();
	G_RunEntities();
	G_RunGametype();
	GClip_BackUpCollisionFrame();

	game.prevServerTime = svs.gametime;
}
