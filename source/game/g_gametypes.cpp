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
#include "qcommon/time.h"
#include "game/g_maps.h"
#include "gameshared/cdmap.h"

Cvar *g_warmup_timelimit;
Cvar *g_scorelimit;

void G_Match_Autorecord_Start() {
	if( !g_autorecord->integer )
		return;

	// do not start autorecording if all playing clients are bots
	bool has_players = false;
	for( int team = Team_One; team < Team_Count; team++ ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		for( u8 i = 0; i < current_team->num_players; i++ ) {
			if( game.edicts[ current_team->player_indices[ i ] ].s.svflags & SVF_FAKECLIENT ) {
				continue;
			}

			has_players = true;
			break;
		}
	}

	if( !has_players )
		return;

	char date[ 128 ];
	FormatCurrentTime( date, sizeof( date ), "%Y-%m-%d_%H-%M" );

	snprintf( level.autorecord_name, sizeof( level.autorecord_name ), "%s_%s_auto%04i", date, sv.mapname, RandomUniform( &svs.rng, 1, 10000 ) );

	Cbuf_Add( "serverrecord {}", level.autorecord_name );
}

void G_Match_Autorecord_Stop() {
	if( g_autorecord->integer ) {
		Cbuf_Add( "{}", "serverrecordstop 1" );

		if( g_autorecord_maxdemos->integer > 0 ) {
			Cbuf_Add( "{}", "serverrecordpurge" );
		}
	}
}

static void G_Match_CheckStateAbort() {
	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
		return;
	}

	int teams_with_players = 0;
	for( int i = 0; i < level.gametype.numTeams; i++ ) {
		if( server_gs.gameState.teams[ Team_One + i ].num_players > 0 ) {
			teams_with_players++;
		}
	}

	bool playable = teams_with_players >= 2;

	// if waiting, turn on match states when enough players joined
	if( GS_MatchWaiting( &server_gs ) && playable ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
	}
	// turn off active match states if not enough players left
	else if( server_gs.gameState.match_state == MatchState_Warmup && !playable && server_gs.gameState.match_duration ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	}
	else if( server_gs.gameState.match_state == MatchState_Countdown && !playable ) {
		G_ClearCenterPrint( NULL );
		G_Match_LaunchState( MatchState_Warmup );
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	}
	// match running, but not enough players left
	else if( server_gs.gameState.match_state == MatchState_Playing && !playable ) {
		G_PrintMsg( NULL, "Not enough players left. Match aborted.\n" );
		G_CenterPrintMsg( NULL, "MATCH ABORTED" );
		G_EndMatch();
	}
}

void G_Match_LaunchState( MatchState matchState ) {
	G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );

	if( matchState == MatchState_PostMatch ) {
		level.finalMatchDuration = svs.gametime - server_gs.gameState.match_state_start_time;
	}

	switch( matchState ) {
		default:
		case MatchState_Warmup:
			server_gs.gameState.match_state = MatchState_Warmup;
			server_gs.gameState.match_duration = (int64_t)( Abs( g_warmup_timelimit->number * 60 ) * 1000 );
			server_gs.gameState.match_state_start_time = svs.gametime;
			break;

		case MatchState_Countdown:
			server_gs.gameState.match_state = MatchState_Countdown;
			server_gs.gameState.match_duration = 2000;
			server_gs.gameState.match_state_start_time = svs.gametime;
			break;

		case MatchState_Playing:
			G_Match_Autorecord_Start();

			server_gs.gameState.match_state = MatchState_Playing;
			server_gs.gameState.match_duration = 0;
			server_gs.gameState.match_state_start_time = svs.gametime;
			break;

		case MatchState_PostMatch:
			server_gs.gameState.match_state = MatchState_PostMatch;
			server_gs.gameState.match_duration = 4000;
			server_gs.gameState.match_state_start_time = svs.gametime;

			G_Timeout_Reset();
			break;

		case MatchState_WaitExit:
			G_Match_Autorecord_Stop();

			server_gs.gameState.match_state = MatchState_WaitExit;
			server_gs.gameState.match_duration = 3000;
			server_gs.gameState.match_state_start_time = svs.gametime;

			level.exitNow = false;
			break;
	}

	// give the gametype the chance to setup for the new state
	GT_CallMatchStateStarted();
}

bool G_Match_ScorelimitHit() {
	if( server_gs.gameState.match_state != MatchState_Playing ) {
		return false;
	}

	if( g_scorelimit->integer > 0 ) {
		for( int i = 0; i < level.gametype.numTeams; i++ ) {
			u8 score = server_gs.gameState.teams[ Team_One + i ].score;
			if( score >= u8( g_scorelimit->integer ) ) {
				return true;
			}
		}
	}

	return false;
}

bool G_Match_TimelimitHit() {
	// check for timelimit hit
	if( !server_gs.gameState.match_duration || svs.gametime < server_gs.gameState.match_state_start_time + server_gs.gameState.match_duration ) {
		return false;
	}

	if( server_gs.gameState.match_state == MatchState_WaitExit ) {
		level.exitNow = true;
		return false; // don't advance into next state. The match will be restarted
	}

	return true;
}

void G_EndMatch() {
	G_Match_LaunchState( MatchState_PostMatch );
}

void G_Match_CheckReadys() {
	if( server_gs.gameState.match_state != MatchState_Warmup ) {
		return;
	}

	int teamsready = 0;
	for( int team = Team_One; team < Team_Count; team++ ) {
		int readys = 0;
		int notreadys = 0;
		SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		for( u8 i = 0; i < current_team->num_players; i++ ) {
			const edict_t * e = game.edicts + current_team->player_indices[ i ];

			if( !e->r.inuse ) {
				continue;
			}
			if( e->s.team == Team_None ) { //ignore spectators
				continue;
			}

			if( level.ready[PLAYERNUM( e )] ) {
				readys++;
			} else {
				notreadys++;
			}
		}
		if( !notreadys && readys ) {
			teamsready++;
		}
	}

	if( teamsready >= 2 ) {
		G_PrintMsg( NULL, "All players are ready. Match starting!\n" );
		G_Match_LaunchState( MatchState_Countdown );
	}
}

void G_Match_Ready( edict_t *ent ) {
	if( ( ent->s.svflags & SVF_FAKECLIENT ) && level.ready[PLAYERNUM( ent )] ) {
		return;
	}

	if( ent->s.team == Team_None ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Warmup ) {
		if( !( ent->s.svflags & SVF_FAKECLIENT ) ) {
			G_PrintMsg( ent, "We're not in warmup.\n" );
		}
		return;
	}

	if( level.ready[ PLAYERNUM( ent ) ] ) {
		G_PrintMsg( ent, "You are already ready.\n" );
		return;
	}

	level.ready[ PLAYERNUM( ent ) ] = true;
	G_ClientGetStats( ent )->ready = true;

	G_PrintMsg( NULL, "%s is %sREADY\n", ent->r.client->netname, S_COLOR_GREEN );

	G_Match_CheckReadys();
}

void G_Match_NotReady( edict_t *ent ) {
	if( ent->s.team == Team_None ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Warmup ) {
		G_PrintMsg( ent, "A match is not being setup.\n" );
		return;
	}

	if( !level.ready[PLAYERNUM( ent )] ) {
		G_PrintMsg( ent, "You weren't ready.\n" );
		return;
	}

	level.ready[ PLAYERNUM( ent ) ] = false;
	G_ClientGetStats( ent )->ready = false;

	G_PrintMsg( NULL, "%s is %sNOT READY\n", ent->r.client->netname, S_COLOR_RED );
}

void G_Match_ToggleReady( edict_t *ent ) {
	if( !level.ready[PLAYERNUM( ent )] ) {
		G_Match_Ready( ent );
	} else {
		G_Match_NotReady( ent );
	}
}

//======================================================
//		Game types

static bool G_EachNewSecond() {
	static int lastsecond;
	static int second;

	second = (int)( level.time * 0.001 );
	if( lastsecond == second ) {
		return false;
	}

	lastsecond = second;
	return true;
}

static void G_CheckNumBots() {
	if( level.spawnedTimeStamp + 5000 > svs.realtime ) {
		return;
	}

	if( g_numbots->integer > server_gs.maxclients ) {
		Cvar_SetInteger( "g_numbots", server_gs.maxclients );
	}

	int desiredNumBots = g_numbots->integer;
	if( desiredNumBots < game.numBots ) {
		for( edict_t *ent = game.edicts + server_gs.maxclients; PLAYERNUM( ent ) >= 0 && desiredNumBots < game.numBots; ent-- ) {
			if( !ent->r.inuse || !( ent->s.svflags & SVF_FAKECLIENT ) ) {
				continue;
			}
			PF_DropClient( ent, NULL );
			game.numBots--;
		}
	}
	else if( desiredNumBots > game.numBots ) {
		for( edict_t *ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients && game.numBots < desiredNumBots; ent++ ) {
			if( !ent->r.inuse && PF_GetClientState( PLAYERNUM( ent ) ) == CS_FREE ) {
				AI_SpawnBot();
			}
		}
	}
}

static bool G_EachNewMinute() {
	static int lastminute;
	static int minute;

	minute = (int)( level.time * 0.001f / 60.0f );
	if( lastminute == minute ) {
		return false;
	}

	lastminute = minute;
	return true;
}

static void G_CheckEvenTeam() {
	int max = 0;
	int min = server_gs.maxclients + 1;
	Team uneven = Team_None;

	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		return;
	}

	if( g_teams_allow_uneven->integer ) {
		return;
	}

	for( int i = Team_One; i < Team_Count; i++ ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ i ];
		if( max < current_team->num_players ) {
			max = current_team->num_players;
			uneven = Team( i );
		}
		if( min > current_team->num_players ) {
			min = current_team->num_players;
		}
	}

	if( max - min > 1 ) {
		SyncTeamState * uneven_team = &server_gs.gameState.teams[ uneven ];
		for( u8 i = 0; i < uneven_team->num_players; i++ ) {
			edict_t * e = game.edicts + uneven_team->player_indices[ i ];
			if( !e->r.inuse ) {
				continue;
			}
			G_CenterPrintMsg( e, "Teams are uneven. Please switch into another team." ); // FIXME: need more suitable message :P
			G_PrintMsg( e, "%sTeams are uneven. Please switch into another team.\n", S_COLOR_CYAN ); // FIXME: need more suitable message :P
		}

		// FIXME: switch team forcibly?
	}
}

void G_RunGametype() {
	TracyZoneScoped;

	G_Teams_UpdateMembersList();
	G_Match_CheckStateAbort();

	if( G_Match_TimelimitHit() ) {
		G_Match_LaunchState( MatchState( server_gs.gameState.match_state + 1 ) );
		G_ClearCenterPrint( NULL );
	}

	level.gametype.Think();

	if( G_EachNewSecond() ) {
		G_CheckNumBots();
	}

	if( G_EachNewMinute() ) {
		G_CheckEvenTeam();
	}
}

void GT_CallMatchStateStarted() {
	if( server_gs.gameState.match_state == MatchState_Playing ) {
		for( int i = 0; i < server_gs.maxclients; i++ ) {
			*G_ClientGetStats( PLAYERENT( i ) ) = { };
		}
	}

	level.gametype.MatchStateStarted();
}

void GT_CallPlayerConnected( edict_t * ent ) {
	if( level.gametype.PlayerConnected != NULL ) {
		level.gametype.PlayerConnected( ent );
	}
}

void GT_CallPlayerRespawning( edict_t * ent ) {
	if( level.gametype.PlayerRespawning != NULL ) {
		level.gametype.PlayerRespawning( ent );
	}
}

void GT_CallPlayerRespawned( edict_t * ent, Team old_team, Team new_team ) {
	level.gametype.PlayerRespawned( ent, old_team, new_team );
}

void GT_CallPlayerKilled( edict_t * victim, edict_t * attacker, edict_t * inflictor ) {
	level.gametype.PlayerKilled( victim, attacker, inflictor );
}

const edict_t * GT_CallSelectSpawnPoint( const edict_t * ent ) {
	return level.gametype.SelectSpawnPoint( ent );
}

const edict_t * GT_CallSelectDeadcam() {
	if( level.gametype.SelectDeadcam != NULL ) {
		return level.gametype.SelectDeadcam();
	}
	return NULL;
}

static bool IsGladiatorMap() {
	return GetWorldspawnKey( FindServerMap( server_gs.gameState.map ), "gametype" ) == "gladiator";
}

void InitGametype() {
	g_warmup_timelimit = NewCvar( "g_warmup_timelimit", "5", CvarFlag_Archive );
	g_scorelimit = NewCvar( "g_scorelimit", "10", CvarFlag_Archive );

	level.gametype = IsGladiatorMap() ? GetGladiatorGametype() : GetBombGametype();
	level.gametype.Init();
}

void ShutdownGametype() {
	if( level.gametype.Shutdown != NULL ) {
		level.gametype.Shutdown();
	}
	level.gametype = { };
}
