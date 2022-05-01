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
	EntityField_Int,
	EntityField_Float,
	EntityField_StringHash,
	EntityField_Asset,
	EntityField_Vec3,
	EntityField_Angle,
	EntityField_Scale,
	EntityField_RGBA,
};

struct EntityField {
	StringHash name;
	size_t ofs;
	EntityFieldType type;
	bool temp;
};

#define FOFS( x ) offsetof( edict_t, x )
#define STOFS( x ) offsetof( spawn_temp_t, x )

static constexpr EntityField entity_keys[] = {
	{ "classname", FOFS( classname ), EntityField_StringHash },
	{ "origin", FOFS( s.origin ), EntityField_Vec3 },
	{ "model", FOFS( s.model ), EntityField_Asset },
	{ "model2", FOFS( s.model2 ), EntityField_Asset },
	{ "material", FOFS( s.material ), EntityField_Asset },
	{ "color", FOFS( s.color ), EntityField_RGBA },
	{ "spawnflags", FOFS( spawnflags ), EntityField_Int },
	{ "speed", FOFS( speed ), EntityField_Float },
	{ "target", FOFS( target ), EntityField_StringHash },
	{ "targetname", FOFS( name ), EntityField_StringHash },
	{ "pathtarget", FOFS( pathtarget ), EntityField_StringHash },
	{ "killtarget", FOFS( killtarget ), EntityField_StringHash },
	{ "deadcam", FOFS( deadcam ), EntityField_StringHash },
	{ "wait", FOFS( wait ), EntityField_Int },
	{ "delay", FOFS( delay ), EntityField_Int },
	{ "style", FOFS( style ), EntityField_Int },
	{ "count", FOFS( count ), EntityField_Int },
	{ "health", FOFS( health ), EntityField_Float },
	{ "dmg", FOFS( dmg ), EntityField_Int },
	{ "angles", FOFS( s.angles ), EntityField_Vec3 },
	{ "angle", FOFS( s.angles ), EntityField_Angle },
	{ "modelscale_vec", FOFS( s.scale ), EntityField_Vec3 },
	{ "modelscale", FOFS( s.scale ), EntityField_Scale },
	{ "mass", FOFS( mass ), EntityField_Int },
	{ "random", FOFS( wait_randomness ), EntityField_Int },

	// temp spawn vars -- only valid when the spawn function is called
	{ "lip", STOFS( lip ), EntityField_Int, true },
	{ "distance", STOFS( distance ), EntityField_Int, true },
	{ "radius", STOFS( radius ), EntityField_Float, true },
	{ "height", STOFS( height ), EntityField_Int, true },
	{ "noise", STOFS( noise ), EntityField_StringHash, true },
	{ "noise_start", STOFS( noise_start ), EntityField_StringHash, true },
	{ "noise_stop", STOFS( noise_stop ), EntityField_StringHash, true },
	{ "pausetime", STOFS( pausetime ), EntityField_Float, true },
	{ "gameteam", STOFS( gameteam ), EntityField_Int, true },
	{ "size", STOFS( size ), EntityField_Int, true },
	{ "spawn_probability", STOFS( spawn_probability ), EntityField_Float, true },
};

static void SP_worldspawn( edict_t * ent, const spawn_temp_t * st );

struct EntitySpawnCallback {
	StringHash classname;
	void ( *cb )( edict_t * ent, const spawn_temp_t * st );
};

static constexpr EntitySpawnCallback spawn_callbacks[] = {
	{ "worldspawn", SP_worldspawn },

	{ "post_match_camera", SP_post_match_camera },
	{ "deadcam", SP_post_match_camera },

	{ "func_door", SP_func_door },
	{ "func_door_rotating", SP_func_door_rotating },
	{ "func_rotating", SP_func_rotating },
	{ "func_train", SP_func_train },
	{ "func_wall", SP_func_wall },
	{ "func_static", SP_func_static },

	{ "trigger_always", SP_trigger_always },
	{ "trigger_once", SP_trigger_once },
	{ "trigger_multiple", SP_trigger_multiple },
	{ "trigger_push", SP_trigger_push },
	{ "trigger_hurt", SP_trigger_hurt },

	{ "target_laser", SP_target_laser },
	{ "target_position", SP_target_position },
	{ "target_delay", SP_target_delay },

	{ "path_corner", SP_path_corner },

	{ "trigger_teleport", SP_trigger_teleport },
	{ "misc_teleporter_dest", SP_target_position },

	{ "model", SP_model },
	{ "decal", SP_decal },

	{ "spike", SP_spike },
	{ "spikes", SP_spikes },
	{ "jumppad", SP_jumppad },

	{ "speaker_wall", SP_speaker_wall },
};

static bool SpawnEntity( edict_t * ent, const spawn_temp_t * st ) {
	for( EntitySpawnCallback s : spawn_callbacks ) {
		if( s.classname == ent->classname ) {
			s.cb( ent, st );
			return true;
		}
	}

	if( level.gametype.SpawnEntity != NULL && level.gametype.SpawnEntity( ent->classname, ent ) ) {
		return true;
	}

	Com_GGPrint( "{} doesn't have a spawn function", st->classname );

	return false;
}

static void ED_ParseField( Span< const char > key, Span< const char > value, edict_t * ent, spawn_temp_t * st ) {
	StringHash key_hash = StringHash( key );

	for( EntityField f : entity_keys ) {
		if( f.name != key_hash )
			continue;

		uint8_t *b;
		if( f.temp ) {
			b = (uint8_t *)st;
		} else {
			b = (uint8_t *)ent;
		}

		switch( f.type ) {
			case EntityField_StringHash:
				*(StringHash *)( b + f.ofs ) = StringHash( value );
				break;
			case EntityField_Asset:
				if( value[ 0 ] == '*' ) {
					*(StringHash *)( b + f.ofs ) = StringHash( Hash64( value.ptr, value.n, svs.cms->base_hash ) );
				}
				else {
					*(StringHash *)( b + f.ofs ) = StringHash( value );
				}
				break;
			case EntityField_Int:
				*(int *)( b + f.ofs ) = SpanToInt( value, 0 );
				break;
			case EntityField_Float:
				*(float *)( b + f.ofs ) = SpanToFloat( value, 0.0f );
				break;
			case EntityField_Angle:
				*(Vec3 *)( b + f.ofs ) = Vec3( 0.0f, SpanToFloat( value, 0.0f ), 0.0f );
				break;
			case EntityField_Scale:
				*(Vec3 *)( b + f.ofs ) = Vec3( SpanToFloat( value, 1.0f ) );
				break;

			case EntityField_Vec3: {
				Vec3 vec;
				vec.x = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				vec.y = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				vec.z = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
				*(Vec3 *)( b + f.ofs ) = vec;
			} break;

			case EntityField_RGBA: {
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

	if( key.n > 0 && key[ 0 ] != '_' ) {
		Com_GGPrint( "{} is not a field", key );
	}
}

static void ED_ParseEntity( Span< const char > * cursor, edict_t * ent, spawn_temp_t * st ) {
	while( true ) {
		Span< const char > key = ParseToken( cursor, Parse_DontStopOnNewLine );
		if( key == "}" )
			break;
		if( key.ptr == NULL ) {
			Com_Error( "ED_ParseEntity: EOF without closing brace" );
		}

		Span< const char > value = ParseToken( cursor, Parse_StopOnNewLine );
		if( value.ptr == NULL ) {
			Com_Error( "ED_ParseEntity: EOF without closing brace" );
		}
		if( value == "}" ) {
			Com_Error( "ED_ParseEntity: closing brace without data" );
		}

		ED_ParseField( key, value, ent, st );

		if( StrCaseEqual( key, "classname" ) ) {
			st->classname = value;
		}
	}
}

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

static void SpawnMapEntities() {
	level.spawnedTimeStamp = svs.gametime;
	level.canSpawnEntities = true;

	Span< const char > cursor = MakeSpan( CM_EntityString( svs.cms ) );
	edict_t * ent = NULL;

	while( true ) {
		// parse the opening brace
		Span< const char > brace = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( brace == "" )
			break;
		if( brace != "{" ) {
			Com_Error( "SpawnMapEntities: entity string doesn't begin with {" );
		}

		if( ent == NULL ) {
			ent = world;
			G_InitEdict( world );
		}
		else {
			ent = G_Spawn();
		}

		spawn_temp_t st = { };
		st.spawn_probability = 1.0f;

		ED_ParseEntity( &cursor, ent, &st );

		bool ok = true;
		bool rng = Probability( &svs.rng, st.spawn_probability );
		ok = ok && st.classname != "";
		ok = ok && rng;
		ok = ok && SpawnEntity( ent, &st );

		if( !ok ) {
			G_FreeEdict( ent );
		}
	}

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
	ResetEntityIDSequence();

	GClip_ClearWorld(); // clear areas links

	memset( &level, 0, sizeof( level_locals_t ) );
	level.time = levelTime;

	memset( &server_gs.gameState, 0, sizeof( server_gs.gameState ) );

	server_gs.gameState.map = StringHash( mapname );
	server_gs.gameState.map_checksum = svs.cms->checksum;

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
	G_InitGameCommands();

	G_Teams_Init();

	InitGametype();

	SpawnMapEntities();

	// always start in warmup match state and let the thinking code
	// revert it to wait state if empty ( so gametype based item masks are setup )
	G_Match_LaunchState( MatchState_Warmup );

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		if( game.edicts[ i + 1 ].r.inuse ) {
			G_Teams_SetTeam( &game.edicts[ i + 1 ], TEAM_SPECTATOR );
		}
	}

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		if( game.edicts[ i + 1 ].r.inuse ) {
			G_Teams_JoinAnyTeam( &game.edicts[ i + 1 ], true );
		}
	}
}

void G_ResetLevel() {
	G_FreeEdict( world );
	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		G_FreeEdict( game.edicts + i );
	}

	SpawnMapEntities();
}

void G_RespawnLevel() {
	ShutdownGametype();
	G_InitLevel( sv.mapname, level.time );

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = &game.edicts[ i + 1 ];
		if( !ent->r.inuse )
			continue;
		GT_CallPlayerConnected( ent );
	}
}

void G_LoadMap( const char * name ) {
	TempAllocator temp = svs.frame_arena.temp();

	if( svs.cms != NULL ) {
		CM_Free( CM_Server, svs.cms );
	}

	Q_strncpyz( sv.mapname, name, sizeof( sv.mapname ) );

	Span< u8 > data;
	defer { FREE( sys_allocator, data.ptr ); };

	const char * bsp_path = temp( "{}/base/maps/{}.bsp", RootDirPath(), name );
	data = ReadFileBinary( sys_allocator, bsp_path );

	if( data.ptr == NULL ) {
		const char * zst_path = temp( "{}.zst", bsp_path );
		Span< u8 > compressed = ReadFileBinary( sys_allocator, zst_path );
		defer { FREE( sys_allocator, compressed.ptr ); };
		if( compressed.ptr == NULL ) {
			Fatal( "Couldn't find map %s", name );
		}

		bool ok = Decompress( zst_path, sys_allocator, compressed, &data );
		if( !ok ) {
			Fatal( "Couldn't decompress %s", zst_path );
		}
	}

	u64 hash = Hash64( name );
	svs.cms = CM_LoadMap( CM_Server, data, hash );

	server_gs.gameState.map = StringHash( hash );
	server_gs.gameState.map_checksum = svs.cms->checksum;
}

void G_HotloadMap() {
	char map[ ARRAY_COUNT( sv.mapname ) ];
	Q_strncpyz( map, sv.mapname, sizeof( map ) );
	G_LoadMap( map );
	G_ResetLevel();

	if( level.gametype.MapHotloaded != NULL ) {
		level.gametype.MapHotloaded();
	}
}

// TODO: game module init is a mess and I'm not sure how to clean this up
void G_Aasdf() {
	GClip_ClearWorld(); // clear areas links
	G_ResetLevel();
	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		edict_t * ent = &game.edicts[ i ];
		if( ent->r.inuse ) {
			ent->s.teleported = true;
		}
	}
}

static void SP_worldspawn( edict_t * ent, const spawn_temp_t * st ) {
	ent->movetype = MOVETYPE_PUSH;
	ent->r.solid = SOLID_YES;
	ent->r.inuse = true; // since the world doesn't use G_Spawn()
	ent->s.origin = Vec3( 0.0f );
	ent->s.angles = Vec3( 0.0f );

	const char * model_name = "*0";
	ent->s.model = StringHash( Hash64( model_name, strlen( model_name ), svs.cms->base_hash ) );
	GClip_SetBrushModel( ent );
}
