#include "qcommon/base.h"
#include "windows/miniwindows.h"

void FatalGLE( const char * msg ) {
	int err = GetLastError();

	char buf[ 1024 ];
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), buf, sizeof( buf ), NULL );

	Fatal( "%s: %s (%d)", msg, buf, err );
}

void EnableFPE() { }
void DisableFPE() { }

void Sys_Init() {
	SetConsoleOutputCP( CP_UTF8 );
}
