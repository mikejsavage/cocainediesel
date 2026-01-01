#include "qcommon/platform.h"

#if PLATFORM_UNIX

#include "qcommon/base.h"
#include "qcommon/application.h"
#include "qcommon/fs.h"
#include "qcommon/platform/fs.h"
#include "gameshared/q_shared.h"

#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

Span< char > FindHomeDirectory( Allocator * a ) {
	const char * xdg_data_home = getenv( "XDG_DATA_HOME" );
	if( xdg_data_home != NULL ) {
		return a->sv( "{}/{}", xdg_data_home, APPLICATION );
	}

	const char * home = getenv( "HOME" );
	if( home == NULL ) {
		Fatal( "Can't find home directory" );
	}

	return a->sv( "{}/.local/share/{}", home, APPLICATION );
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

struct ListDirHandle {
	DIR * dir;
};

Opaque< ListDirHandle > BeginListDir( Allocator * a, const char * path ) {
	return ListDirHandle { opendir( path ) };
}

bool ListDirNext( Opaque< ListDirHandle > * opaque, const char ** path, bool * dir ) {
	ListDirHandle * handle = opaque->unwrap();
	if( handle->dir == NULL )
		return false;

	dirent * dirent;
	while( ( dirent = readdir( handle->dir ) ) != NULL ) {
		if( StrEqual( dirent->d_name, "." ) || StrEqual( dirent->d_name, ".." ) )
			continue;

		*path = dirent->d_name;
		*dir = dirent->d_type == DT_DIR;

		return true;
	}

	closedir( handle->dir );

	return false;
}

#endif // #if PLATFORM_UNIX
