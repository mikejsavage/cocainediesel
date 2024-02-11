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

DamageType meansOfDeath;
Vec3 knockbackOfDeath;
int damageFlagsOfDeath;

Cvar *sv_password;

Cvar *g_maxvelocity;

Cvar *sv_cheats;

Cvar *g_floodprotection_messages;
Cvar *g_floodprotection_team;
Cvar *g_floodprotection_seconds;
Cvar *g_floodprotection_penalty;

Cvar *g_inactivity_maxtime;

Cvar *g_projectile_prestep;
Cvar *g_numbots;
Cvar *g_maxtimeouts;
Cvar *g_antilag;
Cvar *g_antilag_maxtimedelta;
Cvar *g_antilag_timenudge;
Cvar *g_autorecord;
Cvar *g_autorecord_maxdemos;

Cvar *g_allow_spectator_voting;

static trace_t G_GS_Trace( Vec3 start, MinMax3 bounds, Vec3 end, int ignore, SolidBits solid_mask, int timeDelta ) {
	edict_t * passent = NULL;
	if( ignore >= 0 && ignore < MAX_EDICTS ) {
		passent = &game.edicts[ ignore ];
	}

	return G_Trace4D( start, bounds, end, passent, solid_mask, timeDelta );
}

/*
* G_InitGameShared
* give gameshared access to some utilities
*/
static void G_InitGameShared() {
	if( sv_maxclients->integer < 1 || sv_maxclients->integer > MAX_EDICTS ) {
		Fatal( "Invalid maxclients value %d\n", sv_maxclients->integer );
	}

	server_gs = { };
	server_gs.module = GS_MODULE_GAME;
	server_gs.maxclients = sv_maxclients->integer;

	server_gs.api.PredictedEvent = G_PredictedEvent;
	server_gs.api.PredictedFireWeapon = G_PredictedFireWeapon;
	server_gs.api.PredictedAltFireWeapon = G_PredictedAltFireWeapon;
	server_gs.api.PredictedUseGadget = G_PredictedUseGadget;
	server_gs.api.Trace = G_GS_Trace;
	server_gs.api.PMoveTouchTriggers = G_PMoveTouchTriggers;
}

/*
* G_Init
*
* This will be called when the dll is first loaded, which
* only happens when a new game is started or a save game is loaded.
*/
void G_Init( unsigned int framemsec ) {
	Com_Printf( "==== G_Init ====\n" );

	TempAllocator temp = svs.frame_arena.temp();

	G_InitGameShared();

	game.snapFrameTime = framemsec;
	game.frametime = game.snapFrameTime;
	game.numBots = 0;

	g_maxvelocity = NewCvar( "g_maxvelocity", "16000" );
	if( g_maxvelocity->integer < 20 ) {
		Cvar_SetInteger( "g_maxvelocity", 20 );
	}

	sv_cheats = NewCvar( "sv_cheats", is_public_build ? "0" : "1", CvarFlag_ServerReadOnly );

	sv_password = NewCvar( "sv_password", "" );

	g_projectile_prestep = NewCvar( "g_projectile_prestep", temp.sv( "{}", PROJECTILE_PRESTEP ), CvarFlag_Developer );
	g_numbots = NewCvar( "g_numbots", "0", CvarFlag_Archive );
	g_maxtimeouts = NewCvar( "g_maxtimeouts", "2", CvarFlag_Archive );
	g_antilag_maxtimedelta = NewCvar( "g_antilag_maxtimedelta", "200", CvarFlag_Archive );
	g_antilag_maxtimedelta->modified = true;
	g_antilag_timenudge = NewCvar( "g_antilag_timenudge", "0", CvarFlag_Archive );
	g_antilag_timenudge->modified = true;

	g_allow_spectator_voting = NewCvar( "g_allow_spectator_voting", "1", CvarFlag_Archive );

	// flood control
	g_floodprotection_messages = NewCvar( "g_floodprotection_messages", "4" );
	g_floodprotection_messages->modified = true;
	g_floodprotection_team = NewCvar( "g_floodprotection_team", "0" );
	g_floodprotection_team->modified = true;
	g_floodprotection_seconds = NewCvar( "g_floodprotection_seconds", "4" );
	g_floodprotection_seconds->modified = true;
	g_floodprotection_penalty = NewCvar( "g_floodprotection_delay", "2" );
	g_floodprotection_penalty->modified = true;

	g_inactivity_maxtime = NewCvar( "g_inactivity_maxtime", "90.0" );
	g_inactivity_maxtime->modified = true;

	// helper cvars to show current status in serverinfo reply
	NewCvar( "g_needpass", "", CvarFlag_ServerInfo | CvarFlag_ReadOnly );

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

	const char * nextmapname = G_NextMap();

	// if it's the same map see if we can restart without loading
	if( StrEqual( nextmapname, sv.mapname ) ) {
		G_RespawnLevel();
		loadmap = false;
	}

	if( loadmap ) {
		TempAllocator temp = svs.frame_arena.temp();
		Cmd_Execute( &temp, "map \"{}\"", nextmapname );
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
			ent->s.team = Team_None;
		}
	}
}
