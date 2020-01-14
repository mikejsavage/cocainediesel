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

enum EntityFieldType {
	F_INT,
	F_FLOAT,
	F_LSTRING,      // string on disk, pointer in memory, TAG_LEVEL
	F_VECTOR,
	F_ANGLE,
	F_RGBA,
};

struct EntityField {
	const char *name;
	size_t ofs;
	EntityFieldType type;
	int flags;
};

#define FFL_SPAWNTEMP       1

static const EntityField fields[] = {
	{ "classname", FOFS( classname ), F_LSTRING },
	{ "origin", FOFS( s.origin ), F_VECTOR },
	{ "model", FOFS( model ), F_LSTRING },
	{ "model2", FOFS( model2 ), F_LSTRING },
	{ "spawnflags", FOFS( spawnflags ), F_INT },
	{ "speed", FOFS( speed ), F_FLOAT },
	{ "target", FOFS( target ), F_LSTRING },
	{ "targetname", FOFS( targetname ), F_LSTRING },
	{ "pathtarget", FOFS( pathtarget ), F_LSTRING },
	{ "killtarget", FOFS( killtarget ), F_LSTRING },
	{ "message", FOFS( message ), F_LSTRING },
	{ "team", FOFS( team ), F_LSTRING },
	{ "wait", FOFS( wait ), F_FLOAT },
	{ "delay", FOFS( delay ), F_FLOAT },
	{ "style", FOFS( style ), F_INT },
	{ "count", FOFS( count ), F_INT },
	{ "health", FOFS( health ), F_FLOAT },
	{ "light", FOFS( light ), F_FLOAT },
	{ "color", FOFS( color ), F_VECTOR },
	{ "dmg", FOFS( dmg ), F_INT },
	{ "angles", FOFS( s.angles ), F_VECTOR },
	{ "mangle", FOFS( s.angles ), F_VECTOR },
	{ "angle", FOFS( s.angles ), F_ANGLE },
	{ "mass", FOFS( mass ), F_INT },
	{ "attenuation", FOFS( attenuation ), F_FLOAT },
	{ "random", FOFS( random ), F_FLOAT },

	// temp spawn vars -- only valid when the spawn function is called
	{ "lip", STOFS( lip ), F_INT, FFL_SPAWNTEMP },
	{ "distance", STOFS( distance ), F_INT, FFL_SPAWNTEMP },
	{ "radius", STOFS( radius ), F_FLOAT, FFL_SPAWNTEMP },
	{ "height", STOFS( height ), F_INT, FFL_SPAWNTEMP },
	{ "noise", STOFS( noise ), F_LSTRING, FFL_SPAWNTEMP },
	{ "noise_start", STOFS( noise_start ), F_LSTRING, FFL_SPAWNTEMP },
	{ "noise_stop", STOFS( noise_stop ), F_LSTRING, FFL_SPAWNTEMP },
	{ "pausetime", STOFS( pausetime ), F_FLOAT, FFL_SPAWNTEMP },
	{ "gravity", STOFS( gravity ), F_LSTRING, FFL_SPAWNTEMP },
	{ "gameteam", STOFS( gameteam ), F_INT, FFL_SPAWNTEMP },
	{ "debris1", STOFS( debris1 ), F_LSTRING, FFL_SPAWNTEMP },
	{ "debris2", STOFS( debris2 ), F_LSTRING, FFL_SPAWNTEMP },
	{ "size", STOFS( size ), F_INT, FFL_SPAWNTEMP },
	{ "rgba", STOFS( rgba ), F_RGBA, FFL_SPAWNTEMP },
};

typedef struct
{
	const char *name;
	void ( *spawn )( edict_t *ent );
} spawn_t;

static void SP_worldspawn( edict_t *ent );

static spawn_t spawns[] = {
	{ "info_player_start", SP_info_player_start },
	{ "info_player_deathmatch", SP_info_player_deathmatch },

	{ "post_match_camera", SP_post_match_camera },
	{ "info_player_intermission", SP_post_match_camera },

	{ "func_plat", SP_func_plat },
	{ "func_button", SP_func_button },
	{ "func_door", SP_func_door },
	{ "func_door_rotating", SP_func_door_rotating },
	{ "func_rotating", SP_func_rotating },
	{ "func_train", SP_func_train },
	{ "func_timer", SP_func_timer },
	{ "func_wall", SP_func_wall },
	{ "func_object", SP_func_object },
	{ "func_explosive", SP_func_explosive },
	{ "func_static", SP_func_static },

	{ "trigger_always", SP_trigger_always },
	{ "trigger_once", SP_trigger_once },
	{ "trigger_multiple", SP_trigger_multiple },
	{ "trigger_push", SP_trigger_push },
	{ "trigger_hurt", SP_trigger_hurt },
	{ "trigger_elevator", SP_trigger_elevator },
	{ "trigger_gravity", SP_trigger_gravity },

	{ "target_explosion", SP_target_explosion },
	{ "target_laser", SP_target_laser },
	{ "target_position", SP_target_position },
	{ "target_print", SP_target_print },
	{ "target_delay", SP_target_delay },
	{ "target_teleporter", SP_target_teleporter },

	{ "worldspawn", SP_worldspawn },

	{ "path_corner", SP_path_corner },

	{ "trigger_teleport", SP_trigger_teleport },
	{ "misc_teleporter_dest", SP_target_position },

	{ "misc_model", SP_misc_model },
	{ "model", SP_model },

	{ "spikes", SP_spikes },
};

/*
* G_CallSpawn
*
* Finds the spawn function for the entity and calls it
*/
bool G_CallSpawn( edict_t *ent ) {
	if( !ent->classname ) {
		if( developer->integer ) {
			Com_Printf( "G_CallSpawn: NULL classname\n" );
		}
		return false;
	}

	// check normal spawn functions
	for( spawn_t s : spawns ) {
		if( !Q_stricmp( s.name, ent->classname ) ) {
			s.spawn( ent );
			return true;
		}
	}

	// see if there's a spawn definition in the gametype scripts
	if( G_asCallMapEntitySpawnScript( ent->classname, ent ) ) {
		return true; // handled by the script
	}

	if( sv_cheats->integer || developer->integer ) { // mappers load their maps with devmap
		Com_Printf( "%s doesn't have a spawn function\n", ent->classname );
	}

	return false;
}

/*
* G_GetEntitySpawnKey
*/
const char *G_GetEntitySpawnKey( const char *key, edict_t *self ) {
	static char value[MAX_TOKEN_CHARS];
	char keyname[MAX_TOKEN_CHARS];
	char *com_token;
	const char *data = NULL;

	value[0] = 0;

	if( self ) {
		data = self->spawnString;
	}

	if( data && data[0] && key && key[0] ) {
		// go through all the dictionary pairs
		while( 1 ) {
			// parse key
			com_token = COM_Parse( &data );
			if( com_token[0] == '}' ) {
				break;
			}

			if( !data ) {
				Com_Error( ERR_DROP, "G_GetEntitySpawnKey: EOF without closing brace" );
			}

			Q_strncpyz( keyname, com_token, sizeof( keyname ) );

			// parse value
			com_token = COM_Parse( &data );
			if( !data ) {
				Com_Error( ERR_DROP, "G_GetEntitySpawnKey: EOF without closing brace" );
			}

			if( com_token[0] == '}' ) {
				Com_Error( ERR_DROP, "G_GetEntitySpawnKey: closing brace without data" );
			}

			// key names with a leading underscore are used for utility comments and are immediately discarded
			if( keyname[0] == '_' ) {
				continue;
			}

			if( !Q_stricmp( key, keyname ) ) {
				Q_strncpyz( value, com_token, sizeof( value ) );
				break;
			}
		}
	}

	return value;
}

/*
* ED_NewString
*/
static char *ED_NewString( const char *string ) {
	char *newb, *new_p;
	size_t i, l;

	l = strlen( string ) + 1;
	newb = &level.map_parsed_ents[level.map_parsed_len];
	level.map_parsed_len += l;

	new_p = newb;

	for( i = 0; i < l; i++ ) {
		if( string[i] == '\\' && i < l - 1 ) {
			i++;
			if( string[i] == 'n' ) {
				*new_p++ = '\n';
			} else {
				*new_p++ = '/';
				*new_p++ = string[i];
			}
		} else {
			*new_p++ = string[i];
		}
	}

	*new_p = '\0';
	return newb;
}

/*
* ED_ParseField
*
* Takes a key/value pair and sets the binary values
* in an edict
*/
static void ED_ParseField( char *key, char *value, edict_t *ent ) {
	for( EntityField f : fields ) {
		if( Q_stricmp( f.name, key ) != 0 )
			continue;

		uint8_t *b;
		if( f.flags & FFL_SPAWNTEMP ) {
			b = (uint8_t *)&st;
		} else {
			b = (uint8_t *)ent;
		}

		switch( f.type ) {
			case F_LSTRING:
				*(char **)( b + f.ofs ) = ED_NewString( value );
				break;
			case F_INT:
				*(int *)( b + f.ofs ) = atoi( value );
				break;
			case F_FLOAT:
				*(float *)( b + f.ofs ) = atof( value );
				break;
			case F_ANGLE:
				( (float *)( b + f.ofs ) )[0] = 0;
				( (float *)( b + f.ofs ) )[1] = atof( value );
				( (float *)( b + f.ofs ) )[2] = 0;
				break;

			case F_VECTOR: {
				vec3_t vec;
				sscanf( value, "%f %f %f", &vec[0], &vec[1], &vec[2] );
				( (float *)( b + f.ofs ) )[0] = vec[0];
				( (float *)( b + f.ofs ) )[1] = vec[1];
				( (float *)( b + f.ofs ) )[2] = vec[2];
			} break;

			case F_RGBA: {
				*( int * ) ( b + f.ofs ) = COM_ReadColorRGBAString( value );
			} break;
		}
		return;
	}

	if( developer->integer ) {
		Com_Printf( "%s is not a field\n", key );
	}
}

/*
* ED_ParseEdict
*
* Parses an edict out of the given string, returning the new position
* ed should be a properly initialized empty edict.
*/
static char *ED_ParseEdict( char *data, edict_t *ent ) {
	bool init;
	char keyname[256];
	char *com_token;

	init = false;
	memset( &st, 0, sizeof( st ) );
	level.spawning_entity = ent;

	// go through all the dictionary pairs
	while( 1 ) {
		// parse key
		com_token = COM_Parse( &data );
		if( com_token[0] == '}' ) {
			break;
		}
		if( !data ) {
			Com_Error( ERR_DROP, "ED_ParseEntity: EOF without closing brace" );
		}

		Q_strncpyz( keyname, com_token, sizeof( keyname ) );

		// parse value
		com_token = COM_Parse( &data );
		if( !data ) {
			Com_Error( ERR_DROP, "ED_ParseEntity: EOF without closing brace" );
		}

		if( com_token[0] == '}' ) {
			Com_Error( ERR_DROP, "ED_ParseEntity: closing brace without data" );
		}

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if( keyname[0] == '_' ) {
			continue;
		}

		ED_ParseField( keyname, com_token, ent );
	}

	if( !init ) {
		ent->classname = NULL;
	}

	return data;
}

/*
* G_FindTeams
*
* Chain together all entities with a matching team field.
*
* All but the first will have the FL_TEAMSLAVE flag set.
* All but the last will have the teamchain field set to the next one
*/
static void G_FindTeams( void ) {
	edict_t *e, *e2, *chain;
	int i, j;
	int c, c2;

	c = 0;
	c2 = 0;
	for( i = 1, e = game.edicts + i; i < game.numentities; i++, e++ ) {
		if( !e->r.inuse ) {
			continue;
		}
		if( !e->team ) {
			continue;
		}
		if( e->flags & FL_TEAMSLAVE ) {
			continue;
		}
		chain = e;
		e->teammaster = e;
		c++;
		c2++;
		for( j = i + 1, e2 = e + 1; j < game.numentities; j++, e2++ ) {
			if( !e2->r.inuse ) {
				continue;
			}
			if( !e2->team ) {
				continue;
			}
			if( e2->flags & FL_TEAMSLAVE ) {
				continue;
			}
			if( !strcmp( e->team, e2->team ) ) {
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	if( developer->integer ) {
		Com_Printf( "%i teams with %i entities\n", c, c2 );
	}
}

void G_PrecacheMedia( void ) {
	//
	// MODELS
	//

	// THIS ORDER MUST MATCH THE DEFINES IN gs_public.h
	// you can add more, max 255

	trap_ModelIndex( "#gunblade/gunblade" );      // Weapon_Knife
	trap_ModelIndex( "#machinegun/machinegun" );    // Weapon_MachineGun
	trap_ModelIndex( "#riotgun/riotgun" );        // Weapon_Shotgun
	trap_ModelIndex( "#glauncher/glauncher" );    // Weapon_GrenadeLauncher
	trap_ModelIndex( "#rl" );    // Weapon_RocketLauncher
	trap_ModelIndex( "#plasmagun/plasmagun" );    // Weapon_Plasma
	trap_ModelIndex( "#lg" );      // Weapon_Laser
	trap_ModelIndex( "#electrobolt/electrobolt" ); // Weapon_Railgun

	//-------------------

	// FIXME: Temporarily use normal gib until the head is fixed
	trap_ModelIndex( "models/objects/gibs/gib" );

	//
	// SOUNDS
	//

	// jalfixme : most of these sounds can be played from the clients

	trap_SoundIndex( S_WORLD_WATER_IN );    // feet hitting water
	trap_SoundIndex( S_WORLD_WATER_OUT );       // feet leaving water
	trap_SoundIndex( S_WORLD_UNDERWATER );

	trap_SoundIndex( S_WORLD_SLIME_IN );
	trap_SoundIndex( S_WORLD_SLIME_OUT );
	trap_SoundIndex( S_WORLD_UNDERSLIME );

	trap_SoundIndex( S_WORLD_LAVA_IN );
	trap_SoundIndex( S_WORLD_LAVA_OUT );
	trap_SoundIndex( S_WORLD_UNDERLAVA );

	trap_SoundIndex( S_HIT_WATER );

	trap_SoundIndex( S_WEAPON_NOAMMO );

	// announcer

	// countdown
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_GET_READY_TO_FIGHT_1_to_2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_GET_READY_TO_FIGHT_1_to_2, 2 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_READY_1_to_2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_READY_1_to_2, 2 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 1, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 3, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 1, 2 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 2, 2 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 3, 2 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_FIGHT_1_to_2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_COUNTDOWN_FIGHT_1_to_2, 2 ) );

	// postmatch
	trap_SoundIndex( va( S_ANNOUNCER_POSTMATCH_GAMEOVER_1_to_2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_POSTMATCH_GAMEOVER_1_to_2, 2 ) );

	// timeout
	trap_SoundIndex( va( S_ANNOUNCER_TIMEOUT_TIMEOUT_1_to_2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_TIMEOUT_TIMEOUT_1_to_2, 2 ) );
	trap_SoundIndex( va( S_ANNOUNCER_TIMEOUT_TIMEIN_1_to_2, 1 ) );
	trap_SoundIndex( va( S_ANNOUNCER_TIMEOUT_TIMEIN_1_to_2, 2 ) );
}

/*
* G_FreeEntities
*/
static void G_FreeEntities( void ) {
	int i;

	if( !level.time ) {
		memset( game.edicts, 0, game.maxentities * sizeof( game.edicts[0] ) );
	} else {
		G_FreeEdict( world );
		for( i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
			if( game.edicts[i].r.inuse ) {
				G_FreeEdict( game.edicts + i );
			}
		}
	}

	game.numentities = server_gs.maxclients + 1;
}

/*
* G_SpawnEntities
*/
static void G_SpawnEntities( void ) {
	int i;
	edict_t *ent;
	char *token;
	char *entities;

	level.spawnedTimeStamp = svs.gametime;
	level.canSpawnEntities = true;

	G_InitBodyQueue(); // reserve some spots for dead player bodies

	entities = level.mapString;
	level.map_parsed_ents[0] = 0;
	level.map_parsed_len = 0;

	i = 0;
	ent = NULL;
	while( 1 ) {
		level.spawning_entity = NULL;

		// parse the opening brace
		token = COM_Parse( &entities );
		if( !entities ) {
			break;
		}
		if( token[0] != '{' ) {
			Com_Error( ERR_DROP, "G_SpawnMapEntities: found %s when expecting {", token );
		}

		if( !ent ) {
			ent = world;
			G_InitEdict( world );
		} else {
			ent = G_Spawn();
		}

		ent->spawnString = entities; // keep track of string definition of this entity

		entities = ED_ParseEdict( entities, ent );
		if( !ent->classname ) {
			i++;
			G_FreeEdict( ent );
			continue;
		}

		if( !G_CallSpawn( ent ) ) {
			i++;
			G_FreeEdict( ent );
			continue;
		}
	}

	// is the parsing string sane?
	assert( level.map_parsed_len < level.mapStrlen );
	level.map_parsed_ents[level.map_parsed_len] = 0;

	G_FindTeams();

	// make sure server got the edicts data
	trap_LocateEntities( game.edicts, sizeof( game.edicts[0] ), game.numentities, game.maxentities );
}

/*
* G_InitLevel
*
* Creates a server's entity / program execution context by
* parsing textual entity definitions out of an ent file.
*/
void G_InitLevel( char *mapname, char *entities, int entstrlen, int64_t levelTime ) {
	char *mapString = NULL;
	char name[MAX_CONFIGSTRING_CHARS];
	int i;

	G_asGarbageCollect( true );

	GT_asCallShutdown();

	GT_asShutdownScript();

	G_FreeCallvotes();

	GClip_ClearWorld(); // clear areas links

	if( !entities ) {
		Com_Error( ERR_DROP, "G_SpawnLevel: NULL entities string\n" );
	}

	// make a copy of the raw entities string so it's not freed with the pool
	mapString = ( char * )G_Malloc( entstrlen + 1 );
	memcpy( mapString, entities, entstrlen );
	Q_strncpyz( name, mapname, sizeof( name ) );

	// clear old data

	G_LevelInitPool( strlen( mapname ) + 1 + ( entstrlen + 1 ) * 2 + G_LEVELPOOL_BASE_SIZE );

	G_StringPoolInit();

	memset( &level, 0, sizeof( level_locals_t ) );
	memset( &server_gs.gameState, 0, sizeof( server_gs.gameState ) );

	level.time = levelTime;
	level.gravity = g_gravity->value;

	// get the strings back
	Q_strncpyz( level.mapname, name, sizeof( level.mapname ) );
	level.mapString = ( char * )G_LevelMalloc( entstrlen + 1 );
	level.mapStrlen = entstrlen;
	memcpy( level.mapString, mapString, entstrlen );
	G_Free( mapString );
	mapString = NULL;

	// make a copy of the raw entities string for parsing
	level.map_parsed_ents = ( char * )G_LevelMalloc( entstrlen + 1 );
	level.map_parsed_ents[0] = 0;

	G_FreeEntities();

	// link client fields on player ents
	for( i = 0; i < server_gs.maxclients; i++ ) {
		game.edicts[i + 1].s.number = i + 1;
		game.edicts[i + 1].r.client = &game.clients[i];
		game.edicts[i + 1].r.inuse = ( trap_GetClientState( i ) >= CS_CONNECTED ) ? true : false;
		memset( &game.clients[i].level, 0, sizeof( game.clients[0].level ) );
		game.clients[i].level.timeStamp = level.time;
	}

	// initialize game subsystems
	trap_ConfigString( CS_MAPNAME, level.mapname );
	trap_ConfigString( CS_MATCHSCORE, "" );

	G_InitGameCommands();
	G_CallVotes_Init();
	G_SpawnQueue_Init();
	G_Teams_Init();

	G_Gametype_Init();

	G_PrecacheMedia();
	G_PrecacheGameCommands(); // adding commands after this point won't update them to the client

	// start spawning entities
	G_SpawnEntities();

	GT_asCallSpawn();

	// always start in warmup match state and let the thinking code
	// revert it to wait state if empty ( so gametype based item masks are setup )
	G_Match_LaunchState( MATCH_STATE_WARMUP );

	G_asGarbageCollect( true );
}

void G_ResetLevel( void ) {
	G_FreeEdict( world );
	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		if( game.edicts[i].r.inuse ) {
			G_FreeEdict( game.edicts + i );
		}
	}

	G_SpawnEntities();

	GT_asCallSpawn();
}

void G_RespawnLevel( void ) {
	G_InitLevel( level.mapname, level.mapString, level.mapStrlen, level.time );
}

static void SP_worldspawn( edict_t *ent ) {
	ent->movetype = MOVETYPE_PUSH;
	ent->r.solid = SOLID_YES;
	ent->r.inuse = true;       // since the world doesn't use G_Spawn()
	VectorClear( ent->s.origin );
	VectorClear( ent->s.angles );
	GClip_SetBrushModel( ent, "*0" ); // sets mins / maxs and modelindex 1

	if( st.gravity ) {
		level.gravity = atof( st.gravity );
	}
}
