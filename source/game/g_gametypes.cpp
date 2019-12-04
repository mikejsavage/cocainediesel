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

g_teamlist_t teamlist[GS_MAX_TEAMS];

//==========================================================
//					Matches
//==========================================================

cvar_t *g_warmup_timelimit;
cvar_t *g_match_extendedtime;
cvar_t *g_scorelimit;

//==========================================================
//					Matches
//==========================================================

/*
* G_GetGameState
*/
game_state_t *G_GetGameState( void ) {
	return &server_gs.gameState;
}

/*
* G_Match_SetAutorecordState
*/
static void G_Match_SetAutorecordState( const char *state ) {
	trap_ConfigString( CS_AUTORECORDSTATE, state );
}

/*
* G_Match_Autorecord_Start
*/
void G_Match_Autorecord_Start( void ) {
	G_Match_SetAutorecordState( "start" );

	if( !g_autorecord->integer )
		return;

	// do not start autorecording if all playing clients are bots
	bool has_players = false;
	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		for( int i = 0; i < teamlist[team].numplayers; i++ ) {
			if( game.edicts[ teamlist[team].playerIndices[i] ].r.svflags & SVF_FAKECLIENT ) {
				continue;
			}

			has_players = true;
			break;
		}
	}

	if( !has_players )
		return;

	char datetime[17];
	time_t long_time;
	struct tm *newtime;

	// date & time
	time( &long_time );
	newtime = localtime( &long_time );

	Q_snprintfz( datetime, sizeof( datetime ), "%04d-%02d-%02d_%02d-%02d", newtime->tm_year + 1900,
				 newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min );

	// combine
	Q_snprintfz( level.autorecord_name, sizeof( level.autorecord_name ), "%s_%s_auto%04i",
				 datetime, level.mapname, (int)brandom( 1, 9999 ) );

	trap_Cmd_ExecuteText( EXEC_APPEND, va( "serverrecord %s\n", level.autorecord_name ) );
}

/*
* G_Match_Autorecord_AltStart
*/
void G_Match_Autorecord_AltStart( void ) {
	G_Match_SetAutorecordState( "altstart" );
}

/*
* G_Match_Autorecord_Stats
*/
void G_Match_Autorecord_Stats( void ) {
	edict_t *ent;

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		if( !ent->r.inuse || ent->s.team == TEAM_SPECTATOR || ( ent->r.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}
		trap_GameCmd( ent, va( "plstats \"%s\"", G_StatsMessage( ent ) ) );
	}
}

/*
* G_Match_Autorecord_Stop
*/
void G_Match_Autorecord_Stop( void ) {
	G_Match_SetAutorecordState( "stop" );

	if( g_autorecord->integer ) {
		// stop it
		trap_Cmd_ExecuteText( EXEC_APPEND, "serverrecordstop 1\n" );

		// check if we wanna delete some
		if( g_autorecord_maxdemos->integer > 0 ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "serverrecordpurge %i\n", g_autorecord_maxdemos->integer ) );
		}
	}
}

/*
* G_Match_Autorecord_Cancel
*/
void G_Match_Autorecord_Cancel( void ) {
	G_Match_SetAutorecordState( "cancel" );

	if( g_autorecord->integer ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "serverrecordcancel 1\n" );
	}
}

/*
* G_Match_CheckStateAbort
*/
static void G_Match_CheckStateAbort( void ) {
	bool any = false;
	bool enough;

	if( GS_MatchState( &server_gs ) <= MATCH_STATE_NONE || GS_MatchState( &server_gs ) >= MATCH_STATE_POSTMATCH
		|| level.gametype.matchAbortDisabled ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
		return;
	}

	if( GS_TeamBasedGametype( &server_gs ) ) {
		int team, emptyteams = 0;

		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( !teamlist[team].numplayers ) {
				emptyteams++;
			} else {
				any = true;
			}
		}

		enough = ( emptyteams == 0 );
	} else {
		enough = ( teamlist[TEAM_PLAYERS].numplayers > 1 );
		any = ( teamlist[TEAM_PLAYERS].numplayers > 0 );
	}

	// if waiting, turn on match states when enough players joined
	if( GS_MatchWaiting( &server_gs ) && enough ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
	}
	// turn off active match states if not enough players left
	else if( GS_MatchState( &server_gs ) == MATCH_STATE_WARMUP && !enough && GS_MatchDuration( &server_gs ) ) {
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	} else if( GS_MatchState( &server_gs ) == MATCH_STATE_COUNTDOWN && !enough ) {
		if( any ) {
			G_PrintMsg( NULL, "Not enough players left. Countdown aborted.\n" );
			G_CenterPrintMsg( NULL, "COUNTDOWN ABORTED" );
		}
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MATCH_STATE_WARMUP );
		G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	}
	// match running, but not enough players left
	else if( GS_MatchState( &server_gs ) == MATCH_STATE_PLAYTIME && !enough ) {
		if( any ) {
			G_PrintMsg( NULL, "Not enough players left. Match aborted.\n" );
			G_CenterPrintMsg( NULL, "MATCH ABORTED" );
		}
		G_EndMatch();
	}
}

/*
* G_Match_LaunchState
*/
void G_Match_LaunchState( int matchState ) {
	static bool advance_queue = false;

	// give the gametype a chance to refuse the state change, or to set up things for it
	if( !GT_asCallMatchStateFinished( matchState ) ) {
		return;
	}

	G_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );

	if( matchState == MATCH_STATE_POSTMATCH ) {
		level.finalMatchDuration = svs.gametime - GS_MatchStartTime( &server_gs );
	}

	switch( matchState ) {
		default:
		case MATCH_STATE_WARMUP:
		{
			advance_queue = false;
			level.forceStart = false;

			server_gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_WARMUP;
			server_gs.gameState.stats[GAMESTAT_MATCHDURATION] = (int64_t)( fabs( g_warmup_timelimit->value * 60 ) * 1000 );
			server_gs.gameState.stats[GAMESTAT_MATCHSTART] = svs.gametime;

			break;
		}

		case MATCH_STATE_COUNTDOWN:
		{
			advance_queue = true;

			server_gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_COUNTDOWN;
			server_gs.gameState.stats[GAMESTAT_MATCHDURATION] = 5000;
			server_gs.gameState.stats[GAMESTAT_MATCHSTART] = svs.gametime;

			break;
		}

		case MATCH_STATE_PLAYTIME:
		{
			// ch : should clear some statcollection memory from warmup?

			advance_queue = true; // shouldn't be needed here
			level.forceStart = false;

			server_gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_PLAYTIME;
			server_gs.gameState.stats[GAMESTAT_MATCHDURATION] = 0;
			server_gs.gameState.stats[GAMESTAT_MATCHSTART] = svs.gametime;
		}
		break;

		case MATCH_STATE_POSTMATCH:
		{
			server_gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_POSTMATCH;
			server_gs.gameState.stats[GAMESTAT_MATCHDURATION] = 4000; // postmatch time in seconds
			server_gs.gameState.stats[GAMESTAT_MATCHSTART] = svs.gametime;

			G_Timeout_Reset();
			level.teamlock = false;
			level.forceExit = false;

			G_Match_Autorecord_Stats();
		}
		break;

		case MATCH_STATE_WAITEXIT:
		{
			if( advance_queue ) {
				G_Teams_AdvanceChallengersQueue();
				advance_queue = true;
			}

			server_gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_WAITEXIT;
			server_gs.gameState.stats[GAMESTAT_MATCHDURATION] = 3000;
			server_gs.gameState.stats[GAMESTAT_MATCHSTART] = svs.gametime;

			level.exitNow = false;
		}
		break;
	}

	// give the gametype the chance to setup for the new state
	GT_asCallMatchStateStarted();
}

/*
* G_Match_ScorelimitHit
*/
bool G_Match_ScorelimitHit( void ) {
	edict_t *e;

	if( GS_MatchState( &server_gs ) != MATCH_STATE_PLAYTIME ) {
		return false;
	}

	if( g_scorelimit->integer ) {
		if( !GS_TeamBasedGametype( &server_gs ) ) {
			for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
				if( !e->r.inuse ) {
					continue;
				}

				if( e->r.client->level.stats.score >= g_scorelimit->integer ) {
					return true;
				}
			}
		} else {
			int team;

			for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
				if( teamlist[team].stats.score >= g_scorelimit->integer ) {
					return true;
				}
			}
		}
	}

	return false;
}

/*
* G_Match_TimelimitHit
*/
bool G_Match_TimelimitHit( void ) {
	// check for timelimit hit
	if( !GS_MatchDuration( &server_gs ) || svs.gametime < GS_MatchEndTime( &server_gs ) ) {
		return false;
	}

	if( GS_MatchState( &server_gs ) == MATCH_STATE_WARMUP ) {
		level.forceStart = true; // force match starting when timelimit is up, even if someone goes unready

	}
	if( GS_MatchState( &server_gs ) == MATCH_STATE_WAITEXIT ) {
		level.exitNow = true;
		return false; // don't advance into next state. The match will be restarted
	}

	return true;
}


static bool score_announcement_init = false;
static int last_leaders[MAX_CLIENTS];
static int leaders[MAX_CLIENTS];

/*
* G_IsLeading
*/
static bool G_IsLeading( edict_t *ent ) {
	int num, i;

	if( GS_TeamBasedGametype( &server_gs ) ) {
		num = ent->s.team;
	} else {
		num = PLAYERNUM( ent ) + 1;
	}

	for( i = 0; i < MAX_CLIENTS && leaders[i] != 0; i++ ) {
		if( leaders[i] == num ) {
			return true;
		}
	}

	return false;
}

/*
* G_WasLeading
*/
static bool G_WasLeading( edict_t *ent ) {
	int num, i;

	if( GS_TeamBasedGametype( &server_gs ) ) {
		num = ent->s.team;
	} else {
		num = PLAYERNUM( ent ) + 1;
	}

	for( i = 0; i < MAX_CLIENTS && last_leaders[i] != 0; i++ ) {
		if( last_leaders[i] == num ) {
			return true;
		}
	}

	return false;
}

/*
* G_Match_ScoreAnnouncement
*/
static void G_Match_ScoreAnnouncement( void ) {
	int i;
	edict_t *e, *chased;
	int num_leaders, team;

	if( !level.gametype.scoreAnnouncementEnabled ) {
		return;
	}

	num_leaders = 0;
	memset( leaders, 0, sizeof( leaders ) );

	if( GS_TeamBasedGametype( &server_gs ) ) {
		int score_max = -999999999;

		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( !teamlist[team].numplayers ) {
				continue;
			}

			if( teamlist[team].stats.score > score_max ) {
				score_max = teamlist[team].stats.score;
				leaders[0] = team;
				num_leaders = 1;
			} else if( teamlist[team].stats.score == score_max ) {
				leaders[num_leaders++] = team;
			}
		}
		leaders[num_leaders] = 0;
	} else {
		int score_max = -999999999;

		for( i = 0; i < MAX_CLIENTS && i < teamlist[TEAM_PLAYERS].numplayers; i++ ) {
			if( game.clients[teamlist[TEAM_PLAYERS].playerIndices[i] - 1].level.stats.score > score_max ) {
				score_max = game.clients[teamlist[TEAM_PLAYERS].playerIndices[i] - 1].level.stats.score;
				leaders[0] = teamlist[TEAM_PLAYERS].playerIndices[i];
				num_leaders = 1;
			} else if( game.clients[teamlist[TEAM_PLAYERS].playerIndices[i] - 1].level.stats.score == score_max ) {
				leaders[num_leaders++] = teamlist[TEAM_PLAYERS].playerIndices[i];
			}
		}
		leaders[num_leaders] = 0;
	}

	if( !score_announcement_init ) {
		// copy over to last_leaders
		memcpy( last_leaders, leaders, sizeof( leaders ) );
		score_announcement_init = true;
		return;
	}

	for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
		if( !e->r.client || trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
			continue;
		}

		if( e->r.client->resp.chase.active ) {
			chased = &game.edicts[e->r.client->resp.chase.target];
		} else {
			chased = e;
		}

		// floating spectator
		if( chased->s.team == TEAM_SPECTATOR ) {
			if( !GS_TeamBasedGametype( &server_gs ) ) {
				continue;
			}

			if( last_leaders[1] == 0 && leaders[1] != 0 ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_TIED_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else if( leaders[1] == 0 && ( last_leaders[0] != leaders[0] || last_leaders[1] != 0 ) ) {
				//G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_1_to_4_TAKEN_LEAD_1_to_2,
				//	leaders[0]-1, ( rand()&1 )+1 ) ), GS_MAX_TEAMS, true, NULL );
			}
			continue;
		}

		// in the game or chasing someone who is
		if( G_WasLeading( chased ) && !G_IsLeading( chased ) ) {
			if( GS_TeamBasedGametype( &server_gs ) && !GS_IndividualGameType( &server_gs ) ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_LOST_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_LOST_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			}
		} else if( ( !G_WasLeading( chased ) || ( last_leaders[1] != 0 ) ) && G_IsLeading( chased ) && ( leaders[1] == 0 ) ) {
			if( GS_TeamBasedGametype( &server_gs ) && !GS_IndividualGameType( &server_gs ) ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_TAKEN_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TAKEN_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			}
		} else if( ( !G_WasLeading( chased ) || ( last_leaders[1] == 0 ) ) && G_IsLeading( chased ) && ( leaders[1] != 0 ) ) {
			if( GS_TeamBasedGametype( &server_gs ) && !GS_IndividualGameType( &server_gs ) ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_TIED_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TIED_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			}
		}
	}

	// copy over to last_leaders
	memcpy( last_leaders, leaders, sizeof( leaders ) );
}

/*
* G_EndMatch
*/
void G_EndMatch( void ) {
	level.forceExit = true;
	G_Match_LaunchState( MATCH_STATE_POSTMATCH );
}

/*
* G_Match_CheckReadys
*/
void G_Match_CheckReadys( void ) {
	edict_t *e;
	bool allready;
	int readys, notreadys, teamsready;
	int team, i;

	if( GS_MatchState( &server_gs ) != MATCH_STATE_WARMUP && GS_MatchState( &server_gs ) != MATCH_STATE_COUNTDOWN ) {
		return;
	}

	if( GS_MatchState( &server_gs ) == MATCH_STATE_COUNTDOWN && level.forceStart ) {
		return; // never stop countdown if we have run out of warmup_timelimit

	}
	teamsready = 0;
	for( team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		readys = notreadys = 0;
		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			e = game.edicts + teamlist[team].playerIndices[i];

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
	if( GS_TeamBasedGametype( &server_gs ) ) {
		if( teamsready == GS_MAX_TEAMS - TEAM_ALPHA ) {
			allready = true;
		} else {
			allready = false;
		}
	} else {   //ffa
		if( teamsready && teamlist[TEAM_PLAYERS].numplayers > 1 ) {
			allready = true;
		} else {
			allready = false;
		}
	}

	if( allready && GS_MatchState( &server_gs ) != MATCH_STATE_COUNTDOWN ) {
		G_PrintMsg( NULL, "All players are ready. Match starting!\n" );
		G_Match_LaunchState( MATCH_STATE_COUNTDOWN );
	} else if( !allready && GS_MatchState( &server_gs ) == MATCH_STATE_COUNTDOWN ) {
		G_PrintMsg( NULL, "Countdown aborted.\n" );
		G_CenterPrintMsg( NULL, "COUNTDOWN ABORTED" );
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MATCH_STATE_WARMUP );
	}
}

/*
* G_Match_Ready
*/
void G_Match_Ready( edict_t *ent ) {
	if( ( ent->r.svflags & SVF_FAKECLIENT ) && level.ready[PLAYERNUM( ent )] ) {
		return;
	}

	if( ent->s.team == TEAM_SPECTATOR ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( GS_MatchState( &server_gs ) != MATCH_STATE_WARMUP ) {
		if( !( ent->r.svflags & SVF_FAKECLIENT ) ) {
			G_PrintMsg( ent, "We're not in warmup.\n" );
		}
		return;
	}

	if( level.ready[PLAYERNUM( ent )] ) {
		G_PrintMsg( ent, "You are already ready.\n" );
		return;
	}

	level.ready[PLAYERNUM( ent )] = true;

	G_PrintMsg( NULL, "%s is ready!\n", ent->r.client->netname );

	G_Match_CheckReadys();
}

/*
* G_Match_NotReady
*/
void G_Match_NotReady( edict_t *ent ) {
	if( ent->s.team == TEAM_SPECTATOR ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( GS_MatchState( &server_gs ) != MATCH_STATE_WARMUP && GS_MatchState( &server_gs ) != MATCH_STATE_COUNTDOWN ) {
		G_PrintMsg( ent, "A match is not being setup.\n" );
		return;
	}

	if( !level.ready[PLAYERNUM( ent )] ) {
		G_PrintMsg( ent, "You weren't ready.\n" );
		return;
	}

	level.ready[PLAYERNUM( ent )] = false;

	G_PrintMsg( NULL, "%s is no longer ready.\n", ent->r.client->netname );

	G_Match_CheckReadys();
}

/*
* G_Match_ToggleReady
*/
void G_Match_ToggleReady( edict_t *ent ) {
	if( !level.ready[PLAYERNUM( ent )] ) {
		G_Match_Ready( ent );
	} else {
		G_Match_NotReady( ent );
	}
}

/*
* G_Match_RemoveProjectiles
*/
void G_Match_RemoveProjectiles( edict_t *owner ) {
	edict_t *ent;

	for( ent = game.edicts + server_gs.maxclients; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( ent->r.inuse && !ent->r.client && ent->r.svflags & SVF_PROJECTILE && ent->r.solid != SOLID_NOT &&
			( owner == NULL || ent->r.owner->s.number == owner->s.number ) ) {
			G_FreeEdict( ent );
		}
	}
}

/*
* G_Match_FreeBodyQueue
*/
void G_Match_FreeBodyQueue( void ) {
	edict_t *ent;
	int i;

	ent = &game.edicts[server_gs.maxclients + 1];
	for( i = 0; i < BODY_QUEUE_SIZE; ent++, i++ ) {
		if( !ent->r.inuse ) {
			continue;
		}

		if( ent->classname && !Q_stricmp( ent->classname, "body" ) ) {
			GClip_UnlinkEntity( ent );

			ent->deadflag = DEAD_NO;
			ent->movetype = MOVETYPE_NONE;
			ent->r.solid = SOLID_NOT;
			ent->r.svflags = SVF_NOCLIENT;

			ent->s.type = ET_GENERIC;
			ent->s.modelindex = 0;
			ent->s.sound = 0;
			ent->s.effects = 0;

			ent->takedamage = DAMAGE_NO;
			ent->flags |= FL_NO_KNOCKBACK;

			GClip_LinkEntity( ent );
		}
	}

	level.body_que = 0;
}


//======================================================
//		Game types

/*
* G_EachNewSecond
*/
static bool G_EachNewSecond( void ) {
	static int lastsecond;
	static int second;

	second = (int)( level.time * 0.001 );
	if( lastsecond == second ) {
		return false;
	}

	lastsecond = second;
	return true;
}

/*
* G_CheckNumBots
*/
static void G_CheckNumBots( void ) {
	if( level.spawnedTimeStamp + 5000 > svs.realtime ) {
		return;
	}

	// check sanity of g_numbots
	if( g_numbots->integer < 0 ) {
		trap_Cvar_Set( "g_numbots", "0" );
	}

	if( g_numbots->integer > server_gs.maxclients ) {
		trap_Cvar_Set( "g_numbots", va( "%i", server_gs.maxclients ) );
	}

	int desiredNumBots = g_numbots->integer;
	if( desiredNumBots < game.numBots ) {
		for( edict_t *ent = game.edicts + server_gs.maxclients; PLAYERNUM( ent ) >= 0; ent-- ) {
			if( !ent->r.inuse || !( ent->r.svflags & SVF_FAKECLIENT ) ) {
				continue;
			}
			trap_DropClient( ent, DROP_TYPE_GENERAL, NULL );
			game.numBots--;
			break;
		}
	}
	else if( desiredNumBots > game.numBots ) {
		for( edict_t *ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients && game.numBots < desiredNumBots; ent++ ) {
			if( !ent->r.inuse && trap_GetClientState( PLAYERNUM( ent ) ) == CS_FREE ) {
				AI_SpawnBot();
			}
		}
	}
}

/*
* G_EachNewMinute
*/
static bool G_EachNewMinute( void ) {
	static int lastminute;
	static int minute;

	minute = (int)( level.time * 0.001 / 60.0f );
	if( lastminute == minute ) {
		return false;
	}

	lastminute = minute;
	return true;
}

/*
* G_CheckEvenTeam
*/
static void G_CheckEvenTeam( void ) {
	int max = 0;
	int min = server_gs.maxclients + 1;
	int uneven_team = TEAM_SPECTATOR;
	int i;

	if( GS_MatchState( &server_gs ) >= MATCH_STATE_POSTMATCH ) {
		return;
	}

	if( !GS_TeamBasedGametype( &server_gs ) ) {
		return;
	}

	if( g_teams_allow_uneven->integer ) {
		return;
	}

	for( i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
		if( max < teamlist[i].numplayers ) {
			max = teamlist[i].numplayers;
			uneven_team = i;
		}
		if( min > teamlist[i].numplayers ) {
			min = teamlist[i].numplayers;
		}
	}

	if( max - min > 1 ) {
		for( i = 0; i < teamlist[uneven_team].numplayers; i++ ) {
			edict_t *e = game.edicts + teamlist[uneven_team].playerIndices[i];
			if( !e->r.inuse ) {
				continue;
			}
			G_CenterPrintMsg( e, "Teams are uneven. Please switch into another team." ); // FIXME: need more suitable message :P
			G_PrintMsg( e, "%sTeams are uneven. Please switch into another team.\n", S_COLOR_CYAN ); // FIXME: need more suitable message :P
		}

		// FIXME: switch team forcibly?
	}
}

/*
* G_Gametype_ScoreEvent
*/
void G_Gametype_ScoreEvent( gclient_t *client, const char *score_event, const char *args ) {
	if( !score_event || !score_event[0] ) {
		return;
	}

	GT_asCallScoreEvent( client, score_event, args );
}

/*
* G_RunGametype
*/
void G_RunGametype( void ) {
	G_Teams_ExecuteChallengersQueue();
	G_Teams_UpdateMembersList();
	G_Match_CheckStateAbort();

	G_UpdateScoreBoardMessages();

	GT_asCallThinkRules();

	if( G_EachNewSecond() ) {
		G_CheckNumBots();
	}

	if( G_EachNewMinute() ) {
		G_CheckEvenTeam();
	}

	G_Match_ScoreAnnouncement();

	G_asGarbageCollect( false );
}

//======================================================
//		Game type registration
//======================================================

/*
* G_Gametype_SetDefaults
*/
void G_Gametype_SetDefaults( void ) {
	level.gametype.isTeamBased = false;
	level.gametype.isRace = false;
	level.gametype.hasChallengersQueue = false;
	level.gametype.hasChallengersRoulette = false;
	level.gametype.maxPlayersPerTeam = 0;

	level.gametype.readyAnnouncementEnabled = false;
	level.gametype.scoreAnnouncementEnabled = false;
	level.gametype.countdownEnabled = false;
	level.gametype.matchAbortDisabled = false;
	level.gametype.shootingDisabled = false;
	level.gametype.infiniteAmmo = false;
	level.gametype.canForceModels = true;
	level.gametype.customDeadBodyCam = false;
	level.gametype.removeInactivePlayers = true;
	level.gametype.selfDamage = true;

	level.gametype.spawnpointRadius = 64;
}

// this is pretty dirty, parse the first entity and grab the gametype key
// do no validation, G_SpawnEntities will catch it
static bool IsGladiatorMap() {
	const char * entities = level.mapString;
	COM_Parse( &entities ); // {

	while( true ) {
		char key[ MAX_TOKEN_CHARS ];
		COM_Parse_r( key, sizeof( key ), &entities );

		char value[ MAX_TOKEN_CHARS ];
		COM_Parse_r( value, sizeof( value ), &entities );

		if( entities == NULL || strcmp( key, "}" ) == 0 )
			break;

		if( strcmp( key, "gametype" ) == 0 )
			return strcmp( value, "gladiator" ) == 0;
	}

	return false;
}

/*
* G_Gametype_Init
*/
void G_Gametype_Init( void ) {
	// get the match cvars too
	g_warmup_timelimit = trap_Cvar_Get( "g_warmup_timelimit", "5", CVAR_ARCHIVE );
	g_match_extendedtime = trap_Cvar_Get( "g_match_extendedtime", "2", CVAR_ARCHIVE );

	// game settings
	g_scorelimit = trap_Cvar_Get( "g_scorelimit", "10", CVAR_ARCHIVE );

	const char * gt = IsGladiatorMap() ? "gladiator" : "bomb";

	G_InitChallengersQueue();

	G_CheckCvars();

	G_Gametype_SetDefaults();

	if( !GT_asLoadScript( gt ) ) {
		G_Error( "Failed to load %s", gt );
	}

	trap_ConfigString( CS_GAMETYPENAME, gt );
}
