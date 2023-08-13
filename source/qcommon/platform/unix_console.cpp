#include "qcommon/platform.h"

#if PLATFORM_UNIX

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "qcommon/qcommon.h"

static bool stdin_active = true;

const char * Sys_ConsoleInput() {
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if( !is_dedicated_server ) {
		return NULL;
	}

	if( !stdin_active ) {
		return NULL;
	}

	FD_ZERO( &fdset );
	FD_SET( 0, &fdset ); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if( select( 1, &fdset, NULL, NULL, &timeout ) == -1 || !FD_ISSET( 0, &fdset ) ) {
		return NULL;
	}

	len = read( 0, text, sizeof( text ) );
	if( len == 0 ) { // eof!
		Com_Printf( "EOF from stdin, console input disabled...\n" );
		stdin_active = false;
		return NULL;
	}

	if( len < 1 ) {
		return NULL;
	}

	text[len - 1] = 0; // rip off the /n and terminate

	return text;
}

#endif // #if PLATFORM_UNIX
