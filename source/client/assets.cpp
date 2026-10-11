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
	Span< const char > path;
	Span< const u8 > data;
	bool compressed;
	bool virtual_free;
};

static constexpr u32 MAX_ASSETS = 4096;

static Opaque< Mutex > assets_mutex;

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

static void CheckedVirtualFree( const void * ptr ) {
	if( VirtualFree( const_cast< void * >( ptr ), 0, MEM_RELEASE ) == 0 ) {
		FatalGLE( "VirtualFree" );
	}
}

#endif

static void FreeAssetData( Asset * asset ) {
#if PLATFORM_WINDOWS
	if( asset->virtual_free ) {
		// this can be NULL because VirtualAlloc( 0 ) returns NULL
		if( asset->data.ptr != NULL ) {
			CheckedVirtualFree( asset->data.ptr );
		}
		return;
	}
#endif

	Free( sys_allocator, asset->data.ptr );
}

static void AddAsset( Span< const char > path, u64 hash, Span< const u8 > data, IsCompressedBool compressed, UseVirtualFree virtual_free ) {
	TracyZoneScoped;

	Lock( &assets_mutex );
	defer { Unlock( &assets_mutex ); };

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
	Span< const char > path;
	u64 hash;
	Span< const u8 > compressed;
	bool virtual_free;
};

static void DecompressAsset( TempAllocator * temp, void * data ) {
	TracyZoneScoped;

	DecompressAssetJob * job = ( DecompressAssetJob * ) data;

	TracyZoneSpan( job->path );

	Span< const char > path_with_zst = temp->sv( "{}.zst", job->path );
	Span< u8 > decompressed;
	if( Decompress( path_with_zst, sys_allocator, job->compressed, &decompressed ) ) {
		AddAsset( job->path, job->hash, decompressed, IsCompressed_Yes, UseVirtualFree_No );
	}

	Free( sys_allocator, job->path.ptr );

#if PLATFORM_WINDOWS
	if( job->virtual_free ) {
		CheckedVirtualFree( job->compressed.ptr );
	}
	else {
		Free( sys_allocator, job->compressed.ptr );
	}
#else
	Free( sys_allocator, job->compressed.ptr );
#endif

	Free( sys_allocator, job );
}

static void LaunchDecompressAssetJob( Span< const char > path, u64 hash, Span< const u8 > compressed, UseVirtualFree virtual_free ) {
	DecompressAssetJob * job = Alloc< DecompressAssetJob >( sys_allocator );
	job->path = CloneSpan( sys_allocator, path );
	job->hash = hash;
	job->compressed = compressed;
	job->virtual_free = virtual_free;
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
		Lock( &assets_mutex );
		defer { Unlock( &assets_mutex ); };

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

	Span< const u8 > contents = ReadFileBinary( sys_allocator, full_path );
	if( contents.ptr == NULL )
		return;

	if( compressed ) {
		LaunchDecompressAssetJob( game_path_no_zst, hash, contents, UseVirtualFree_No );
	}
	else {
		AddAsset( game_path, hash, contents, IsCompressed_No, UseVirtualFree_No );
	}
}

#if PLATFORM_WINDOWS

static size_t FileSize( HANDLE file ) {
	LARGE_INTEGER size;
	if( GetFileSizeEx( file, &size ) == 0 ) {
		FatalGLE( "GetFileSizeEx" );
	}
	return size.QuadPart;
}

void LoadAssets( TempAllocator * temp, Span< const char * > files, size_t skip ) {
	TracyZoneScoped;

	HANDLE iocp = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if( iocp == NULL ) {
		FatalGLE( "CreateIoCompletionPort" );
	}
	defer { CloseHandle( iocp ); };

	struct LoadAssetJob {
		// in
		HANDLE shared_iocp;
		const char * path;
		size_t completion_key;

		// out
		HANDLE file_handle;
		HANDLE file_iocp;

		OVERLAPPED overlapped;
		Optional< Span< u8 > > buffer;
	};

	Span< LoadAssetJob > jobs = AllocSpan< LoadAssetJob >( temp, files.n );
	{
		TracyZoneScopedN( "Open files" );

		for( size_t i = 0; i < files.n; i++ ) {
			jobs[ i ] = {
				.shared_iocp = iocp,
				.path = files[ i ],
				.completion_key = i,
			};
		}

		ParallelFor( jobs, []( TempAllocator * temp, void * data ) {
			LoadAssetJob * job = ( LoadAssetJob * ) data;
			wchar_t * wide_path = UTF8ToWide( temp, job->path );

			job->file_handle = CreateFileW( wide_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING, NULL );
			if( job->file_handle == INVALID_HANDLE_VALUE ) {
				return;
			}

			job->file_iocp = CreateIoCompletionPort( job->file_handle, job->shared_iocp, checked_cast< ULONG_PTR >( job->completion_key ), 1 );
			if( job->file_iocp == INVALID_HANDLE_VALUE ) {
				FatalGLE( "CreateIoCompletionPort" );
			}

			size_t actual_size = FileSize( job->file_handle );
			size_t aligned_size = AlignPow2( actual_size, 4096 );

			// VirtualAlloc( 0 ) fails so return a dummy but non-NONE Span here
			if( actual_size == 0 ) {
				job->buffer = Span< u8 >();
				return;
			}

			// there's no particular reason to use VirtualAlloc beyond it returning 4k aligned allocs
			// NOTE(mike 20261011): using a single VirtualAlloc benchmarks about 10% slower
			job->buffer = Span< u8 >( ( u8 * ) VirtualAlloc( NULL, aligned_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE ), actual_size );
			if( job->buffer.value.ptr == NULL ) {
				FatalGLE( "VirtualAlloc" );
			}

			BOOL ok = ReadFile( job->file_handle, job->buffer->ptr, AlignPow2( job->buffer->n, 4096 ), NULL, &job->overlapped );
			if( ok != TRUE && GetLastError() != ERROR_IO_PENDING ) {
				CheckedVirtualFree( job->buffer->ptr );
				job->buffer = NONE;
			}
		} );
	}

	size_t num_reads = 0;
	for( const LoadAssetJob & job : jobs ) {
		if( job.buffer.exists ) {
			num_reads++;
		}
	}

	{
		TracyZoneScopedN( "Add assets" );
		for( size_t i = 0; i < num_reads; i++ ) {
			ULONG_PTR key;
			{
				TracyZoneScopedNC( "GetQueuedCompletionStatus", TracyColorWait );
				DWORD r;
				OVERLAPPED * overlapped = NULL;
				if( GetQueuedCompletionStatus( iocp, &r, &key, &overlapped, INFINITE ) == FALSE ) {
					if( overlapped == NULL ) {
						FatalGLE( "GetQueuedCompletionStatus" );
					}
					else {
						continue;
					}
				}
			}

			const LoadAssetJob & job = jobs[ key ];
			Span< const char > game_path = MakeSpan( job.path + skip );
			Span< const char > ext = FileExtension( game_path );
			if( ext == ".zst" ) {
				game_path.n -= ext.n;
				LaunchDecompressAssetJob( game_path, Hash64( game_path ), job.buffer.value, UseVirtualFree_Yes );
			}
			else {
				AddAsset( game_path, Hash64( game_path ), job.buffer.value, IsCompressed_No, UseVirtualFree_Yes );
			}
		}
	}

	ThreadPoolFinish();

	{
		TracyZoneScopedN( "Cleanup" );
		ParallelFor( jobs, []( TempAllocator * temp, void * data ) {
			LoadAssetJob * job = ( LoadAssetJob * ) data;
			CloseHandle( job->file_handle );
			CloseHandle( job->file_iocp );
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

void InitAssets( TempAllocator * temp ) {
	TracyZoneScoped;

	InitMutex( &assets_mutex );

	num_assets = 0;
	num_modified_assets = 0;
	assets_hashtable.clear();

	const char * base = ( *temp )( "{}/base", RootDirPath() );
	Span< const char > base2 = MakeSpan( base );
	fs_change_monitor = NewFSChangeMonitor( sys_allocator, base );

	Span< Span< const char > > files;
	defer {
		for( Span< const char > f : files ) {
			Free( sys_allocator, f.ptr );
		}
		Free( sys_allocator, files.ptr );
	};

	{
		TracyZoneScopedN( "ListDir" );
		files = ListDir( sys_allocator, base2, ListDir_Recurse ).cast< Span< const char > >();
	}

	{
		TracyZoneScopedN( "Sort" );
		nanosort( files.begin(), files.end(), SortSpanStringsComparator );
	}

	// remove file.zst if file is in the list too
	DynamicArray< const char * > deduped( sys_allocator );
	defer {
		for( const char * f : deduped ) {
			Free( sys_allocator, f );
		}
	};
	for( size_t i = 0; i < files.n; i++ ) {
		if( i == 0 || FileExtension( files[ i ] ) != ".zst" || !StrEqual( StripExtension( files[ i ] ), files[ i - 1 ] ) ) {
			deduped.add( ( *sys_allocator )( "{}", files[ i ] ) );
		}
	}

	LoadAssets( temp, deduped.span(), base2.n + 1 );

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

	DeleteMutex( &assets_mutex );
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
