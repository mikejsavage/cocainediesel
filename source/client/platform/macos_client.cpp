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

bool OpenInWebBrowser( const char * url ) {
	char open[] = "open";
	char * argv[] = { open, const_cast< char * >( url ), NULL };
	return posix_spawnp( NULL, open, NULL, NULL, argv, environ ) == 0;
}

bool IsRenderDocAttached() {
	// RenderDoc doesn't run on macOS
	return false;
}

#endif // #if PLATFORM_MACOS
