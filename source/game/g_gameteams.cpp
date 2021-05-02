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


//==========================================================
//					Teams
//==========================================================

cvar_t *g_teams_maxplayers;
cvar_t *g_teams_allow_uneven;
cvar_t *g_teams_autojoin;

/*
* G_Teams_Init
*/
void G_Teams_Init() {
	edict_t *ent;

	g_teams_maxplayers = Cvar_Get( "g_teams_maxplayers", "0", CVAR_ARCHIVE );
	g_teams_allow_uneven = Cvar_Get( "g_teams_allow_uneven", "1", CVAR_ARCHIVE );
	g_teams_autojoin = Cvar_Get( "g_teams_autojoin", "1", CVAR_ARCHIVE );

	// clear up team lists
	memset( server_gs.gameState.teams, 0, sizeof( server_gs.gameState.teams ) );

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		if( ent->r.inuse ) {
			memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
			memset( &ent->r.client->resp, 0, sizeof( ent->r.client->resp ) );

			ent->s.team = ent->r.client->team = TEAM_SPECTATOR;
			G_GhostClient( ent );
			ent->movetype = MOVETYPE_NOCLIP; // allow freefly
			ent->r.client->teamstate.timeStamp = level.time;
			ent->r.client->resp.timeStamp = level.time;
		}
	}
}

static int G_Teams_CompareMembers( const void *a, const void *b ) {
	edict_t *edict_a = game.edicts + *(int *)a;
	edict_t *edict_b = game.edicts + *(int *)b;
	int score_a = G_ClientGetState( edict_a )->score;
	int score_b = G_ClientGetState( edict_b )->score;
	int result = score_b - score_a;
	if( !result ) {
		result = Q_stricmp( edict_a->r.client->netname, edict_b->r.client->netname );
	}
	if( !result ) {
		result = ENTNUM( edict_a ) - ENTNUM( edict_b );
	}
	return result;
}

/*
* G_Teams_UpdateMembersList
* It's better to count the list in detail once per fame, than
* creating a quick list each time we need it.
*/
void G_Teams_UpdateMembersList() {
	edict_t *ent;
	int i, team;

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		GetTeam( team ).numplayers = 0;
		GetTeam( team ).numalive = 0;

		//create a temp list with the clients inside this team
		for( i = 0, ent = game.edicts + 1; i < server_gs.maxclients; i++, ent++ ) {
			if( !ent->r.client || ( PF_GetClientState( PLAYERNUM( ent ) ) < CS_CONNECTED ) ) {
				continue;
			}

			if( ent->s.team == team ) {
				GetTeam( team ).playerIndices[ GetTeam( team ).numplayers++ ] = ENTNUM( ent );
				if( G_ClientGetState( ent )->alive ) {
					GetTeam( team ).numalive++;
				}
			}
		}

		qsort( GetTeam( team ).playerIndices, GetTeam( team ).numplayers, sizeof( GetTeam( team ).playerIndices[0] ), G_Teams_CompareMembers );
	}
}

/*
* G_Teams_SetTeam - sets clients team without any checking
*/
void G_Teams_SetTeam( edict_t *ent, int team ) {
	assert( ent && ent->r.inuse && ent->r.client );
	assert( team >= TEAM_SPECTATOR && team < GS_MAX_TEAMS );

	if( ent->r.client->team != TEAM_SPECTATOR && team != TEAM_SPECTATOR ) {
		// keep scores when switching between non-spectating teams
		int64_t timeStamp = ent->r.client->teamstate.timeStamp;
		memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
		ent->r.client->teamstate.timeStamp = timeStamp;
	} else {
		// clear scores at changing team
		memset( G_ClientGetState( ent ), 0, sizeof( SyncScoreboardPlayer ) );
		memset( G_ClientGetStats( ent ), 0, sizeof( score_stats_t ) );
		memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
		ent->r.client->teamstate.timeStamp = level.time;
	}

	if( ent->r.client->team == TEAM_SPECTATOR || team == TEAM_SPECTATOR ) {
		level.ready[PLAYERNUM( ent )] = false;
	}

	ent->r.client->team = team;

	G_ClientRespawn( ent, true ); // make ghost using G_ClientRespawn so team is updated at ghosting
	G_SpawnQueue_AddClient( ent );
}

enum
{
	ER_TEAM_OK,
	ER_TEAM_INVALID,
	ER_TEAM_FULL,
	ER_TEAM_MATCHSTATE,
	ER_TEAM_CHALLENGERS,
	ER_TEAM_UNEVEN
};

static bool G_Teams_CanKeepEvenTeam( int leaving, int joining ) {
	int max = 0;
	int min = server_gs.maxclients + 1;
	int numplayers;
	int i;

	for( i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
		numplayers = GetTeam( i ).numplayers;
		if( i == leaving ) {
			numplayers--;
		}
		if( i == joining ) {
			numplayers++;
		}

		if( max < numplayers ) {
			max = numplayers;
		}
		if( min > numplayers ) {
			min = numplayers;
		}
	}

	return GetTeam( joining ).numplayers + 1 == min || Abs( max - min ) <= 1;
}

/*
* G_GameTypes_DenyJoinTeam
*/
static int G_GameTypes_DenyJoinTeam( edict_t *ent, int team ) {
	if( team < 0 || team >= GS_MAX_TEAMS ) {
		Com_Printf( "WARNING: 'G_GameTypes_CanJoinTeam' parsing a unrecognized team value\n" );
		return ER_TEAM_INVALID;
	}

	if( team == TEAM_SPECTATOR ) {
		return ER_TEAM_OK;
	}

	if( GS_MatchState( &server_gs ) > MATCH_STATE_PLAYTIME ) {
		return ER_TEAM_MATCHSTATE;
	}

	// waiting for chanllengers queue to be executed
	if( GS_HasChallengers( &server_gs ) && svs.realtime < level.spawnedTimeStamp + (int64_t)( G_CHALLENGERS_MIN_JOINTEAM_MAPTIME + game.snapFrameTime ) ) {
		return ER_TEAM_CHALLENGERS;
	}

	// force eveyone to go through queue so things work on map change
	if( GS_HasChallengers( &server_gs ) && !ent->r.client->queueTimeStamp ) {
		return ER_TEAM_CHALLENGERS;
	}

	if( !level.gametype.isTeamBased ) {
		return team == TEAM_PLAYERS ? ER_TEAM_OK : ER_TEAM_INVALID;
	}

	if( team != TEAM_ALPHA && team != TEAM_BETA )
		return ER_TEAM_INVALID;

	// see if team is full
	int count = GetTeam( team ).numplayers;

	if( ( count + 1 > level.gametype.maxPlayersPerTeam &&
		  level.gametype.maxPlayersPerTeam > 0 ) ||
		( count + 1 > g_teams_maxplayers->integer &&
		  g_teams_maxplayers->integer > 0 ) ) {
		return ER_TEAM_FULL;
	}

	if( !g_teams_allow_uneven->integer && !G_Teams_CanKeepEvenTeam( ent->s.team, team ) ) {
		return ER_TEAM_UNEVEN;
	}

	return ER_TEAM_OK;
}

/*
* G_Teams_JoinTeam - checks that client can join the given team and then joins it
*/
bool G_Teams_JoinTeam( edict_t *ent, int team ) {
	int error;

	G_Teams_UpdateMembersList(); // make sure we have up-to-date data

	if( !ent->r.client ) {
		return false;
	}

	if( ( error = G_GameTypes_DenyJoinTeam( ent, team ) ) ) {
		if( error == ER_TEAM_INVALID ) {
			G_PrintMsg( ent, "Can't join %s\n", GS_TeamName( team ) );
		} else if( error == ER_TEAM_CHALLENGERS ) {
			G_Teams_JoinChallengersQueue( ent );
		} else if( error == ER_TEAM_FULL ) {
			G_PrintMsg( ent, "Team %s is FULL\n", GS_TeamName( team ) );
			G_Teams_JoinChallengersQueue( ent );
		} else if( error == ER_TEAM_MATCHSTATE ) {
			G_PrintMsg( ent, "Can't join %s at this moment\n", GS_TeamName( team ) );
		} else if( error == ER_TEAM_UNEVEN ) {
			G_PrintMsg( ent, "Can't join %s because of uneven teams\n", GS_TeamName( team ) ); // FIXME: need more suitable message :P
			G_Teams_JoinChallengersQueue( ent );
		}
		return false;
	}

	//ok, can join, proceed
	G_Teams_SetTeam( ent, team );
	return true;
}

/*
* G_Teams_JoinAnyTeam - find us a team since we are too lazy to do ourselves
*/
bool G_Teams_JoinAnyTeam( edict_t *ent, bool silent ) {
	int best_numplayers = server_gs.maxclients + 1;
	u8 best_score;
	int i, team = -1;
	bool wasinqueue = ( ent->r.client->queueTimeStamp != 0 );

	G_Teams_UpdateMembersList(); // make sure we have up-to-date data

	//depending on the gametype, of course
	if( !level.gametype.isTeamBased ) {
		if( ent->s.team == TEAM_PLAYERS ) {
			if( !silent ) {
				G_PrintMsg( ent, "You are already in %s team\n", GS_TeamName( TEAM_PLAYERS ) );
			}
			return false;
		}
		if( G_Teams_JoinTeam( ent, TEAM_PLAYERS ) ) {
			if( !silent ) {
				G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
			}
		}
		return true;
	} else {   //team based
		//find the available team with smaller player count or worse score
		for( i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
			if( G_GameTypes_DenyJoinTeam( ent, i ) ) {
				continue;
			}

			u8 team_score = server_gs.gameState.teams[ i ].score;
			if( team == -1 || GetTeam( i ).numplayers < best_numplayers || ( GetTeam( i ).numplayers == best_numplayers && team_score < best_score ) ) {
				best_numplayers = GetTeam( i ).numplayers;
				best_score = team_score;
				team = i;
			}
		}

		if( team == ent->s.team ) { // he is at the right team
			if( !silent ) {
				G_PrintMsg( ent, "%sCouldn't find a better team than team %s.\n",
							S_COLOR_WHITE, GS_TeamName( ent->s.team ) );
			}
			return false;
		}

		if( team != -1 ) {
			if( G_Teams_JoinTeam( ent, team ) ) {
				if( !silent ) {
					G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
				}
				return true;
			}
		}
		if( GS_MatchState( &server_gs ) <= MATCH_STATE_PLAYTIME && !silent ) {
			G_Teams_JoinChallengersQueue( ent );
		}
	}

	// don't print message if we joined the queue
	if( !silent && ( !GS_HasChallengers( &server_gs ) || wasinqueue || !ent->r.client->queueTimeStamp ) ) {
		G_PrintMsg( ent, "You can't join the game now\n" );
	}

	return false;
}

/*
* G_Teams_Join_Cmd
*/
void G_Teams_Join_Cmd( edict_t *ent ) {
	if( !ent->r.client || PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	const char * t = Cmd_Argv( 1 );
	if( !t || *t == 0 ) {
		G_Teams_JoinAnyTeam( ent, false );
		return;
	}

	int team = GS_TeamFromName( t );
	if( team != -1 ) {
		if( team == TEAM_SPECTATOR ) { // special handling for spectator team
			Cmd_ChaseCam_f( ent );
			return;
		}
		if( team == ent->s.team ) {
			G_PrintMsg( ent, "You are already in %s team\n", GS_TeamName( team ) );
			return;
		}
		if( G_Teams_JoinTeam( ent, team ) ) {
			G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
			return;
		}
	} else {
		G_PrintMsg( ent, "No such team.\n" );
		return;
	}
}

//======================================================================
//
// CHALLENGERS QUEUE
//
//======================================================================

static int G_Teams_ChallengersQueueCmp( const edict_t **pe1, const edict_t **pe2 ) {
	const edict_t *e1 = *pe1, *e2 = *pe2;

	if( e1->r.client->queueTimeStamp > e2->r.client->queueTimeStamp ) {
		return 1;
	}
	if( e2->r.client->queueTimeStamp > e1->r.client->queueTimeStamp ) {
		return -1;
	}
	return random_uniform( &svs.rng, 0, 2 ) == 0 ? -1 : 1;
}

/*
* G_Teams_ChallengersQueue
*
* Returns a NULL-terminated list of challengers or NULL if
* there are no challengers.
*/
edict_t **G_Teams_ChallengersQueue() {
	int num_challengers = 0;
	static edict_t *challengers[MAX_CLIENTS + 1];
	edict_t *e;

	// fill the challengers into array, then sort
	for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
		if( !e->r.inuse || !e->r.client || e->s.team != TEAM_SPECTATOR ) {
			continue;
		}
		if( PF_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
			continue;
		}

		gclient_t * cl = e->r.client;
		if( cl->connecting || !cl->queueTimeStamp ) {
			continue;
		}

		challengers[num_challengers++] = e;
	}

	if( !num_challengers ) {
		return NULL;
	}

	// NULL terminator
	challengers[num_challengers] = NULL;

	// sort challengers by the queue time in ascending order
	if( num_challengers > 1 ) {
		qsort( challengers, num_challengers, sizeof( *challengers ), ( int ( * )( const void *, const void * ) )G_Teams_ChallengersQueueCmp );
	}

	return challengers;
}

/*
* G_Teams_ExecuteChallengersQueue
*/
void G_Teams_ExecuteChallengersQueue() {
	edict_t *ent;
	edict_t **challengers;
	bool restartmatch = false;

	// Medar fixme: this is only really makes sense, if playerlimit per team is one
	if( GS_MatchState( &server_gs ) == MATCH_STATE_PLAYTIME ) {
		return;
	}

	if( !GS_HasChallengers( &server_gs ) ) {
		return;
	}

	if( svs.realtime < level.spawnedTimeStamp + G_CHALLENGERS_MIN_JOINTEAM_MAPTIME ) {
		static int time, lasttime;
		time = (int)( ( G_CHALLENGERS_MIN_JOINTEAM_MAPTIME - ( svs.realtime - level.spawnedTimeStamp ) ) * 0.001 );
		if( lasttime && time == lasttime ) {
			return;
		}
		lasttime = time;
		if( lasttime ) {
			G_CenterPrintMsg( NULL, "Waiting... %i", lasttime );
		}
		return;
	}

	// pick players in join order and try to put them in the
	// game until we get the first refused one.
	challengers = G_Teams_ChallengersQueue();
	if( challengers ) {
		int i;

		for( i = 0; challengers[i]; i++ ) {
			ent = challengers[i];
			if( !G_Teams_JoinAnyTeam( ent, true ) ) {
				break;
			}

			// if we successfully execute the challengers queue during the countdown, revert to warmup
			if( GS_MatchState( &server_gs ) == MATCH_STATE_COUNTDOWN ) {
				restartmatch = true;
			}
		}
	}

	if( restartmatch == true ) {
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MATCH_STATE_WARMUP );
	}
}

/*
*
* G_Teams_BestScoreBelow
*/
static edict_t *G_Teams_BestScoreBelow( int maxscore ) {
	int team, i;
	edict_t *e, *best = NULL;
	int bestScore = -9999999;

	if( level.gametype.isTeamBased ) {
		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			for( i = 0; i < GetTeam( team ).numplayers; i++ ) {
				e = game.edicts + GetTeam( team ).playerIndices[i];
				if( G_ClientGetState( e )->score > bestScore &&
					G_ClientGetState( e )->score <= maxscore
					&& !e->r.client->queueTimeStamp ) {
					bestScore = G_ClientGetState( e )->score;
					best = e;
				}
			}
		}
	} else {
		for( i = 0; i < GetTeam( TEAM_PLAYERS ).numplayers; i++ ) {
			e = game.edicts + GetTeam( TEAM_PLAYERS ).playerIndices[i];
			if( G_ClientGetState( e )->score > bestScore &&
				G_ClientGetState( e )->score <= maxscore
				&& !e->r.client->queueTimeStamp ) {
				bestScore = G_ClientGetState( e )->score;
				best = e;
			}
		}
	}

	return best;
}

/*
* G_Teams_AdvanceChallengersQueue
*/
void G_Teams_AdvanceChallengersQueue() {
	int i, team, loserscount, winnerscount, playerscount = 0;
	int maxscore = 999999;
	edict_t *won, *e;
	int START_TEAM = TEAM_PLAYERS, END_TEAM = TEAM_PLAYERS + 1;

	if( !GS_HasChallengers( &server_gs ) ) {
		return;
	}

	G_Teams_UpdateMembersList();

	if( level.gametype.isTeamBased ) {
		START_TEAM = TEAM_ALPHA;
		END_TEAM = GS_MAX_TEAMS;
	}

	// assign new timestamps to all the players inside teams
	for( team = START_TEAM; team < END_TEAM; team++ ) {
		playerscount += GetTeam( team ).numplayers;
	}

	if( !playerscount ) {
		return;
	}

	loserscount = 0;
	if( playerscount > 1 ) {
		loserscount = (int)( playerscount / 2 );
	}
	winnerscount = playerscount - loserscount;

	// put everyone who just played out of the challengers queue
	for( team = START_TEAM; team < END_TEAM; team++ ) {
		for( i = 0; i < GetTeam( team ).numplayers; i++ ) {
			e = game.edicts + GetTeam( team ).playerIndices[i];
			e->r.client->queueTimeStamp = 0;
		}
	}

	if( !level.gametype.hasChallengersRoulette ) {
		// put (back) the best scoring players in first positions of challengers queue
		for( i = 0; i < winnerscount; i++ ) {
			won = G_Teams_BestScoreBelow( maxscore );
			if( won ) {
				maxscore = G_ClientGetState( won )->score;
				won->r.client->queueTimeStamp = 1 + ( winnerscount - i ); // never have 2 players with the same timestamp
			}
		}
	}
}

/*
* G_Teams_LeaveChallengersQueue
*/
void G_Teams_LeaveChallengersQueue( edict_t *ent ) {
	if( !GS_HasChallengers( &server_gs ) ) {
		ent->r.client->queueTimeStamp = 0;
		return;
	}

	if( ent->s.team != TEAM_SPECTATOR ) {
		return;
	}

	// exit the challengers queue
	if( ent->r.client->queueTimeStamp ) {
		ent->r.client->queueTimeStamp = 0;
		G_PrintMsg( ent, "%sYou left the challengers queue\n", S_COLOR_CYAN );
	}
}

/*
* G_Teams_JoinChallengersQueue
*/
void G_Teams_JoinChallengersQueue( edict_t *ent ) {
	int pos = 0;
	edict_t *e;

	if( !GS_HasChallengers( &server_gs ) ) {
		ent->r.client->queueTimeStamp = 0;
		return;
	}

	if( ent->s.team != TEAM_SPECTATOR ) {
		return;
	}

	// enter the challengers queue
	if( !ent->r.client->queueTimeStamp ) {  // enter the line
		ent->r.client->queueTimeStamp = svs.realtime;
		for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
			if( !e->r.inuse || !e->r.client || PF_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
				continue;
			}
			if( !e->r.client->queueTimeStamp || e->s.team != TEAM_SPECTATOR ) {
				continue;
			}

			// if there are other players with the same timestamp, increase ours
			if( e->r.client->queueTimeStamp >= ent->r.client->queueTimeStamp ) {
				ent->r.client->queueTimeStamp = e->r.client->queueTimeStamp + 1;
			}
			if( e->r.client->queueTimeStamp < ent->r.client->queueTimeStamp ) {
				pos++;
			}
		}

		G_PrintMsg( ent, "%sYou entered the challengers queue in position %i\n", S_COLOR_CYAN, pos + 1 );
	}
}

void G_InitChallengersQueue() {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		game.clients[i].queueTimeStamp = 0;
	}
}

//======================================================================
//
//TEAM COMMUNICATIONS
//
//======================================================================

void G_Say_Team( edict_t *who, const char *inmsg, bool checkflood ) {
	if( who->s.team != TEAM_SPECTATOR && ( !level.gametype.isTeamBased || GS_IndividualGameType( &server_gs ) ) ) {
		Cmd_Say_f( who, false, true );
		return;
	}

	if( checkflood && CheckFlood( who, true ) ) {
		return;
	}

	char msgbuf[256];
	Q_strncpyz( msgbuf, inmsg, sizeof( msgbuf ) );

	char * msg = msgbuf;
	if( *msg == '\"' ) {
		msg[strlen( msg ) - 1] = 0;
		msg++;
	}

	if( who->s.team == TEAM_SPECTATOR ) {
		// if speccing, also check for non-team flood
		if( checkflood && CheckFlood( who, false ) ) {
			return;
		}
	}

	G_ChatMsg( NULL, who, true, "%s", msg );
}
