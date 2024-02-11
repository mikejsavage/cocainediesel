#include <sys/types.h>
#include <sys/stat.h>

#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/compression.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "qcommon/threadpool.h"
#include "qcommon/threads.h"
#include "client/assets.h"

#include "nanosort/nanosort.hpp"

struct Asset {
	Span< char > path;
	Span< u8 > data;
	bool compressed;
	bool virtual_free;
};

static constexpr u32 MAX_ASSETS = 4096;

static Mutex * assets_mutex;

static Asset assets[ MAX_ASSETS ];
static Span< const char > asset_paths[ MAX_ASSETS ];
static u32 num_assets;

static FSChangeMonitor * fs_change_monitor;

static Span< const char > modified_asset_paths[ MAX_ASSETS ];
static u32 num_modified_assets;

static Hashtable< MAX_ASSETS * 2 > assets_hashtable;

enum IsCompressedBool : bool {
	IsCompressed_No = false,
	IsCompressed_Yes = true,
};

enum UseVirtualFree : bool {
	UseVirtualFree_No = false,
	UseVirtualFree_Yes = true,
};

#if PLATFORM_WINDOWS

#include "qcommon/platform/windows_mini_windows_h.h"
#include "qcommon/platform/windows_utf8.h"

static void CheckedVirtualFree( void * ptr ) {
	if( VirtualFree( ptr, 0, MEM_RELEASE ) == 0 ) {
		FatalGLE( "VirtualFree" );
	}
}

#endif

static void FreeAssetData( Asset * asset ) {
#if PLATFORM_WINDOWS
	if( asset->virtual_free ) {
		CheckedVirtualFree( asset->data.ptr );
		return;
	}
#endif

	Free( sys_allocator, asset->data.ptr );
}

static void AddAsset( Span< const char > path, u64 hash, Span< u8 > data, IsCompressedBool compressed, UseVirtualFree virtual_free ) {
	TracyZoneScoped;

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
		FreeAssetData( a );
	}
	else {
		a = &assets[ num_assets ];
		a->path = CloneSpan( sys_allocator, path );
		asset_paths[ num_assets ] = a->path;
	}

	a->data = data;
	a->compressed = compressed;
	a->virtual_free = virtual_free;

	modified_asset_paths[ num_modified_assets ] = a->path;
	num_modified_assets++;

	if( !exists ) {
		assets_hashtable.add( hash, num_assets );
		num_assets++;
	}
}

struct DecompressAssetJob {
	Span< char > path;
	u64 hash;
	Span< u8 > compressed;
};

static void DecompressAsset( TempAllocator * temp, void * data ) {
	TracyZoneScoped;

	DecompressAssetJob * job = ( DecompressAssetJob * ) data;

	TracyZoneSpan( job->path );

	Span< char > path_with_zst = temp->sv( "{}.zst", job->path );
	Span< u8 > decompressed;
	if( Decompress( path_with_zst, sys_allocator, job->compressed, &decompressed ) ) {
		AddAsset( job->path, job->hash, decompressed, IsCompressed_Yes, UseVirtualFree_No );
	}

	Free( sys_allocator, job->path.ptr );
#if PLATFORM_WINDOWS
	CheckedVirtualFree( job->compressed.ptr );
#else
	Free( sys_allocator, job->compressed.ptr );
#endif
	Free( sys_allocator, job );
}

static void LaunchDecompressAssetJob( Span< const char > path, u64 hash, Span< u8 > compressed ) {
	DecompressAssetJob * job = Alloc< DecompressAssetJob >( sys_allocator );
	job->path = CloneSpan( sys_allocator, path );
	job->hash = hash;
	job->compressed = compressed;
	ThreadPoolDo( DecompressAsset, job );
}

static void LoadAsset( TempAllocator * temp, Span< const char > game_path, const char * full_path ) {
	TracyZoneScoped;
	TracyZoneSpan( game_path );

	Span< const char > ext = FileExtension( game_path );
	bool compressed = ext == ".zst";

	Span< const char > game_path_no_zst = game_path;
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
				Fatal( "%s", ( *temp )( "Asset hash name collision: {} and {}", game_path, assets[ idx ].path ) );
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
		LaunchDecompressAssetJob( game_path_no_zst, hash, contents );
	}
	else {
		AddAsset( game_path, hash, contents, IsCompressed_No, UseVirtualFree_No );
	}
}

#if PLATFORM_WINDOWS

static constexpr size_t MAX_ASYNC_READ_SIZE = 16 * 1024 * 1024; // 16MB

struct LoadAssetResult {
	Span< u8 > data;
};

struct HandleAndPath {
	HANDLE handle;
	const char * path;
};

static size_t FileSize( HANDLE file ) {
	LARGE_INTEGER size;
	if( GetFileSizeEx( file, &size ) == 0 ) {
		FatalGLE( "GetFileSizeEx" );
	}
	return size.QuadPart;
}

void LoadAssets( TempAllocator * temp, Span< const char * > files, size_t skip ) {
	TracyZoneScoped;

	DynamicArray< LoadAssetResult > results( temp );

	Span< HandleAndPath > handles_and_paths = AllocSpan< HandleAndPath >( temp, files.n );
	{
		TracyZoneScopedN( "Open files" );

		for( size_t i = 0; i < files.n; i++ ) {
			handles_and_paths[ i ] = { .path = files[ i ] };
		}

		ParallelFor( handles_and_paths, []( TempAllocator * temp, void * data ) {
			HandleAndPath * hp = ( HandleAndPath * ) data;
			wchar_t * wide_path = UTF8ToWide( temp, hp->path );
			hp->handle = CreateFileW( wide_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING, NULL );
		} );
	}

	Span< Span< u8 > > buffers = AllocSpan< Span< u8 > >( temp, handles_and_paths.n );
	{
		TracyZoneScopedN( "VirtualAllocs" );
		for( size_t i = 0; i < handles_and_paths.n; i++ ) {
			if( handles_and_paths[ i ].handle == INVALID_HANDLE_VALUE ) {
				buffers[ i ] = Span< u8 >();
				continue;
			}

			size_t actual_size = FileSize( handles_and_paths[ i ].handle );
			size_t aligned_size = AlignPow2( actual_size, 4096 );
			// there's no particular reason to use VirtualAlloc beyond it returning 4k aligned allocs
			buffers[ i ] = Span< u8 >( ( u8 * ) VirtualAlloc( NULL, aligned_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE ), actual_size );
		}
	}

	{
		TracyZoneScopedN( "Async I/O" );

		// keep 32 async reads in flight at a time. chosen semi-arbitrarily by
		// seeing where perf pleateaus on my computer, which is around 60% of
		// my SSD's paper bandwidth
		constexpr size_t overlap = 32;
		OVERLAPPED overlapped[ overlap ];

		for( size_t i = 0; i < files.n + overlap - 1; i++ ) {
			if( i < files.n && handles_and_paths[ i ].handle != INVALID_HANDLE_VALUE ) {
				overlapped[ i % overlap ] = { };
				if( ReadFile( handles_and_paths[ i ].handle, buffers[ i ].ptr, AlignPow2( buffers[ i ].n, 4096 ), NULL, &overlapped[ i % overlap ] ) != TRUE && GetLastError() != ERROR_IO_PENDING ) {
					CheckedVirtualFree( buffers[ i ].ptr );
					buffers[ i ] = Span< u8 >();
				}
			}

			if( i > overlap - 1 ) {
				size_t prev = i - ( overlap - 1 );
				if( handles_and_paths[ prev ].handle != INVALID_HANDLE_VALUE ) {
					DWORD r;
					BOOL ok = GetOverlappedResult( handles_and_paths[ prev ].handle, &overlapped[ prev % overlap ], &r, TRUE );
					if( ok == 0 && GetLastError() || r != buffers[ prev ].n ) {
						CheckedVirtualFree( buffers[ prev ].ptr );
						buffers[ prev ] = Span< u8 >();
					}

					Span< const char > game_path = MakeSpan( files[ prev ] + skip );
					Span< const char > ext = FileExtension( game_path );
					if( ext == ".zst" ) {
						game_path.n -= ext.n;
						LaunchDecompressAssetJob( game_path, Hash64( game_path ), buffers[ prev ] );
					}
					else {
						AddAsset( game_path, Hash64( game_path ), buffers[ prev ], IsCompressed_No, UseVirtualFree_Yes );
					}
				}
			}
		}
	}

	{
		TracyZoneScopedN( "Cleanup" );
		ParallelFor( handles_and_paths, []( TempAllocator * temp, void * data ) {
			HandleAndPath * hp = ( HandleAndPath * ) data;
			CloseHandle( hp->handle );
		} );
	}
}

#else

void LoadAssets( TempAllocator * temp, Span< const char * > files, size_t skip ) {
	TracyZoneScoped;
	for( const char * file : files ) {
		LoadAsset( temp, MakeSpan( file + skip ), file );
	}
}

#endif

static void BuildAssetList( TempAllocator * temp, DynamicArray< const char * > * files, DynamicString * search_path ) {
	ListDirHandle scan = BeginListDir( temp, search_path->c_str() );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = search_path->length();
		search_path->append( "/{}", name );
		if( dir ) {
			BuildAssetList( temp, files, search_path );
		}
		else {
			files->add( CopyString( temp, search_path->c_str() ) );
		}
		search_path->truncate( old_len );
	}
}

void InitAssets( TempAllocator * temp ) {
	TracyZoneScoped;

	assets_mutex = NewMutex();

	num_assets = 0;
	num_modified_assets = 0;
	assets_hashtable.clear();

	DynamicString base( temp, "{}/base", RootDirPath() );
	size_t skip = base.length() + 1;
	fs_change_monitor = NewFSChangeMonitor( sys_allocator, base.c_str() );

	DynamicArray< const char * > files( temp );
	{
		TracyZoneScopedN( "BuildAssetList" );
		BuildAssetList( temp, &files, &base );
		{
			TracyZoneScopedN( "Sort" );
			nanosort( files.begin(), files.end(), SortCStringsComparator );
		}
	}

	// remove file.zst if file is in the list too
	DynamicArray< const char * > deduped( temp );
	for( size_t i = 0; i < files.size(); i++ ) {
		if( i == 0 || FileExtension( files[ i ] ) != ".zst" || !StrEqual( StripExtension( files[ i ] ), files[ i - 1 ] ) ) {
			deduped.add( files[ i ] );
		}
	}

	LoadAssets( temp, deduped.span(), skip );

	// const char * base = temp( "{}/base", RootDirPath() );
	// size_t skip = strlen( base ) + 1;
	// Span< Span< char > > assets = ListDir( sys_allocator, base, true );
	// for( Span< char > asset : assets ) {
	// 	LoadAsset( temp, asset, skip );
	// 	FREE( sys_allocator, asset.ptr );
	// }
	// // FreeAll( sys_allocator, assets );
	// FREE( sys_allocator, assets.ptr );

	num_modified_assets = 0;
}

void HotloadAssets( TempAllocator * temp ) {
	TracyZoneScoped;

	const char * buf[ 1024 ];
	Span< const char * > changes = PollFSChangeMonitor( temp, fs_change_monitor, buf, ARRAY_COUNT( buf ) );
	nanosort( changes.begin(), changes.end(), SortCStringsComparator );

	for( size_t i = 0; i < changes.n; i++ ) {
		if( i > 0 && StrEqual( changes[ i ], changes[ i - 1 ] ) )
			continue;
		const char * full_path = ( *temp )( "{}/base/{}", RootDirPath(), changes[ i ] );
		LoadAsset( temp, MakeSpan( changes[ i ] ), full_path );
	}

	ThreadPoolFinish();

	if( num_modified_assets > 0 ) {
		Com_Printf( "Hotloading:\n" );
		for( Span< const char > path : ModifiedAssetPaths() ) {
			Com_GGPrint( "    {}", path );
		}
	}
}

void DoneHotloadingAssets() {
	num_modified_assets = 0;
}

void ShutdownAssets() {
	TracyZoneScoped;

	for( u32 i = 0; i < num_assets; i++ ) {
		Free( sys_allocator, assets[ i ].path.ptr );
		FreeAssetData( &assets[ i ] );
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

Span< const char > AssetString( Span< const char > path ) {
	return AssetString( StringHash( path ) );
}

Span< const u8 > AssetBinary( StringHash path ) {
	return AssetString( path ).cast< const u8 >();
}

Span< const u8 > AssetBinary( Span< const char > path ) {
	return AssetBinary( StringHash( path ) );
}

Span< Span< const char > > AssetPaths() {
	return Span< Span< const char > >( asset_paths, num_assets );
}

Span< Span< const char > > ModifiedAssetPaths() {
	return Span< Span< const char > >( modified_asset_paths, num_modified_assets );
}
