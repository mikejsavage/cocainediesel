#include <sys/types.h>
#include <sys/stat.h>

#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/compression.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "qcommon/threads.h"
#include "client/assets.h"
#include "client/threadpool.h"

struct Asset {
	char * path;
	Span< char > data;
	s64 modified_time;
	bool compressed;
};

static constexpr u32 MAX_ASSETS = 4096;

static Mutex * assets_mutex;

static Asset assets[ MAX_ASSETS ];
static const char * asset_paths[ MAX_ASSETS ];
static u32 num_assets;

static const char * modified_asset_paths[ MAX_ASSETS ];
static u32 num_modified_assets;

static Hashtable< MAX_ASSETS * 2 > assets_hashtable;

enum IsCompressed {
	IsCompressed_No,
	IsCompressed_Yes,
};

static void AddAsset( const char * path, u64 hash, s64 modified_time, Span< char > contents, IsCompressed compressed ) {
	Lock( assets_mutex );
	defer { Unlock( assets_mutex ); };

	u64 idx;
	bool exists = assets_hashtable.get( hash, &idx );

	Asset * a;
	if( exists ) {
		a = &assets[ idx ];
		FREE( sys_allocator, a->data.ptr );
	}
	else {
		a = &assets[ num_assets ];
		a->path = CopyString( sys_allocator, path );
		asset_paths[ num_assets ] = a->path;
	}

	a->data = contents;
	a->modified_time = modified_time;
	a->compressed = compressed == IsCompressed_Yes;

	modified_asset_paths[ num_modified_assets ] = a->path;
	num_modified_assets++;

	if( !exists ) {
		assets_hashtable.add( hash, num_assets );
		num_assets++;
	}
}

struct DecompressAssetJob {
	char * path;
	u64 hash;
	s64 modified_time;
	Span< char > compressed;
};

static void DecompressAsset( TempAllocator * temp, void * data ) {
	ZoneScoped;

	DecompressAssetJob * job = ( DecompressAssetJob * ) data;

	ZoneText( job->path, strlen( job->path ) );

	Span< u8 > decompressed;
	if( Decompress( job->path, sys_allocator, job->compressed.cast< u8 >(), &decompressed ) ) {
		AddAsset( job->path, job->hash, job->modified_time, decompressed.cast< char >(), IsCompressed_Yes );
	}

	FREE( sys_allocator, job->path );
	FREE( sys_allocator, job->compressed.ptr );
	FREE( sys_allocator, job );
}

static void LoadAsset( TempAllocator * temp, const char * game_path, const char * full_path ) {
	ZoneScoped;
	ZoneText( game_path, strlen( game_path ) );

	Span< const char > ext = LastFileExtension( game_path );
	bool compressed = ext == ".zst";

	Span< const char > game_path_no_zst = MakeSpan( game_path );
	if( compressed ) {
		game_path_no_zst.n -= ext.n;
	}

	u64 hash = Hash64( game_path_no_zst );

	s64 modified_time = FileLastModifiedTime( temp, full_path );

	{
		Lock( assets_mutex );
		defer { Unlock( assets_mutex ); };

		u64 idx;
		bool exists = assets_hashtable.get( hash, &idx );
		if( exists ) {
			if( !StrEqual( game_path_no_zst, asset_paths[ idx ] ) ) {
				Sys_Error( "Asset hash name collision: %s and %s", game_path, assets[ idx ].path );
			}

			bool modified = assets[ idx ].compressed == compressed && assets[ idx ].modified_time != modified_time;
			bool replaces = assets[ idx ].compressed && !compressed;
			if( !( modified || replaces ) ) {
				return;
			}
		}
	}

	Span< char > contents = ReadFileString( sys_allocator, full_path );
	if( contents.ptr == NULL )
		return;

	if( compressed ) {
		DecompressAssetJob * job = ALLOC( sys_allocator, DecompressAssetJob );
		job->path = ( *sys_allocator )( "{}", game_path_no_zst );
		job->hash = hash;
		job->compressed = contents.slice( 0, contents.n - 1 );
		job->modified_time = modified_time;

		ThreadPoolDo( DecompressAsset, job );
	}
	else {
		AddAsset( game_path, hash, modified_time, contents, IsCompressed_No );
	}
}

static void LoadAssetsRecursive( TempAllocator * temp, DynamicString * path, size_t skip ) {
	ListDirHandle scan = BeginListDir( temp, path->c_str() );

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
			LoadAssetsRecursive( temp, path, skip );
		}
		else {
			LoadAsset( temp, path->c_str() + skip, path->c_str() );
		}
		path->truncate( old_len );
	}
}

void InitAssets( TempAllocator * temp ) {
	ZoneScoped;

	assets_mutex = NewMutex();

	num_assets = 0;
	num_modified_assets = 0;
	assets_hashtable.clear();

	const char * root = FS_RootPath( temp );
	DynamicString base( temp, "{}/base", root );
	LoadAssetsRecursive( temp, &base, base.length() + 1 );

	num_modified_assets = 0;
}

void HotloadAssets( TempAllocator * temp ) {
	ZoneScoped;

	num_modified_assets = 0;

	const char * root = FS_RootPath( temp );
	DynamicString base( temp, "{}/base", root );
	LoadAssetsRecursive( temp, &base, base.length() + 1 );

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

	DeleteMutex( assets_mutex );
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
