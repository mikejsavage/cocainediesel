#include <signal.h>
#include <unistd.h>

#include "qcommon/qcommon.h"
#include "qcommon/time.h"

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
	received_shutdown_signal = 0;

	QuitOnSignal( SIGINT );
	QuitOnSignal( SIGQUIT );
	QuitOnSignal( SIGTERM );

	Qcommon_Init( argc, argv );

	Time last_frame_time = Now();
	while( true ) {
		FrameMark;

		Time now = Now();
		if( !Qcommon_Frame( now - last_frame_time ) || received_shutdown_signal == 1 ) {
			break;
		}
		last_frame_time = now;
	}

	Qcommon_Shutdown();

	return 0;
}

