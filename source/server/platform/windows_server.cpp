#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/platform/windows_mini_windows_h.h"

#include "qcommon/qcommon.h"

const bool is_dedicated_server = true;

void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

void OSServerInit() {
}

bool OSServerShouldQuit() {
	return false;
}

#endif // #if PLATFORM_WINDOWS
