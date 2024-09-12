#include "qcommon/base.h"
#include "qcommon/qcommon.h"

void CL_Init() { }
void CL_Shutdown() { }
void CL_Frame( int realmsec, int gamemsec ) { }
void CL_Disconnect( const char * message ) { }
bool CL_DemoPlaying() { return false; }

void Con_Print( Span< const char > str ) { }

void Key_Init() { }
void Key_Shutdown() { }

void OSServerInit();
bool OSServerShouldQuit();

int main( int argc, char ** argv ) {
	OSServerInit();

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

		if( !Qcommon_Frame( dt ) || OSServerShouldQuit() ) {
			break;
		}
	}

	Qcommon_Shutdown();

	return 0;
}
