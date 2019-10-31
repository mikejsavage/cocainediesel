#include <sys/types.h>
#include <sys/stat.h>

#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/assets.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"

struct Asset {
	char * path;
	Span< char > data;
	s64 modified_time;
};

static constexpr u32 MAX_ASSETS = 4096;

static Asset assets[ MAX_ASSETS ];
static const char * asset_paths[ MAX_ASSETS ];
static u32 num_assets;

static const char * modified_asset_paths[ MAX_ASSETS ];
static u32 num_modified_assets;

static Hashtable< MAX_ASSETS * 2 > assets_hashtable;

static void LoadAsset( const char * full_path, size_t skip ) {
	ZoneScoped;

	const char * path = full_path + skip;
	u64 hash = Hash64( path );

	s64 modified_time = FileLastModifiedTime( full_path );

	u64 idx;
	bool exists = assets_hashtable.get( hash, &idx );
	if( exists ) {
		if( assets[ idx ].modified_time == modified_time ) {
			return;
		}
	}

	Span< char > contents = ReadFileString( sys_allocator, full_path );
	if( contents.ptr == NULL )
		return;

	Asset * a;
	if( exists ) {
		a = &assets[ idx ];
		FREE( sys_allocator, a->data.ptr );
	}
	else {
		a = &assets[ num_assets ];
		a->path = ALLOC_MANY( sys_allocator, char, strlen( path ) + 1 );
		Q_strncpyz( a->path, path, strlen( path ) + 1 );
		asset_paths[ num_assets ] = a->path;
	}

	a->data = contents;
	a->modified_time = modified_time;

	modified_asset_paths[ num_modified_assets ] = a->path;
	num_modified_assets++;

	if( !exists ) {
		bool ok = assets_hashtable.add( hash, num_assets );
		num_assets++;

		if( !ok ) {
			Com_Error( ERR_FATAL, "Asset hash name collision %s", path );
		}
	}
}

static void LoadAssetsRecursive( DynamicString * path, size_t skip ) {
	ListDirHandle scan = BeginListDir( path->c_str() );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		if( num_assets == MAX_ASSETS ) {
			Com_Printf( S_COLOR_YELLOW "Too many assets\n" );
			return;
		}

		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		if( dir ) {
			LoadAssetsRecursive( path, skip );
		}
		else {
			LoadAsset( path->c_str(), skip );
		}
		path->truncate( old_len );
	}
}

void InitAssets( TempAllocator * temp ) {
	ZoneScoped;

	num_assets = 0;
	num_modified_assets = 0;
	assets_hashtable.clear();

	const char * root = FS_RootPath( temp );
	DynamicString path( temp, "{}/base", root );
	LoadAssetsRecursive( &path, path.length() + 1 );

	num_modified_assets = 0;
}

void HotloadAssets( TempAllocator * temp ) {
	ZoneScoped;

	num_modified_assets = 0;

	const char * root = FS_RootPath( temp );
	DynamicString path( temp, "{}/base", root );
	LoadAssetsRecursive( &path, path.length() + 1 );

	if( num_modified_assets > 0 ) {
		Com_Printf( "Hotloading:\n" );
		for( const char * path : ModifiedAssetPaths() ) {
			Com_Printf( "    %s\n", path );
		}
	}
}

void DoneHotloadingAssets() {
	num_modified_assets = 0;
}

void ShutdownAssets() {
	for( u32 i = 0; i < num_assets; i++ ) {
		FREE( sys_allocator, assets[ i ].path );
		FREE( sys_allocator, assets[ i ].data.ptr );
	}
}

Span< const char > AssetString( StringHash path ) {
	size_t i;
	if( !assets_hashtable.get( path.hash, &i ) )
		return Span< const char >();
	return assets[ i ].data;
}

Span< const char > AssetString( const char * path ) {
	return AssetString( StringHash( path ) );
}

Span< const u8 > AssetBinary( StringHash path ) {
	size_t i;
	if( !assets_hashtable.get( path.hash, &i ) )
		return Span< const u8 >();
	return Span< const char >( assets[ i ].data.ptr, assets[ i ].data.n - 1 ).cast< const u8 >();
}

Span< const u8 > AssetBinary( const char * path ) {
	return AssetBinary( StringHash( path ) );
}

Span< const char * > AssetPaths() {
	return Span< const char * >( asset_paths, num_assets );
}

Span< const char * > ModifiedAssetPaths() {
	return Span< const char * >( modified_asset_paths, num_modified_assets );
}
