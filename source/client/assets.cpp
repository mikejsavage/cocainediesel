#include <sys/types.h>
#include <sys/stat.h>

#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/compression.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/sort.h"
#include "qcommon/string.h"
#include "qcommon/threads.h"
#include "client/assets.h"
#include "client/threadpool.h"

struct Asset {
	char * path;
	Span< u8 > data;
	bool compressed;
};

static constexpr u32 MAX_ASSETS = 4096;

static Mutex * assets_mutex;

static Asset assets[ MAX_ASSETS ];
static const char * asset_paths[ MAX_ASSETS ];
static u32 num_assets;

static FSChangeMonitor * fs_change_monitor;

static const char * modified_asset_paths[ MAX_ASSETS ];
static u32 num_modified_assets;

static Hashtable< MAX_ASSETS * 2 > assets_hashtable;

enum IsCompressedBool : bool {
	IsCompressed_No = false,
	IsCompressed_Yes = true,
};

static void AddAsset( const char * path, u64 hash, Span< u8 > data, IsCompressedBool compressed ) {
	Lock( assets_mutex );
	defer { Unlock( assets_mutex ); };

	if( num_assets == MAX_ASSETS ) {
		Com_Printf( S_COLOR_YELLOW "Too many assets\n" );
		return;
	}

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

	a->data = data;
	a->compressed = compressed;

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
	Span< u8 > compressed;
};

static void DecompressAsset( TempAllocator * temp, void * data ) {
	TracyZoneScoped;

	DecompressAssetJob * job = ( DecompressAssetJob * ) data;

	TracyZoneText( job->path, strlen( job->path ) );

	char * path_with_zst = ( *temp )( "{}.zst", job->path );
	Span< u8 > decompressed;
	if( Decompress( path_with_zst, sys_allocator, job->compressed, &decompressed ) ) {
		AddAsset( job->path, job->hash, decompressed, IsCompressed_Yes );
	}

	FREE( sys_allocator, job->path );
	FREE( sys_allocator, job->compressed.ptr );
	FREE( sys_allocator, job );
}

static void LoadAsset( TempAllocator * temp, const char * game_path, const char * full_path ) {
	TracyZoneScoped;
	TracyZoneText( game_path, strlen( game_path ) );

	Span< const char > ext = FileExtension( game_path );
	bool compressed = ext == ".zst";

	Span< const char > game_path_no_zst = MakeSpan( game_path );
	if( compressed ) {
		game_path_no_zst.n -= ext.n;
	}

	u64 hash = Hash64( game_path_no_zst );

	{
		Lock( assets_mutex );
		defer { Unlock( assets_mutex ); };

		u64 idx;
		bool exists = assets_hashtable.get( hash, &idx );
		if( exists ) {
			if( !StrEqual( game_path_no_zst, asset_paths[ idx ] ) ) {
				Fatal( "Asset hash name collision: %s and %s", game_path, assets[ idx ].path );
			}

			if( compressed && !assets[ idx ].compressed ) {
				return;
			}
		}
	}

	Span< u8 > contents = ReadFileBinary( sys_allocator, full_path );
	if( contents.ptr == NULL )
		return;

	if( compressed ) {
		DecompressAssetJob * job = ALLOC( sys_allocator, DecompressAssetJob );
		job->path = ( *sys_allocator )( "{}", game_path_no_zst );
		job->hash = hash;
		job->compressed = contents;

		ThreadPoolDo( DecompressAsset, job );
	}
	else {
		AddAsset( game_path, hash, contents, IsCompressed_No );
	}
}

static void LoadAssetsRecursive( TempAllocator * temp, DynamicString * path, size_t skip ) {
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
			LoadAssetsRecursive( temp, path, skip );
		}
		else {
			LoadAsset( temp, path->c_str() + skip, path->c_str() );
		}
		path->truncate( old_len );
	}
}

void InitAssets( TempAllocator * temp ) {
	TracyZoneScoped;

	assets_mutex = NewMutex();

	num_assets = 0;
	num_modified_assets = 0;
	assets_hashtable.clear();

	DynamicString base( temp, "{}/base", RootDirPath() );
	fs_change_monitor = NewFSChangeMonitor( sys_allocator, base.c_str() );
	LoadAssetsRecursive( temp, &base, base.length() + 1 );

	num_modified_assets = 0;
}

void HotloadAssets( TempAllocator * temp ) {
	TracyZoneScoped;

	const char * buf[ 1024 ];
	Span< const char * > changes = PollFSChangeMonitor( temp, fs_change_monitor, buf, ARRAY_COUNT( buf ) );
	Sort( changes.begin(), changes.end(), SortCStringsComparator );

	for( size_t i = 0; i < changes.n; i++ ) {
		if( i > 0 && StrEqual( changes[ i ], changes[ i - 1 ] ) )
			continue;
		char * full_path = ( *temp )( "{}/base/{}", RootDirPath(), changes[ i ] );
		LoadAsset( temp, changes[ i ], full_path );
	}

	ThreadPoolFinish();

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
	TracyZoneScoped;

	for( u32 i = 0; i < num_assets; i++ ) {
		FREE( sys_allocator, assets[ i ].path );
		FREE( sys_allocator, assets[ i ].data.ptr );
	}

	DeleteFSChangeMonitor( sys_allocator, fs_change_monitor );

	DeleteMutex( assets_mutex );
}

Span< const char > AssetString( StringHash path ) {
	u64 i;
	if( !assets_hashtable.get( path.hash, &i ) )
		return Span< const char >();
	return assets[ i ].data.cast< const char >();
}

Span< const char > AssetString( const char * path ) {
	return AssetString( StringHash( path ) );
}

Span< const u8 > AssetBinary( StringHash path ) {
	return AssetString( path ).cast< const u8 >();
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
