#include <signal.h>
#include <unistd.h>

#include "qcommon/qcommon.h"

const bool is_dedicated_server = true;

sig_atomic_t received_shutdown_signal = 0;

static void ShutdownSignal( int sig ) {
	received_shutdown_signal = 1;
}

static void QuitOnSignal( int sig ) {
	struct sigaction sa = { };
	sa.sa_handler = ShutdownSignal;
	sigaction( sig, &sa, NULL );
}

int main( int argc, char ** argv ) {
	QuitOnSignal( SIGINT );
	QuitOnSignal( SIGQUIT );
	QuitOnSignal( SIGTERM );

	Qcommon_Init( argc, argv );

	s64 oldtime = Sys_Milliseconds();
	while( true ) {
		TracyCFrameMark;

		s64 dt = 0;
		{
			TracyZoneScopedN( "Interframe" );
			while( dt == 0 ) {
				dt = Sys_Milliseconds() - oldtime;
			}
			oldtime += dt;
		}

		if( !Qcommon_Frame( dt ) || received_shutdown_signal == 1 ) {
			break;
		}
	}

	Qcommon_Shutdown();

	return 0;
}

