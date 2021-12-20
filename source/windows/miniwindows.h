#pragma once

#include "qcommon/base.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

inline void FatalGLE( const char * msg ) {
	int err = GetLastError();

	char buf[ 1024 ];
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), buf, sizeof( buf ), NULL );

	Fatal( "%s: %s (%d)", msg, buf, err );
}
