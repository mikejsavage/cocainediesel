#include "windows/miniwindows.h"
#include <io.h>
#include <shlobj.h>
#include <objbase.h>
#include <wchar.h>

#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/sys_fs.h"

bool Sys_FS_CreateDirectory( const char *path ) {
	return CreateDirectoryA( path, NULL ) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static char * ReplaceBackslashes( char * path ) {
	char * cursor = path;
	while( ( cursor = strchr( cursor, '\\' ) ) != NULL ) {
		*cursor = '/';
		cursor++;
	}
	return path;
}

static wchar_t * UTF8ToWide( Allocator * a, const char * utf8 ) {
	int len = MultiByteToWideChar( CP_UTF8, 0, utf8, -1, NULL, 0 );
	assert( len != 0 );

	wchar_t * wide = ALLOC_MANY( a, wchar_t, len );
	MultiByteToWideChar( CP_UTF8, 0, utf8, -1, wide, len );

	return wide;
}

static char * WideToUTF8( Allocator * a, Span< const wchar_t > wide ) {
	int len = WideCharToMultiByte( CP_UTF8, 0, wide.ptr, wide.n, NULL, 0, NULL, NULL );
	assert( len != 0 );

	char * utf8 = ALLOC_MANY( a, char, len + 1 );
	WideCharToMultiByte( CP_UTF8, 0, wide.ptr, wide.n, utf8, len, NULL, NULL );
	utf8[ len ] = '\0';

	return utf8;
}

static char * WideToUTF8( Allocator * a, const wchar_t * wide ) {
	return WideToUTF8( a, Span< const wchar_t >( wide, wcslen( wide ) ) );
}

char * FindHomeDirectory( Allocator * a ) {
	wchar_t * wide_documents_path;
	if( SHGetKnownFolderPath( FOLDERID_Documents, 0, NULL, &wide_documents_path ) != S_OK ) {
		FatalGLE( "SHGetKnownFolderPath" );
	}
	defer { CoTaskMemFree( wide_documents_path ); };

	char * documents_path = WideToUTF8( a, wide_documents_path );
	defer { FREE( a, documents_path ); };

	return ReplaceBackslashes( ( *a )( "{}/My Games/{}", documents_path, APPLICATION ) );
}

char * GetExePath( Allocator * a ) {
	DynamicArray< wchar_t > buf( a );
	buf.resize( 1024 );

	while( true ) {
		DWORD n = GetModuleFileNameW( NULL, buf.ptr(), buf.size() );
		if( n == 0 ) {
			FatalGLE( "GetModuleFileNameW" );
		}

		if( n < buf.size() )
			break;

		buf.resize( buf.size() * 2 );
	}

	return ReplaceBackslashes( WideToUTF8( a, buf.ptr() ) );
}

FILE * OpenFile( Allocator * a, const char * path, const char * mode ) {
	wchar_t * wide_path = UTF8ToWide( a, path );
	wchar_t * wide_mode = UTF8ToWide( a, mode );
	defer { FREE( a, wide_path ); };
	defer { FREE( a, wide_mode ); };
	return _wfopen( wide_path, wide_mode );
}

#undef MoveFile
bool MoveFile( Allocator * a, const char * old_path, const char * new_path, MoveFileReplace replace ) {
	wchar_t * wide_old_path = UTF8ToWide( a, old_path );
	wchar_t * wide_new_path = UTF8ToWide( a, new_path );
	defer { FREE( a, wide_old_path ); };
	defer { FREE( a, wide_new_path ); };

	DWORD flags = replace == MoveFile_DoReplace ? MOVEFILE_REPLACE_EXISTING : 0;

	return MoveFileExW( wide_old_path, wide_new_path, flags ) != 0;
}

bool RemoveFile( Allocator * a, const char * path ) {
	wchar_t * wide_path = UTF8ToWide( a, path );
	defer { FREE( a, wide_path ); };
	return DeleteFileW( wide_path ) != 0;
}

#undef CreateDirectory
bool CreateDirectory( Allocator * a, const char * path ) {
	wchar_t * wide_path = UTF8ToWide( a, path );
	defer { FREE( a, wide_path ); };
	return CreateDirectoryW( wide_path, NULL ) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

struct ListDirHandleImpl {
	HANDLE handle;
	Allocator * a;
	WIN32_FIND_DATAW * ffd;
	char * utf8_path;
	bool first;
};

STATIC_ASSERT( sizeof( ListDirHandleImpl ) <= sizeof( ListDirHandle ) );

static ListDirHandleImpl OpaqueToImpl( ListDirHandle opaque ) {
	ListDirHandleImpl impl;
	memcpy( &impl, opaque.impl, sizeof( impl ) );
	return impl;
}

static ListDirHandle ImplToOpaque( ListDirHandleImpl impl ) {
	ListDirHandle opaque;
	memcpy( opaque.impl, &impl, sizeof( impl ) );
	return opaque;
}

ListDirHandle BeginListDir( Allocator * a, const char * path ) {
	ListDirHandleImpl handle = { };
	handle.a = a;
	handle.ffd = ALLOC( a, WIN32_FIND_DATAW );
	handle.first = true;

	char * path_and_wildcard = ( *a )( "{}/*", path );
	defer { FREE( a, path_and_wildcard ); };

	wchar_t * wide = UTF8ToWide( a, path_and_wildcard );
	defer { FREE( a, wide ); };

	handle.handle = FindFirstFileW( wide, handle.ffd );
	if( handle.handle == INVALID_HANDLE_VALUE ) {
		FREE( handle.a, handle.ffd );
		handle.handle = NULL;
	}

	return ImplToOpaque( handle );
}

bool ListDirNext( ListDirHandle * opaque, const char ** path, bool * dir ) {
	ListDirHandleImpl handle = OpaqueToImpl( *opaque );
	if( handle.handle == NULL )
		return false;

	FREE( handle.a, handle.utf8_path );

	if( !handle.first ) {
		if( FindNextFileW( handle.handle, handle.ffd ) == 0 ) {
			FindClose( handle.handle );
			FREE( handle.a, handle.ffd );
			return false;
		}
	}

	handle.utf8_path = WideToUTF8( handle.a, handle.ffd->cFileName );
	handle.first = false;

	*path = handle.utf8_path;
	*dir = ( handle.ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;

	*opaque = ImplToOpaque( handle );

	return true;
}

struct FSChangeMonitor {
	HANDLE dir;
	HANDLE event;
	OVERLAPPED overlapped;
	u8 buf[ 16384 ];
	bool poll_again;
};

FSChangeMonitor * NewFSChangeMonitor( Allocator * a, const char * path ) {
	FSChangeMonitor * monitor = ALLOC( a, FSChangeMonitor );

	wchar_t * wide_path = UTF8ToWide( a, path );
	defer { FREE( a, wide_path ); };
	monitor->dir = CreateFileW( wide_path, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL );

	monitor->event = CreateEvent( NULL, FALSE, FALSE, NULL );
	monitor->overlapped.hEvent = monitor->event;
	monitor->poll_again = true;

	return monitor;
}

void DeleteFSChangeMonitor( Allocator * a, FSChangeMonitor * monitor ) {
	CloseHandle( monitor->event );
	CloseHandle( monitor->dir );
	FREE( a, monitor );
}

Span< const char * > PollFSChangeMonitor( TempAllocator * temp, FSChangeMonitor * monitor, const char ** results, size_t n ) {
	if( monitor->poll_again ) {
		DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;
		BOOL ok = ReadDirectoryChangesW( monitor->dir, monitor->buf, sizeof( monitor->buf ), true, filter, NULL, &monitor->overlapped, NULL );
		if( ok == FALSE ) {
			FatalGLE( "ReadDirectoryChangesW" );
		}
		monitor->poll_again = false;
	}

	DWORD dont_care;
	BOOL ok = GetOverlappedResult( monitor->dir, &monitor->overlapped, &dont_care, FALSE );
	if( ok == FALSE ) {
		if( GetLastError() == ERROR_IO_INCOMPLETE ) {
			return Span< const char * >();
		}
		FatalGLE( "GetOverlappedResult" );
	}

	monitor->poll_again = true;

	size_t num_results = 0;
	const FILE_NOTIFY_INFORMATION * entry = ( const FILE_NOTIFY_INFORMATION * ) monitor->buf;
	while( num_results < n ) {
		if( entry->Action == FILE_ACTION_ADDED || entry->Action == FILE_ACTION_MODIFIED || entry->Action == FILE_ACTION_RENAMED_NEW_NAME ) {
			Span< const wchar_t > wide_path = Span< const wchar_t >( entry->FileName, entry->FileNameLength / 2 );
			results[ num_results ] = ReplaceBackslashes( WideToUTF8( temp, wide_path ) );
			num_results++;
		}

		if( entry->NextEntryOffset == 0 )
			break;

		entry = ( const FILE_NOTIFY_INFORMATION * ) ( ( ( const char * ) entry ) + entry->NextEntryOffset );
	}

	return Span< const char * >( results, num_results );
}
