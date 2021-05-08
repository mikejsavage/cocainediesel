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

#include "qcommon/base.h"
#include "qcommon/compression.h"
#include "qcommon/cmodel.h"
#include "qcommon/fs.h"
#include "game/g_local.h"

enum EntityFieldType {
	F_INT,
	F_FLOAT,
	F_LSTRING,      // string on disk, pointer in memory, TAG_LEVEL
	F_HASH,
	F_ASSET,
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
	{ "model", FOFS( s.model ), F_ASSET },
	{ "model2", FOFS( s.model2 ), F_ASSET },
	{ "material", FOFS( s.material ), F_ASSET },
	{ "color", FOFS( s.color ), F_RGBA },
	{ "spawnflags", FOFS( spawnflags ), F_INT },
	{ "speed", FOFS( speed ), F_FLOAT },
	{ "target", FOFS( target ), F_LSTRING },
	{ "targetname", FOFS( targetname ), F_LSTRING },
	{ "pathtarget", FOFS( pathtarget ), F_LSTRING },
	{ "killtarget", FOFS( killtarget ), F_LSTRING },
	{ "message", FOFS( message ), F_LSTRING },
	{ "wait", FOFS( wait ), F_FLOAT },
	{ "delay", FOFS( delay ), F_FLOAT },
	{ "style", FOFS( style ), F_INT },
	{ "count", FOFS( count ), F_INT },
	{ "health", FOFS( health ), F_FLOAT },
	{ "dmg", FOFS( dmg ), F_INT },
	{ "angles", FOFS( s.angles ), F_VECTOR },
	{ "angle", FOFS( s.angles ), F_ANGLE },
	{ "mass", FOFS( mass ), F_INT },
	{ "random", FOFS( random ), F_FLOAT },

	// temp spawn vars -- only valid when the spawn function is called
	{ "lip", STOFS( lip ), F_INT, FFL_SPAWNTEMP },
	{ "distance", STOFS( distance ), F_INT, FFL_SPAWNTEMP },
	{ "radius", STOFS( radius ), F_FLOAT, FFL_SPAWNTEMP },
	{ "height", STOFS( height ), F_INT, FFL_SPAWNTEMP },
	{ "noise", STOFS( noise ), F_HASH, FFL_SPAWNTEMP },
	{ "noise_start", STOFS( noise_start ), F_HASH, FFL_SPAWNTEMP },
	{ "noise_stop", STOFS( noise_stop ), F_HASH, FFL_SPAWNTEMP },
	{ "pausetime", STOFS( pausetime ), F_FLOAT, FFL_SPAWNTEMP },
	{ "gameteam", STOFS( gameteam ), F_INT, FFL_SPAWNTEMP },
	{ "size", STOFS( size ), F_INT, FFL_SPAWNTEMP },
	{ "spawn_probability", STOFS( spawn_probability ), F_FLOAT, FFL_SPAWNTEMP },
};

struct spawn_t {
	const char *name;
	void ( *spawn )( edict_t *ent );
};

static void SP_worldspawn( edict_t *ent );

static spawn_t spawns[] = {
	{ "post_match_camera", SP_post_match_camera },

	{ "func_plat", SP_func_plat },
	{ "func_button", SP_func_button },
	{ "func_door", SP_func_door },
	{ "func_door_rotating", SP_func_door_rotating },
	{ "func_rotating", SP_func_rotating },
	{ "func_train", SP_func_train },
	{ "func_timer", SP_func_timer },
	{ "func_wall", SP_func_wall },
	{ "func_explosive", SP_func_explosive },
	{ "func_static", SP_func_static },

	{ "trigger_always", SP_trigger_always },
	{ "trigger_once", SP_trigger_once },
	{ "trigger_multiple", SP_trigger_multiple },
	{ "trigger_push", SP_trigger_push },
	{ "trigger_hurt", SP_trigger_hurt },
	{ "trigger_elevator", SP_trigger_elevator },

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

	{ "model", SP_model },
	{ "decal", SP_decal },

	{ "spike", SP_spike },
	{ "spikes", SP_spikes },

	{ "speaker_wall", SP_speaker_wall },
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
* ED_NewString
*/
static char *ED_NewString( Span< const char > token ) {
	char * newb = &level.map_parsed_ents[ level.map_parsed_len ];
	level.map_parsed_len += token.n + 1;

	char * new_p = newb;

	for( size_t i = 0; i < token.n; i++ ) {
		if( token[ i ] == '\\' && i < token.n - 1 ) {
			i++;
			if( token[ i ] == 'n' ) {
				*new_p++ = '\n';
			} else {
				*new_p++ = '/';
				*new_p++ = token[ i ];
			}
		} else {
			*new_p++ = token[ i ];
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
static void ED_ParseField( Span< const char > key, Span< const char > value, edict_t * ent ) {
	for( EntityField f : fields ) {
		if( !StrCaseEqual( f.name, key ) )
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
			case F_HASH:
				*(StringHash *)( b + f.ofs ) = StringHash( value );
				break;
			case F_ASSET:
				if( value[ 0 ] == '*' ) {
					*(StringHash *)( b + f.ofs ) = StringHash( Hash64( value.ptr, value.n, svs.cms->base_hash ) );
				}
				else {
					*(StringHash *)( b + f.ofs ) = StringHash( value );
				}
				break;
			case F_INT:
				*(int *)( b + f.ofs ) = SpanToInt( value, 0 );
				break;
			case F_FLOAT:
				*(float *)( b + f.ofs ) = SpanToFloat( value, 0.0f );
				break;
			case F_ANGLE:
				*(Vec3 *)( b + f.ofs ) = Vec3( 0.0f, SpanToFloat( value, 0.0f ), 0.0f );
				break;

			case F_VECTOR: {
				Vec3 vec;
				vec.x = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				vec.y = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				vec.z = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				*(Vec3 *)( b + f.ofs ) = vec;
			} break;

			case F_RGBA: {
				RGBA8 rgba;
				rgba.r = ParseInt( &value, 255, Parse_StopOnNewLine );
				rgba.g = ParseInt( &value, 255, Parse_StopOnNewLine );
				rgba.b = ParseInt( &value, 255, Parse_StopOnNewLine );
				rgba.a = ParseInt( &value, 255, Parse_StopOnNewLine );
				*(RGBA8 *)( b + f.ofs ) = rgba;
			} break;
		}
		return;
	}

	if( developer->integer ) {
		Com_GGPrint( "{} is not a field", key );
	}
}

static void ED_ParseEntity( Span< const char > * cursor, edict_t * ent ) {
	memset( &st, 0, sizeof( st ) );
	st.spawn_probability = 1.0f;

	while( true ) {
		Span< const char > key = ParseToken( cursor, Parse_DontStopOnNewLine );
		if( key == "}" )
			break;
		if( key.ptr == NULL ) {
			Com_Error( ERR_DROP, "ED_ParseEntity: EOF without closing brace" );
		}

		Span< const char > value = ParseToken( cursor, Parse_StopOnNewLine );
		if( value.ptr == NULL ) {
			Com_Error( ERR_DROP, "ED_ParseEntity: EOF without closing brace" );
		}
		if( value == "}" ) {
			Com_Error( ERR_DROP, "ED_ParseEntity: closing brace without data" );
		}

		ED_ParseField( key, value, ent );
	}
}

/*
* G_FreeEntities
*/
static void G_FreeEntities() {
	if( !level.time ) {
		memset( game.edicts, 0, game.maxentities * sizeof( game.edicts[0] ) );
	}
	else {
		G_FreeEdict( world );
		for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
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
static void G_SpawnEntities() {
	level.spawnedTimeStamp = svs.gametime;
	level.canSpawnEntities = true;

	level.map_parsed_ents[0] = 0;
	level.map_parsed_len = 0;

	if( Hash64( level.mapString ) != svs.ent_string_checksum ) {
		Com_Printf( "oh mein gott\n" );
	}

	Span< const char > cursor = MakeSpan( level.mapString );
	edict_t * ent = NULL;

	while( true ) {
		// parse the opening brace
		Span< const char > brace = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( brace == "" )
			break;
		if( brace != "{" ) {
			Com_Error( ERR_DROP, "G_SpawnEntities: entity string doesn't begin with {" );
		}

		if( ent == NULL ) {
			ent = world;
			G_InitEdict( world );
		}
		else {
			ent = G_Spawn();
		}

		ED_ParseEntity( &cursor, ent );

		bool ok = true;
		bool rng = random_p( &svs.rng, st.spawn_probability );
		ok = ok && ent->classname != NULL;
		ok = ok && rng;
		ok = ok && G_CallSpawn( ent );

		if( !rng ) {
			Com_Printf( "shit's fucked\n" );
		}

		if( !ok ) {
			G_FreeEdict( ent );
		}
	}

	// is the parsing string sane?
	assert( level.map_parsed_len < level.mapStrlen );
	level.map_parsed_ents[level.map_parsed_len] = 0;

	// make sure server got the edicts data
	SV_LocateEntities( game.edicts, game.numentities, game.maxentities );
}

/*
* G_InitLevel
*
* Creates a server's entity / program execution context by
* parsing textual entity definitions out of an ent file.
*/
void G_InitLevel( const char *mapname, int64_t levelTime ) {
	TempAllocator temp = svs.frame_arena.temp();

	G_asGarbageCollect( true );

	GT_asCallShutdown();

	GT_asShutdownScript();

	GClip_ClearWorld(); // clear areas links

	memset( &level, 0, sizeof( level_locals_t ) );
	memset( &server_gs.gameState, 0, sizeof( server_gs.gameState ) );

	const char * path = temp( "maps/{}", mapname );
	server_gs.gameState.map = StringHash( path );
	server_gs.gameState.map_checksum = svs.cms->checksum;

	// clear old data
	int entstrlen = CM_EntityStringLen( svs.cms );
	G_LevelInitPool( strlen( mapname ) + 1 + ( entstrlen + 1 ) * 2 + G_LEVELPOOL_BASE_SIZE );
	G_StringPoolInit();

	level.time = levelTime;

	// get the strings back
	level.mapString = ( char * )G_LevelMalloc( entstrlen + 1 );
	level.mapStrlen = entstrlen;
	strcpy( level.mapString, CM_EntityString( svs.cms ) );

	// make a copy of the raw entities string for parsing
	level.map_parsed_ents = ( char * )G_LevelMalloc( entstrlen + 1 );

	G_FreeEntities();

	// link client fields on player ents
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		game.edicts[i + 1].s.number = i + 1;
		game.edicts[i + 1].r.client = &game.clients[i];
		game.edicts[i + 1].r.inuse = PF_GetClientState( i ) >= CS_CONNECTED;
		memset( &game.clients[i].level, 0, sizeof( game.clients[0].level ) );
		game.clients[i].level.timeStamp = level.time;
	}

	// initialize game subsystems
	PF_ConfigString( CS_MATCHSCORE, "" );

	G_InitGameCommands();

	G_SpawnQueue_Init();
	G_Teams_Init();

	G_Gametype_Init();

	G_PrecacheGameCommands(); // adding commands after this point won't update them to the client

	// start spawning entities
	G_SpawnEntities();

	GT_asCallSpawn();

	// always start in warmup match state and let the thinking code
	// revert it to wait state if empty ( so gametype based item masks are setup )
	G_Match_LaunchState( MATCH_STATE_WARMUP );

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		if( game.edicts[ i + 1 ].r.inuse ) {
			G_Teams_JoinAnyTeam( &game.edicts[ i + 1 ], true );
		}
	}

	G_asGarbageCollect( true );
}

void G_ResetLevel() {
	G_FreeEdict( world );
	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		if( game.edicts[i].r.inuse ) {
			G_FreeEdict( game.edicts + i );
		}
	}

	G_SpawnEntities();

	GT_asCallSpawn();
}

void G_RespawnLevel() {
	G_InitLevel( sv.mapname, level.time );
}

void G_LoadMap( const char * name ) {
	TempAllocator temp = svs.frame_arena.temp();

	if( svs.cms != NULL ) {
		CM_Free( CM_Server, svs.cms );
	}

	Q_strncpyz( sv.mapname, name, sizeof( sv.mapname ) );

	const char * base_path = temp( "maps/{}", name );

	Span< u8 > data;
	defer { FREE( sys_allocator, data.ptr ); };

	const char * bsp_path = temp( "{}/base/maps/{}.bsp", RootDirPath(), name );
	data = ReadFileBinary( sys_allocator, bsp_path );

	if( data.ptr == NULL ) {
		const char * zst_path = temp( "{}.zst", bsp_path );
		Span< u8 > compressed = ReadFileBinary( sys_allocator, zst_path );
		defer { FREE( sys_allocator, compressed.ptr ); };
		if( compressed.ptr == NULL ) {
			Com_Error( ERR_FATAL, "Couldn't find map %s", name );
		}

		bool ok = Decompress( zst_path, sys_allocator, compressed, &data );
		if( !ok ) {
			Com_Error( ERR_FATAL, "Couldn't decompress %s", zst_path );
		}
	}

	u64 base_hash = Hash64( base_path );
	svs.cms = CM_LoadMap( CM_Server, data, base_hash );
	svs.ent_string_checksum = Hash64( CM_EntityString( svs.cms ), CM_EntityStringLen( svs.cms ) );

	server_gs.gameState.map = StringHash( base_hash );
	server_gs.gameState.map_checksum = svs.cms->checksum;
}

void G_HotloadMap() {
	char map[ ARRAY_COUNT( sv.mapname ) ];
	Q_strncpyz( map, sv.mapname, sizeof( map ) );
	G_LoadMap( map );
}

// TODO: game module init is a mess and I'm not sure how to clean this up
void G_Aasdf() {
	GClip_ClearWorld(); // clear areas links

	int len = CM_EntityStringLen( svs.cms );
	G_LevelInitPool( strlen( sv.mapname ) + 1 + ( len + 1 ) * 2 + G_LEVELPOOL_BASE_SIZE );
	G_StringPoolInit();

	level.mapString = ( char * )G_LevelMalloc( len + 1 );
	level.mapStrlen = len;

	strcpy( level.mapString, CM_EntityString( svs.cms ) );

	level.map_parsed_ents = ( char * )G_LevelMalloc( len + 1 );

	G_ResetLevel();
}

static void SP_worldspawn( edict_t *ent ) {
	ent->movetype = MOVETYPE_PUSH;
	ent->r.solid = SOLID_YES;
	ent->r.inuse = true;       // since the world doesn't use G_Spawn()
	ent->s.origin = Vec3( 0.0f );
	ent->s.angles = Vec3( 0.0f );

	const char * model_name = "*0";
	ent->s.model = StringHash( Hash64( model_name, strlen( model_name ), svs.cms->base_hash ) );
	GClip_SetBrushModel( ent );
}
