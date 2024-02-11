#include "qcommon/platform.h"

#if PLATFORM_UNIX

#include "qcommon/base.h"
#include "qcommon/application.h"
#include "qcommon/fs.h"
#include "qcommon/platform/fs.h"
#include "gameshared/q_shared.h"

// these must come after qcommon because both tracy and one of these defines BLOCK_SIZE
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

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

FILE * OpenFile( Allocator * a, const char * path, OpenFileMode mode ) {
	return fopen( path, OpenFileModeToString( mode ) );
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

	dirent * dirent;
	while( ( dirent = readdir( handle.dir ) ) != NULL ) {
		if( StrEqual( dirent->d_name, "." ) || StrEqual( dirent->d_name, ".." ) )
			continue;

		*path = dirent->d_name;
		*dir = dirent->d_type == DT_DIR;

		return true;
	}

	closedir( handle.dir );

	return false;
}

#endif // #if PLATFORM_UNIX
