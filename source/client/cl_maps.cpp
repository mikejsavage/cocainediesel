#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/compression.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/maps.h"
#include "client/renderer/model.h"
#include "game/hotload_map.h"
#include "gameshared/cdmap.h"
#include "gameshared/collision.h"

constexpr u32 MAX_MAPS = 128;
constexpr u32 MAX_MAP_MODELS = 1024;

static Map maps[ MAX_MAPS ];
static Hashtable< MAX_MAPS * 2 > maps_hashtable;

static MapSubModelRenderData map_models[ MAX_MAP_MODELS ];
static Hashtable< MAX_MAP_MODELS * 2 > map_models_hashtable;

static CollisionModelStorage collision_models;

static void DeleteMap( Map * map ) {
	FREE( sys_allocator, const_cast< char * >( map->name ) );
	DeleteMapRenderData( map->render_data );
}

static void FillMapModelsHashtable() {
	TracyZoneScoped;

	map_models_hashtable.clear();

	for( u32 i = 0; i < maps_hashtable.size(); i++ ) {
		const Map * map = &maps[ i ];
		for( size_t j = 0; j < map->data.models.n; j++ ) {
			String< 16 > suffix( "*{}", j );
			u64 hash = Hash64( suffix.c_str(), suffix.length(), map->base_hash.hash );

			assert( map_models_hashtable.size() < ARRAY_COUNT( map_models ) ); // TODO: must be if Fatal because this matters in release builds too
			map_models[ map_models_hashtable.size() ] = {
				StringHash( map->base_hash ),
				checked_cast< u32 >( j )
			};
			map_models_hashtable.add( hash, map_models_hashtable.size() );
		}
	}
}

bool AddMap( Span< const u8 > data, const char * path ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	Span< const char > name = StripPrefix( StripExtension( path ), "maps/" );
	StringHash hash = StringHash( name );

	Map map = { };

	DecodeMapResult res = DecodeMap( &map.data, data );
	if( res != DecodeMapResult_Ok ) {
		return false;
	}

	map.name = ( *sys_allocator )( "{}", name );
	map.base_hash = hash;
	map.render_data = NewMapRenderData( map.data, path );

	u64 idx = maps_hashtable.size();
	if( !maps_hashtable.get( hash.hash, &idx ) ) {
		maps_hashtable.add( hash.hash, maps_hashtable.size() );
	}
	else {
		DeleteMap( &maps[ idx ] );
	}

	maps[ idx ] = map;

	FillMapModelsHashtable();

	LoadMapCollisionData( &collision_models, &map.data, hash );

	return true;
}

void InitMaps() {
	TracyZoneScoped;

	maps_hashtable.clear();
	map_models_hashtable.clear();

	InitCollisionModelStorage( &collision_models );

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

	for( u32 i = 0; i < maps_hashtable.size(); i++ ) {
		DeleteMap( &maps[ i ] );
	}

	maps_hashtable.clear();
	map_models_hashtable.clear();

	ShutdownCollisionModelStorage( &collision_models );
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

const MapSubModelRenderData * FindMapSubModelRenderData( StringHash name ) {
	u64 idx;
	if( !map_models_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &map_models[ idx ];
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
