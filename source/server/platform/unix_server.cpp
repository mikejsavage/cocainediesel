#include "qcommon/platform.h"

#if PLATFORM_UNIX

#include <signal.h>
#include <unistd.h>

#include "qcommon/qcommon.h"

const bool is_dedicated_server = true;

void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

static sig_atomic_t received_shutdown_signal;

static void ShutdownSignal( int sig ) {
	received_shutdown_signal = 1;
}

static void QuitOnSignal( int sig ) {
	struct sigaction sa = { };
	sa.sa_handler = ShutdownSignal;
	sigaction( sig, &sa, NULL );
}

void OSServerInit() {
	received_shutdown_signal = 0;

	QuitOnSignal( SIGINT );
	QuitOnSignal( SIGTERM );
}

bool OSServerShouldQuit() {
	return received_shutdown_signal == 1;
}

#endif // #if PLATFORM_UNIX
