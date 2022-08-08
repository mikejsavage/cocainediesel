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
#include "qcommon/maplist.h"
#include "qcommon/string.h"

static Optional< bool > clientVote[MAX_CLIENTS];
static s64 lastclientVote[MAX_CLIENTS];

Cvar *g_callvote_electtime;          // in seconds
Cvar *g_callvote_enabled;

static constexpr float callvoteElectPercent = 55.0f / 100.0f;
static constexpr int callvoteCooldown = 5000;
static constexpr int voteChangeCooldown = 500;

// Data that can be used by the vote specific functions
struct callvotetype_t;

struct callvotedata_t {
	edict_t *caller;
	const callvotetype_t *callvote;
	int argc;
	char *argv[MAX_STRING_TOKENS];
	String< MAX_CONFIGSTRING_CHARS > string; // can be used to overwrite the displayed vote string
	int target;
};

struct callvotetype_t {
	const char *name;
	int expectedargs;               // -1 = any amount, -2 = any amount except 0
	bool ( *validate )( callvotedata_t *data, bool first );
	void ( *execute )( callvotedata_t *vote );
	const char *( *current )();
	void ( *extraHelp )( edict_t * ent, String< MAX_STRING_CHARS > * msg );
	const char *argument_format;
	const char *help;
};

// Data that will only be used by the common callvote functions
struct callvotestate_t {
	int64_t timeout;           // time to finish
	callvotedata_t vote;
};

static callvotestate_t callvoteState;

static void ListPlayersExcept( edict_t * ent, String< MAX_STRING_CHARS > * msg, bool include_specs ) {
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		const edict_t * e = &game.edicts[ i + 1 ];
		if( !e->r.inuse || e == ent )
			continue;
		if( !include_specs && e->s.team == Team_None )
			continue;
		msg->append( "\n{}: {}", i, e->r.client->netname );
	}
}

/*
* map
*/

static void G_VoteMapExtraHelp( edict_t * ent, String< MAX_STRING_CHARS > * msg ) {
	TempAllocator temp = svs.frame_arena.temp();
	RefreshMapList( &temp );

	msg->append( "\n- Available maps:" );

	Span< const char * > maps = GetMapList();
	for( const char * map : maps ) {
		msg->append( " {}", map );
	}
}

static bool G_VoteMapValidate( callvotedata_t *data, bool first ) {
	char mapname[MAX_CONFIGSTRING_CHARS];

	if( !first ) { // map can't become invalid while voting
		return true;
	}

	if( strlen( "maps/" ) + strlen( data->argv[0] ) + strlen( ".bsp" ) >= MAX_CONFIGSTRING_CHARS ) {
		G_PrintMsg( data->caller, "%sToo long map name\n", S_COLOR_RED );
		return false;
	}

	Q_strncpyz( mapname, data->argv[0], sizeof( mapname ) );
	COM_SanitizeFilePath( mapname );

	if( StrEqual( sv.mapname, mapname ) ) {
		G_PrintMsg( data->caller, "%sYou are already on that map\n", S_COLOR_RED );
		return false;
	}

	if( !COM_ValidateRelativeFilename( mapname ) || strchr( mapname, '/' ) || strchr( mapname, '.' ) ) {
		G_PrintMsg( data->caller, "%sInvalid map name\n", S_COLOR_RED );
		return false;
	}

	if( MapExists( mapname ) ) {
		data->string.format( "{}", mapname );
		return true;
	}

	G_PrintMsg( data->caller, "%sNo such map available on this server\n", S_COLOR_RED );

	return false;
}

static void G_VoteMapPassed( callvotedata_t *vote ) {
	Q_strncpyz( level.callvote_map, vote->argv[0], sizeof( level.callvote_map ) );
	G_EndMatch();
}

static const char *G_VoteMapCurrent() {
	return sv.mapname;
}

/*
* start
*/

static bool G_VoteStartValidate( callvotedata_t *vote, bool first ) {
	int notreadys = 0;
	edict_t *ent;

	if( server_gs.gameState.match_state >= MatchState_Countdown ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sThe game is not in warmup mode\n", S_COLOR_RED );
		}
		return false;
	}

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			continue;
		}

		if( ent->s.team > Team_None && !level.ready[PLAYERNUM( ent )] ) {
			notreadys++;
		}
	}

	if( !notreadys ) {
		if( first ) {
			G_PrintMsg( vote->caller, "%sMatch is already starting\n", S_COLOR_RED );
		}
		return false;
	}

	return true;
}

static void G_VoteStartPassed( callvotedata_t *vote ) {
	for( edict_t * ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			continue;
		}

		if( ent->s.team > Team_None && !level.ready[PLAYERNUM( ent )] ) {
			level.ready[PLAYERNUM( ent )] = true;
			G_Match_CheckReadys();
		}
	}
}

/*
* spectate
*/

static void G_VoteSpectateExtraHelp( edict_t * ent, String< MAX_STRING_CHARS > * msg ) {
	ListPlayersExcept( ent, msg, false );
}

static bool G_VoteSpectateValidate( callvotedata_t *vote, bool first ) {
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
		} else if( tokick->s.team == Team_None ) {
			G_PrintMsg( vote->caller, "Player %s%s%s is already spectator.\n", S_COLOR_WHITE,
						tokick->r.client->netname, S_COLOR_RED );

			return false;
		} else {
			// we save the player id to be removed, so we don't later get confused by new ids or players changing names
			vote->target = who;
		}
	} else {
		who = vote->target;
	}

	if( !game.edicts[who + 1].r.inuse || game.edicts[who + 1].s.team == Team_None ) {
		return false;
	}

	vote->string.format( "{}", game.edicts[who + 1].r.client->netname );
	return true;
}

static void G_VoteSpectatePassed( callvotedata_t *vote ) {
	edict_t * ent = &game.edicts[vote->target + 1];

	// may have disconnect along the callvote time
	if( !ent->r.inuse || !ent->r.client || ent->s.team == Team_None ) {
		return;
	}

	G_PrintMsg( NULL, "%s moved to spectator.\n", ent->r.client->netname );

	G_Teams_SetTeam( ent, Team_None );
}


/*
* kick
*/

static void G_VoteKickExtraHelp( edict_t * ent, String< MAX_STRING_CHARS > * msg ) {
	ListPlayersExcept( ent, msg, true );
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
			// we save the player id to be kicked, so we don't later get
			//confused by new ids or players changing names
			vote->target = who;
		} else {
			G_PrintMsg( vote->caller, "%sNo such player\n", S_COLOR_RED );
			return false;
		}
	} else {
		who = vote->target;
	}

	if( !game.edicts[who + 1].r.inuse )
		return false;

	vote->string.format( "{}", game.edicts[who + 1].r.client->netname );
	return true;
}

static void G_VoteKickPassed( callvotedata_t *vote ) {
	edict_t * ent = &game.edicts[vote->target + 1];
	if( !ent->r.inuse || !ent->r.client ) { // may have disconnected along the callvote time
		return;
	}

	PF_DropClient( ent, "Kicked" );
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
	level.timeout.caller = Team_None;
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

//===================================================================
// Common functions
//===================================================================

static callvotetype_t votes[] = {
	{
		"map",
		1,
		G_VoteMapValidate,
		G_VoteMapPassed,
		G_VoteMapCurrent,
		G_VoteMapExtraHelp,
		"<name>",
		"Changes map",
	},

	{
		"start",
		0,
		G_VoteStartValidate,
		G_VoteStartPassed,
		NULL,
		NULL,
		NULL,
		"Sets all players as ready so the match can start",
	},

	{
		"spectate",
		1,
		G_VoteSpectateValidate,
		G_VoteSpectatePassed,
		NULL,
		G_VoteSpectateExtraHelp,
		"<player>",
		"Forces player back to spectator mode",
	},

	{
		"kick",
		1,
		G_VoteKickValidate,
		G_VoteKickPassed,
		NULL,
		G_VoteKickExtraHelp,
		"<player>",
		"Removes player from the server",
	},

	{
		"timeout",
		0,
		G_VoteTimeoutValidate,
		G_VoteTimeoutPassed,
		NULL,
		NULL,
		NULL,
		"Pauses the game",
	},

	{
		"timein",
		0,
		G_VoteTimeinValidate,
		G_VoteTimeinPassed,
		NULL,
		NULL,
		NULL,
		"Resumes the game if in timeout",
	},
};

void G_CallVotes_ResetClient( int n ) {
	clientVote[n] = NONE;
}

static void G_CallVotes_Reset( bool vote_happened ) {
	if( vote_happened && callvoteState.vote.caller && callvoteState.vote.caller->r.client ) {
		callvoteState.vote.caller->r.client->level.callvote_when = svs.realtime;
	}

	callvoteState.vote.callvote = NULL;
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		G_CallVotes_ResetClient( i );
	}
	callvoteState.timeout = 0;

	callvoteState.vote.caller = NULL;
	callvoteState.vote.string.clear();
	callvoteState.vote.target = 0;
	for( int i = 0; i < callvoteState.vote.argc; i++ ) {
		if( callvoteState.vote.argv[i] ) {
			FREE( sys_allocator, callvoteState.vote.argv[i] );
		}
	}

	Q_strncpyz( server_gs.gameState.callvote, "", sizeof( server_gs.gameState.callvote ) );

	server_gs.gameState.callvote_required_votes = 0;
	server_gs.gameState.callvote_yes_votes = 0;

	memset( &callvoteState, 0, sizeof( callvoteState ) );
}

void G_FreeCallvotes() {
	G_CallVotes_Reset( false );
}

static void G_CallVotes_PrintUsagesToPlayer( edict_t *ent ) {
	G_PrintMsg( ent, "Available votes:\n" );
	for( callvotetype_t callvote : votes ) {
		if( callvote.argument_format ) {
			G_PrintMsg( ent, " %s %s\n", callvote.name, callvote.argument_format );
		} else {
			G_PrintMsg( ent, " %s\n", callvote.name );
		}
	}
}

static void G_CallVotes_PrintHelpToPlayer( edict_t *ent, const callvotetype_t *callvote ) {
	String< MAX_STRING_CHARS > msg( "Usage: {}", callvote->name );

	if( callvote->argument_format != NULL ) {
		msg.append( " {}", callvote->argument_format );
	}

	if( callvote->current != NULL ) {
		msg.append( "\nCurrent: {}", callvote->current() );
	}

	if( callvote->help != NULL ) {
		msg.append( "\n- {}", callvote->help );
	}

	if( callvote->extraHelp != NULL ) {
		callvote->extraHelp( ent, &msg );
	}

	G_PrintMsg( ent, "%s\n", msg.c_str() );
}

static const char *G_CallVotes_ArgsToString( const callvotedata_t *vote ) {
	static char argstring[MAX_STRING_CHARS];

	argstring[0] = 0;

	if( vote->argc > 0 ) {
		Q_strncatz( argstring, vote->argv[0], sizeof( argstring ) );
	}
	for( int i = 1; i < vote->argc; i++ ) {
		Q_strncatz( argstring, " ", sizeof( argstring ) );
		Q_strncatz( argstring, vote->argv[i], sizeof( argstring ) );
	}

	return argstring;
}

static const char *G_CallVotes_Arguments( const callvotedata_t *vote ) {
	const char *arguments;
	if( vote->string.length() > 0 ) {
		arguments = vote->string.c_str();
	} else {
		arguments = G_CallVotes_ArgsToString( vote );
	}
	return arguments;
}

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

static void G_CallVotes_CheckState() {
	if( !callvoteState.vote.callvote ) {
		return;
	}

	if( callvoteState.vote.callvote->validate != NULL && !callvoteState.vote.callvote->validate( &callvoteState.vote, false ) ) {
		G_PrintMsg( NULL, "Vote %s%s%s canceled\n", S_COLOR_YELLOW, G_CallVotes_String( &callvoteState.vote ), S_COLOR_WHITE );
		G_CallVotes_Reset( true );
		return;
	}

	int yeses = 0, voters = 0, noes = 0;

	for( const edict_t * ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
		gclient_t *client = ent->r.client;

		if( !ent->r.inuse || PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			continue;
		}

		if( ent->s.svflags & SVF_FAKECLIENT ) {
			continue;
		}

		// ignore inactive players unless they have voted
		if( g_inactivity_maxtime->number > 0 ) {
			bool inactive = client->level.last_activity + g_inactivity_maxtime->number * 1000 < level.time;
			if( inactive && !clientVote[PLAYERNUM( ent )].exists )
				continue;
		}

		voters++;
		if( clientVote[PLAYERNUM( ent )].exists ) {
			if( clientVote[PLAYERNUM( ent )].value ) {
				yeses++;
			}
			else {
				noes++;
			}
		}
	}

	int needvotes = int( voters * callvoteElectPercent ) + 1;
	if( yeses >= needvotes ) {
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

	server_gs.gameState.callvote_required_votes = needvotes;
	server_gs.gameState.callvote_yes_votes = yeses;
}

static void Vote( edict_t * ent, bool vote ) {
	if( !ent->r.client ) {
		return;
	}

	if( ent->s.svflags & SVF_FAKECLIENT ) {
		return;
	}

	if( !callvoteState.vote.callvote ) {
		G_PrintMsg( ent, "%sThere's no vote in progress\n", S_COLOR_RED );
		return;
	}

	if( clientVote[PLAYERNUM( ent )].exists && clientVote[PLAYERNUM( ent )].value == vote ) {
		return;
	}

	if( ( svs.realtime - lastclientVote[PLAYERNUM( ent )] ) < voteChangeCooldown ) {
		G_PrintMsg( ent, "%sYou can't vote that fast\n", S_COLOR_RED );
		return;
	}

	clientVote[PLAYERNUM( ent )] = vote;
	lastclientVote[PLAYERNUM( ent )] = svs.realtime;

	G_CallVotes_CheckState();
}

void G_CallVotes_VoteYes( edict_t * ent, msg_t args ) {
	Vote( ent, true );
}

void G_CallVotes_VoteNo( edict_t * ent, msg_t args ) {
	Vote( ent, false );
}

void G_CallVotes_Think() {
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

bool G_Callvotes_HasVoted( edict_t * ent ) {
	return clientVote[ PLAYERNUM( ent ) ].exists;
}

void G_CallVote_Cmd( edict_t *ent, msg_t args ) {
	Cmd_TokenizeString( MSG_ReadString( &args ) );
	if( ent->s.svflags & SVF_FAKECLIENT ) {
		return;
	}

	if( !g_callvote_enabled->integer ) {
		G_PrintMsg( ent, "%sCallvoting is disabled on this server\n", S_COLOR_RED );
		return;
	}

	if( callvoteState.vote.callvote ) {
		G_PrintMsg( ent, "%sA vote is already in progress\n", S_COLOR_RED );
		return;
	}

	const char * votename = Cmd_Argv( 0 );
	if( !votename || !votename[0] ) {
		G_CallVotes_PrintUsagesToPlayer( ent );
		return;
	}

	//find the actual callvote command
	const callvotetype_t * callvote = NULL;
	for( const callvotetype_t & vote : votes ) {
		if( StrCaseEqual( vote.name, votename ) ) {
			callvote = &vote;
			break;
		}
	}

	//unrecognized callvote type
	if( callvote == NULL ) {
		G_PrintMsg( ent, "%sUnrecognized vote: %s\n", S_COLOR_RED, votename );
		G_CallVotes_PrintUsagesToPlayer( ent );
		return;
	}

	if( ent->r.client->level.callvote_when && ent->r.client->level.callvote_when + callvoteCooldown > svs.realtime ) {
		G_PrintMsg( ent, "%sYou can not call a vote right now\n", S_COLOR_RED );
		return;
	}

	//we got a valid type. Get the parameters if any
	if( callvote->expectedargs != Cmd_Argc() - 1 ) {
		if( callvote->expectedargs != -1 &&
			( callvote->expectedargs != -2 || Cmd_Argc() - 1 > 0 ) ) {
			// wrong number of parametres
			G_CallVotes_PrintHelpToPlayer( ent, callvote );
			return;
		}
	}

	callvoteState.vote.argc = Cmd_Argc() - 1;
	for( int i = 0; i < callvoteState.vote.argc; i++ ) {
		callvoteState.vote.argv[i] = CopyString( sys_allocator, Cmd_Argv( i + 1 ) );
	}

	callvoteState.vote.callvote = callvote;
	callvoteState.vote.caller = ent;

	//validate if there's a validation func
	if( callvote->validate != NULL && !callvote->validate( &callvoteState.vote, true ) ) {
		G_CallVotes_PrintHelpToPlayer( ent, callvote );
		G_CallVotes_Reset( false ); // free the args
		return;
	}

	//we're done. Proceed launching the election
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		G_CallVotes_ResetClient( i );
	}
	callvoteState.timeout = svs.realtime + ( g_callvote_electtime->integer * 1000 );

	//caller is assumed to vote YES
	clientVote[PLAYERNUM( ent )] = true;

	ent->r.client->level.callvote_when = callvoteState.timeout;

	Q_strncpyz( server_gs.gameState.callvote, G_CallVotes_String( &callvoteState.vote ), sizeof( server_gs.gameState.callvote ) );

	G_PrintMsg( NULL, "%s" S_COLOR_WHITE " requested to vote " S_COLOR_YELLOW "%s\n",
				ent->r.client->netname, G_CallVotes_String( &callvoteState.vote ) );

	G_CallVotes_Think(); // make the first think
}

void G_CallVotes_Init() {
	g_callvote_electtime = NewCvar( "g_vote_electtime", "20", CvarFlag_Archive );
	g_callvote_enabled = NewCvar( "g_vote_allowed", "1", CvarFlag_Archive );

	G_CallVotes_Reset( true );
}
