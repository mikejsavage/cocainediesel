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

mempool_s *gamepool;

int meansOfDeath;
vec3_t knockbackOfDeath;

cvar_t *password;
cvar_t *g_operator_password;
cvar_t *g_select_empty;

cvar_t *filterban;

cvar_t *g_maxvelocity;
cvar_t *g_gravity;

cvar_t *sv_cheats;
cvar_t *sv_mm_enable;

cvar_t *g_maplist;
cvar_t *g_maprotation;

cvar_t *g_enforce_map_pool;
cvar_t *g_map_pool;

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

cvar_t *g_self_knockback;
cvar_t *g_knockback_scale;

cvar_t *g_respawn_delay_min;
cvar_t *g_respawn_delay_max;
cvar_t *g_deadbody_followkiller;

cvar_t *g_allow_spectator_voting;

cvar_t *g_asGC_stats;
cvar_t *g_asGC_interval;

static char *map_rotation_s = NULL;
static char **map_rotation_p = NULL;
static int map_rotation_current = -1;
static int map_rotation_count = 0;

/*
* G_GS_Trace - Used only for gameshared linking
*/
static void G_GS_Trace( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask, int timeDelta ) {
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
static void G_InitGameShared( void ) {
	int maxclients = atoi( trap_GetConfigString( CS_MAXCLIENTS ) );
	if( maxclients < 1 || maxclients > MAX_EDICTS ) {
		Com_Error( ERR_DROP, "Invalid maxclients value %i\n", maxclients );
	}

	server_gs = { };
	server_gs.module = GS_MODULE_GAME;
	server_gs.maxclients = maxclients;

	server_gs.api.PredictedEvent = G_PredictedEvent;
	server_gs.api.Trace = G_GS_Trace;
	server_gs.api.GetEntityState = G_GetEntityStateForDeltaTime;
	server_gs.api.PointContents = G_PointContents4D;
	server_gs.api.PMoveTouchTriggers = G_PMoveTouchTriggers;
	server_gs.api.GetConfigString = trap_GetConfigString;
}

void G_GamestatSetFlag( int flag, bool b ) {
	if( b ) {
		server_gs.gameState.stats[GAMESTAT_FLAGS] |= flag;
	}
	else {
		server_gs.gameState.stats[GAMESTAT_FLAGS] &= ~flag;
	}
}

/*
* G_Init
*
* This will be called when the dll is first loaded, which
* only happens when a new game is started or a save game is loaded.
*/
void G_Init( unsigned int framemsec ) {
	gamepool = _Mem_AllocPool( NULL, "Game", MEMPOOL_GAME, __FILE__, __LINE__ );

	cvar_t *g_maxentities;

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

	g_gravity = Cvar_Get( "g_gravity", va( "%i", GRAVITY ), 0 );
	developer = Cvar_Get( "developer", "0", 0 );

	// latched vars
	sv_cheats = Cvar_Get( "sv_cheats", "0", CVAR_SERVERINFO | CVAR_LATCH );
	sv_mm_enable = Cvar_Get( "sv_mm_enable", "0", CVAR_ARCHIVE | CVAR_NOSET | CVAR_SERVERINFO );

	password = Cvar_Get( "password", "", CVAR_USERINFO );
	password->modified = true; // force an update of g_needpass in G_UpdateServerInfo
	g_operator_password = Cvar_Get( "g_operator_password", "", CVAR_ARCHIVE );
	filterban = Cvar_Get( "filterban", "1", 0 );

	g_projectile_prestep = Cvar_Get( "g_projectile_prestep", va( "%i", PROJECTILE_PRESTEP ), CVAR_DEVELOPER );
	g_self_knockback = Cvar_Get( "g_self_knockback", "1.18", CVAR_DEVELOPER );
	g_knockback_scale = Cvar_Get( "g_knockback_scale", "1.0", CVAR_ARCHIVE );
	g_respawn_delay_min = Cvar_Get( "g_respawn_delay_min", "600", CVAR_DEVELOPER );
	g_respawn_delay_max = Cvar_Get( "g_respawn_delay_max", "6000", CVAR_DEVELOPER );
	g_numbots = Cvar_Get( "g_numbots", "0", CVAR_ARCHIVE );
	g_deadbody_followkiller = Cvar_Get( "g_deadbody_followkiller", "1", CVAR_DEVELOPER );
	g_maxtimeouts = Cvar_Get( "g_maxtimeouts", "2", CVAR_ARCHIVE );
	g_antilag_maxtimedelta = Cvar_Get( "g_antilag_maxtimedelta", "200", CVAR_ARCHIVE );
	g_antilag_maxtimedelta->modified = true;
	g_antilag_timenudge = Cvar_Get( "g_antilag_timenudge", "0", CVAR_ARCHIVE );
	g_antilag_timenudge->modified = true;

	g_allow_spectator_voting = Cvar_Get( "g_allow_spectator_voting", "1", CVAR_ARCHIVE );

	if( GAME_IMPORT.is_dedicated_server ) {
		g_autorecord = Cvar_Get( "g_autorecord", "1", CVAR_ARCHIVE );
		g_autorecord_maxdemos = Cvar_Get( "g_autorecord_maxdemos", "200", CVAR_ARCHIVE );
	} else {
		g_autorecord = Cvar_Get( "g_autorecord", "0", CVAR_ARCHIVE );
		g_autorecord_maxdemos = Cvar_Get( "g_autorecord_maxdemos", "0", CVAR_ARCHIVE );
	}

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

	// map list
	g_maplist = Cvar_Get( "g_maplist", "", CVAR_ARCHIVE );
	g_maprotation = Cvar_Get( "g_maprotation", "1", CVAR_ARCHIVE );

	// map pool
	g_enforce_map_pool = Cvar_Get( "g_enforce_map_pool", "0", CVAR_ARCHIVE );
	g_map_pool = Cvar_Get( "g_map_pool", "", CVAR_ARCHIVE );

	// helper cvars to show current status in serverinfo reply
	Cvar_Get( "g_match_time", "", CVAR_SERVERINFO | CVAR_READONLY );
	Cvar_Get( "g_match_score", "", CVAR_SERVERINFO | CVAR_READONLY );
	Cvar_Get( "g_needpass", "", CVAR_SERVERINFO | CVAR_READONLY );

	g_asGC_stats = Cvar_Get( "g_asGC_stats", "0", CVAR_ARCHIVE );
	g_asGC_interval = Cvar_Get( "g_asGC_interval", "10", CVAR_ARCHIVE );

	// initialize all entities for this game
	g_maxentities = Cvar_Get( "sv_maxentities", "4096", CVAR_LATCH );
	game.maxentities = g_maxentities->integer;
	game.edicts = ( edict_t * )G_Malloc( game.maxentities * sizeof( game.edicts[0] ) );

	// initialize all clients for this game
	game.clients = ( gclient_t * )G_Malloc( server_gs.maxclients * sizeof( game.clients[0] ) );

	game.numentities = server_gs.maxclients + 1;

	trap_LocateEntities( game.edicts, sizeof( game.edicts[0] ), game.numentities, game.maxentities );

	// server console commands
	G_AddServerCommands();

	// init AS engine
	G_asInitGameModuleEngine();
}

/*
* G_Shutdown
*/
void G_Shutdown( void ) {
	Com_Printf( "==== G_Shutdown ====\n" );

	GT_asCallShutdown();

	GT_asShutdownScript();
	G_asShutdownGameModuleEngine();

	SV_WriteIPList();

	G_RemoveCommands();

	G_FreeCallvotes();

	G_LevelFreePool();

	for( int i = 0; i < game.numentities; i++ ) {
		if( game.edicts[i].r.inuse ) {
			G_FreeEdict( &game.edicts[i] );
		}
	}

	G_Free( game.edicts );
	G_Free( game.clients );

	Mem_FreePool( &gamepool );
}

//======================================================================

/*
* G_UpdateMapRotation
*
* Reads current map rotation into internal list
*/
static void G_UpdateMapRotation( void ) {
	int count, i;
	bool thiswhitespace, lastwhitespace, found;
	char *p, *start;
	static const char *seps = " ,\n\r";

	if( g_maplist->modified || !map_rotation_s || !map_rotation_p ) {
		g_maplist->modified = false;

		// reread the maplist
		if( map_rotation_s ) {
			G_Free( map_rotation_s );
		}
		if( map_rotation_p ) {
			G_Free( map_rotation_p );
		}

		map_rotation_s = G_CopyString( g_maplist->string );
		map_rotation_p = NULL;
		map_rotation_current = -1;  // reset the mapcounter too
		map_rotation_count = 0;

		// count the number of tokens
		p = map_rotation_s;
		count = 0;
		lastwhitespace = true;
		start = NULL;
		found = false;
		while( *p ) {
			thiswhitespace = ( strchr( seps, *p ) != NULL ) ? true : false;
			if( lastwhitespace && !thiswhitespace ) {
				start = p;
				count++;
			} else if( thiswhitespace && !lastwhitespace && !found && start ) {
				found = true;
				for( i = 0; start + i < p; i++ ) {
					if( tolower( start[i] ) != tolower( level.mapname[i] ) ) {
						found = false;
					}
				}
				if( found ) {
					map_rotation_current = count - 1;
				}
			}

			lastwhitespace = thiswhitespace;
			p++;
		}

		if( !count ) {
			return;
		}

		// allocate the array of pointers
		map_rotation_p = ( char ** )G_Malloc( ( count + 1 ) * sizeof( *map_rotation_p ) );

		// now convert the string to tokens by nulling the separators
		p = map_rotation_s;
		count = 0;
		lastwhitespace = true;
		while( *p ) {
			thiswhitespace = ( strchr( seps, *p ) != NULL ) ? true : false;
			if( lastwhitespace && !thiswhitespace ) {
				map_rotation_p[count++] = p;
			}

			if( thiswhitespace ) {
				*p = 0;
			}

			lastwhitespace = thiswhitespace;
			p++;
		}

		// final null pointer to mark the end
		map_rotation_p[count] = NULL;

		map_rotation_count = count;
	}
}

/*
* G_MapRotationNormal
*/
static const char *G_MapRotationNormal( void ) {
	G_UpdateMapRotation();

	if( !map_rotation_count ) {
		return NULL;
	}

	map_rotation_current++;

	if( map_rotation_current >= map_rotation_count || map_rotation_p[map_rotation_current] == NULL ) {
		map_rotation_current = 0;
	}

	return map_rotation_p[map_rotation_current];
}

static const char *G_NextMap( void ) {
	if( strlen( level.callvote_map ) > 0 )
		return level.callvote_map;

	if( !( *g_maplist->string ) || g_maplist->string[0] == '\0' || g_maprotation->integer == 0 ) {
		// same map again
		return level.mapname;
	}

	const char *next = G_MapRotationNormal();
	return next ? next : level.mapname;
}

/*
* G_ExitLevel
*/
void G_ExitLevel( void ) {
	bool loadmap = true;

	level.exitNow = false;

	const char *nextmapname = G_NextMap();

	// if it's the same map see if we can restart without loading
	if( !level.hardReset && !Q_stricmp( nextmapname, level.mapname ) ) {
		G_RespawnLevel();
		loadmap = false;
	}

	if( loadmap ) {
		char command[256];
		snprintf( command, sizeof( command ), "gamemap \"%s\"\n", nextmapname );
		Cbuf_ExecuteText( EXEC_APPEND, command );
	}

	G_SnapClients();

	// clear some things before going to next level
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t *ent = game.edicts + 1 + i;
		if( !ent->r.inuse ) {
			continue;
		}

		ent->r.client->level.showscores = false;

		if( ent->health > ent->max_health ) {
			ent->health = ent->max_health;
		}

		if( loadmap ) {
			ent->r.client->connecting = true; // set all connected players as "reconnecting"
			ent->s.team = TEAM_SPECTATOR;
		}
	}
}
