#include "qcommon/base.h"
#include "qcommon/compression.h"
#include "qcommon/fs.h"
#include "game/g_maps.h"
#include "gameshared/cdmap.h"
#include "gameshared/collision.h"
#include "server/server.h"

static CollisionModelStorage collision_models;

struct ServerMapData {
	StringHash base_hash;
	Span< u8 > data;
};

static ServerMapData maps[ CollisionModelStorage::MAX_MAPS ];
static size_t num_maps;

void InitServerCollisionModels() {
	InitCollisionModelStorage( &collision_models );

	num_maps = 0;
}

void ShutdownServerCollisionModels() {
	ShutdownCollisionModelStorage( &collision_models );

	for( size_t i = 0; i < num_maps; i++ ) {
		FREE( sys_allocator, maps[ i ].data.ptr );
	}
}

bool LoadServerMap( const char * name ) {
	TempAllocator temp = svs.frame_arena.temp();

	const char * path = temp( "{}/base/maps/{}.cdmap", RootDirPath(), name );

	ServerMapData map;
	map.base_hash = StringHash( name );
	map.data = ReadFileBinary( sys_allocator, path );

	if( map.data.ptr == NULL ) {
		const char * zst_path = temp( "{}.zst", path );
		Span< u8 > compressed = ReadFileBinary( sys_allocator, zst_path );
		defer { FREE( sys_allocator, compressed.ptr ); };
		if( compressed.ptr == NULL ) {
			Com_Printf( "Couldn't find map %s\n", name );
			return false;
		}

		bool ok = Decompress( zst_path, sys_allocator, compressed, &map.data );
		if( !ok ) {
			Com_Printf( "Couldn't decompress %s\n", zst_path );
			return false;
		}
	}

	MapData decoded;
	DecodeMapResult res = DecodeMap( &decoded, map.data );
	if( res != DecodeMapResult_Ok ) {
		Com_Printf( "Can't decode map %s\n", name );
		FREE( sys_allocator, map.data.ptr );
		return false;
	}

	LoadMapCollisionData( &collision_models, &decoded, map.base_hash );

	if( num_maps == ARRAY_COUNT( maps ) ) {
		Fatal( "Too many maps" );
	}

	maps[ num_maps ] = map;
	num_maps++;

	return true;
}

const MapData * FindServerMap( StringHash name ) {
	const MapSharedCollisionData * map = FindMapSharedCollisionData( &collision_models, name );
	return map == NULL ? NULL : &map->data;
}

const MapSubModelCollisionData * FindServerMapSubModelCollisionData( StringHash name ) {
	return FindMapSubModelCollisionData( &collision_models, name );
}
