#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"

#include <errno.h>
#include <mach-o/dyld.h>

Span< char > GetExePath( Allocator * a ) {
	NonRAIIDynamicArray< char > buf( a );
	buf.resize( 1024 );

	uint32_t len = buf.size();
	if( _NSGetExecutablePath( buf.ptr(), &len ) == -1 ) {
		buf.resize( len );
		if( _NSGetExecutablePath( buf.ptr(), &len ) == -1 ) {
			Fatal( "_NSGetExecutablePath" );
		}
	}

	buf.resize( strlen( buf.ptr() ) );

	return buf.span();
}

bool MoveFile( Allocator * a, const char * old_path, const char * new_path, MoveFileReplace replace ) {
	unsigned int flags = replace == MoveFile_DontReplace ? RENAME_EXCL : 0;

	if( renamex_np( old_path, new_path, flags ) == 0 ) {
		return true;
	}

	if( errno == EINVAL || errno == EFAULT ) {
		FatalErrno( "renamex_np" );
	}

	return false;
}

FSChangeMonitor * NewFSChangeMonitor( Allocator * a, const char * path ) {
	return NULL;
}

void DeleteFSChangeMonitor( Allocator * a, FSChangeMonitor * monitor ) {
}

Span< const char * > PollFSChangeMonitor( TempAllocator * temp, FSChangeMonitor * monitor, const char ** results, size_t n ) {
	// Fatal( "not implemented" );
	return { };
}

#endif // #if PLATFORM_MACOS
