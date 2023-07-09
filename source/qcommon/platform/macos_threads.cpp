#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"

#include <dispatch/dispatch.h>

struct Semaphore { dispatch_semaphore_t sem; };

Semaphore * NewSemaphore() {
	dispatch_semaphore_t s = dispatch_semaphore_create( 0 );
	if( s == NULL ) {
		Fatal( "dispatch_semaphore_create" );
	}

	Semaphore * sem = Alloc< Semaphore >( sys_allocator );
	sem->sem = s;
	return sem;
}

void DeleteSemaphore( Semaphore * sem ) {
	dispatch_release( sem->sem );
	Free( sys_allocator, sem );
}

void Signal( Semaphore * sem, int n ) {
	for( int i = 0; i < n; i++ ) {
		dispatch_semaphore_signal( sem->sem );
	}
}

void Wait( Semaphore * sem ) {
	dispatch_semaphore_wait( sem->sem, DISPATCH_TIME_FOREVER );
}

#endif // #if PLATFORM_MACOS
