#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"

#include <errno.h>
#include <mach-o/dyld.h>

char * GetExePath( Allocator * a ) {
	NonRAIIDynamicArray< char > buf( a );
	buf.resize( 1024 );

	uint32_t len = buf.size();
	if( _NSGetExecutablePath( buf.ptr(), &len ) == -1 ) {
		buf.resize( len );
		if( _NSGetExecutablePath( buf.ptr(), &len ) == -1 ) {
			Fatal( "_NSGetExecutablePath" );
		}
	}

	return buf.ptr();
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

// TODO: FSChangeMonitor

#endif // #ifdef PLATFORM_MACOS
