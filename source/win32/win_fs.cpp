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

#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "qcommon/fs.h"

#include "qcommon/sys_fs.h"

#include "winquake.h"
#include <direct.h>
#include <shlobj.h>

#ifndef CSIDL_APPDATA
# define CSIDL_APPDATA                  0x001A
#endif

#ifndef CSIDL_PERSONAL
# define CSIDL_PERSONAL                 0x0005        // My Documents
#endif

static char *findbase = NULL;
static char *findpath = NULL;
static size_t findpath_size = 0;
static intptr_t findhandle = -1;

/*
* CompareAttributes
*/
static bool CompareAttributes( unsigned found, unsigned musthave, unsigned canthave ) {
	if( ( found & _A_RDONLY ) && ( canthave & SFF_RDONLY ) ) {
		return false;
	}
	if( ( found & _A_HIDDEN ) && ( canthave & SFF_HIDDEN ) ) {
		return false;
	}
	if( ( found & _A_SYSTEM ) && ( canthave & SFF_SYSTEM ) ) {
		return false;
	}
	if( ( found & _A_SUBDIR ) && ( canthave & SFF_SUBDIR ) ) {
		return false;
	}
	if( ( found & _A_ARCH ) && ( canthave & SFF_ARCH ) ) {
		return false;
	}

	if( ( musthave & SFF_RDONLY ) && !( found & _A_RDONLY ) ) {
		return false;
	}
	if( ( musthave & SFF_HIDDEN ) && !( found & _A_HIDDEN ) ) {
		return false;
	}
	if( ( musthave & SFF_SYSTEM ) && !( found & _A_SYSTEM ) ) {
		return false;
	}
	if( ( musthave & SFF_SUBDIR ) && !( found & _A_SUBDIR ) ) {
		return false;
	}
	if( ( musthave & SFF_ARCH ) && !( found & _A_ARCH ) ) {
		return false;
	}

	return true;
}

/*
* _Sys_Utf8FileNameToWide
*/
static void _Sys_Utf8FileNameToWide( const char *utf8name, wchar_t *wname, size_t wchars ) {
	MultiByteToWideChar( CP_UTF8, 0, utf8name, -1, wname, wchars );
	wname[wchars - 1] = '\0';
}

/*
* _Sys_WideFileNameToUtf8
*/
static void _Sys_WideFileNameToUtf8( const wchar_t *wname, char *utf8name, size_t utf8chars ) {
	WideCharToMultiByte( CP_UTF8, 0, wname, -1, utf8name, utf8chars, NULL, NULL );
	utf8name[utf8chars - 1] = '\0';
}

/*
* Sys_FS_FindFirst
*/
const char *Sys_FS_FindFirst( const char *path, unsigned musthave, unsigned canthave ) {
	size_t size;
	struct _wfinddata_t findinfo;
	char finame[MAX_PATH];
	WCHAR wpath[MAX_PATH];

	assert( path );
	assert( findhandle == -1 );
	assert( !findbase && !findpath && !findpath_size );

	if( findhandle != -1 ) {
		Sys_Error( "Sys_FindFirst without close" );
	}

	findbase = ( char * ) Mem_TempMalloc( sizeof( char ) * ( strlen( path ) + 1 ) );
	Q_strncpyz( findbase, path, ( strlen( path ) + 1 ) );
	COM_StripFilename( findbase );

	_Sys_Utf8FileNameToWide( path, wpath, ARRAY_COUNT( wpath ) );

	findhandle = _wfindfirst( wpath, &findinfo );

	if( findhandle == -1 ) {
		return NULL;
	}

	_Sys_WideFileNameToUtf8( findinfo.name, finame, sizeof( finame ) );

	if( strcmp( finame, "." ) && strcmp( finame, ".." ) &&
		CompareAttributes( findinfo.attrib, musthave, canthave ) ) {
		size_t finame_len = strlen( finame );
		size = sizeof( char ) * ( strlen( findbase ) + 1 + finame_len + 1 + 1 );
		if( findpath_size < size ) {
			if( findpath ) {
				Mem_TempFree( findpath );
			}
			findpath_size = size * 2; // extra space to reduce reallocs
			findpath = ( char * ) Mem_TempMalloc( findpath_size );
		}

		Q_snprintfz( findpath, findpath_size, "%s/%s%s", findbase, finame,
					 ( findinfo.attrib & _A_SUBDIR ) && finame[finame_len - 1] != '/' ? "/" : "" );
		return findpath;
	}

	return Sys_FS_FindNext( musthave, canthave );
}

/*
* Sys_FS_FindNext
*/
const char *Sys_FS_FindNext( unsigned musthave, unsigned canthave ) {
	size_t size;
	struct _wfinddata_t findinfo;
	char finame[MAX_PATH];

	assert( findhandle != -1 );
	assert( findbase );

	if( findhandle == -1 ) {
		return NULL;
	}

	while( _wfindnext( findhandle, &findinfo ) != -1 ) {
		_Sys_WideFileNameToUtf8( findinfo.name, finame, sizeof( finame ) );

		if( strcmp( finame, "." ) && strcmp( finame, ".." ) &&
			CompareAttributes( findinfo.attrib, musthave, canthave ) ) {
			size_t finame_len = strlen( finame );
			size = sizeof( char ) * ( strlen( findbase ) + 1 + finame_len + 1 + 1 );
			if( findpath_size < size ) {
				if( findpath ) {
					Mem_TempFree( findpath );
				}
				findpath_size = size * 2; // extra space to reduce reallocs
				findpath = ( char * ) Mem_TempMalloc( findpath_size );
			}

			Q_snprintfz( findpath, findpath_size, "%s/%s%s", findbase, finame,
						 ( findinfo.attrib & _A_SUBDIR ) && finame[finame_len - 1] != '/' ? "/" : "" );
			return findpath;
		}
	}

	return NULL;
}

/*
* Sys_FS_FindClose
*/
void Sys_FS_FindClose( void ) {
	assert( findbase );

	if( findhandle != -1 ) {
		_findclose( findhandle );
		findhandle = -1;
	}

	Mem_TempFree( findbase );
	findbase = NULL;

	if( findpath ) {
		Mem_TempFree( findpath );
		findpath = NULL;
		findpath_size = 0;
	}
}

/*
* Sys_FS_GetHomeDirectory
*/
const char *Sys_FS_GetHomeDirectory( void ) {
	int csidl = CSIDL_PERSONAL;

	static char home[MAX_PATH] = { '\0' };
	if( home[0] != '\0' ) {
		return home;
	}

#ifndef SHGetFolderPath
	HINSTANCE shFolderDll = LoadLibrary( "shfolder.dll" );

	if( !shFolderDll ) {
		shFolderDll = LoadLibrary( "shell32.dll" );
	}

	SHGetFolderPath = GetProcAddress( shFolderDll, "SHGetFolderPathA" );
	if( SHGetFolderPath ) {
		SHGetFolderPath( NULL, csidl, 0, 0, home );
	}

	FreeLibrary( shFolderDll );
#else
	SHGetFolderPath( 0, csidl, 0, 0, home );
#endif

	if( home[0] == '\0' ) {
		return NULL;
	}

	Q_strncpyz( home, va( "%s/My Games/%s 0.0", COM_SanitizeFilePath( home ), APPLICATION ), sizeof( home ) );

	return home;
}

/*
* Sys_FS_GetCacheDirectory
*/
const char *Sys_FS_GetCacheDirectory( void ) {
	return NULL;
}

/*
* Sys_FS_LockFile
*/
void *Sys_FS_LockFile( const char *path ) {
	HANDLE handle;

	handle = CreateFile( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if( handle == INVALID_HANDLE_VALUE ) {
		return NULL;
	}
	return (void *)handle;
}

/*
* Sys_FS_UnlockFile
*/
void Sys_FS_UnlockFile( void *handle ) {
	CloseHandle( (HANDLE)handle );
}

/*
* Sys_FS_CreateDirectory
*/
bool Sys_FS_CreateDirectory( const char *path ) {
	return ( !_mkdir( path ) );
}

/*
* Sys_FS_RemoveDirectory
*/
bool Sys_FS_RemoveDirectory( const char *path ) {
	return ( !_rmdir( path ) );
}

/*
* Sys_FS_FileNo
*/
int Sys_FS_FileNo( FILE *fp ) {
	return _fileno( fp );
}

struct ListDirHandleImpl {
	HANDLE handle;
	WIN32_FIND_DATAA * ffd;
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

ListDirHandle BeginListDir( const char * path ) {
	ListDirHandleImpl handle;
	handle.handle = NULL;
	handle.first = true;

	if( strlen( path ) > MAX_PATH - 3 )
		return ImplToOpaque( handle );

	handle.ffd = ( WIN32_FIND_DATAA * ) malloc( sizeof( *handle.ffd ) );
	if( handle.ffd == NULL )
		return ImplToOpaque( handle );

	String< MAX_PATH > path_and_wildcard( "{}/*", path );
	handle.handle = FindFirstFileA( path_and_wildcard.c_str(), handle.ffd );

	return ImplToOpaque( handle );
}

bool ListDirNext( ListDirHandle * opaque, const char ** path, bool * dir ) {
	ListDirHandleImpl handle = OpaqueToImpl( *opaque );
	if( handle.handle == NULL )
		return false;

	if( !handle.first ) {
		if( FindNextFileA( handle.handle, handle.ffd ) == 0 ) {
			FindClose( handle.handle );
			free( handle.ffd );
			return false;
		}
	}

	*path = handle.ffd->cFileName;
	*dir = ( handle.ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;

	handle.first = false;
	*opaque = ImplToOpaque( handle );

	return true;
}

s64 FileLastModifiedTime( const char * path ) {
	HANDLE handle = CreateFileA( path, 0, 0, NULL, OPEN_EXISTING, 0, NULL );
	if( handle == INVALID_HANDLE_VALUE ) {
		return 0;
	}

	defer { CloseHandle( handle ); };

	FILETIME modified;
	if( GetFileTime( handle, NULL, NULL, &modified ) == 0 ) {
		return 0;
	}

	ULARGE_INTEGER modified64;
	memcpy( &modified64, &modified, sizeof( modified ) );
	return modified64.QuadPart;
}
