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

#include "g_local.h"

//===================================================================

int clientVoted[MAX_CLIENTS];
int clientVoteChanges[MAX_CLIENTS];

cvar_t *g_callvote_electpercentage;
cvar_t *g_callvote_electtime;          // in seconds
cvar_t *g_callvote_enabled;
cvar_t *g_callvote_maxchanges;
cvar_t *g_callvote_cooldowntime;

enum
{
	VOTED_NOTHING = 0,
	VOTED_YES,
	VOTED_NO
};

// Data that can be used by the vote specific functions
typedef struct
{
	edict_t *caller;
	bool operatorcall;
	struct callvotetype_s *callvote;
	int argc;
	char *argv[MAX_STRING_TOKENS];
	char *string;               // can be used to overwrite the displayed vote string
	void *data;                 // any data vote wants to carry over multiple calls of validate and to execute
} callvotedata_t;

typedef struct callvotetype_s
{
	char *name;
	int expectedargs;               // -1 = any amount, -2 = any amount except 0
	bool ( *validate )( callvotedata_t *data, bool first );
	void ( *execute )( callvotedata_t *vote );
	const char *( *current )( void );
	void ( *extraHelp )( edict_t *ent );
	char *argument_format;
	char *help;
	char *argument_type;
	struct callvotetype_s *next;
} callvotetype_t;

// Data that will only be used by the common callvote functions
typedef struct
{
	int64_t timeout;           // time to finish
	callvotedata_t vote;
} callvotestate_t;

static callvotestate_t callvoteState;

static callvotetype_t *callvotesHeadNode = NULL;

//==============================================
//		Vote specifics
//==============================================

/*
* map
*/

#define MAPLIST_SEPS " ,"

static void G_VoteMapExtraHelp( edict_t *ent ) {
	char *s;
	char buffer[MAX_STRING_CHARS];
	char message[MAX_STRING_CHARS / 4 * 3];    // use buffer to send only one print message
	int nummaps, i, start;
	size_t length, msglength;

	// update the maplist
	trap_ML_Update();

	if( g_enforce_map_pool->integer && strlen( g_map_pool->string ) > 2 ) {
		G_PrintMsg( ent, "Maps available [map pool enforced]:\n %s\n", g_map_pool->string );
		return;
	}

	// don't use Q_strncatz and Q_strncpyz below because we
	// check length of the message string manually

	memset( message, 0, sizeof( message ) );
	strcpy( message, "- Available maps:" );

	for( nummaps = 0; trap_ML_GetMapByNum( nummaps, NULL, 0 ); nummaps++ )
		;

	if( Cmd_Argc() > 2 ) {
		start = atoi( Cmd_Argv( 2 ) ) - 1;
		if( start < 0 ) {
			start = 0;
		}
	} else {
		start = 0;
	}

	i = start;
	msglength = strlen( message );
	while( trap_ML_GetMapByNum( i, buffer, sizeof( buffer ) ) ) {
		i++;
		s = buffer;
		length = strlen( s );
		if( msglength + length + 3 >= sizeof( message ) ) {
			break;
		}

		strcat( message, " " );
		strcat( message, s );

		msglength += length + 1;
	}

	if( i == start ) {
		strcat( message, "\nNone" );
	}

	G_PrintMsg( ent, "%s\n", message );

	if( i < nummaps ) {
		G_PrintMsg( ent, "Type 'callvote map %i' for more maps\n", i + 1 );
	}
}

static bool G_VoteMapValidate( callvotedata_t *data, bool first ) {
	char mapname[MAX_CONFIGSTRING_CHARS];

	if( !first ) { // map can't become invalid while voting
		return true;
	}
	if( Q_isdigit( data->argv[0] ) ) { // FIXME
		return false;
	}

	if( strlen( "maps/" ) + strlen( data->argv[0] ) + strlen( ".bsp" ) >= MAX_CONFIGSTRING_CHARS ) {
		G_PrintMsg( data->caller, "%sToo long map name\n", S_COLOR_RED );
		return false;
	}

	Q_strncpyz( mapname, data->argv[0], sizeof( mapname ) );
	COM_SanitizeFilePath( mapname );

	if( !Q_stricmp( level.mapname, mapname ) ) {
		G_PrintMsg( data->caller, "%sYou are already on that map\n", S_COLOR_RED );
		return false;
	}

	if( !COM_ValidateRelativeFilename( mapname ) || strchr( mapname, '/' ) || strchr( mapname, '.' ) ) {
		G_PrintMsg( data->caller, "%sInvalid map name\n", S_COLOR_RED );
		return false;
	}

	if( trap_ML_FilenameExists( mapname ) ) {
		// check if valid map is in map pool when on
		if( g_enforce_map_pool->integer ) {
			char *s, *tok;

			// if map pool is empty, basically turn it off
			if( strlen( g_map_pool->string ) < 2 ) {
				return true;
			}

			s = G_CopyString( g_map_pool->string );
			tok = strtok( s, MAPLIST_SEPS );
			while( tok != NULL ) {
				if( !Q_stricmp( tok, mapname ) ) {
					G_Free( s );
					goto valid_map;
				} else {
					tok = strtok( NULL, MAPLIST_SEPS );
				}
			}
			G_Free( s );
			G_PrintMsg( data->caller, "%sMap is not in map pool.\n", S_COLOR_RED );
			return false;
		}

valid_map:
		if( data->string ) {
			G_Free( data->string );
		}
		data->string = G_CopyString( mapname );
		return true;
	}

	G_PrintMsg( data->caller, "%sNo such map available on this server\n", S_COLOR_RED );

	return false;
}

static void G_VoteMapPassed( callvotedata_t *vote ) {
	Q_strncpyz( level.callvote_map, Q_strlwr( vote->argv[0] ), sizeof( level.callvote_map ) );
	G_EndMatch();
}

static const char *G_VoteMapCurrent( void ) {
	return level.mapname;
}

/*
* nextmap
*/

static void G_VoteNextMapPassed( callvotedata_t *vote ) {
	level.callvote_map[0] = 0;
	G_EndMatch();
}

/*
* scorelimit
*/

static bool G_VoteScorelimitValidate( callvotedata_t *vote, bool first ) {
	int scorelimit = atoi( vote->argv[0] );

	if( scorelimit < 0 ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sCan't set negative scorelimit\n", S_COLOR_RED );
		}
		return false;
	}

	if( scorelimit == g_scorelimit->integer ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sScorelimit is already set to %i\n", S_COLOR_RED, scorelimit );
		}
		return false;
	}

	return true;
}

static void G_VoteScorelimitPassed( callvotedata_t *vote ) {
	Cvar_Set( "g_scorelimit", va( "%i", atoi( vote->argv[0] ) ) );
}

static const char *G_VoteScorelimitCurrent( void ) {
	return va( "%i", g_scorelimit->integer );
}

/*
* warmup_timelimit
*/

static bool G_VoteWarmupTimelimitValidate( callvotedata_t *vote, bool first ) {
	int warmup_timelimit = atoi( vote->argv[0] );

	if( warmup_timelimit < 0 ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sCan't set negative warmup timelimit\n", S_COLOR_RED );
		}
		return false;
	}

	if( warmup_timelimit == g_warmup_timelimit->integer ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sWarmup timelimit is already set to %i\n", S_COLOR_RED, warmup_timelimit );
		}
		return false;
	}

	return true;
}

static void G_VoteWarmupTimelimitPassed( callvotedata_t *vote ) {
	Cvar_Set( "g_warmup_timelimit", va( "%i", atoi( vote->argv[0] ) ) );
}

static const char *G_VoteWarmupTimelimitCurrent( void ) {
	return va( "%i", g_warmup_timelimit->integer );
}

/*
* allready
*/

static bool G_VoteAllreadyValidate( callvotedata_t *vote, bool first ) {
	int notreadys = 0;
	edict_t *ent;

	if( GS_MatchState( &server_gs ) >= MATCH_STATE_COUNTDOWN ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sThe game is not in warmup mode\n", S_COLOR_RED );
		}
		return false;
	}

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			continue;
		}

		if( ent->s.team > TEAM_SPECTATOR && !level.ready[PLAYERNUM( ent )] ) {
			notreadys++;
		}
	}

	if( !notreadys ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sEveryone is already ready\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteAllreadyPassed( callvotedata_t *vote ) {
	for( edict_t * ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			continue;
		}

		if( ent->s.team > TEAM_SPECTATOR && !level.ready[PLAYERNUM( ent )] ) {
			level.ready[PLAYERNUM( ent )] = true;
			G_Match_CheckReadys();
		}
	}
}

/*
* maxteamplayers
*/

static bool G_VoteMaxTeamplayersValidate( callvotedata_t *vote, bool first ) {
	int maxteamplayers = atoi( vote->argv[0] );

	if( maxteamplayers < 1 ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sThe maximum number of players in team can't be less than 1\n",
						S_COLOR_RED );
		}
		return false;
	}

	if( g_teams_maxplayers->integer == maxteamplayers ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sMaximum number of players in team is already %i\n",
						S_COLOR_RED, maxteamplayers );
		}
		return false;
	}

	return true;
}

static void G_VoteMaxTeamplayersPassed( callvotedata_t *vote ) {
	Cvar_Set( "g_teams_maxplayers", va( "%i", atoi( vote->argv[0] ) ) );
}

static const char *G_VoteMaxTeamplayersCurrent( void ) {
	return va( "%i", g_teams_maxplayers->integer );
}

/*
* lock
*/

static bool G_VoteLockValidate( callvotedata_t *vote, bool first ) {
	if( GS_MatchState( &server_gs ) > MATCH_STATE_PLAYTIME ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sCan't lock teams after the match\n", S_COLOR_RED );
		}
		return false;
	}

	if( level.teamlock ) {
		if( GS_MatchState( &server_gs ) < MATCH_STATE_COUNTDOWN && first ) {
			G_PrintMsg( vote->caller, "%sTeams are already set to be locked on match start\n", S_COLOR_RED );
		} else if( first ) {
			G_PrintMsg( vote->caller, "%sTeams are already locked\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteLockPassed( callvotedata_t *vote ) {
	int team;

	level.teamlock = true;

	// if we are inside a match, update the teams state
	if( GS_MatchState( &server_gs ) >= MATCH_STATE_COUNTDOWN && GS_MatchState( &server_gs ) <= MATCH_STATE_PLAYTIME ) {
		if( GS_TeamBasedGametype( &server_gs ) ) {
			for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ )
				G_Teams_LockTeam( team );
		} else {
			G_Teams_LockTeam( TEAM_PLAYERS );
		}
		G_PrintMsg( NULL, "Teams locked\n" );
	} else {
		G_PrintMsg( NULL, "Teams will be locked when the match starts\n" );
	}
}

/*
* unlock
*/

static bool G_VoteUnlockValidate( callvotedata_t *vote, bool first ) {
	if( GS_MatchState( &server_gs ) > MATCH_STATE_PLAYTIME ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sCan't unlock teams after the match\n", S_COLOR_RED );
		}
		return false;
	}

	if( !level.teamlock ) {
		if( GS_MatchState( &server_gs ) < MATCH_STATE_COUNTDOWN && first ) {
			G_PrintMsg( vote->caller, "%sTeams are not set to be locked\n", S_COLOR_RED );
		} else if( first ) {
			G_PrintMsg( vote->caller, "%sTeams are not locked\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteUnlockPassed( callvotedata_t *vote ) {
	int team;

	level.teamlock = false;

	// if we are inside a match, update the teams state
	if( GS_MatchState( &server_gs ) >= MATCH_STATE_COUNTDOWN && GS_MatchState( &server_gs ) <= MATCH_STATE_PLAYTIME ) {
		if( GS_TeamBasedGametype( &server_gs ) ) {
			for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ )
				G_Teams_UnLockTeam( team );
		} else {
			G_Teams_UnLockTeam( TEAM_PLAYERS );
		}
		G_PrintMsg( NULL, "Teams unlocked\n" );
	} else {
		G_PrintMsg( NULL, "Teams will no longer be locked when the match starts\n" );
	}
}

/*
* remove
*/

static void G_VoteRemoveExtraHelp( edict_t *ent ) {
	int i;
	edict_t *e;
	char msg[1024];

	msg[0] = 0;
	Q_strncatz( msg, "- List of players in game:\n", sizeof( msg ) );

	if( GS_TeamBasedGametype( &server_gs ) ) {
		int team;

		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			Q_strncatz( msg, va( "%s:\n", GS_TeamName( team ) ), sizeof( msg ) );
			for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
				if( !e->r.inuse || e->s.team != team ) {
					continue;
				}

				Q_strncatz( msg, va( "%3i: %s\n", PLAYERNUM( e ), e->r.client->netname ), sizeof( msg ) );
			}
		}
	} else {
		for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
			if( !e->r.inuse || e->s.team != TEAM_PLAYERS ) {
				continue;
			}

			Q_strncatz( msg, va( "%3i: %s\n", PLAYERNUM( e ), e->r.client->netname ), sizeof( msg ) );
		}
	}

	G_PrintMsg( ent, "%s", msg );
}

static bool G_VoteRemoveValidate( callvotedata_t *vote, bool first ) {
	int who = -1;

	if( first ) {
		edict_t *tokick = G_PlayerForText( vote->argv[0] );

		if( tokick ) {
			who = PLAYERNUM( tokick );
		} else {
			who = -1;
		}

		if( who == -1 ) {
			G_PrintMsg( vote->caller, "%sNo such player\n", S_COLOR_RED );
			return false;
		} else if( tokick->s.team == TEAM_SPECTATOR ) {
			G_PrintMsg( vote->caller, "Player %s%s%s is already spectator.\n", S_COLOR_WHITE,
						tokick->r.client->netname, S_COLOR_RED );

			return false;
		} else {
			// we save the player id to be removed, so we don't later get confused by new ids or players changing names
			vote->data = G_Malloc( sizeof( int ) );
			memcpy( vote->data, &who, sizeof( int ) );
		}
	} else {
		memcpy( &who, vote->data, sizeof( int ) );
	}

	if( !game.edicts[who + 1].r.inuse || game.edicts[who + 1].s.team == TEAM_SPECTATOR ) {
		return false;
	} else {
		if( !vote->string || Q_stricmp( vote->string, game.edicts[who + 1].r.client->netname ) ) {
			if( vote->string ) {
				G_Free( vote->string );
			}
			vote->string = G_CopyString( game.edicts[who + 1].r.client->netname );
		}

		return true;
	}
}

static void G_VoteRemovePassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];

	// may have disconnect along the callvote time
	if( !ent->r.inuse || !ent->r.client || ent->s.team == TEAM_SPECTATOR ) {
		return;
	}

	G_PrintMsg( NULL, "Player %s%s removed from team %s%s.\n", ent->r.client->netname, S_COLOR_WHITE,
				GS_TeamName( ent->s.team ), S_COLOR_WHITE );

	G_Teams_SetTeam( ent, TEAM_SPECTATOR );
	ent->r.client->queueTimeStamp = 0;
}


/*
* kick
*/

static void G_VoteKickExtraHelp( edict_t *ent ) {
	int i;
	edict_t *e;
	char msg[1024];

	msg[0] = 0;
	Q_strncatz( msg, "- List of current players:\n", sizeof( msg ) );

	for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
		if( !e->r.inuse ) {
			continue;
		}

		Q_strncatz( msg, va( "%3i: %s\n", PLAYERNUM( e ), e->r.client->netname ), sizeof( msg ) );
	}

	G_PrintMsg( ent, "%s", msg );
}

static bool G_VoteKickValidate( callvotedata_t *vote, bool first ) {
	int who = -1;

	if( first ) {
		edict_t *tokick = G_PlayerForText( vote->argv[0] );

		if( tokick ) {
			who = PLAYERNUM( tokick );
		} else {
			who = -1;
		}

		if( who != -1 ) {
			if( game.edicts[who + 1].r.client->isoperator ) {
				G_PrintMsg( vote->caller, S_COLOR_RED "%s is a game operator.\n", game.edicts[who + 1].r.client->netname );
				return false;
			}

			// we save the player id to be kicked, so we don't later get
			//confused by new ids or players changing names

			vote->data = G_Malloc( sizeof( int ) );
			memcpy( vote->data, &who, sizeof( int ) );
		} else {
			G_PrintMsg( vote->caller, "%sNo such player\n", S_COLOR_RED );
			return false;
		}
	} else {
		memcpy( &who, vote->data, sizeof( int ) );
	}

	if( !game.edicts[who + 1].r.inuse ) {
		return false;
	} else {
		if( !vote->string || Q_stricmp( vote->string, game.edicts[who + 1].r.client->netname ) ) {
			if( vote->string ) {
				G_Free( vote->string );
			}

			vote->string = G_CopyString( game.edicts[who + 1].r.client->netname );
		}

		return true;
	}
}

static void G_VoteKickPassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnected along the callvote time
		return;
	}

	trap_DropClient( ent, DROP_TYPE_NORECONNECT, "Kicked" );
}


/*
* kickban
*/

static void G_VoteKickBanExtraHelp( edict_t *ent ) {
	int i;
	edict_t *e;
	char msg[1024];

	msg[0] = 0;
	Q_strncatz( msg, "- List of current players:\n", sizeof( msg ) );

	for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
		if( !e->r.inuse ) {
			continue;
		}

		Q_strncatz( msg, va( "%3i: %s\n", PLAYERNUM( e ), e->r.client->netname ), sizeof( msg ) );
	}

	G_PrintMsg( ent, "%s", msg );
}

static bool G_VoteKickBanValidate( callvotedata_t *vote, bool first ) {
	int who = -1;

	if( !filterban->integer ) {
		G_PrintMsg( vote->caller, "%sFilterban is disabled on this server\n", S_COLOR_RED );
		return false;
	}

	if( first ) {
		edict_t *tokick = G_PlayerForText( vote->argv[0] );

		if( tokick ) {
			who = PLAYERNUM( tokick );
		} else {
			who = -1;
		}

		if( who != -1 ) {
			if( game.edicts[who + 1].r.client->isoperator ) {
				G_PrintMsg( vote->caller, S_COLOR_RED "%s is a game operator.\n", game.edicts[who + 1].r.client->netname );
				return false;
			}

			// we save the player id to be kicked, so we don't later get
			// confused by new ids or players changing names

			vote->data = G_Malloc( sizeof( int ) );
			memcpy( vote->data, &who, sizeof( int ) );
		} else {
			G_PrintMsg( vote->caller, "%sNo such player\n", S_COLOR_RED );
			return false;
		}
	} else {
		memcpy( &who, vote->data, sizeof( int ) );
	}

	if( !game.edicts[who + 1].r.inuse ) {
		return false;
	} else {
		if( !vote->string || Q_stricmp( vote->string, game.edicts[who + 1].r.client->netname ) ) {
			if( vote->string ) {
				G_Free( vote->string );
			}

			vote->string = G_CopyString( game.edicts[who + 1].r.client->netname );
		}

		return true;
	}
}

static void G_VoteKickBanPassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnected along the callvote time
		return;
	}

	Cbuf_ExecuteText( EXEC_APPEND, va( "addip %s %i\n", ent->r.client->ip, 15 ) );
	trap_DropClient( ent, DROP_TYPE_NORECONNECT, "Kicked" );
}

/*
* mute
*/

static void G_VoteMuteExtraHelp( edict_t *ent ) {
	int i;
	edict_t *e;
	char msg[1024];

	msg[0] = 0;
	Q_strncatz( msg, "- List of current players:\n", sizeof( msg ) );

	for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
		if( !e->r.inuse ) {
			continue;
		}

		Q_strncatz( msg, va( "%3i: %s\n", PLAYERNUM( e ), e->r.client->netname ), sizeof( msg ) );
	}

	G_PrintMsg( ent, "%s", msg );
}

static bool G_VoteMuteValidate( callvotedata_t *vote, bool first ) {
	int who = -1;

	if( first ) {
		edict_t *tomute = G_PlayerForText( vote->argv[0] );

		if( tomute ) {
			who = PLAYERNUM( tomute );
		} else {
			who = -1;
		}

		if( who != -1 ) {
			// we save the player id to be kicked, so we don't later get confused by new ids or players changing names
			vote->data = G_Malloc( sizeof( int ) );
			memcpy( vote->data, &who, sizeof( int ) );
		} else {
			G_PrintMsg( vote->caller, "%sNo such player\n", S_COLOR_RED );
			return false;
		}
	} else {
		memcpy( &who, vote->data, sizeof( int ) );
	}

	if( !game.edicts[who + 1].r.inuse ) {
		return false;
	} else {
		if( !vote->string || Q_stricmp( vote->string, game.edicts[who + 1].r.client->netname ) ) {
			if( vote->string ) {
				G_Free( vote->string );
			}
			vote->string = G_CopyString( game.edicts[who + 1].r.client->netname );
		}

		return true;
	}
}

// chat mute
static void G_VoteMutePassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnect along the callvote time
		return;
	}

	ent->r.client->muted |= 1;
}

// vsay mute
static void G_VoteVMutePassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnect along the callvote time
		return;
	}

	ent->r.client->muted |= 2;
}

/*
* unmute
*/

static void G_VoteUnmuteExtraHelp( edict_t *ent ) {
	int i;
	edict_t *e;
	char msg[1024];

	msg[0] = 0;
	Q_strncatz( msg, "- List of current players:\n", sizeof( msg ) );

	for( i = 0, e = game.edicts + 1; i < server_gs.maxclients; i++, e++ ) {
		if( !e->r.inuse ) {
			continue;
		}

		Q_strncatz( msg, va( "%3i: %s\n", PLAYERNUM( e ), e->r.client->netname ), sizeof( msg ) );
	}

	G_PrintMsg( ent, "%s", msg );
}

static bool G_VoteUnmuteValidate( callvotedata_t *vote, bool first ) {
	int who = -1;

	if( first ) {
		edict_t *tomute = G_PlayerForText( vote->argv[0] );

		if( tomute ) {
			who = PLAYERNUM( tomute );
		} else {
			who = -1;
		}

		if( who != -1 ) {
			// we save the player id to be kicked, so we don't later get confused by new ids or players changing names
			vote->data = G_Malloc( sizeof( int ) );
			memcpy( vote->data, &who, sizeof( int ) );
		} else {
			G_PrintMsg( vote->caller, "%sNo such player\n", S_COLOR_RED );
			return false;
		}
	} else {
		memcpy( &who, vote->data, sizeof( int ) );
	}

	if( !game.edicts[who + 1].r.inuse ) {
		return false;
	} else {
		if( !vote->string || Q_stricmp( vote->string, game.edicts[who + 1].r.client->netname ) ) {
			if( vote->string ) {
				G_Free( vote->string );
			}
			vote->string = G_CopyString( game.edicts[who + 1].r.client->netname );
		}

		return true;
	}
}

// chat unmute
static void G_VoteUnmutePassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnect along the callvote time
		return;
	}

	ent->r.client->muted &= ~1;
}

// vsay unmute
static void G_VoteVUnmutePassed( callvotedata_t *vote ) {
	int who;
	edict_t *ent;

	memcpy( &who, vote->data, sizeof( int ) );
	ent = &game.edicts[who + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnect along the callvote time
		return;
	}

	ent->r.client->muted &= ~2;
}

/*
* timeout
*/
static bool G_VoteTimeoutValidate( callvotedata_t *vote, bool first ) {
	if( GS_MatchPaused( &server_gs ) && ( level.timeout.endtime - level.timeout.time ) >= 2 * TIMEIN_TIME ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sTimeout already in progress\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteTimeoutPassed( callvotedata_t *vote ) {
	G_GamestatSetFlag( GAMESTAT_FLAG_PAUSED, true );
	level.timeout.caller = 0;
	level.timeout.endtime = level.timeout.time + TIMEOUT_TIME + FRAMETIME;
}

/*
* timein
*/
static bool G_VoteTimeinValidate( callvotedata_t *vote, bool first ) {
	if( !GS_MatchPaused( &server_gs ) ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sNo timeout in progress\n", S_COLOR_RED );
		}
		return false;
	}

	if( level.timeout.endtime - level.timeout.time <= 2 * TIMEIN_TIME ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sTimeout is about to end already\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteTimeinPassed( callvotedata_t *vote ) {
	level.timeout.endtime = level.timeout.time + TIMEIN_TIME + FRAMETIME;
}

/*
* allow_uneven
*/
static bool G_VoteAllowUnevenValidate( callvotedata_t *vote, bool first ) {
	int allow_uneven = atoi( vote->argv[0] );

	if( allow_uneven != 0 && allow_uneven != 1 ) {
		return false;
	}

	if( allow_uneven && g_teams_allow_uneven->integer ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sUneven teams is already allowed.\n", S_COLOR_RED );
		}
		return false;
	}

	if( !allow_uneven && !g_teams_allow_uneven->integer ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sUneven teams is already disallowed\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteAllowUnevenPassed( callvotedata_t *vote ) {
	Cvar_Set( "g_teams_allow_uneven", va( "%i", atoi( vote->argv[0] ) ) );
}

static const char *G_VoteAllowUnevenCurrent( void ) {
	if( g_teams_allow_uneven->integer ) {
		return "1";
	} else {
		return "0";
	}
}

//================================================
//
//================================================


callvotetype_t *G_RegisterCallvote( const char *name ) {
	callvotetype_t *callvote;

	for( callvote = callvotesHeadNode; callvote != NULL; callvote = callvote->next ) {
		if( !Q_stricmp( callvote->name, name ) ) {
			return callvote;
		}
	}

	// create a new callvote
	callvote = ( callvotetype_t * )G_LevelMalloc( sizeof( callvotetype_t ) );
	memset( callvote, 0, sizeof( callvotetype_t ) );
	callvote->next = callvotesHeadNode;
	callvotesHeadNode = callvote;

	callvote->name = G_LevelCopyString( name );
	return callvote;
}

void G_FreeCallvotes( void ) {
	callvotetype_t *callvote;

	while( callvotesHeadNode ) {
		callvote = callvotesHeadNode->next;

		if( callvotesHeadNode->name ) {
			G_LevelFree( callvotesHeadNode->name );
		}
		if( callvotesHeadNode->argument_format ) {
			G_LevelFree( callvotesHeadNode->argument_format );
		}
		if( callvotesHeadNode->help ) {
			G_LevelFree( callvotesHeadNode->help );
		}

		G_LevelFree( callvotesHeadNode );
		callvotesHeadNode = callvote;
	}

	callvotesHeadNode = NULL;
}

//===================================================================
// Common functions
//===================================================================

/*
* G_CallVotes_ResetClient
*/
void G_CallVotes_ResetClient( int n ) {
	clientVoted[n] = VOTED_NOTHING;
	clientVoteChanges[n] = g_callvote_maxchanges->integer;
	if( clientVoteChanges[n] < 1 ) {
		clientVoteChanges[n] = 1;
	}
}

/*
* G_CallVotes_Reset
*/
static void G_CallVotes_Reset( bool vote_happened ) {
	if( vote_happened && callvoteState.vote.caller && callvoteState.vote.caller->r.client ) {
		callvoteState.vote.caller->r.client->level.callvote_when = svs.realtime;
	}

	callvoteState.vote.callvote = NULL;
	for( int i = 0; i < server_gs.maxclients; i++ )
		G_CallVotes_ResetClient( i );
	callvoteState.timeout = 0;

	callvoteState.vote.caller = NULL;
	if( callvoteState.vote.string ) {
		G_Free( callvoteState.vote.string );
	}
	if( callvoteState.vote.data ) {
		G_Free( callvoteState.vote.data );
	}
	for( int i = 0; i < callvoteState.vote.argc; i++ ) {
		if( callvoteState.vote.argv[i] ) {
			G_Free( callvoteState.vote.argv[i] );
		}
	}

	trap_ConfigString( CS_CALLVOTE, "" );
	trap_ConfigString( CS_CALLVOTE_REQUIRED_VOTES, "" );
	trap_ConfigString( CS_CALLVOTE_YES_VOTES, "" );
	trap_ConfigString( CS_CALLVOTE_NO_VOTES, "" );

	memset( &callvoteState, 0, sizeof( callvoteState ) );
}

/*
* G_CallVotes_PrintUsagesToPlayer
*/
static void G_CallVotes_PrintUsagesToPlayer( edict_t *ent ) {
	callvotetype_t *callvote;

	G_PrintMsg( ent, "Available votes:\n" );
	for( callvote = callvotesHeadNode; callvote != NULL; callvote = callvote->next ) {
		if( Cvar_Value( va( "g_disable_vote_%s", callvote->name ) ) ) {
			continue;
		}

		if( callvote->argument_format ) {
			G_PrintMsg( ent, " %s %s\n", callvote->name, callvote->argument_format );
		} else {
			G_PrintMsg( ent, " %s\n", callvote->name );
		}
	}
}

/*
* G_CallVotes_PrintHelpToPlayer
*/
static void G_CallVotes_PrintHelpToPlayer( edict_t *ent, callvotetype_t *callvote ) {

	if( !callvote ) {
		return;
	}

	G_PrintMsg( ent, "Usage: %s %s\n%s%s%s\n", callvote->name,
				( callvote->argument_format ? callvote->argument_format : "" ),
				( callvote->current ? va( "Current: %s\n", callvote->current() ) : "" ),
				( callvote->help ? "- " : "" ), ( callvote->help ? callvote->help : "" ) );
	if( callvote->extraHelp != NULL ) {
		callvote->extraHelp( ent );
	}
}

/*
* G_CallVotes_ArgsToString
*/
static const char *G_CallVotes_ArgsToString( const callvotedata_t *vote ) {
	static char argstring[MAX_STRING_CHARS];
	int i;

	argstring[0] = 0;

	if( vote->argc > 0 ) {
		Q_strncatz( argstring, vote->argv[0], sizeof( argstring ) );
	}
	for( i = 1; i < vote->argc; i++ ) {
		Q_strncatz( argstring, " ", sizeof( argstring ) );
		Q_strncatz( argstring, vote->argv[i], sizeof( argstring ) );
	}

	return argstring;
}

/*
* G_CallVotes_Arguments
*/
static const char *G_CallVotes_Arguments( const callvotedata_t *vote ) {
	const char *arguments;
	if( vote->string ) {
		arguments = vote->string;
	} else {
		arguments = G_CallVotes_ArgsToString( vote );
	}
	return arguments;
}

/*
* G_CallVotes_String
*/
static const char *G_CallVotes_String( const callvotedata_t *vote ) {
	const char *arguments;
	static char string[MAX_CONFIGSTRING_CHARS];

	arguments = G_CallVotes_Arguments( vote );
	if( arguments[0] ) {
		snprintf( string, sizeof( string ), "%s %s", vote->callvote->name, arguments );
		return string;
	}
	return vote->callvote->name;
}

/*
* G_CallVotes_CheckState
*/
static void G_CallVotes_CheckState( void ) {
	edict_t *ent;
	int yeses = 0, voters = 0, noes = 0;

	if( !callvoteState.vote.callvote ) {
		return;
	}

	if( callvoteState.vote.callvote->validate != NULL && !callvoteState.vote.callvote->validate( &callvoteState.vote, false ) ) {
		G_PrintMsg( NULL, "Vote %s%s%s canceled\n", S_COLOR_YELLOW, G_CallVotes_String( &callvoteState.vote ), S_COLOR_WHITE );
		G_CallVotes_Reset( true );
		return;
	}

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		gclient_t *client = ent->r.client;

		if( !ent->r.inuse || trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			continue;
		}

		if( ent->r.svflags & SVF_FAKECLIENT ) {
			continue;
		}

		// ignore inactive players unless they have voted
		if( g_inactivity_maxtime->value > 0 ) {
			bool inactive = client->level.last_activity + g_inactivity_maxtime->value * 1000 < level.time;
			if( inactive && clientVoted[PLAYERNUM( ent )] == VOTED_NOTHING )
				continue;
		}

		voters++;
		if( clientVoted[PLAYERNUM( ent )] == VOTED_YES ) {
			yeses++;
		} else if( clientVoted[PLAYERNUM( ent )] == VOTED_NO ) {
			noes++;
		}
	}

	int needvotes = int( voters * g_callvote_electpercentage->value / 100.0f ) + 1;
	if( yeses >= needvotes || callvoteState.vote.operatorcall ) {
		G_PrintMsg( NULL, "Vote %s%s%s passed\n", S_COLOR_YELLOW, G_CallVotes_String( &callvoteState.vote ), S_COLOR_WHITE );
		if( callvoteState.vote.callvote->execute != NULL ) {
			callvoteState.vote.callvote->execute( &callvoteState.vote );
		}
		G_CallVotes_Reset( true );
		return;
	}

	if( svs.realtime > callvoteState.timeout || needvotes + noes > voters ) {
		G_PrintMsg( NULL, "Vote %s%s%s failed\n", S_COLOR_YELLOW, G_CallVotes_String( &callvoteState.vote ), S_COLOR_WHITE );
		G_CallVotes_Reset( true );
		return;
	}

	trap_ConfigString( CS_CALLVOTE_REQUIRED_VOTES, va( "%d", needvotes ) );
	trap_ConfigString( CS_CALLVOTE_YES_VOTES, va( "%d", yeses ) );
	trap_ConfigString( CS_CALLVOTE_NO_VOTES, va( "%d", noes ) );
}

/*
* G_CallVotes_CmdVote
*/
void G_CallVotes_CmdVote( edict_t *ent ) {
	const char *vote;
	int vote_id;

	if( !ent->r.client ) {
		return;
	}
	if( ent->r.svflags & SVF_FAKECLIENT ) {
		return;
	}

	if( !callvoteState.vote.callvote ) {
		G_PrintMsg( ent, "%sThere's no vote in progress\n", S_COLOR_RED );
		return;
	}

	vote = Cmd_Argv( 1 );
	if( !Q_stricmp( vote, "yes" ) ) {
		vote_id = VOTED_YES;
	}
	else if( !Q_stricmp( vote, "no" ) ) {
		vote_id = VOTED_NO;
	}
	else {
		G_PrintMsg( ent, "%sInvalid vote: %s%s%s. Use yes or no\n", S_COLOR_RED,
					S_COLOR_YELLOW, vote, S_COLOR_RED );
		return;
	}

	if( clientVoted[PLAYERNUM( ent )] == vote_id ) {
		G_PrintMsg( ent, "%sYou have already voted %s\n", S_COLOR_RED, vote );
		return;
	}

	if( clientVoteChanges[PLAYERNUM( ent )] == 0 ) {
		G_PrintMsg( ent, "%sYou cannot change your vote anymore\n", S_COLOR_RED );
		return;
	}

	clientVoted[PLAYERNUM( ent )] = vote_id;
	clientVoteChanges[PLAYERNUM( ent )]--;
	G_CallVotes_CheckState();
}

/*
* G_CallVotes_Think
*/
void G_CallVotes_Think( void ) {
	static int64_t callvotethinktimer = 0;

	if( !callvoteState.vote.callvote ) {
		callvotethinktimer = 0;
		return;
	}

	if( callvotethinktimer < svs.realtime ) {
		G_CallVotes_CheckState();
		callvotethinktimer = svs.realtime + 1000;
	}
}

/*
* G_CallVote
*/
static void G_CallVote( edict_t *ent, bool isopcall ) {
	int i;
	const char *votename;
	callvotetype_t *callvote;

	if( !isopcall && ent->s.team == TEAM_SPECTATOR && GS_IndividualGameType( &server_gs )
		&& GS_MatchState( &server_gs ) == MATCH_STATE_PLAYTIME && !GS_MatchPaused( &server_gs ) ) {
		int team, count;
		edict_t *e;

		for( count = 0, team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( !teamlist[team].numplayers ) {
				continue;
			}

			for( i = 0; i < teamlist[team].numplayers; i++ ) {
				e = game.edicts + teamlist[team].playerIndices[i];
				if( e->r.inuse && ( e->r.svflags & SVF_FAKECLIENT ) ) {
					count++;
				}
			}
		}

		if( !count ) {
			G_PrintMsg( ent, "%sSpectators cannot start a vote while a match is in progress\n", S_COLOR_RED );
			return;
		}
	}

	if( !g_callvote_enabled->integer ) {
		G_PrintMsg( ent, "%sCallvoting is disabled on this server\n", S_COLOR_RED );
		return;
	}

	if( callvoteState.vote.callvote ) {
		G_PrintMsg( ent, "%sA vote is already in progress\n", S_COLOR_RED );
		return;
	}

	votename = Cmd_Argv( 1 );
	if( !votename || !votename[0] ) {
		G_CallVotes_PrintUsagesToPlayer( ent );
		return;
	}

	if( strlen( votename ) > MAX_QPATH ) {
		G_PrintMsg( ent, "%sInvalid vote\n", S_COLOR_RED );
		G_CallVotes_PrintUsagesToPlayer( ent );
		return;
	}

	//find the actual callvote command
	for( callvote = callvotesHeadNode; callvote != NULL; callvote = callvote->next ) {
		if( callvote->name && !Q_stricmp( callvote->name, votename ) ) {
			break;
		}
	}

	//unrecognized callvote type
	if( callvote == NULL ) {
		G_PrintMsg( ent, "%sUnrecognized vote: %s\n", S_COLOR_RED, votename );
		G_CallVotes_PrintUsagesToPlayer( ent );
		return;
	}

	// wsw : pb : server admin can now disable a specific vote command (g_disable_vote_<vote name>)
	// check if vote is disabled
	if( !isopcall && Cvar_Value( va( "g_disable_vote_%s", callvote->name ) ) ) {
		G_PrintMsg( ent, "%sCallvote %s is disabled on this server\n", S_COLOR_RED, callvote->name );
		return;
	}

	// allow a second cvar specific for opcall
	if( isopcall && Cvar_Value( va( "g_disable_opcall_%s", callvote->name ) ) ) {
		G_PrintMsg( ent, "%sOpcall %s is disabled on this server\n", S_COLOR_RED, callvote->name );
		return;
	}

	if( !isopcall && ent->r.client->level.callvote_when &&
		( ent->r.client->level.callvote_when + g_callvote_cooldowntime->integer * 1000 > svs.realtime ) ) {
		G_PrintMsg( ent, "%sYou can not call a vote right now\n", S_COLOR_RED );
		return;
	}

	//we got a valid type. Get the parameters if any
	if( callvote->expectedargs != Cmd_Argc() - 2 ) {
		if( callvote->expectedargs != -1 &&
			( callvote->expectedargs != -2 || Cmd_Argc() - 2 > 0 ) ) {
			// wrong number of parametres
			G_CallVotes_PrintHelpToPlayer( ent, callvote );
			return;
		}
	}

	callvoteState.vote.argc = Cmd_Argc() - 2;
	for( i = 0; i < callvoteState.vote.argc; i++ )
		callvoteState.vote.argv[i] = G_CopyString( Cmd_Argv( i + 2 ) );

	callvoteState.vote.callvote = callvote;
	callvoteState.vote.caller = ent;
	callvoteState.vote.operatorcall = isopcall;

	//validate if there's a validation func
	if( callvote->validate != NULL && !callvote->validate( &callvoteState.vote, true ) ) {
		G_CallVotes_PrintHelpToPlayer( ent, callvote );
		G_CallVotes_Reset( false ); // free the args
		return;
	}

	//we're done. Proceed launching the election
	for( i = 0; i < server_gs.maxclients; i++ )
		G_CallVotes_ResetClient( i );
	callvoteState.timeout = svs.realtime + ( g_callvote_electtime->integer * 1000 );

	//caller is assumed to vote YES
	clientVoted[PLAYERNUM( ent )] = VOTED_YES;
	clientVoteChanges[PLAYERNUM( ent )]--;

	ent->r.client->level.callvote_when = callvoteState.timeout;

	trap_ConfigString( CS_CALLVOTE, G_CallVotes_String( &callvoteState.vote ) );

	G_PrintMsg( NULL, "%s" S_COLOR_WHITE " requested to vote " S_COLOR_YELLOW "%s\n",
				ent->r.client->netname, G_CallVotes_String( &callvoteState.vote ) );

	G_CallVotes_Think(); // make the first think
}

bool G_Callvotes_HasVoted( edict_t * ent ) {
	return clientVoted[ PLAYERNUM( ent ) ] != VOTED_NOTHING;
}

/*
* G_CallVote_Cmd
*/
void G_CallVote_Cmd( edict_t *ent ) {
	if( ent->r.svflags & SVF_FAKECLIENT ) {
		return;
	}
	G_CallVote( ent, false );
}

/*
* G_OperatorVote_Cmd
*/
void G_OperatorVote_Cmd( edict_t *ent ) {
	edict_t *other;
	int forceVote;

	if( !ent->r.client ) {
		return;
	}
	if( ent->r.svflags & SVF_FAKECLIENT ) {
		return;
	}

	if( !ent->r.client->isoperator ) {
		G_PrintMsg( ent, "You are not a game operator\n" );
		return;
	}

	if( !Q_stricmp( Cmd_Argv( 1 ), "help" ) ) {
		G_PrintMsg( ent, "Opcall can be used with all callvotes and the following commands:\n" );
		G_PrintMsg( ent, "-help\n - passvote\n- cancelvote\n- putteam\n" );
		return;
	}

	if( !Q_stricmp( Cmd_Argv( 1 ), "cancelvote" ) ) {
		forceVote = VOTED_NO;
	} else if( !Q_stricmp( Cmd_Argv( 1 ), "passvote" ) ) {
		forceVote = VOTED_YES;
	} else {
		forceVote = VOTED_NOTHING;
	}

	if( forceVote != VOTED_NOTHING ) {
		if( !callvoteState.vote.callvote ) {
			G_PrintMsg( ent, "There's no callvote to cancel.\n" );
			return;
		}

		for( other = game.edicts + 1; PLAYERNUM( other ) < server_gs.maxclients; other++ ) {
			if( !other->r.inuse || trap_GetClientState( PLAYERNUM( other ) ) < CS_SPAWNED ) {
				continue;
			}
			if( other->r.svflags & SVF_FAKECLIENT ) {
				continue;
			}

			clientVoted[PLAYERNUM( other )] = forceVote;
		}

		G_PrintMsg( NULL, "Callvote has been %s by %s\n",
					forceVote == VOTED_NO ? "cancelled" : "passed", ent->r.client->netname );
		return;
	}

	if( !Q_stricmp( Cmd_Argv( 1 ), "putteam" ) ) {
		const char *splayer = Cmd_Argv( 2 );
		const char *steam = Cmd_Argv( 3 );
		edict_t *playerEnt;
		int newTeam;

		if( !steam || !steam[0] || !splayer || !splayer[0] ) {
			G_PrintMsg( ent, "Usage 'putteam <player id > <team name>'.\n" );
			return;
		}

		if( ( newTeam = GS_TeamFromName( steam ) ) < 0 ) {
			G_PrintMsg( ent, "The team '%s' doesn't exist.\n", steam );
			return;
		}

		if( ( playerEnt = G_PlayerForText( splayer ) ) == NULL ) {
			G_PrintMsg( ent, "The player '%s' couldn't be found.\n", splayer );
			return;
		}

		if( playerEnt->s.team == newTeam ) {
			G_PrintMsg( ent, "The player '%s' is already in team '%s'.\n", playerEnt->r.client->netname, GS_TeamName( newTeam ) );
			return;
		}

		G_Teams_SetTeam( playerEnt, newTeam );
		G_PrintMsg( NULL, "%s was moved to team %s by %s.\n", playerEnt->r.client->netname, GS_TeamName( newTeam ), ent->r.client->netname );

		return;
	}

	G_CallVote( ent, true );
}

/*
* G_VoteFromScriptValidate
*/
static bool G_VoteFromScriptValidate( callvotedata_t *vote, bool first ) {
	char argsString[MAX_STRING_CHARS];
	int i;

	if( !vote || !vote->callvote || !vote->caller ) {
		return false;
	}

	snprintf( argsString, MAX_STRING_CHARS, "\"%s\"", vote->callvote->name );
	for( i = 0; i < vote->argc; i++ ) {
		Q_strncatz( argsString, " ", MAX_STRING_CHARS );
		Q_strncatz( argsString, va( " \"%s\"", vote->argv[i] ), MAX_STRING_CHARS );
	}

	return GT_asCallGameCommand( vote->caller->r.client, "callvotevalidate", argsString, vote->argc + 1 );
}

/*
* G_VoteFromScriptPassed
*/
static void G_VoteFromScriptPassed( callvotedata_t *vote ) {
	char argsString[MAX_STRING_CHARS];
	int i;

	if( !vote || !vote->callvote || !vote->caller ) {
		return;
	}

	snprintf( argsString, MAX_STRING_CHARS, "\"%s\"", vote->callvote->name );
	for( i = 0; i < vote->argc; i++ ) {
		Q_strncatz( argsString, " ", MAX_STRING_CHARS );
		Q_strncatz( argsString, va( " \"%s\"", vote->argv[i] ), MAX_STRING_CHARS );
	}

	GT_asCallGameCommand( vote->caller->r.client, "callvotepassed", argsString, vote->argc + 1 );
}

/*
* G_RegisterGametypeScriptCallvote
*/
void G_RegisterGametypeScriptCallvote( const char *name, const char *usage, const char *type, const char *help ) {
	callvotetype_t *vote;

	if( !name ) {
		return;
	}

	vote = G_RegisterCallvote( name );
	vote->expectedargs = 1;
	vote->validate = G_VoteFromScriptValidate;
	vote->execute = G_VoteFromScriptPassed;
	vote->current = NULL;
	vote->extraHelp = NULL;
	vote->argument_format = usage ? G_LevelCopyString( usage ) : NULL;
	vote->argument_type = type ? G_LevelCopyString( type ) : NULL;
	vote->help = help ? G_LevelCopyString( va( "%s", help ) ) : NULL;
}

/*
* G_CallVotes_Init
*/
void G_CallVotes_Init( void ) {
	callvotetype_t *callvote;

	g_callvote_electpercentage =    Cvar_Get( "g_vote_percent", "55", CVAR_ARCHIVE );
	g_callvote_electtime =      Cvar_Get( "g_vote_electtime", "20", CVAR_ARCHIVE );
	g_callvote_enabled =        Cvar_Get( "g_vote_allowed", "1", CVAR_ARCHIVE );
	g_callvote_maxchanges =     Cvar_Get( "g_vote_maxchanges", "3", CVAR_ARCHIVE );
	g_callvote_cooldowntime =   Cvar_Get( "g_vote_cooldowntime", "5", CVAR_ARCHIVE );

	// register all callvotes

	callvote = G_RegisterCallvote( "map" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteMapValidate;
	callvote->execute = G_VoteMapPassed;
	callvote->current = G_VoteMapCurrent;
	callvote->extraHelp = G_VoteMapExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<name>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Changes map" );

	callvote = G_RegisterCallvote( "nextmap" );
	callvote->expectedargs = 0;
	callvote->validate = NULL;
	callvote->execute = G_VoteNextMapPassed;
	callvote->current = NULL;
	callvote->extraHelp = NULL;
	callvote->argument_format = NULL;
	callvote->argument_type = NULL;
	callvote->help = G_LevelCopyString( "Jumps to the next map" );

	callvote = G_RegisterCallvote( "scorelimit" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteScorelimitValidate;
	callvote->execute = G_VoteScorelimitPassed;
	callvote->current = G_VoteScorelimitCurrent;
	callvote->extraHelp = NULL;
	callvote->argument_format = G_LevelCopyString( "<number>" );
	callvote->argument_type = G_LevelCopyString( "integer" );
	callvote->help = G_LevelCopyString( "Sets the number of frags or caps needed to win the match\nSpecify 0 to disable" );

	callvote = G_RegisterCallvote( "warmup_timelimit" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteWarmupTimelimitValidate;
	callvote->execute = G_VoteWarmupTimelimitPassed;
	callvote->current = G_VoteWarmupTimelimitCurrent;
	callvote->extraHelp = NULL;
	callvote->argument_format = G_LevelCopyString( "<minutes>" );
	callvote->argument_type = G_LevelCopyString( "integer" );
	callvote->help = G_LevelCopyString( "Sets the number of minutes after which the warmup ends\nSpecify 0 to disable" );

	callvote = G_RegisterCallvote( "maxteamplayers" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteMaxTeamplayersValidate;
	callvote->execute = G_VoteMaxTeamplayersPassed;
	callvote->current = G_VoteMaxTeamplayersCurrent;
	callvote->extraHelp = NULL;
	callvote->argument_format = G_LevelCopyString( "<number>" );
	callvote->argument_type = G_LevelCopyString( "integer" );
	callvote->help = G_LevelCopyString( "Sets the maximum number of players in one team" );

	callvote = G_RegisterCallvote( "lock" );
	callvote->expectedargs = 0;
	callvote->validate = G_VoteLockValidate;
	callvote->execute = G_VoteLockPassed;
	callvote->current = NULL;
	callvote->extraHelp = NULL;
	callvote->argument_format = NULL;
	callvote->argument_type = NULL;
	callvote->help = G_LevelCopyString( "Locks teams to disallow players joining in mid-game" );

	callvote = G_RegisterCallvote( "unlock" );
	callvote->expectedargs = 0;
	callvote->validate = G_VoteUnlockValidate;
	callvote->execute = G_VoteUnlockPassed;
	callvote->current = NULL;
	callvote->extraHelp = NULL;
	callvote->argument_format = NULL;
	callvote->argument_type = NULL;
	callvote->help = G_LevelCopyString( "Unlocks teams to allow players joining in mid-game" );

	callvote = G_RegisterCallvote( "allready" );
	callvote->expectedargs = 0;
	callvote->validate = G_VoteAllreadyValidate;
	callvote->execute = G_VoteAllreadyPassed;
	callvote->current = NULL;
	callvote->extraHelp = NULL;
	callvote->argument_format = NULL;
	callvote->argument_type = NULL;
	callvote->help = G_LevelCopyString( "Sets all players as ready so the match can start" );

	callvote = G_RegisterCallvote( "remove" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteRemoveValidate;
	callvote->execute = G_VoteRemovePassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteRemoveExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Forces player back to spectator mode" );

	callvote = G_RegisterCallvote( "kick" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteKickValidate;
	callvote->execute = G_VoteKickPassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteKickExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Removes player from the server" );

	callvote = G_RegisterCallvote( "kickban" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteKickBanValidate;
	callvote->execute = G_VoteKickBanPassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteKickBanExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Removes player from the server and bans his IP-address for 15 minutes" );

	callvote = G_RegisterCallvote( "mute" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteMuteValidate;
	callvote->execute = G_VoteMutePassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteMuteExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Disallows chat messages from the muted player" );

	callvote = G_RegisterCallvote( "vmute" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteMuteValidate;
	callvote->execute = G_VoteVMutePassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteMuteExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Disallows voice chat messages from the muted player" );

	callvote = G_RegisterCallvote( "unmute" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteUnmuteValidate;
	callvote->execute = G_VoteUnmutePassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteUnmuteExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Reallows chat messages from the unmuted player" );

	callvote = G_RegisterCallvote( "vunmute" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteUnmuteValidate;
	callvote->execute = G_VoteVUnmutePassed;
	callvote->current = NULL;
	callvote->extraHelp = G_VoteUnmuteExtraHelp;
	callvote->argument_format = G_LevelCopyString( "<player>" );
	callvote->argument_type = G_LevelCopyString( "option" );
	callvote->help = G_LevelCopyString( "Reallows voice chat messages from the unmuted player" );

	callvote = G_RegisterCallvote( "timeout" );
	callvote->expectedargs = 0;
	callvote->validate = G_VoteTimeoutValidate;
	callvote->execute = G_VoteTimeoutPassed;
	callvote->current = NULL;
	callvote->extraHelp = NULL;
	callvote->argument_format = NULL;
	callvote->argument_type = NULL;
	callvote->help = G_LevelCopyString( "Pauses the game" );

	callvote = G_RegisterCallvote( "timein" );
	callvote->expectedargs = 0;
	callvote->validate = G_VoteTimeinValidate;
	callvote->execute = G_VoteTimeinPassed;
	callvote->current = NULL;
	callvote->extraHelp = NULL;
	callvote->argument_format = NULL;
	callvote->argument_type = NULL;
	callvote->help = G_LevelCopyString( "Resumes the game if in timeout" );

	callvote = G_RegisterCallvote( "allow_uneven" );
	callvote->expectedargs = 1;
	callvote->validate = G_VoteAllowUnevenValidate;
	callvote->execute = G_VoteAllowUnevenPassed;
	callvote->current = G_VoteAllowUnevenCurrent;
	callvote->extraHelp = NULL;
	callvote->argument_format = G_LevelCopyString( "<1 or 0>" );
	callvote->argument_type = G_LevelCopyString( "bool" );

	// wsw : pb : server admin can now disable a specific callvote command (g_disable_vote_<callvote name>)
	for( callvote = callvotesHeadNode; callvote != NULL; callvote = callvote->next ) {
		Cvar_Get( va( "g_disable_vote_%s", callvote->name ), "0", CVAR_ARCHIVE );
	}

	G_CallVotes_Reset( true );
}
