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

#include "qcommon/string.h"
#include "game/g_local.h"

/*
* G_Timeout_Reset
*/
void G_Timeout_Reset() {
	G_GamestatSetFlag( GAMESTAT_FLAG_PAUSED, false );
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

	if( !GS_MatchPaused( &server_gs ) ) {
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
		level.timeout.caller = -1;
		G_GamestatSetFlag( GAMESTAT_FLAG_PAUSED, false );

		timeout_printtime = 0;
		timeout_last_endtime = -1;

		G_PrintMsg( NULL, "Match resumed\n" );
	} else if( timeout_printtime == 0 || level.timeout.time - timeout_printtime >= 1000 ) {
		if( level.timeout.endtime ) {
			int seconds_left = (int)( ( level.timeout.endtime - level.timeout.time ) / 1000.0 + 0.5 );

			if( seconds_left == ( TIMEIN_TIME * 2 ) / 1000 ) {
				G_AnnouncerSound( NULL, StringHash( "sounds/announcer/ready" ), GS_MAX_TEAMS, false, NULL );
			} else if( seconds_left >= 1 && seconds_left <= 3 ) {
				constexpr StringHash countdown[] = {
					"sounds/announcer/1",
					"sounds/announcer/2",
					"sounds/announcer/3",
				};
				G_AnnouncerSound( NULL, countdown[ seconds_left + 1 ], GS_MAX_TEAMS, false, NULL );
			}

			G_CenterPrintMsg( NULL, "Match will resume in %i %s", seconds_left, seconds_left == 1 ? "second" : "seconds" );
		} else {
			G_CenterPrintMsg( NULL, "Match paused" );
		}

		timeout_printtime = level.timeout.time;
	}
}

/*
* G_UpdateServerInfo
* update the cvars which show the match state at server browsers
*/
static void G_UpdateServerInfo() {
	// g_match_time
	if( GS_MatchState( &server_gs ) <= MATCH_STATE_WARMUP ) {
		Cvar_ForceSet( "g_match_time", "Warmup" );
	} else if( GS_MatchState( &server_gs ) == MATCH_STATE_COUNTDOWN ) {
		Cvar_ForceSet( "g_match_time", "Countdown" );
	} else if( GS_MatchState( &server_gs ) == MATCH_STATE_PLAYTIME ) {
		// partly from G_GetMatchState
		char extra[MAX_INFO_VALUE];
		int clocktime, timelimit, mins, secs;

		if( GS_MatchDuration( &server_gs ) ) {
			timelimit = ( ( GS_MatchDuration( &server_gs ) ) * 0.001 ) / 60;
		} else {
			timelimit = 0;
		}

		clocktime = (float)( svs.gametime - GS_MatchStartTime( &server_gs ) ) * 0.001f;

		if( clocktime <= 0 ) {
			mins = 0;
			secs = 0;
		} else {
			mins = clocktime / 60;
			secs = clocktime - mins * 60;
		}

		extra[0] = 0;
		if( GS_MatchPaused( &server_gs ) ) {
			Q_strncatz( extra, " (in timeout)", sizeof( extra ) );
		}

		if( timelimit ) {
			Cvar_ForceSet( "g_match_time", va( "%02i:%02i / %02i:00%s", mins, secs, timelimit, extra ) );
		} else {
			Cvar_ForceSet( "g_match_time", va( "%02i:%02i%s", mins, secs, extra ) );
		}
	} else {
		Cvar_ForceSet( "g_match_time", "Finished" );
	}

	// g_match_score
	if( GS_MatchState( &server_gs ) >= MATCH_STATE_PLAYTIME && level.gametype.isTeamBased ) {
		String< MAX_INFO_STRING > score( "{}: {} {}: {}",
			GS_TeamName( TEAM_ALPHA ), server_gs.gameState.teams[ TEAM_ALPHA ].score,
			GS_TeamName( TEAM_BETA ), server_gs.gameState.teams[ TEAM_BETA ].score );

		Cvar_ForceSet( "g_match_score", score.c_str() );
	} else {
		Cvar_ForceSet( "g_match_score", "" );
	}

	// g_needpass
	if( password->modified ) {
		if( password->string && strlen( password->string ) ) {
			Cvar_ForceSet( "g_needpass", "1" );
		} else {
			Cvar_ForceSet( "g_needpass", "0" );
		}
		password->modified = false;
	}
}

static void G_UpdateClientScoreboard( edict_t * ent ) {
	const score_stats_t * stats = G_ClientGetStats( ent );
	SyncScoreboardPlayer * player = &server_gs.gameState.players[ PLAYERNUM( ent ) ];

	player->ping = stats->ping;
	player->score = stats->score;
	player->kills = stats->kills;
	player->ready = stats->ready;
	player->carrier = stats->carrier;
	player->alive = stats->alive;
}


/*
* G_CheckCvars
* Check for cvars that have been modified and need the game to be updated
*/
void G_CheckCvars() {
	if( g_antilag_maxtimedelta->modified ) {
		if( g_antilag_maxtimedelta->integer < 0 ) {
			Cvar_SetValue( "g_antilag_maxtimedelta", Abs( g_antilag_maxtimedelta->integer ) );
		}
		g_antilag_maxtimedelta->modified = false;
		g_antilag_timenudge->modified = true;
	}

	if( g_antilag_timenudge->modified ) {
		if( g_antilag_timenudge->integer > g_antilag_maxtimedelta->integer ) {
			Cvar_SetValue( "g_antilag_timenudge", g_antilag_maxtimedelta->integer );
		} else if( g_antilag_timenudge->integer < -g_antilag_maxtimedelta->integer ) {
			Cvar_SetValue( "g_antilag_timenudge", -g_antilag_maxtimedelta->integer );
		}
		g_antilag_timenudge->modified = false;
	}

	if( g_warmup_timelimit->modified ) {
		// if we are inside timelimit period, update the endtime
		if( GS_MatchState( &server_gs ) == MATCH_STATE_WARMUP ) {
			server_gs.gameState.match_duration = (int64_t)Abs( 60.0f * 1000 * g_warmup_timelimit->integer );
		}
		g_warmup_timelimit->modified = false;
	}

	// update gameshared server settings

	// FIXME: This should be restructured so gameshared settings are the master settings
	G_GamestatSetFlag( GAMESTAT_FLAG_HASCHALLENGERS, level.gametype.hasChallengersQueue );

	G_GamestatSetFlag( GAMESTAT_FLAG_ISTEAMBASED, level.gametype.isTeamBased );
	G_GamestatSetFlag( GAMESTAT_FLAG_ISRACE, level.gametype.isRace );

	G_GamestatSetFlag( GAMESTAT_FLAG_COUNTDOWN, level.gametype.countdownEnabled );
	G_GamestatSetFlag( GAMESTAT_FLAG_INHIBITSHOOTING, level.gametype.shootingDisabled );

	server_gs.gameState.max_team_players = Clamp( 0, level.gametype.maxPlayersPerTeam, 255 );

}

//===================================================================
//		SNAP FRAMES
//===================================================================

/*
* G_SnapClients
*/
void G_SnapClients() {
	int i;
	edict_t *ent;

	// calc the player views now that all pushing and damage has been added
	for( i = 0; i < server_gs.maxclients; i++ ) {
		ent = game.edicts + 1 + i;
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
		if( !ent->r.inuse || ( ent->r.svflags & SVF_NOCLIENT ) ) {
			continue;
		}

		if( ent->s.type == ET_PLAYER || ent->s.type == ET_CORPSE ) {
			// this is pretty hackish
			ent->s.origin2 = ent->velocity;
		}

		if( ent->s.type == ET_PLAYER ) {
			if( ent->snap.damage_taken && !( ent->flags & FL_GODMODE ) ) {
				float damage = Min2( ent->snap.damage_taken, 120.0f );

				Vec3 dir = SafeNormalize( ent->snap.damage_dir );
				Vec3 origin = ent->s.origin + ent->snap.damage_at;

				edict_t * event = G_SpawnEvent( EV_BLOOD, DirToU64( dir ), &origin );
				event->s.radius = HEALTH_TO_INT( damage );
				event->s.ownerNum = ent->s.number;
				event->s.team = ent->s.team;

				if( !G_IsDead( ent ) ) {
					// play an apropriate pain sound
					if( level.time >= ent->pain_debounce_time ) {
						G_AddEvent( ent, EV_PAIN, ent->health <= 25 ? PAIN_20 : PAIN_100, true );
						ent->pain_debounce_time = level.time + 400;
					}
				}
			}
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
	edict_t *ent;

	svs.realtime = Sys_Milliseconds(); // level.time etc. might not be real time

	// clear gametype's clock override
	server_gs.gameState.clock_override = 0;

	// clear all events in the snap
	for( ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( ISEVENTENTITY( &ent->s ) ) { // events do not persist after a snapshot
			G_FreeEdict( ent );
			continue;
		}

		// events only last for a single message
		memset( ent->s.events, 0, sizeof( ent->s.events ) );
		ent->numEvents = 0;
		ent->eventPriority[0] = ent->eventPriority[1] = false;
		ent->s.teleported = false; // remove teleported bit.
	}

	// recover some info, let players respawn and finally clear the snap structures
	for( ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( !GS_MatchPaused( &server_gs ) ) {
			// copy origin to old origin ( this old_origin is for snaps )
			G_CheckClientRespawnClick( ent );
		}

		if( GS_MatchPaused( &server_gs ) ) {
			ent->s.sound = entity_sound_backup[ENTNUM( ent )];
		}

		// clear the snap temp info
		memset( &ent->snap, 0, sizeof( ent->snap ) );
		if( ent->r.client && PF_GetClientState( PLAYERNUM( ent ) ) >= CS_SPAWNED ) {
			memset( &ent->r.client->resp.snap, 0, sizeof( ent->r.client->resp.snap ) );
		}
	}
}

/*
* G_SnapFrame
* It's time to send a new snap, so set the world up for sending
*/
void G_SnapFrame() {
	edict_t *ent;
	svs.realtime = Sys_Milliseconds(); // level.time etc. might not be real time

	//others
	G_UpdateServerInfo();

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
		if( ent->s.number != ENTNUM( ent ) ) {
			if( developer->integer ) {
				Com_Printf( "fixing ent->s.number (etype:%i, classname:%s)\n", ent->s.type, ent->classname ? ent->classname : "noclassname" );
			}
			ent->s.number = ENTNUM( ent );
		}

		// temporary filter (Q2 system to ensure reliability)
		// ignore ents without visible models unless they have an effect
		if( !ent->r.inuse ) {
			ent->r.svflags |= SVF_NOCLIENT;
			continue;
		} else if( ent->s.type >= ET_TOTAL_TYPES || ent->s.type < 0 ) {
			if( developer->integer ) {
				Com_Printf( "'G_SnapFrame': Inhibiting invalid entity type %i\n", ent->s.type );
			}
			ent->r.svflags |= SVF_NOCLIENT;
			continue;
		}

		ent->s.effects &= ~EF_TAKEDAMAGE;
		if( ent->takedamage ) {
			ent->s.effects |= EF_TAKEDAMAGE;
		}

		if( GS_MatchPaused( &server_gs ) ) {
			// when in timeout, we don't send entity sounds
			entity_sound_backup[ENTNUM( ent )] = ent->s.sound;
			ent->s.sound = EMPTY_HASH;
		}
	}
}

//===================================================================
//		WORLD FRAMES
//===================================================================

/*
* G_RunEntities
* treat each object in turn
* even the world and clients get a chance to think
*/
static void G_RunEntities() {
	ZoneScoped;

	edict_t *ent;

	for( ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( !ent->r.inuse ) {
			continue;
		}
		if( ISEVENTENTITY( &ent->s ) ) {
			continue; // events do not think
		}
		level.current_entity = ent;

		// backup oldstate ( for world frame ).
		ent->olds = ent->s;

		// if the ground entity moved, make sure we are still on it
		if( !ent->r.client ) {
			if( ( ent->groundentity ) && ( ent->groundentity->linkcount != ent->groundentity_linkcount ) ) {
				G_CheckGround( ent );
			}
		}

		G_RunEntity( ent );

		if( ent->takedamage ) {
			ent->s.effects |= EF_TAKEDAMAGE;
		} else {
			ent->s.effects &= ~EF_TAKEDAMAGE;
		}
	}
}

/*
* G_RunClients
*/
static void G_RunClients() {
	ZoneScoped;

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t *ent = game.edicts + 1 + i;
		if( !ent->r.inuse ) {
			continue;
		}

		G_ClientThink( ent );

		if( ent->takedamage ) {
			ent->s.effects |= EF_TAKEDAMAGE;
		} else {
			ent->s.effects &= ~EF_TAKEDAMAGE;
		}
	}
}

/*
* G_RunFrame
* Advances the world
*/
void G_RunFrame( unsigned int msec ) {
	ZoneScoped;

	G_CheckCvars();

	game.frametime = msec;
	G_Timeout_Update( msec );

	G_CallVotes_Think();

	if( GS_MatchPaused( &server_gs ) ) {
		unsigned int serverTimeDelta = svs.gametime - game.prevServerTime;
		// freeze match clock and linear projectiles
		server_gs.gameState.match_start += serverTimeDelta;
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
	if( GS_MatchWaiting( &server_gs ) ) {
		server_gs.gameState.match_start = svs.gametime;
	}

	level.framenum++;
	level.time += msec;

	G_SpawnQueue_Think();

	// run the world
	G_RunClients();
	G_RunEntities();
	G_RunGametype();
	GClip_BackUpCollisionFrame();

	game.prevServerTime = svs.gametime;
}
