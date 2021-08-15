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
#include "qcommon/fs.h"
#include "qcommon/sys_fs.h"

// these must come after qcommon because both tracy and one of these defines BLOCK_SIZE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/syscall.h>

/*
* Sys_FS_CreateDirectory
*/
bool Sys_FS_CreateDirectory( const char *path ) {
	return ( !mkdir( path, 0777 ) );
}

char * FindHomeDirectory( Allocator * a ) {
	const char * xdg_data_home = getenv( "XDG_DATA_HOME" );
	if( xdg_data_home != NULL ) {
		return ( *a )( "{}/{}", xdg_data_home, APPLICATION );
	}

	const char * home = getenv( "HOME" );
	if( home == NULL ) {
		Fatal( "Can't find home directory" );
	}

	return ( *a )( "{}/.local/share/{}", home, APPLICATION );
}

FILE * OpenFile( Allocator * a, const char * path, const char * mode ) {
	return fopen( path, mode );
}

bool MoveFile( Allocator * a, const char * old_path, const char * new_path, MoveFileReplace replace ) {
	unsigned int flags = replace == MoveFile_DontReplace ? RENAME_NOREPLACE : 0;

	// the glibc on appveyor doesn't have renameat2 so call it directly
	if( syscall( SYS_renameat2, AT_FDCWD, old_path, AT_FDCWD, new_path, flags ) == 0 ) {
		return true;
	}

	if( errno == ENOSYS || errno == EINVAL || errno == EFAULT ) {
		FatalErrno( "rename" );
	}

	return false;
}

bool RemoveFile( Allocator * a, const char * path ) {
	return unlink( path ) == 0;
}

bool CreateDirectory( Allocator * a, const char * path ) {
	return mkdir( path, 0755 ) == 0 || errno == EEXIST;
}

struct ListDirHandleImpl {
	DIR * dir;
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
	ListDirHandleImpl handle;
	handle.dir = opendir( path );
	return ImplToOpaque( handle );
}

bool ListDirNext( ListDirHandle * opaque, const char ** path, bool * dir ) {
	ListDirHandleImpl handle = OpaqueToImpl( *opaque );
	if( handle.dir == NULL )
		return false;

	dirent64 * dirent;
	while( ( dirent = readdir64( handle.dir ) ) != NULL ) {
		if( strcmp( dirent->d_name, "." ) == 0 || strcmp( dirent->d_name, ".." ) == 0 )
			continue;

		*path = dirent->d_name;
		*dir = dirent->d_type == DT_DIR;

		return true;
	}

	closedir( handle.dir );

	return false;
}

FileMetadata FileMetadataOrZeroes( TempAllocator * temp, const char * path ) {
	struct stat buf;
	if( stat( path, &buf ) == -1 ) {
		return { };
	}

	FileMetadata metadata;
	metadata.size = checked_cast< u64 >( buf.st_size );
	metadata.modified_time = checked_cast< s64 >( buf.st_mtim.tv_sec ) * 1000 + checked_cast< s64 >( buf.st_mtim.tv_nsec ) / 1000000;

	return metadata;
}

char * ExecutablePath( Allocator * a ) {
	size_t buf_size = 1024;
	char * buf = ALLOC_MANY( a, char, buf_size );

	while( true ) {
		ssize_t n = readlink( "/proc/self/exe", buf, buf_size );
		if( n == -1 ) {
			FatalErrno( "readlink" );
		}

		if( size_t( n ) < buf_size ) {
			buf[ n ] = '\0';
			break;
		}

		buf = REALLOC_MANY( a, char, buf, buf_size, buf_size * 2 );
		buf_size *= 2;
	}

	return buf;
}
