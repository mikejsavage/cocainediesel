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

//==========================================================
//					Matches
//==========================================================

cvar_t *g_warmup_timelimit;
cvar_t *g_match_extendedtime;
cvar_t *g_scorelimit;

//==========================================================
//					Matches
//==========================================================

static void G_Match_SetAutorecordState( const char *state ) {
	PF_ConfigString( CS_AUTORECORDSTATE, state );
}

void G_Match_Autorecord_Start() {
	G_Match_SetAutorecordState( "start" );

	if( !g_autorecord->integer )
		return;

	// do not start autorecording if all playing clients are bots
	bool has_players = false;
	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		for( u8 i = 0; i < current_team->num_players; i++ ) {
			if( game.edicts[ current_team->player_indices[ i ] ].r.svflags & SVF_FAKECLIENT ) {
				continue;
			}

			has_players = true;
			break;
		}
	}

	if( !has_players )
		return;

	char date[ 128 ];
	Sys_FormatTime( date, sizeof( date ), "%Y-%m-%d_%H-%M" );

	snprintf( level.autorecord_name, sizeof( level.autorecord_name ), "%s_%s_auto%04i", date, sv.mapname, RandomUniform( &svs.rng, 1, 10000 ) );

	Cbuf_ExecuteText( EXEC_APPEND, va( "serverrecord %s\n", level.autorecord_name ) );
}

void G_Match_Autorecord_AltStart() {
	G_Match_SetAutorecordState( "altstart" );
}

void G_Match_Autorecord_Stop() {
	G_Match_SetAutorecordState( "stop" );

	if( g_autorecord->integer ) {
		// stop it
		Cbuf_ExecuteText( EXEC_APPEND, "serverrecordstop 1\n" );

		// check if we wanna delete some
		if( g_autorecord_maxdemos->integer > 0 ) {
			Cbuf_ExecuteText( EXEC_APPEND, "serverrecordpurge\n" );
		}
	}
}

void G_Match_Autorecord_Cancel() {
	G_Match_SetAutorecordState( "cancel" );

	if( g_autorecord->integer ) {
		Cbuf_ExecuteText( EXEC_APPEND, "serverrecordcancel 1\n" );
	}
}

static void G_Match_CheckStateAbort() {
	bool any = false;
	bool enough;

	if( server_gs.gameState.match_state >= MatchState_PostMatch || level.gametype.matchAbortDisabled ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
		return;
	}

	if( level.gametype.isTeamBased ) {
		int team, emptyteams = 0;

		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( server_gs.gameState.teams[ team ].num_players == 0 ) {
				emptyteams++;
			}
			else {
				any = true;
			}
		}

		enough = emptyteams == 0;
	}
	else {
		SyncTeamState * team_players = &server_gs.gameState.teams[ TEAM_PLAYERS ];
		enough = team_players->num_players > 1;
		any = team_players->num_players > 0;
	}

	// if waiting, turn on match states when enough players joined
	if( GS_MatchWaiting( &server_gs ) && enough ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
	}
	// turn off active match states if not enough players left
	else if( server_gs.gameState.match_state == MatchState_Warmup && !enough && server_gs.gameState.match_duration ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	} else if( server_gs.gameState.match_state == MatchState_Countdown && !enough ) {
		if( any ) {
			G_ClearCenterPrint( NULL );
		}
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MatchState_Warmup );
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	}
	// match running, but not enough players left
	else if( server_gs.gameState.match_state == MatchState_Playing && !enough ) {
		if( any ) {
			G_PrintMsg( NULL, "Not enough players left. Match aborted.\n" );
			G_CenterPrintMsg( NULL, "MATCH ABORTED" );
		}
		G_EndMatch();
	}
}

void G_Match_LaunchState( int matchState ) {
	static bool advance_queue = false;

	// give the gametype a chance to refuse the state change, or to set up things for it
	if( !GT_asCallMatchStateFinished( matchState ) ) {
		return;
	}

	G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );

	if( matchState == MatchState_PostMatch ) {
		level.finalMatchDuration = svs.gametime - server_gs.gameState.match_state_start_time;
	}

	switch( matchState ) {
		default:
		case MatchState_Warmup:
		{
			advance_queue = false;

			server_gs.gameState.match_state = MatchState_Warmup;
			server_gs.gameState.match_duration = (int64_t)( Abs( g_warmup_timelimit->value * 60 ) * 1000 );
			server_gs.gameState.match_state_start_time = svs.gametime;

			break;
		}

		case MatchState_Countdown:
		{
			advance_queue = true;

			server_gs.gameState.match_state = MatchState_Countdown;
			server_gs.gameState.match_duration = 5000;
			server_gs.gameState.match_state_start_time = svs.gametime;

			break;
		}

		case MatchState_Playing:
		{
			// ch : should clear some statcollection memory from warmup?

			advance_queue = true; // shouldn't be needed here

			server_gs.gameState.match_state = MatchState_Playing;
			server_gs.gameState.match_duration = 0;
			server_gs.gameState.match_state_start_time = svs.gametime;
		}
		break;

		case MatchState_PostMatch:
		{
			server_gs.gameState.match_state = MatchState_PostMatch;
			server_gs.gameState.match_duration = 4000; // postmatch time in seconds
			server_gs.gameState.match_state_start_time = svs.gametime;

			G_Timeout_Reset();
			level.forceExit = false;
		}
		break;

		case MatchState_WaitExit:
		{
			if( advance_queue ) {
				G_Teams_AdvanceChallengersQueue();
				advance_queue = true;
			}

			server_gs.gameState.match_state = MatchState_WaitExit;
			server_gs.gameState.match_duration = 3000;
			server_gs.gameState.match_state_start_time = svs.gametime;

			level.exitNow = false;
		}
		break;
	}

	// give the gametype the chance to setup for the new state
	GT_asCallMatchStateStarted();
}

bool G_Match_ScorelimitHit() {
	edict_t *e;

	if( server_gs.gameState.match_state != MatchState_Playing ) {
		return false;
	}

	if( g_scorelimit->integer ) {
		if( !level.gametype.isTeamBased ) {
			for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
				if( !e->r.inuse ) {
					continue;
				}

				if( G_ClientGetStats( e )->score >= g_scorelimit->integer ) {
					return true;
				}
			}
		} else {
			u8 high_score = Max2( server_gs.gameState.teams[ TEAM_ALPHA ].score, server_gs.gameState.teams[ TEAM_BETA ].score );
			if( int( high_score ) >= g_scorelimit->integer )
				return true;
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
	level.forceExit = true;
	G_Match_LaunchState( MatchState_PostMatch );
}

void G_Match_CheckReadys() {
	if( server_gs.gameState.match_state != MatchState_Warmup ) {
		return;
	}

	int teamsready = 0;
	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		int readys = 0;
		int notreadys = 0;
		SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		for( u8 i = 0; i < current_team->num_players; i++ ) {
			const edict_t * e = game.edicts + current_team->player_indices[ i ];

			if( !e->r.inuse ) {
				continue;
			}
			if( e->s.team == TEAM_SPECTATOR ) { //ignore spectators
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

	// everyone has commited
	bool allready;
	if( level.gametype.isTeamBased ) {
		allready = teamsready == GS_MAX_TEAMS - TEAM_ALPHA;
	}
	else {
		allready = teamsready == 1;
	}

	if( allready ) {
		G_PrintMsg( NULL, "All players are ready. Match starting!\n" );
		G_Match_LaunchState( MatchState_Countdown );
	}
}

void G_Match_Ready( edict_t *ent ) {
	if( ( ent->r.svflags & SVF_FAKECLIENT ) && level.ready[PLAYERNUM( ent )] ) {
		return;
	}

	if( ent->s.team == TEAM_SPECTATOR ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Warmup ) {
		if( !( ent->r.svflags & SVF_FAKECLIENT ) ) {
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

	G_PrintMsg( NULL, "%s is ready!\n", ent->r.client->netname );

	G_Match_CheckReadys();
}

void G_Match_NotReady( edict_t *ent ) {
	if( ent->s.team == TEAM_SPECTATOR ) {
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

	G_PrintMsg( NULL, "%s is no longer ready.\n", ent->r.client->netname );
}

void G_Match_ToggleReady( edict_t *ent ) {
	if( !level.ready[PLAYERNUM( ent )] ) {
		G_Match_Ready( ent );
	} else {
		G_Match_NotReady( ent );
	}
}

void G_Match_RemoveProjectiles( edict_t *owner ) {
	edict_t *ent;

	for( ent = game.edicts + server_gs.maxclients; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( ent->r.inuse && !ent->r.client && ent->r.svflags & SVF_PROJECTILE && ent->r.solid != SOLID_NOT &&
			( owner == NULL || ent->r.owner->s.number == owner->s.number ) ) {
			G_FreeEdict( ent );
		}
	}
}

void G_Match_FreeBodyQueue() {
	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		edict_t * ent = &game.edicts[ i ];
		if( ent->r.inuse && ent->s.type == ET_CORPSE ) {
			G_FreeEdict( ent );
		}
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

	// check sanity of g_numbots
	if( g_numbots->integer < 0 ) {
		Cvar_Set( "g_numbots", "0" );
	}

	if( g_numbots->integer > server_gs.maxclients ) {
		Cvar_Set( "g_numbots", va( "%i", server_gs.maxclients ) );
	}

	int desiredNumBots = g_numbots->integer;
	if( desiredNumBots < game.numBots ) {
		for( edict_t *ent = game.edicts + server_gs.maxclients; PLAYERNUM( ent ) >= 0; ent-- ) {
			if( !ent->r.inuse || !( ent->r.svflags & SVF_FAKECLIENT ) ) {
				continue;
			}
			PF_DropClient( ent, DROP_TYPE_GENERAL, NULL );
			game.numBots--;
			break;
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

	minute = (int)( level.time * 0.001 / 60.0f );
	if( lastminute == minute ) {
		return false;
	}

	lastminute = minute;
	return true;
}

static void G_CheckEvenTeam() {
	int max = 0;
	int min = server_gs.maxclients + 1;
	int uneven = TEAM_SPECTATOR;

	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		return;
	}

	if( !level.gametype.isTeamBased ) {
		return;
	}

	if( g_teams_allow_uneven->integer ) {
		return;
	}

	for( int i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ i ];
		if( max < current_team->num_players ) {
			max = current_team->num_players;
			uneven = i;
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

void G_Gametype_ScoreEvent( gclient_t *client, const char *score_event, const char *args ) {
	if( !score_event || !score_event[0] ) {
		return;
	}

	GT_asCallScoreEvent( client, score_event, args );
}

void G_RunGametype() {
	ZoneScoped;

	G_Teams_ExecuteChallengersQueue();
	G_Teams_UpdateMembersList();
	G_Match_CheckStateAbort();

	GT_asCallThinkRules();

	if( G_EachNewSecond() ) {
		G_CheckNumBots();
	}

	if( G_EachNewMinute() ) {
		G_CheckEvenTeam();
	}

	G_asGarbageCollect( false );
}

//======================================================
//		Game type registration
//======================================================

void G_Gametype_SetDefaults() {
	level.gametype.isTeamBased = false;
	level.gametype.hasChallengersQueue = false;
	level.gametype.hasChallengersRoulette = false;

	level.gametype.countdownEnabled = false;
	level.gametype.matchAbortDisabled = false;
	level.gametype.shootingDisabled = false;
	level.gametype.removeInactivePlayers = true;
	level.gametype.selfDamage = true;
}

// this is pretty dirty, parse the first entity and grab the gametype key
// do no validation, G_SpawnEntities will catch it
static bool IsGladiatorMap() {
	const char * entities = CM_EntityString( svs.cms );
	ParseToken( &entities, Parse_DontStopOnNewLine ); // {

	while( true ) {
		Span< const char > key = ParseToken( &entities, Parse_DontStopOnNewLine );
		Span< const char > value = ParseToken( &entities, Parse_DontStopOnNewLine );

		if( entities == NULL || key == "}" )
			break;

		if( key == "gametype" ) {
			return value == "gladiator";
		}
	}

	return false;
}

void G_Gametype_Init() {
	// get the match cvars too
	g_warmup_timelimit = Cvar_Get( "g_warmup_timelimit", "5", CVAR_ARCHIVE );
	g_match_extendedtime = Cvar_Get( "g_match_extendedtime", "2", CVAR_ARCHIVE );

	// game settings
	g_scorelimit = Cvar_Get( "g_scorelimit", "10", CVAR_ARCHIVE );

	const char * gt = IsGladiatorMap() ? "gladiator" : "bomb";

	G_InitChallengersQueue();

	G_CheckCvars();

	G_Gametype_SetDefaults();

	if( !GT_asLoadScript( gt ) ) {
#if PUBLIC_BUILD
		Sys_Error( "Failed to load %s", gt );
#endif
	}
}
