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

Cvar *g_teams_maxplayers;
Cvar *g_teams_allow_uneven;

void G_Teams_Init() {
	g_teams_maxplayers = NewCvar( "g_teams_maxplayers", "0", CvarFlag_Archive );
	g_teams_allow_uneven = NewCvar( "g_teams_allow_uneven", "1", CvarFlag_Archive );
	NewCvar( "g_force_even_teams", "0", CvarFlag_Archive );

	memset( server_gs.gameState.teams, 0, sizeof( server_gs.gameState.teams ) );
}

static bool G_Teams_CompareMembers( int a, int b ) {
	edict_t * edict_a = game.edicts + a;
	edict_t * edict_b = game.edicts + b;
	int result = G_ClientGetStats( edict_a )->score - G_ClientGetStats( edict_b )->score;
	if( result == 0 ) {
		result = strcmp( edict_a->r.client->name, edict_b->r.client->name );
	}
	return result > 0;
}

/*
* G_Teams_UpdateMembersList
* It's better to count the list in detail once per fame, than
* creating a quick list each time we need it.
*/
void G_Teams_UpdateMembersList() {
	for( int team = Team_None; team < Team_Count; team++ ) {
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

u8 PlayersAliveOnTeam( Team team ) {
	u8 alive = 0;
	for( u8 i = 0; i < server_gs.gameState.teams[ team ].num_players; i++ ) {
		edict_t * ent = PLAYERENT( server_gs.gameState.teams[ team ].player_indices[ i ] - 1 );
		if( !G_ISGHOSTING( ent ) && ent->health > 0 ) {
			alive++;
		}
	}
	return alive;
}

void G_Teams_SetTeam( edict_t * ent, Team team ) {
	Assert( ent && ent->r.inuse && ent->r.client );
	Assert( team >= Team_None && team < Team_Count );

	if( ent->r.client->team != Team_None && team != Team_None ) {
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

	if( ent->r.client->team == Team_None || team == Team_None ) {
		level.ready[PLAYERNUM( ent )] = false;
	}

	ent->r.client->team = team;

	if( team == Team_None || !level.gametype.autoRespawn ) {
		G_ClientRespawn( ent, true );
	}
	else {
		G_ClientRespawn( ent, false );
	}

	G_Match_CheckReadys(); // TODO: just do this every frame
}

enum {
	ER_TEAM_OK,
	ER_TEAM_INVALID,
	ER_TEAM_MATCHSTATE,
	ER_TEAM_UNEVEN,
};

static bool G_Teams_CanKeepEvenTeam( Team leaving, Team joining ) {
	int max = 0;
	int min = server_gs.maxclients + 1;

	for( int i = Team_One; i < Team_Count; i++ ) {
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

static int G_GameTypes_DenyJoinTeam( edict_t *ent, Team team ) {
	if( team >= Team_Count ) {
		Com_Printf( "WARNING: 'G_GameTypes_CanJoinTeam' parsing a unrecognized team value\n" );
		return ER_TEAM_INVALID;
	}

	if( team == Team_None ) {
		return ER_TEAM_OK;
	}

	if( server_gs.gameState.match_state > MatchState_Playing ) {
		return ER_TEAM_MATCHSTATE;
	}

	if( team >= Team_One + level.gametype.numTeams ) {
		return ER_TEAM_INVALID;
	}

	if( !g_teams_allow_uneven->integer && !G_Teams_CanKeepEvenTeam( ent->s.team, team ) ) {
		return ER_TEAM_UNEVEN;
	}

	return ER_TEAM_OK;
}

bool G_Teams_JoinTeam( edict_t *ent, Team team ) {
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

	const SyncTeamState * teams = server_gs.gameState.teams;

	bool all_same_count = true;
	for( int i = 0; i < level.gametype.numTeams; i++ ) {
		if( teams[ Team_One + i ].num_players != teams[ Team_One ].num_players ) {
			all_same_count = false;
			break;
		}
	}

	Team best_team = Team_None;
	u8 best_value = U8_MAX;
	if( all_same_count ) {
		for( int i = 0; i < level.gametype.numTeams; i++ ) {
			if( best_team == Team_None || teams[ Team_One + i ].score < best_value ) {
				best_team = Team( Team_One + i );
				best_value = teams[ Team_One + i ].score;
			}
		}
	}
	else {
		for( int i = 0; i < level.gametype.numTeams; i++ ) {
			if( best_team == Team_None || teams[ Team_One + i ].num_players < best_value ) {
				best_team = Team( Team_One + i );
				best_value = teams[ Team_One + i ].num_players;
			}
		}
	}

	if( best_team == ent->s.team ) {
		if( !silent ) {
			G_PrintMsg( ent, "Couldn't find a better team than team %s.\n", GS_TeamName( ent->s.team ) );
		}
		return false;
	}

	if( G_Teams_JoinTeam( ent, best_team ) ) {
		if( !silent ) {
			G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->name, GS_TeamName( ent->s.team ) );
		}
		return true;
	}

	return false;
}

void G_Teams_Join_Cmd( edict_t * ent, msg_t args ) {
	if( !ent->r.client || PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	const char * t = MSG_ReadString( &args );
	if( StrEqual( t, "" ) ) {
		G_Teams_JoinAnyTeam( ent, false );
		return;
	}

	Team team = GS_TeamFromName( t );
	if( team == Team_None ) {
		G_PrintMsg( ent, "No such team.\n" );
		return;
	}

	if( team == Team_None ) { // special handling for spectator team
		Cmd_Spectate( ent );
		return;
	}
	if( team == ent->s.team ) {
		G_PrintMsg( ent, "You are already in %s team\n", GS_TeamName( team ) );
		return;
	}
	if( G_Teams_JoinTeam( ent, team ) ) {
		G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->name, GS_TeamName( ent->s.team ) );
		return;
	}
}

void GhostEveryone() {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( PF_GetClientState( i ) >= CS_SPAWNED ) {
			G_ClientRespawn( ent, true );
		}
	}
}

void InitRespawnQueues( RespawnQueues * queues ) {
	for( RespawnQueues::Queue & queue : queues->teams ) {
		for( int & slot : queue.players ) {
			slot = -1;
		}
	}
}

static void RemoveIndexFromRespawnQueue( RespawnQueues::Queue * queue, size_t idx ) {
	queue->players[ idx ] = -1; // in case this is the last element
	memcpy( &queue->players[ idx ], &queue->players[ idx + 1 ], ARRAY_COUNT( queue->players ) - ( idx + 1 ) );
}

void RemovePlayerFromRespawnQueues( RespawnQueues * queues, int player ) {
	for( RespawnQueues::Queue & queue : queues->teams ) {
		for( size_t i = 0; i < ARRAY_COUNT( queue.players ); i++ ) {
			if( queue.players[ i ] == player ) {
				RemoveIndexFromRespawnQueue( &queue, i );
			}
		}
	}
}

void RemoveDisconnectedPlayersFromRespawnQueues( RespawnQueues * queues ) {
	// drop disconnected players
	int disconnected[ MAX_CLIENTS ] = { };
	size_t num_disconnected = 0;

	for( const RespawnQueues::Queue & queue : queues->teams ) {
		for( int player : queue.players ) {
			if( player == -1 )
				break;

			if( !PLAYERENT( player )->r.inuse ) {
				disconnected[ num_disconnected ] = player;
				num_disconnected++;
			}
		}
	}

	for( size_t i = 0; i < num_disconnected; i++ ) {
		RemovePlayerFromRespawnQueues( queues, disconnected[ i ] );
	}
}

void EnqueueRespawn( RespawnQueues * queues, Team team, int player ) {
	for( int & slot : queues->teams[ team ].players ) {
		if( slot == -1 ) {
			slot = player;
			return;
		}
	}

	Assert( false );
}

static Optional< int > DequeueRespawn( RespawnQueues * queues, Team team ) {
	RespawnQueues::Queue * queue = &queues->teams[ team ];

	int player = queue->players[ 0 ];
	if( player == -1 )
		return NONE;

	RemoveIndexFromRespawnQueue( queue, 0 );
	EnqueueRespawn( queues, team, player );
	return player;
}

void SpawnTeams( RespawnQueues * queues ) {
	u8 even_n = U8_MAX;
	for( int i = 0; i < level.gametype.numTeams; i++ ) {
		const SyncTeamState * team = &server_gs.gameState.teams[ Team_One + i ];
		if( team->num_players > 0 ) {
			even_n = Min2( even_n, team->num_players );
		}
	}

	for( int i = 0; i < level.gametype.numTeams; i++ ) {
		const SyncTeamState * team = &server_gs.gameState.teams[ Team_One + i ];
		if( team->num_players == 0 )
			continue;

		u8 num_to_spawn = Cvar_Bool( "g_force_even_teams" ) ? even_n : team->num_players;

		for( u8 j = 0; j < num_to_spawn; j++ ) {
			Optional< int > player = DequeueRespawn( queues, Team( Team_One + i ) );
			edict_t * ent = PLAYERENT( player.value );
			G_ClientRespawn( ent, false );
		}

		for( u8 j = 0; j < team->num_players - num_to_spawn; j++ ) {
			int player = queues->teams[ Team_One + i ].players[ j ];
			edict_t * ent = PLAYERENT( player );
			G_PrintMsg( NULL, "%s is sitting out to make the teams even\n", ent->r.client->name );
			G_ClientRespawn( ent, true );
		}
	}
}
