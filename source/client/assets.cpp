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
	char * data;
	size_t len;
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

static void AddAsset( const char * path, u64 hash, s64 modified_time, char * contents, size_t len, IsCompressed compressed ) {
	Lock( assets_mutex );
	defer { Unlock( assets_mutex ); };

	u64 idx;
	bool exists = assets_hashtable.get( hash, &idx );

	Asset * a;
	if( exists ) {
		a = &assets[ idx ];
		FREE( sys_allocator, a->data );
	}
	else {
		a = &assets[ num_assets ];
		a->path = CopyString( sys_allocator, path );
		asset_paths[ num_assets ] = a->path;
	}

	a->data = contents;
	a->len = len;
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
	Span< u8 > compressed;
};

static void DecompressAsset( TempAllocator * temp, void * data ) {
	ZoneScoped;

	DecompressAssetJob * job = ( DecompressAssetJob * ) data;

	ZoneText( job->path, strlen( job->path ) );

	Span< u8 > decompressed;
	if( Decompress( job->path, sys_allocator, job->compressed, &decompressed ) ) {
		// TODO: we need to add a null terminator too so AssetString etc works
		char * decompressed_and_terminated = ALLOC_MANY( sys_allocator, char, decompressed.n + 1 );
		memcpy( decompressed_and_terminated, decompressed.ptr, decompressed.n );
		decompressed_and_terminated[ decompressed.n ] = '\0';

		AddAsset( job->path, job->hash, job->modified_time, decompressed_and_terminated, decompressed.n, IsCompressed_Yes );
	}

	FREE( sys_allocator, decompressed.ptr );
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

	size_t len;
	char * contents = ReadFileString( sys_allocator, full_path, &len );
	if( contents == NULL )
		return;

	if( compressed ) {
		DecompressAssetJob * job = ALLOC( sys_allocator, DecompressAssetJob );
		job->path = ( *sys_allocator )( "{}", game_path_no_zst );
		job->hash = hash;
		job->compressed = Span< u8 >( ( u8 * ) contents, len );
		job->modified_time = modified_time;

		ThreadPoolDo( DecompressAsset, job );
	}
	else {
		AddAsset( game_path, hash, modified_time, contents, len, IsCompressed_No );
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

	DynamicString base( temp, "{}/base", RootDirPath() );
	LoadAssetsRecursive( temp, &base, base.length() + 1 );

	num_modified_assets = 0;
}

void HotloadAssets( TempAllocator * temp ) {
	ZoneScoped;

	num_modified_assets = 0;

	DynamicString base( temp, "{}/base", RootDirPath() );
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
		FREE( sys_allocator, assets[ i ].data );
	}

	DeleteMutex( assets_mutex );
}

Span< const char > AssetString( StringHash path ) {
	size_t i;
	if( !assets_hashtable.get( path.hash, &i ) )
		return Span< const char >();
	return Span< const char >( assets[ i ].data, assets[ i ].len );
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
