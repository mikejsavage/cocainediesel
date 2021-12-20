#include "windows/miniwindows.h"

#include "qcommon/qcommon.h"
#include "qcommon/time.h"

const bool is_dedicated_server = true;

void Sys_InitTime();

void ShowErrorAndAbort( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
	abort();
}

void Sys_Init() {
	SetConsoleOutputCP( CP_UTF8 );
	Sys_InitTime();
}

int main( int argc, char ** argv ) {
	int64_t oldtime, newtime, time;

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();

	Time last_frame_time = Now();
	while( true ) {
		FrameMark;

		Time now = Now();
		if( !Qcommon_Frame( now - last_frame_time ) ) {
			break;
		}
		last_frame_time = now;
	}

	Qcommon_Shutdown();

	return 0;
}
