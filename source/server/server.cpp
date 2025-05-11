#include "qcommon/qcommon.h"
#include "qcommon/base.h"
#include "qcommon/threads.h"
#include "qcommon/time.h"

#include <signal.h>
#include <atomic>

void CL_Init() { }
void CL_Shutdown() { }
void CL_Frame( int realmsec, int gamemsec ) { }
void CL_Disconnect( const char * message ) { }
bool CL_DemoPlaying() { return false; }

void Con_Print( Span< const char > str ) { }

void InitKeys() { }
void ShutdownKeys() { }

void OSServerInit();
bool OSServerShouldQuit();

static bool RunUnitTests( bool break_on_fail ) {
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

	return failed == 0;
}

#if PUBLIC_BUILD && PLATFORM_LINUX
static Opaque< Thread > hang_detector_thread;
static Opaque< Semaphore > hang_detector_shutdown_semaphore;
static std::atomic< s64 > hang_time;

bool Wait( Opaque< Semaphore > * sem, int timeout_seconds );

static void HangDetector( void * ) {
	while( true ) {
		s64 now = Sys_Milliseconds();
		if( now - hang_time.load() > 10 * 1000 ) {
			printf( "The game has hung for 10 seconds, quitting...\n" );
			raise( SIGQUIT ); // SIGQUIT generates a core dump too
			return;
		}

		if( Wait( &hang_detector_shutdown_semaphore, 1 ) ) {
			break;
		}
	}
}
#endif

int main( int argc, char ** argv ) {
	if( !is_public_build && argc == 2 && ( StrEqual( argv[ 1 ], "--test" ) || StrEqual( argv[ 1 ], "--testdbg" ) ) ) {
		return RunUnitTests( StrEqual( argv[ 1 ], "--testdbg" ) ) ? 0 : 1;
	}

	OSServerInit();

	Qcommon_Init( argc, argv );

#if PUBLIC_BUILD && PLATFORM_LINUX
	InitSemaphore( &hang_detector_shutdown_semaphore );
	hang_time.store( Sys_Milliseconds() );
	hang_detector_thread = NewThread( HangDetector, NULL );
#endif

	s64 oldtime = Sys_Milliseconds();
	while( true ) {
		TracyCFrameMark;

		s64 dt = 0;
		{
			TracyZoneScopedNC( "Interframe", 0xff0000 );
			while( dt == 0 ) {
				dt = Sys_Milliseconds() - oldtime;
			}
			oldtime += dt;
		}

		if( !Qcommon_Frame( dt ) || OSServerShouldQuit() ) {
			break;
		}

#if PUBLIC_BUILD && PLATFORM_LINUX
		hang_time.store( oldtime );
#endif
	}

#if PUBLIC_BUILD && PLATFORM_LINUX
	Signal( &hang_detector_shutdown_semaphore, 1 );
	JoinThread( hang_detector_thread );
	DeleteSemaphore( &hang_detector_shutdown_semaphore );
#endif

	Qcommon_Shutdown();

	return 0;
}
