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

game_locals_t game;
gs_state_t server_gs;
level_locals_t level;
spawn_temp_t st;

DamageType meansOfDeath;
Vec3 knockbackOfDeath;
int damageFlagsOfDeath;

cvar_t *sv_password;
cvar_t *g_operator_password;
cvar_t *g_select_empty;

cvar_t *filterban;

cvar_t *g_maxvelocity;

cvar_t *sv_cheats;

cvar_t *g_floodprotection_messages;
cvar_t *g_floodprotection_team;
cvar_t *g_floodprotection_seconds;
cvar_t *g_floodprotection_penalty;

cvar_t *g_inactivity_maxtime;

cvar_t *g_projectile_prestep;
cvar_t *g_numbots;
cvar_t *g_maxtimeouts;
cvar_t *g_antilag;
cvar_t *g_antilag_maxtimedelta;
cvar_t *g_antilag_timenudge;
cvar_t *g_autorecord;
cvar_t *g_autorecord_maxdemos;

cvar_t *g_deadbody_followkiller;

cvar_t *g_allow_spectator_voting;

cvar_t *g_asGC_stats;
cvar_t *g_asGC_interval;

/*
* G_GS_Trace - Used only for gameshared linking
*/
static void G_GS_Trace( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask, int timeDelta ) {
	edict_t *passent = NULL;
	if( ignore >= 0 && ignore < MAX_EDICTS ) {
		passent = &game.edicts[ignore];
	}

	G_Trace4D( tr, start, mins, maxs, end, passent, contentmask, timeDelta );
}

/*
* G_InitGameShared
* give gameshared access to some utilities
*/
static void G_InitGameShared() {
	int maxclients = atoi( PF_GetConfigString( CS_MAXCLIENTS ) );
	if( maxclients < 1 || maxclients > MAX_EDICTS ) {
		Fatal( "Invalid maxclients value %i\n", maxclients );
	}

	server_gs = { };
	server_gs.module = GS_MODULE_GAME;
	server_gs.maxclients = maxclients;

	server_gs.api.PredictedEvent = G_PredictedEvent;
	server_gs.api.PredictedFireWeapon = G_PredictedFireWeapon;
	server_gs.api.PredictedUseGadget = G_PredictedUseGadget;
	server_gs.api.Trace = G_GS_Trace;
	server_gs.api.GetEntityState = G_GetEntityStateForDeltaTime;
	server_gs.api.PointContents = G_PointContents4D;
	server_gs.api.PMoveTouchTriggers = G_PMoveTouchTriggers;
}

void G_GamestatSetFlag( int flag, bool b ) {
	if( b ) {
		server_gs.gameState.flags |= flag;
	}
	else {
		server_gs.gameState.flags &= ~flag;
	}
}

/*
* G_Init
*
* This will be called when the dll is first loaded, which
* only happens when a new game is started or a save game is loaded.
*/
void G_Init( unsigned int framemsec ) {
	Com_Printf( "==== G_Init ====\n" );

	G_InitGameShared();

	SV_ReadIPList();

	game.snapFrameTime = framemsec;
	game.frametime = game.snapFrameTime;
	game.numBots = 0;

	g_maxvelocity = Cvar_Get( "g_maxvelocity", "16000", 0 );
	if( g_maxvelocity->value < 20 ) {
		Cvar_SetValue( "g_maxvelocity", 20 );
	}

	developer = Cvar_Get( "developer", "0", 0 );

	// latched vars
	sv_cheats = Cvar_Get( "sv_cheats", is_public_build ? "0" : "1", CVAR_SERVERINFO | CVAR_LATCH );

	sv_password = Cvar_Get( "password", "", CVAR_USERINFO );
	sv_password->modified = true; // force an update of g_needpass in G_UpdateServerInfo
	g_operator_password = Cvar_Get( "g_operator_password", "", CVAR_ARCHIVE );
	filterban = Cvar_Get( "filterban", "1", 0 );

	g_projectile_prestep = Cvar_Get( "g_projectile_prestep", va( "%i", PROJECTILE_PRESTEP ), CVAR_DEVELOPER );
	g_numbots = Cvar_Get( "g_numbots", "0", CVAR_ARCHIVE );
	g_deadbody_followkiller = Cvar_Get( "g_deadbody_followkiller", "1", CVAR_DEVELOPER );
	g_maxtimeouts = Cvar_Get( "g_maxtimeouts", "2", CVAR_ARCHIVE );
	g_antilag_maxtimedelta = Cvar_Get( "g_antilag_maxtimedelta", "200", CVAR_ARCHIVE );
	g_antilag_maxtimedelta->modified = true;
	g_antilag_timenudge = Cvar_Get( "g_antilag_timenudge", "0", CVAR_ARCHIVE );
	g_antilag_timenudge->modified = true;

	g_allow_spectator_voting = Cvar_Get( "g_allow_spectator_voting", "1", CVAR_ARCHIVE );

	// flood control
	g_floodprotection_messages = Cvar_Get( "g_floodprotection_messages", "4", 0 );
	g_floodprotection_messages->modified = true;
	g_floodprotection_team = Cvar_Get( "g_floodprotection_team", "0", 0 );
	g_floodprotection_team->modified = true;
	g_floodprotection_seconds = Cvar_Get( "g_floodprotection_seconds", "4", 0 );
	g_floodprotection_seconds->modified = true;
	g_floodprotection_penalty = Cvar_Get( "g_floodprotection_delay", "2", 0 );
	g_floodprotection_penalty->modified = true;

	g_inactivity_maxtime = Cvar_Get( "g_inactivity_maxtime", "90.0", 0 );
	g_inactivity_maxtime->modified = true;

	// helper cvars to show current status in serverinfo reply
	Cvar_Get( "g_match_time", "", CVAR_SERVERINFO | CVAR_READONLY );
	Cvar_Get( "g_match_score", "", CVAR_SERVERINFO | CVAR_READONLY );
	Cvar_Get( "g_needpass", "", CVAR_SERVERINFO | CVAR_READONLY );

	g_asGC_stats = Cvar_Get( "g_asGC_stats", "0", CVAR_ARCHIVE );
	g_asGC_interval = Cvar_Get( "g_asGC_interval", "10", CVAR_ARCHIVE );

	game.maxentities = MAX_EDICTS;
	memset( game.edicts, 0, sizeof( game.edicts ) );
	memset( game.clients, 0, sizeof( game.clients ) );

	game.numentities = server_gs.maxclients + 1;

	SV_LocateEntities( game.edicts, game.numentities, game.maxentities );

	// server console commands
	G_AddServerCommands();
}

void G_Shutdown() {
	Com_Printf( "==== G_Shutdown ====\n" );

	ShutdownGametype();

	SV_WriteIPList();

	G_RemoveCommands();

	G_FreeCallvotes();

	for( int i = 0; i < game.numentities; i++ ) {
		if( game.edicts[i].r.inuse ) {
			G_FreeEdict( &game.edicts[i] );
		}
	}
}

//======================================================================

static const char *G_NextMap() {
	if( strlen( level.callvote_map ) > 0 )
		return level.callvote_map;

	// same map again
	return sv.mapname;
}

void G_ExitLevel() {
	bool loadmap = true;

	level.exitNow = false;

	const char *nextmapname = G_NextMap();

	// if it's the same map see if we can restart without loading
	if( !level.hardReset && !Q_stricmp( nextmapname, sv.mapname ) ) {
		G_RespawnLevel();
		loadmap = false;
	}

	if( loadmap ) {
		char command[256];
		snprintf( command, sizeof( command ), "map \"%s\"\n", nextmapname );
		Cbuf_ExecuteText( EXEC_APPEND, command );
	}

	G_SnapClients();

	// clear some things before going to next level
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t *ent = game.edicts + 1 + i;
		if( !ent->r.inuse ) {
			continue;
		}

		if( loadmap ) {
			ent->r.client->connecting = true; // set all connected players as "reconnecting"
			ent->s.team = TEAM_SPECTATOR;
		}
	}
}
