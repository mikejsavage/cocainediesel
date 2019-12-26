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

/*
* G_Timeout_Reset
*/
void G_Timeout_Reset( void ) {
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
	static int countdown_set = 1;

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
				G_AnnouncerSound( NULL, trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_READY_1_to_2, random_uniform( &svs.rng, 1, 3 ) ) ),
								  GS_MAX_TEAMS, false, NULL );
				countdown_set = random_uniform( &svs.rng, 1, 3 );
			} else if( seconds_left >= 1 && seconds_left <= 3 ) {
				G_AnnouncerSound( NULL, trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, seconds_left,
															 countdown_set ) ), GS_MAX_TEAMS, false, NULL );
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
static void G_UpdateServerInfo( void ) {
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
	if( GS_MatchState( &server_gs ) >= MATCH_STATE_PLAYTIME && GS_TeamBasedGametype( &server_gs ) ) {
		char score[MAX_INFO_STRING];

		score[0] = 0;
		Q_strncatz( score, va( " %s: %i", GS_TeamName( TEAM_ALPHA ), teamlist[TEAM_ALPHA].stats.score ), sizeof( score ) );
		Q_strncatz( score, va( " %s: %i", GS_TeamName( TEAM_BETA ), teamlist[TEAM_BETA].stats.score ), sizeof( score ) );

		if( strlen( score ) >= MAX_INFO_VALUE ) {
			// prevent "invalid info cvar value" flooding
			score[0] = '\0';
		}
		Cvar_ForceSet( "g_match_score", score );
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

/*
* G_CheckCvars
* Check for cvars that have been modified and need the game to be updated
*/
void G_CheckCvars( void ) {
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
			server_gs.gameState.stats[GAMESTAT_MATCHDURATION] = (int64_t)Abs( 60.0f * 1000 * g_warmup_timelimit->integer );
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
	G_GamestatSetFlag( GAMESTAT_FLAG_INFINITEAMMO, level.gametype.infiniteAmmo );
	G_GamestatSetFlag( GAMESTAT_FLAG_CANFORCEMODELS, level.gametype.canForceModels );

	server_gs.gameState.stats[GAMESTAT_MAXPLAYERSINTEAM] = Clamp( 0, level.gametype.maxPlayersPerTeam, 255 );

}

//===================================================================
//		SNAP FRAMES
//===================================================================

static bool g_snapStarted = false;

/*
* G_SnapClients
*/
void G_SnapClients( void ) {
	int i;
	edict_t *ent;

	// calc the player views now that all pushing and damage has been added
	for( i = 0; i < server_gs.maxclients; i++ ) {
		ent = game.edicts + 1 + i;
		if( !ent->r.inuse || !ent->r.client ) {
			continue;
		}

		G_Client_InactivityRemove( ent->r.client );

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
			if( !G_ISGHOSTING( ent ) ) {
				VectorCopy( ent->velocity, ent->s.origin2 );
			}
		}

		if( ISEVENTENTITY( ent ) || G_ISGHOSTING( ent ) || !ent->takedamage ) {
			continue;
		}

		// types which can have accumulated damage effects
		if( ent->s.type == ET_GENERIC || ent->s.type == ET_PLAYER || ent->s.type == ET_CORPSE ) { // doors don't bleed
			// Until we get a proper damage saved effect, we accumulate both into the blood fx
			// so, at least, we don't send 2 entities where we can send one
			ent->snap.damage_taken += ent->snap.damage_saved;

			//ent->snap.damage_saved = 0;

			//spawn accumulated damage
			if( ent->snap.damage_taken && !( ent->flags & FL_GODMODE ) && HEALTH_TO_INT( ent->health ) > 0 ) {
				float damage = Min2( ent->snap.damage_taken, 120.0f );

				vec3_t dir, origin;
				VectorCopy( ent->snap.damage_dir, dir );
				VectorNormalize( dir );
				VectorAdd( ent->s.origin, ent->snap.damage_at, origin );

				if( ent->s.type == ET_PLAYER || ent->s.type == ET_CORPSE ) {
					edict_t * event = G_SpawnEvent( EV_BLOOD, DirToByte( dir ), origin );
					event->s.damage = HEALTH_TO_INT( damage );
					event->s.ownerNum = i; // set owner
					event->s.team = ent->s.team;

					// ET_PLAYERS can also spawn sound events
					if( ent->s.type == ET_PLAYER && !G_IsDead( ent ) ) {
						// play an apropriate pain sound
						if( level.time >= ent->pain_debounce_time ) {
							if( ent->health <= 20 ) {
								G_AddEvent( ent, EV_PAIN, PAIN_20, true );
							} else if( ent->health <= 35 ) {
								G_AddEvent( ent, EV_PAIN, PAIN_35, true );
							} else if( ent->health <= 60 ) {
								G_AddEvent( ent, EV_PAIN, PAIN_60, true );
							} else {
								G_AddEvent( ent, EV_PAIN, PAIN_100, true );
							}

							ent->pain_debounce_time = level.time + 400;
						}
					}
				} else {
					edict_t * event = G_SpawnEvent( EV_SPARKS, DirToByte( dir ), origin );
					event->s.damage = HEALTH_TO_INT( damage );
				}
			}
		}
	}
}

/*
* G_StartFrameSnap
* a snap was just sent, set up for new one
*/
static void G_StartFrameSnap( void ) {
	g_snapStarted = true;
}

// backup entitiy sounds in timeout
static int entity_sound_backup[MAX_EDICTS];

/*
* G_ClearSnap
* We just run G_SnapFrame, the server just sent the snap to the clients,
* it's now time to clean up snap specific data to start the next snap from clean.
*/
void G_ClearSnap( void ) {
	edict_t *ent;

	svs.realtime = Sys_Milliseconds(); // level.time etc. might not be real time

	// clear gametype's clock override
	server_gs.gameState.stats[GAMESTAT_CLOCKOVERRIDE] = 0;

	// clear all events in the snap
	for( ent = &game.edicts[0]; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( ISEVENTENTITY( &ent->s ) ) { // events do not persist after a snapshot
			G_FreeEdict( ent );
			continue;
		}

		// events only last for a single message
		ent->s.events[0] = ent->s.events[1] = 0;
		ent->s.eventParms[0] = ent->s.eventParms[1] = 0;
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
		if( ent->r.client && trap_GetClientState( PLAYERNUM( ent ) ) >= CS_SPAWNED ) {
			memset( &ent->r.client->resp.snap, 0, sizeof( ent->r.client->resp.snap ) );

			// set race stats to invisible
			ent->r.client->ps.stats[STAT_TIME_SELF] = STAT_NOTSET;
			ent->r.client->ps.stats[STAT_TIME_BEST] = STAT_NOTSET;
			ent->r.client->ps.stats[STAT_TIME_RECORD] = STAT_NOTSET;
			ent->r.client->ps.stats[STAT_TIME_ALPHA] = STAT_NOTSET;
			ent->r.client->ps.stats[STAT_TIME_BETA] = STAT_NOTSET;
		}
	}

	g_snapStarted = false;
}

/*
* G_SnapFrame
* It's time to send a new snap, so set the world up for sending
*/
void G_SnapFrame( void ) {
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
		} else if( !( ent->r.svflags & SVF_NOCLIENT ) && !ent->s.modelindex && !ent->s.effects
				   && !ent->s.sound && !ISEVENTENTITY( &ent->s ) && !ent->s.light && !ent->r.client && ent->s.type != ET_HUD ) {
			if( developer->integer ) {
				Com_Printf( "'G_SnapFrame': fixing missing SVF_NOCLIENT flag (no effect)\n" );
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
			ent->s.sound = 0;
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
static void G_RunEntities( void ) {
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
static void G_RunClients( void ) {
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
	G_CheckCvars();

	game.frametime = msec;
	G_Timeout_Update( msec );

	if( !g_snapStarted ) {
		G_StartFrameSnap();
	}

	G_CallVotes_Think();

	if( GS_MatchPaused( &server_gs ) ) {
		unsigned int serverTimeDelta = svs.gametime - game.prevServerTime;
		// freeze match clock and linear projectiles
		server_gs.gameState.stats[GAMESTAT_MATCHSTART] += serverTimeDelta;
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
		server_gs.gameState.stats[GAMESTAT_MATCHSTART] = svs.gametime;
	}

	level.framenum++;
	level.time += msec;

	G_SpawnQueue_Think();

	// run the world
	G_RunClients();
	G_RunEntities();
	G_RunGametype();
	GClip_BackUpCollisionFrame();
}
