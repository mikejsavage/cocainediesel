/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "windows/miniwindows.h"
#include <io.h>
#include <shlobj.h>
#include <objbase.h>

#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "qcommon/fs.h"
#include "qcommon/sys_fs.h"
#include "qcommon/utf8.h"

/*
* Sys_FS_CreateDirectory
*/
bool Sys_FS_CreateDirectory( const char *path ) {
	return CreateDirectoryA( path, NULL ) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static wchar_t * UTF8ToWide( Allocator * a, const char * utf8 ) {
	int len = MultiByteToWideChar( CP_UTF8, 0, utf8, -1, NULL, 0 );
	assert( len != 0 );

	wchar_t * wide = ALLOC_MANY( a, wchar_t, len );
	MultiByteToWideChar( CP_UTF8, 0, utf8, -1, wide, len );

	return wide;
}

static char * WideToUTF8( Allocator * a, const wchar_t * wide ) {
	int len = WideCharToMultiByte( CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL );
	assert( len != 0 );

	char * utf8 = ALLOC_MANY( a, char, len );
	WideCharToMultiByte( CP_UTF8, 0, wide, -1, utf8, len, NULL, NULL );

	return utf8;
}

char * FindHomeDirectory( Allocator * a ) {
	wchar_t * wide_documents_path;
	if( SHGetKnownFolderPath( FOLDERID_Documents, 0, NULL, &wide_documents_path ) != S_OK ) {
		Sys_Error( "SHGetKnownFolderPath" );
	}
	defer { CoTaskMemFree( wide_documents_path ); };

	char * documents_path = WideToUTF8( a, wide_documents_path );
	defer { FREE( a, documents_path ); };

	return ( *a )( "{}/My Games/{}", documents_path, APPLICATION );
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

	DynamicString path_and_wildcard( a, "{}/*", path );

	wchar_t * wide = UTF8ToWide( a, path_and_wildcard.c_str() );
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

FileMetadata FileMetadataOrZeroes( TempAllocator * temp, const char * path ) {
	wchar_t * wide = UTF8ToWide( temp, path );

	HANDLE handle = CreateFileW( wide, 0, 0, NULL, OPEN_EXISTING, 0, NULL );
	if( handle == INVALID_HANDLE_VALUE ) {
		return { };
	}

	defer { CloseHandle( handle ); };

	LARGE_INTEGER size;
	if( GetFileSizeEx( handle, &size ) == 0 ) {
		return { };
	}

	FILETIME modified;
	if( GetFileTime( handle, NULL, NULL, &modified ) == 0 ) {
		return { };
	}

	FileMetadata metadata;

	ULARGE_INTEGER modified64;
	memcpy( &modified64, &modified, sizeof( modified ) );

	metadata.size = size.QuadPart;
	metadata.modified_time = modified64.QuadPart;

	return metadata;
}
