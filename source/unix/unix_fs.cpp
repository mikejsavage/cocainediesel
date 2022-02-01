#include "qcommon/base.h"
#include "qcommon/application.h"
#include "qcommon/fs.h"
#include "qcommon/sys_fs.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"

// these must come after qcommon because both tracy and one of these defines BLOCK_SIZE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/fs.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/syscall.h>

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

char * GetExePath( Allocator * a ) {
	NonRAIIDynamicArray< char > buf( a );
	buf.resize( 1024 );

	while( true ) {
		ssize_t n = readlink( "/proc/self/exe", buf.ptr(), buf.size() );
		if( n == -1 ) {
			FatalErrno( "readlink" );
		}

		if( size_t( n ) < buf.size() ) {
			buf[ n ] = '\0';
			break;
		}

		buf.resize( buf.size() * 2 );
	}

	return buf.ptr();
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

constexpr size_t MAX_INOTIFY_WATCHES = 4096;
struct FSChangeMonitor {
	int fd;
	char * wd_paths[ MAX_INOTIFY_WATCHES ];
	size_t num_wd_paths;
	Hashtable< MAX_INOTIFY_WATCHES * 2 > wd_to_path;
};

static void AddInotifyWatchesRecursive( Allocator * a, FSChangeMonitor * monitor, DynamicString * path, size_t skip ) {
	u32 filter = IN_CREATE | IN_MODIFY | IN_MOVED_TO;
	int wd = inotify_add_watch( monitor->fd, path->c_str(), filter );
	if( wd == -1 ) {
		FatalErrno( "inotify_add_watch" );
	}

	/*
	 * we want wd_path to "" when path is "base". we also don't do {}/{}
	 * when building the full path so we don't accidentally add a leading
	 * slash to the "base" case, so add the trailing slash here for
	 * non-base paths
	 */
	if( path->length() == skip ) {
		monitor->wd_paths[ monitor->num_wd_paths ] = CopyString( a, "" );
	}
	else {
		monitor->wd_paths[ monitor->num_wd_paths ] = ( *a )( "{}/", path->c_str() + skip + 1 );
	}
	monitor->wd_to_path.add( Hash64( wd ), monitor->num_wd_paths );
	monitor->num_wd_paths++;

	ListDirHandle scan = BeginListDir( a, path->c_str() );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' || !dir )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		AddInotifyWatchesRecursive( a, monitor, path, skip );
		path->truncate( old_len );
	}
}

FSChangeMonitor * NewFSChangeMonitor( Allocator * a, const char * path ) {
	FSChangeMonitor * monitor = ALLOC( a, FSChangeMonitor );
	monitor->fd = inotify_init();
	if( monitor->fd == -1 ) {
		FatalErrno( "inotify_init" );
	}

	monitor->num_wd_paths = 0;
	monitor->wd_to_path.clear();

	DynamicString base( a, "{}", path );
	AddInotifyWatchesRecursive( a, monitor, &base, base.length() );

	return monitor;
}

void DeleteFSChangeMonitor( Allocator * a, FSChangeMonitor * monitor ) {
	for( size_t i = 0; i < monitor->num_wd_paths; i++ ) {
		FREE( a, monitor->wd_paths[ i ] );
	}
	close( monitor->fd );
	FREE( a, monitor );
}

Span< const char * > PollFSChangeMonitor( TempAllocator * temp, FSChangeMonitor * monitor, const char ** results, size_t n ) {
	{
		pollfd fd = { };
		fd.fd = monitor->fd;
		fd.events = POLLIN;

		int res = poll( &fd, 1, 0 );
		if( res == -1 ) {
			FatalErrno( "poll" );
		}
		if( res == 0 ) {
			return Span< const char * >();
		}
	}

	alignas( inotify_event ) char buf[ 16384 ];
	ssize_t bytes_read = read( monitor->fd, buf, sizeof( buf ) );
	if( bytes_read == -1 ) {
		if( errno == EINTR ) {
			return Span< const char * >();
		}
		FatalErrno( "read" );
	}

	size_t num_results = 0;
	const inotify_event * cursor = ( const inotify_event * ) buf;
	while( ( const char * ) cursor < buf + bytes_read ) {
		if( ( cursor->mask & IN_ISDIR ) == 0 ) {
			u64 idx;
			bool ok = monitor->wd_to_path.get( Hash64( cursor->wd ), &idx );
			assert( ok );

			results[ num_results ] = ( *temp )( "{}{}", monitor->wd_paths[ idx ], cursor->name );
			num_results++;
		}

		cursor = ( const inotify_event * ) ( ( ( const char * ) cursor ) + sizeof( *cursor ) + cursor->len );

	}

	return Span< const char * >( results, num_results );
}
