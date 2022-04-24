#include "windows/miniwindows.h"

#include "qcommon/qcommon.h"

const bool is_dedicated_server = true;

void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

int main( int argc, char ** argv ) {
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

		if( !Qcommon_Frame( dt ) ) {
			break;
		}
	}

	Qcommon_Shutdown();

	return 0;
}
