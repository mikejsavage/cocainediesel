#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/compression.h"
#include "qcommon/hashmap.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/maps.h"
#include "client/renderer/model.h"
#include "game/hotload.h"
#include "gameshared/cdmap.h"
#include "gameshared/collision.h"

#include "cgltf/cgltf.h"

static HashMap< Map, 128 > maps;
static HashMap< MapSubModelRenderData, 1024 > map_models;

static CollisionModelStorage collision_models;

static void FillMapModelsHashtable() {
	TracyZoneScoped;

	map_models.clear();

	for( u32 i = 0; i < maps.size(); i++ ) {
		const Map * map = &maps.span()[ i ];
		for( size_t j = 0; j < map->data.models.n; j++ ) {
			String< 16 > suffix( "*{}", j );
			u64 hash = Hash64( suffix.span(), map->base_hash.hash );

			MapSubModelRenderData submodel = { StringHash( map->base_hash ), checked_cast< u32 >( j ) };
			if( !map_models.add( hash, submodel ) ) {
				Fatal( "Too many map submodels" );
			}
		}
	}
}

bool AddMap( Span< const u8 > data, Span< const char > path ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	Span< const char > name = StripPrefix( StripExtension( path ), "maps/" );
	StringHash hash = StringHash( name );

	MapData map_data;
	DecodeMapResult res = DecodeMap( &map_data, data );
	if( res != DecodeMapResult_Ok ) {
		return false;
	}

	Map * slot = maps.get( hash.hash );
	if( slot == NULL ) {
		slot = maps.add( hash.hash );
		if( slot == NULL ) {
			Com_Printf( S_COLOR_YELLOW "Too many maps!\n" );
			return false;
		}
	}
	else {
		Free( sys_allocator, const_cast< char * >( slot->name.ptr ) );
	}

	*slot = Map {
		.name = CloneSpan( sys_allocator, name ),
		.base_hash = hash,
		.data = map_data,
		.render_data = NewMapRenderData( map_data, path ),
	};

	FillMapModelsHashtable();

	LoadMapCollisionData( &collision_models, &map_data, hash );

	return true;
}

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

void InitMaps() {
	TracyZoneScoped;

	maps.clear();
	map_models.clear();

	InitCollisionModelStorage( &collision_models );

	for( Span< const char > path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext == ".cdmap" ) {
			AddMap( AssetBinary( path ), path );
		}
		else if( ext == ".glb" ) {
			AddGLTFModel( AssetBinary( path ), StripExtension( path ) );
		}
	}
}

void HotloadMaps() {
	TracyZoneScoped;

	bool hotloaded_anything = false;

	for( Span< const char > path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext == ".cdmap" ) {
			AddMap( AssetBinary( path ), path );
			hotloaded_anything = true;
		}
		else if( ext == ".glb" ) {
			AddGLTFModel( AssetBinary( path ), StripExtension( path ) );
			hotloaded_anything = true;
		}
	}

	// if we hotload a map while playing a local game just assume we're
	// playing on it and always hotload
	if( hotloaded_anything && Com_ServerState() != ss_dead ) {
		G_HotloadCollisionModels();
	}
}

void ShutdownMaps() {
	TracyZoneScoped;

	for( u32 i = 0; i < maps.size(); i++ ) {
		Free( sys_allocator, const_cast< char * >( maps.span()[ i ].name.ptr ) );
	}

	maps.clear();
	map_models.clear();

	ShutdownCollisionModelStorage( &collision_models );
}

const Map * FindMap( StringHash name ) {
	return maps.get( name.hash );
}

const Map * FindMap( const char * name ) {
	return FindMap( StringHash( name ) );
}

const MapSubModelRenderData * FindMapSubModelRenderData( StringHash name ) {
	return map_models.get( name.hash );
}

const MapSharedCollisionData * FindClientMapSharedCollisionData( StringHash name ) {
	return FindMapSharedCollisionData( &collision_models, name );
}

const MapSubModelCollisionData * FindClientMapSubModelCollisionData( StringHash name ) {
	return FindMapSubModelCollisionData( &collision_models, name );
}

const CollisionModelStorage * ClientCollisionModelStorage() {
	return &collision_models;
}
