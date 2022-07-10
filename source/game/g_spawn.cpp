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
#include "qcommon/fs.h"
#include "qcommon/srgb.h"
#include "game/g_local.h"
#include "game/g_maps.h"
#include "gameshared/cdmap.h"

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
	{ "func_rotating", SP_func_rotating },
	{ "func_train", SP_func_train },

	{ "trigger_push", SP_trigger_push },
	{ "trigger_hurt", SP_trigger_hurt },

	{ "target_laser", SP_target_laser },
	{ "target_position", SP_target_position },

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

static bool DoField( const char * name, int * x, Span< const char > key, Span< const char > value ) {
	if( !StrEqual( name, key ) )
		return false;
	*x = SpanToInt( value, 0 );
	return true;
}

static bool DoField( const char * name, s64 * x, Span< const char > key, Span< const char > value ) {
	if( !StrEqual( name, key ) )
		return false;
	*x = SpanToU64( value, 0 ); // TODO: wrong type
	return true;
}

static bool DoField( const char * name, float * x, Span< const char > key, Span< const char > value ) {
	if( !StrEqual( name, key ) )
		return false;
	*x = SpanToFloat( value, 0.0f );
	return true;
}

static bool DoField( const char * name, Vec3 * x, Span< const char > key, Span< const char > value ) {
	if( !StrEqual( name, key ) )
		return false;
	for( int i = 0; i < 3; i++ ) {
		( *x )[ i ] = ParseFloat( &value, 0.0f, Parse_StopOnNewLine );
	}
	return true;
}

static bool DoField( const char * name, RGBA8 * x, Span< const char > key, Span< const char > value ) {
	if( !StrEqual( name, key ) )
		return false;
	// TODO: accept hex colors etc
	Vec4 vec4;
	for( int i = 0; i < 4; i++ ) {
		vec4[ i ] = ParseFloat( &value, 1.0f, Parse_StopOnNewLine );
	}
	*x = LinearTosRGB( vec4 );
	return true;
}

static bool DoField( const char * name, StringHash * x, Span< const char > key, Span< const char > value ) {
	if( !StrEqual( name, key ) )
		return false;
	*x = StringHash( value );
	return true;
}

static bool DoField( const char * name, StringHash * x, Span< const char > key, Span< const char > value, StringHash map_base_hash ) {
	if( !StrEqual( name, key ) )
		return false;

	if( value.n > 0 && value[ 0 ] == '*' ) {
		*x = StringHash( Hash64( value.ptr, value.n, map_base_hash.hash ) );
	}
	else {
		*x = StringHash( value );
	}

	return true;
}

static void ED_ParseField( Span< const char > key, Span< const char > value, StringHash map_base_hash, edict_t * ent, spawn_temp_t * st ) {
	bool used = false;

	used = used || DoField( "classname", &ent->classname, key, value );
	used = used || DoField( "origin", &ent->s.origin, key, value );
	used = used || DoField( "model", &ent->s.model, key, value, map_base_hash );
	used = used || DoField( "model2", &ent->s.model2, key, value, map_base_hash );
	used = used || DoField( "material", &ent->s.material, key, value );
	used = used || DoField( "color", &ent->s.color, key, value );
	used = used || DoField( "spawnflags", &ent->spawnflags, key, value );
	used = used || DoField( "speed", &ent->speed, key, value );
	used = used || DoField( "target", &ent->target, key, value );
	used = used || DoField( "targetname", &ent->name, key, value );
	used = used || DoField( "pathtarget", &ent->pathtarget, key, value );
	used = used || DoField( "killtarget", &ent->killtarget, key, value );
	used = used || DoField( "deadcam", &ent->deadcam, key, value );
	used = used || DoField( "wait", &ent->wait, key, value );
	used = used || DoField( "delay", &ent->delay, key, value );
	used = used || DoField( "style", &ent->style, key, value );
	used = used || DoField( "count", &ent->count, key, value );
	used = used || DoField( "health", &ent->health, key, value );
	used = used || DoField( "dmg", &ent->dmg, key, value );
	used = used || DoField( "angles", &ent->s.angles, key, value );
	used = used || DoField( "modelscale", &ent->s.scale, key, value );
	used = used || DoField( "mass", &ent->mass, key, value );
	used = used || DoField( "random", &ent->wait_randomness, key, value );

	// yaw
	if( key == "angle" ) {
		ent->s.angles = Vec3( 0.0f, SpanToFloat( value, 0.0f ), 0.0f );
		used = true;
	}

	// 1d scale
	if( key == "modelscale" ) {
		ent->s.scale = Vec3( SpanToFloat( value, 1.0f ) );
		used = true;
	}

	used = used || DoField( "lip", &st->lip, key, value );
	used = used || DoField( "distance", &st->distance, key, value );
	used = used || DoField( "height", &st->height, key, value );
	used = used || DoField( "noise", &st->noise, key, value );
	used = used || DoField( "noise_start", &st->noise_start, key, value );
	used = used || DoField( "noise_stop", &st->noise_stop, key, value );
	used = used || DoField( "pausetime", &st->pausetime, key, value );
	used = used || DoField( "gameteam", &st->gameteam, key, value );
	used = used || DoField( "size", &st->size, key, value );
	used = used || DoField( "spawn_probability", &st->spawn_probability, key, value );
	used = used || DoField( "power", &st->power, key, value );

	if( !used && key.n > 0 && key[ 0 ] != '_' ) {
		Com_GGPrint( "{} is not a valid entity key", key );
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

	const MapData * map = FindServerMap( server_gs.gameState.map );
	for( size_t i = 0; i < map->entities.n; i++ ) {
		const MapEntity * map_entity = &map->entities[ i ];
		edict_t * ent;
		if( i == 0 ) {
			ent = world;
			G_InitEdict( ent );
		}
		else {
			ent = G_Spawn();
		}

		spawn_temp_t st = { };
		st.spawn_probability = 1.0f;

		for( u32 j = 0; j < map_entity->num_key_values; j++ ) {
			const MapEntityKeyValue * kv = &map->entity_kvs[ map_entity->first_key_value + j ];

			Span< const char > key = map->entity_data.slice( kv->offset, kv->offset + kv->key_size );
			Span< const char > value = map->entity_data.slice( kv->offset + kv->key_size, kv->offset + kv->key_size + kv->value_size );

			ED_ParseField( key, value, server_gs.gameState.map, ent, &st );

			if( key == "classname" ) {
				st.classname = value;
			}
		}

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

	ShutdownServerCollisionModels();
	InitServerCollisionModels();

	memset( &level, 0, sizeof( level_locals_t ) );
	level.time = levelTime;

	memset( &server_gs.gameState, 0, sizeof( server_gs.gameState ) );
	server_gs.gameState.map = StringHash( mapname );
	LoadServerMap( mapname );// TODO: errors???
	GClip_ClearWorld(); // clear areas links

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
			G_Teams_SetTeam( &game.edicts[ i + 1 ], Team_None );
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

void G_HotloadMap() {
	// TODO: come back to this
	char map[ ARRAY_COUNT( sv.mapname ) ];
	Q_strncpyz( map, sv.mapname, sizeof( map ) );
	G_ResetLevel();

	if( level.gametype.MapHotloaded != NULL ) {
		level.gametype.MapHotloaded();
	}
}

static void SP_worldspawn( edict_t * ent, const spawn_temp_t * st ) {
	ent->s.svflags &= ~SVF_NOCLIENT;

	ent->movetype = MOVETYPE_PUSH;
	ent->r.solid = SOLID_YES;

	const char * model_name = "*0";
	ent->s.model = StringHash( Hash64( model_name, strlen( model_name ), server_gs.gameState.map.hash ) );
}
