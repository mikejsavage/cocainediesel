#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/compression.h"
#include "qcommon/hashtable.h"
#include "client/maps.h"
#include "client/renderer/model.h"

constexpr u32 MAX_MAPS = 128;

static Map maps[ MAX_MAPS ];
static u32 num_maps;
static Hashtable< MAX_MAPS * 2 > maps_hashtable;

void InitMaps() {
	ZoneScoped;

	num_maps = 0;

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".bsp" )
			continue;

		Span< const u8 > compressed = AssetBinary( path );
		if( compressed.n < 4 ) {
			Com_Printf( S_COLOR_RED "BSP too small %s\n", path );
			continue;
		}

		Span< u8 > decompressed;
		defer { FREE( sys_allocator, decompressed.ptr ); };
		bool ok = Decompress( path, sys_allocator, compressed, &decompressed );
		if( !ok )
			continue;

		Span< const u8 > data = decompressed.ptr == NULL ? compressed : decompressed;

		maps[ num_maps ].name = CopyString( sys_allocator, path );
		u64 base_hash = Hash64( path, strlen( path ) - ext.n );

		if( !LoadBSPRenderData( &maps[ num_maps ], path, base_hash, data ) )
			continue;

		maps[ num_maps ].cms = CM_LoadMap( CM_Client, data, base_hash );
		if( maps[ num_maps ].cms == NULL )
			// TODO: free render data
			continue;

		maps_hashtable.add( base_hash, num_maps );
		num_maps++;
	}

}

void ShutdownMaps() {
	for( u32 i = 0; i < num_maps; i++ ) {
		FREE( sys_allocator, const_cast< char * >( maps[ i ].name ) );
		CM_Free( CM_Client, maps[ i ].cms );
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
