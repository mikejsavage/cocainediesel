#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "qcommon/threads.h"

#include <dispatch/dispatch.h>

struct Semaphore { dispatch_semaphore_t sem; };

void InitSemaphore( Opaque< Semaphore > * sem ) {
	dispatch_semaphore_t s = dispatch_semaphore_create( 0 );
	if( s == NULL ) {
		Fatal( "dispatch_semaphore_create" );
	}
	sem->unwrap()->sem = s;
}

void DeleteSemaphore( Opaque< Semaphore > * sem ) {
	dispatch_release( sem->unwrap()->sem );
}

void Signal( Opaque< Semaphore > * sem, int n ) {
	for( int i = 0; i < n; i++ ) {
		dispatch_semaphore_signal( sem->unwrap()->sem );
	}
}

void Wait( Opaque< Semaphore > * sem ) {
	dispatch_semaphore_wait( sem->unwrap()->sem, DISPATCH_TIME_FOREVER );
}

#endif // #if PLATFORM_MACOS
