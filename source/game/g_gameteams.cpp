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
#include <algorithm>

cvar_t *g_teams_maxplayers;
cvar_t *g_teams_allow_uneven;

void G_Teams_Init() {
	g_teams_maxplayers = Cvar_Get( "g_teams_maxplayers", "0", CVAR_ARCHIVE );
	g_teams_allow_uneven = Cvar_Get( "g_teams_allow_uneven", "1", CVAR_ARCHIVE );

	memset( server_gs.gameState.teams, 0, sizeof( server_gs.gameState.teams ) );
}

static bool G_Teams_CompareMembers( int a, int b ) {
	edict_t *edict_a = game.edicts + a;
	edict_t *edict_b = game.edicts + b;
	int result = G_ClientGetStats( edict_a )->score - G_ClientGetStats( edict_b )->score;
	if( !result ) {
		result = Q_stricmp( edict_a->r.client->netname, edict_b->r.client->netname );
	}
	if( !result ) {
		result = ENTNUM( edict_a ) - ENTNUM( edict_b );
	}
	return result > 0;
}

/*
* G_Teams_UpdateMembersList
* It's better to count the list in detail once per fame, than
* creating a quick list each time we need it.
*/
void G_Teams_UpdateMembersList() {
	for( int team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		current_team->num_players = 0;

		for( int i = 0; i < server_gs.maxclients; i++ ) {
			const edict_t * ent = &game.edicts[ i + 1 ];
			if( !ent->r.client || ( PF_GetClientState( PLAYERNUM( ent ) ) < CS_CONNECTED ) ) {
				continue;
			}

			if( ent->s.team == team ) {
				current_team->player_indices[ current_team->num_players++ ] = ENTNUM( ent );
			}
		}

		std::sort( current_team->player_indices, current_team->player_indices + current_team->num_players, G_Teams_CompareMembers );
	}
}

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
		memset( G_ClientGetStats( ent ), 0, sizeof( score_stats_t ) );
		memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
		ent->r.client->teamstate.timeStamp = level.time;
	}

	if( ent->r.client->team == TEAM_SPECTATOR || team == TEAM_SPECTATOR ) {
		level.ready[PLAYERNUM( ent )] = false;
	}

	ent->r.client->team = team;

	if( team == TEAM_SPECTATOR || !level.gametype.autoRespawn ) {
		G_ClientRespawn( ent, true );
		G_ChasePlayer( ent, NULL, team != TEAM_SPECTATOR, 0 );
	}
	else {
		G_ClientRespawn( ent, false );
	}
}

enum {
	ER_TEAM_OK,
	ER_TEAM_INVALID,
	ER_TEAM_MATCHSTATE,
	ER_TEAM_UNEVEN,
};

static bool G_Teams_CanKeepEvenTeam( int leaving, int joining ) {
	int max = 0;
	int min = server_gs.maxclients + 1;

	for( int i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
		u8 numplayers = server_gs.gameState.teams[ i ].num_players;
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

	return server_gs.gameState.teams[ joining ].num_players + 1 == min || Abs( max - min ) <= 1;
}

static int G_GameTypes_DenyJoinTeam( edict_t *ent, int team ) {
	if( team < 0 || team >= GS_MAX_TEAMS ) {
		Com_Printf( "WARNING: 'G_GameTypes_CanJoinTeam' parsing a unrecognized team value\n" );
		return ER_TEAM_INVALID;
	}

	if( team == TEAM_SPECTATOR ) {
		return ER_TEAM_OK;
	}

	if( server_gs.gameState.match_state > MatchState_Playing ) {
		return ER_TEAM_MATCHSTATE;
	}

	if( !level.gametype.isTeamBased ) {
		return team == TEAM_PLAYERS ? ER_TEAM_OK : ER_TEAM_INVALID;
	}

	if( team != TEAM_ALPHA && team != TEAM_BETA )
		return ER_TEAM_INVALID;

	if( !g_teams_allow_uneven->integer && !G_Teams_CanKeepEvenTeam( ent->s.team, team ) ) {
		return ER_TEAM_UNEVEN;
	}

	return ER_TEAM_OK;
}

bool G_Teams_JoinTeam( edict_t *ent, int team ) {
	int error;

	G_Teams_UpdateMembersList(); // make sure we have up-to-date data

	if( !ent->r.client ) {
		return false;
	}

	if( ( error = G_GameTypes_DenyJoinTeam( ent, team ) ) ) {
		if( error == ER_TEAM_INVALID ) {
			G_PrintMsg( ent, "Can't join %s\n", GS_TeamName( team ) );
		} else if( error == ER_TEAM_UNEVEN ) {
			G_PrintMsg( ent, "Can't join %s because of uneven teams\n", GS_TeamName( team ) );
		}
		return false;
	}

	G_Teams_SetTeam( ent, team );
	return true;
}

bool G_Teams_JoinAnyTeam( edict_t *ent, bool silent ) {
	G_Teams_UpdateMembersList(); // make sure we have up-to-date data

	if( !level.gametype.isTeamBased ) {
		if( ent->s.team == TEAM_PLAYERS ) {
			return false;
		}
		if( G_Teams_JoinTeam( ent, TEAM_PLAYERS ) ) {
			if( !silent ) {
				G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
			}
		}
		return true;
	}

	const SyncTeamState * alpha = &server_gs.gameState.teams[ TEAM_ALPHA ];
	const SyncTeamState * beta = &server_gs.gameState.teams[ TEAM_BETA ];

	int team;
	if( alpha->num_players != beta->num_players ) {
		team = alpha->num_players <= beta->num_players ? TEAM_ALPHA : TEAM_BETA;
	}
	else {
		team = alpha->score <= beta->score ? TEAM_ALPHA : TEAM_BETA;
	}

	if( team == ent->s.team ) { // he is at the right team
		if( !silent ) {
			G_PrintMsg( ent, "Couldn't find a better team than team %s.\n", GS_TeamName( ent->s.team ) );
		}
		return false;
	}

	if( G_Teams_JoinTeam( ent, team ) ) {
		if( !silent ) {
			G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
		}
		return true;
	}

	return false;
}

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

void G_Say_Team( edict_t *who, const char *inmsg, bool checkflood ) {
	if( who->s.team != TEAM_SPECTATOR && !level.gametype.isTeamBased ) {
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
