#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/compression.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "game/g_maps.h"
#include "gameshared/cdmap.h"
#include "gameshared/collision.h"
#include "server/server.h"

#include "cgltf/cgltf.h"

static CollisionModelStorage collision_models;

struct ServerMapData {
	StringHash base_hash;
	Span< u8 > data;
};

static BoundedDynamicArray< ServerMapData, CollisionModelStorage::MAX_MAPS > maps;

static bool AddGLTFModel( Span< const u8 > data, Span< const char > path ) {
	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * gltf;
	if( cgltf_parse( &options, data.ptr, data.num_bytes(), &gltf ) != cgltf_result_success ) {
		Com_GGPrint( S_COLOR_YELLOW "{} isn't a GLTF file", path );
		return false;
	}

	defer { cgltf_free( gltf ); };

	if( !LoadGLBBuffers( gltf ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Couldn't load buffers in {}", path );
		return false;
	}

	if( cgltf_validate( gltf ) != cgltf_result_success ) {
		Com_GGPrint( S_COLOR_YELLOW "{} is invalid GLTF", path );
		return false;
	}

	StringHash name = StringHash( path );
	LoadGLTFCollisionData( &collision_models, gltf, path, name );

	return true;
}

void InitServerCollisionModels() {
	TracyZoneScoped;

	InitCollisionModelStorage( &collision_models );

	TempAllocator temp = svs.frame_arena.temp();
	Span< const char > base = temp.sv( "{}/base", RootDirPath() );
	for( Span< const char > file : ListDir( &temp, base, ListDir_Recurse ) ) {
		if( FileExtension( file ) == ".glb" ) {
			Span< u8 > data = ReadFileBinary( sys_allocator, temp( "{}", file ) );
			AddGLTFModel( data, StripExtension( file + base.n + 1 ) );
			Free( sys_allocator, data.ptr );
		}
	}

	maps.clear();
}

void ShutdownServerCollisionModels() {
	TracyZoneScoped;

	ShutdownCollisionModelStorage( &collision_models );

	for( ServerMapData & map : maps ) {
		Free( sys_allocator, map.data.ptr );
	}
}

bool LoadServerMap( Span< const char > name ) {
	TempAllocator temp = svs.frame_arena.temp();

	const char * path = temp( "{}/base/maps/{}.cdmap", RootDirPath(), name );

	ServerMapData map;
	map.base_hash = StringHash( name );
	map.data = ReadFileBinary( sys_allocator, path );

	if( map.data.ptr == NULL ) {
		const char * zst_path = temp( "{}.zst", path );
		Span< u8 > compressed = ReadFileBinary( sys_allocator, zst_path );
		defer { Free( sys_allocator, compressed.ptr ); };
		if( compressed.ptr == NULL ) {
			Com_GGPrint( "Couldn't find map {}", name );
			return false;
		}

		bool ok = Decompress( MakeSpan( zst_path ), sys_allocator, compressed, &map.data );
		if( !ok ) {
			Com_Printf( "Couldn't decompress %s\n", zst_path );
			return false;
		}
	}

	MapData decoded;
	DecodeMapResult res = DecodeMap( &decoded, map.data );
	if( res != DecodeMapResult_Ok ) {
		Com_GGPrint( "Can't decode map {}: {}", name, res == DecodeMapResult_NotAMap ? "not a map" : "wrong cdmap version" );
		Free( sys_allocator, map.data.ptr );
		return false;
	}

	LoadMapCollisionData( &collision_models, &decoded, map.base_hash );

	if( !maps.add( map ) ) {
		Fatal( "Too many maps" );
	}

	return true;
}

const MapData * FindServerMap( StringHash name ) {
	const MapSharedCollisionData * map = FindMapSharedCollisionData( &collision_models, name );
	return map == NULL ? NULL : &map->data;
}

const MapSubModelCollisionData * FindServerMapSubModelCollisionData( StringHash name ) {
	return FindMapSubModelCollisionData( &collision_models, name );
}

MinMax3 ServerEntityBounds( const SyncEntityState * ent ) {
	return EntityBounds( &collision_models, ent );
}

const CollisionModelStorage * ServerCollisionModelStorage() {
	return &collision_models;
}
