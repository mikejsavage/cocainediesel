#include "qcommon/base.h"
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

static ServerMapData maps[ CollisionModelStorage::MAX_MAPS ];
static size_t num_maps;

// like cgltf_load_buffers, but doesn't try to load URIs
static bool LoadBinaryBuffers( cgltf_data * data ) {
	if( data->buffers_count && data->buffers[0].data == NULL && data->buffers[0].uri == NULL && data->bin ) {
		if( data->bin_size < data->buffers[0].size )
			return false;
		data->buffers[0].data = const_cast< void * >( data->bin );
	}

	for( cgltf_size i = 0; i < data->buffers_count; i++ ) {
		if( data->buffers[i].data == NULL )
			return false;
	}

	return true;
}


static bool AddGLTFModel( Span< const u8 > data, Span< const char > path ) {
	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * gltf;
	if( cgltf_parse( &options, data.ptr, data.num_bytes(), &gltf ) != cgltf_result_success ) {
		Com_Printf( S_COLOR_YELLOW "%s isn't a GLTF file\n", path );
		return false;
	}

	defer { cgltf_free( gltf ); };

	if( !LoadBinaryBuffers( gltf ) ) {
		Com_Printf( S_COLOR_YELLOW "Couldn't load buffers in %s\n", path );
		return false;
	}

	if( cgltf_validate( gltf ) != cgltf_result_success ) {
		Com_Printf( S_COLOR_YELLOW "%s is invalid GLTF\n", path );
		return false;
	}

  StringHash name = StringHash( path );
  LoadGLTFCollisionData( &collision_models, gltf, path, name );

	return true;
}

static void LoadModelsRecursive( TempAllocator * temp, DynamicString * path, size_t skip ) {
  ListDirHandle scan = BeginListDir( temp, path->c_str() );

  const char * name;
  bool dir;
  while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		if( dir ) {
			LoadModelsRecursive( temp, path, skip );
		}
		else if( FileExtension( path->c_str() ) == ".glb" ) {
			Span< u8 > data = ReadFileBinary( temp, path->c_str() );
			AddGLTFModel( data, StripExtension( path->c_str() + skip ) );
		}
		path->truncate( old_len );
  }
}

void InitServerCollisionModels() {
	InitCollisionModelStorage( &collision_models );

	TempAllocator temp = svs.frame_arena.temp();
	DynamicString base( &temp, "{}/base", RootDirPath() );
	LoadModelsRecursive( &temp, &base, base.length() + 1 );

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

MinMax3 ServerEntityBounds( const SyncEntityState * ent ) {
	return EntityBounds( &collision_models, ent );
}

const CollisionModelStorage * ServerCollisionModelStorage() {
	return &collision_models;
}