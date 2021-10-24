#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/cmodel.h"
#include "qcommon/compression.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "client/assets.h"
#include "client/maps.h"
#include "client/renderer/model.h"
#include "game/hotload_map.h"

constexpr u32 MAX_MAPS = 128;
constexpr u32 MAX_MAP_MODELS = 1024;

static Map maps[ MAX_MAPS ];
static u32 num_maps;
static Hashtable< MAX_MAPS * 2 > maps_hashtable;

static Hashtable< MAX_MAP_MODELS * 2 > map_models_hashtable;

static void DeleteMap( Map * map ) {
	FREE( sys_allocator, const_cast< char * >( map->name ) );
	CM_Free( CM_Client, map->cms );
	DeleteBSPRenderData( map );
}

bool AddMap( Span< const u8 > data, const char * path ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	u64 hash = Hash64( StripExtension( path ) );

	u64 idx = num_maps;
	if( !maps_hashtable.get( hash, &idx ) ) {
		maps_hashtable.add( hash, num_maps );
		num_maps++;
	}
	else {
		DeleteMap( &maps[ idx ] );
	}

	maps[ idx ].name = CopyString( sys_allocator, path );

	// TODO: need more map validation because they can be downloaded from the server
	if( !LoadBSPRenderData( path, &maps[ idx ], hash, data ) ) {
		return false;
	}

	maps[ idx ].cms = CM_LoadMap( CM_Client, data, hash );
	if( maps[ idx ].cms == NULL ) {
		// TODO: free render data
		return false;
	}

	return true;
}

static void FillMapModelsHashtable() {
	map_models_hashtable.clear();

	for( u32 i = 0; i < num_maps; i++ ) {
		const Map * map = &maps[ i ];
		for( u32 j = 0; j < map->num_models; j++ ) {
			String< 16 > suffix( "*{}", j );
			u64 hash = Hash64( suffix.c_str(), suffix.length(), map->base_hash );

			assert( map_models_hashtable.size() < MAX_MAP_MODELS );
			map_models_hashtable.add( hash, uintptr_t( &map->models[ j ] ) );
		}
	}
}

void InitMaps() {
	ZoneScoped;

	num_maps = 0;

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".bsp" )
			continue;

		AddMap( AssetBinary( path ), path );
	}

	FillMapModelsHashtable();
}

void HotloadMaps() {
	ZoneScoped;

	bool hotloaded_anything = false;

	for( const char * path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".bsp" )
			continue;

		AddMap( AssetBinary( path ), path );
		hotloaded_anything = true;
	}

	if( hotloaded_anything && Com_ServerState() != ss_dead ) {
		FillMapModelsHashtable();
		G_HotloadMap();
	}
}

void ShutdownMaps() {
	for( u32 i = 0; i < num_maps; i++ ) {
		DeleteMap( &maps[ i ] );
	}
}

const Map * FindMap( StringHash name ) {
	u64 idx;
	if( !maps_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &maps[ idx ];
}

const Map * FindMap( const char * name ) {
	return FindMap( StringHash( name ) );
}

const Model * FindMapModel( StringHash name ) {
	u64 idx;
	if( !map_models_hashtable.get( name.hash, &idx ) )
		return NULL;
	return ( const Model * ) uintptr_t( idx );
}
