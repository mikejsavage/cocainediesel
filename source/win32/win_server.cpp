#include <windows.h>

#include "qcommon/qcommon.h"

const bool is_dedicated_server = true;

void Sys_InitTime();

void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Sys_ConsoleOutput( msg );

	exit( 1 );
}

void Sys_Quit() {
	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();

	FreeConsole();

	Qcommon_Shutdown();

	exit( 0 );
}

void Sys_Sleep( unsigned int millis ) {
	Sleep( millis );
}

void Sys_Init() {
	SetConsoleOutputCP( CP_UTF8 );
	Sys_InitTime();
}

int main( int argc, char ** argv ) {
	int64_t oldtime, newtime, time;

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();

	while( 1 ) {
		Sleep( 1 );

		do {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if( time > 0 ) {
				break;
			}
			Sys_Sleep( 0 );
		} while( 1 );
		oldtime = newtime;

		Qcommon_Frame( time );
	}

	return TRUE;
}
