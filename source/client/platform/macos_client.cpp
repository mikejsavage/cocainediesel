#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"

#include <spawn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>

// TODO: do something like https://github.com/aaronmjacobs/Boxer/blob/master/src/boxer_mac.mm
void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

extern char ** environ;

bool Sys_OpenInWebBrowser( const char * url ) {
	char open[] = "open";
	char * argv[] = { open, const_cast< char * >( url ), NULL };
	return posix_spawnp( NULL, open, NULL, NULL, argv, environ ) == 0;
}

// https://developer.apple.com/library/archive/qa/qa1361/_index.html
bool Sys_BeingDebugged() {
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
	struct kinfo_proc info = { };
	size_t size = sizeof( info );
	int ok = sysctl( mib, ARRAY_COUNT( mib ), &info, &size, NULL, 0 );
	if( ok != 0 ) {
		Fatal( "Sys_BeingDebugged" );
	}

	return ( info.kp_proc.p_flag & P_TRACED ) != 0;
}

bool IsRenderDocAttached() {
	// RenderDoc doesn't run on macOS
	return false;
}

#endif // #ifdef PLATFORM_MACOS
