#include <signal.h>
#include <unistd.h>

#include "qcommon/qcommon.h"

const bool is_dedicated_server = true;

sig_atomic_t received_shutdown_signal;

static void ShutdownSignal( int sig ) {
	received_shutdown_signal = 1;
}

static void QuitOnSignal( int sig ) {
	struct sigaction sa = { };
	sa.sa_handler = ShutdownSignal;
	sigaction( sig, &sa, NULL );
}

int main( int argc, char ** argv ) {
	unsigned int oldtime, newtime, time;

	received_shutdown_signal = 0;
	QuitOnSignal( SIGINT );
	QuitOnSignal( SIGQUIT );
	QuitOnSignal( SIGTERM );

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();
	while( true ) {
		FrameMark;

		// find time spent rendering last frame
		do {
			ZoneScopedN( "Interframe" );

			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if( time > 0 ) {
				break;
			}
		} while( 1 );
		oldtime = newtime;

		if( !Qcommon_Frame( time ) || received_shutdown_signal == 1 ) {
			break;
		}
	}

	Qcommon_Shutdown();

	return 0;
}

