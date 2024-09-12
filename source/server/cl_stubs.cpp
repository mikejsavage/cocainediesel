#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/time.h"

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
	if( !is_public_build && argc == 2 && ( StrEqual( argv[ 1 ], "--test" ) || StrEqual( argv[ 1 ], "--testdbg" ) ) ) {
		bool break_on_fail = StrEqual( argv[ 1 ], "--testdbg" );
		u32 passed = 0;
		u32 failed = 0;

		const UnitTest * test = UnitTest::tests_head;
		while( test != NULL ) {
			Time before = Now();
			bool ok = test->callback();
			Time dt = Now() - before;

			if( dt > Milliseconds( 1 ) ) {
				printf( "\033[1;33m%s took %.1fms (%s:%d)\033[0m\n", test->name, ToSeconds( dt ) * 1000.0f, test->src_loc.file, test->src_loc.line );
			}

			if( ok ) {
				passed++;
			}
			else {
				printf( "\033[1;31m%s failed (%s:%d)\033[0m\n", test->name, test->src_loc.file, test->src_loc.line );
				failed++;

				if( break_on_fail ) {
					Breakpoint();
					test->callback();
				}
			}

			test = test->next;
		}

		u32 total = passed + failed;
		printf( "%u/%u test%s passed\n", passed, total, total == 1 ? "" : "s" );

		exit( failed == 0 ? 0 : 1 );
	}

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
