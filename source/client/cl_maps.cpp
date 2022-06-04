#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/cmodel.h"
#include "qcommon/compression.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/maps.h"
#include "client/renderer/model.h"
#include "game/hotload_map.h"
#include "gameshared/cdmap.h"

constexpr u32 MAX_MAPS = 128;
constexpr u32 MAX_MAP_MODELS = 1024;

static Map maps[ MAX_MAPS ];
static u32 num_maps;
static Hashtable< MAX_MAPS * 2 > maps_hashtable;

static Hashtable< MAX_MAP_MODELS * 2 > map_models_hashtable;

static void DeleteMap( Map * map ) {
	FREE( sys_allocator, const_cast< char * >( map->name ) );
	CM_Free( CM_Client, map->cms );
	DeleteMapRenderData( map->render_data );
}

static void FillMapModelsHashtable() {
	TracyZoneScoped;

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

bool AddMap( Span< const u8 > data, const char * path ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	Span< const char > name = StripPrefix( StripExtension( path ), "maps/" );
	u64 hash = Hash64( name );

	MapData map;
	DecodeMapResult res = DecodeMap( &map, data );
	if( res != DecodeMapResult_Ok ) {
		return false;
	}

	MapRenderData render_data = NewMapRenderData( map, path );

	u64 idx = num_maps;
	if( !maps_hashtable.get( hash, &idx ) ) {
		maps_hashtable.add( hash, num_maps );
		num_maps++;
	}
	else {
		DeleteMap( &maps[ idx ] );
	}

	render_data.name = ( *sys_allocator )( "{}", name );

	maps[ idx ] = render_data;

	FillMapModelsHashtable();

	return true;
}

void InitMaps() {
	TracyZoneScoped;

	num_maps = 0;

	TempAllocator temp = cls.frame_arena.temp();

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".cdmap" )
			continue;

		AddMap( AssetBinary( path ), path );
	}
}

void HotloadMaps() {
	TracyZoneScoped;

	bool hotloaded_anything = false;

	for( const char * path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".cdmap" )
			continue;

		AddMap( AssetBinary( path ), path );
		hotloaded_anything = true;
	}

	// if we hotload a map while playing a local game just assume we're
	// playing on it and always hotload
	if( hotloaded_anything && Com_ServerState() != ss_dead ) {
		G_HotloadMap();
	}
}

void ShutdownMaps() {
	TracyZoneScoped;

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
