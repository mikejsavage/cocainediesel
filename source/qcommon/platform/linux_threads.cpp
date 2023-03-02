#include "qcommon/platform.h"

#if PLATFORM_LINUX

#include "qcommon/base.h"

#include <errno.h>
#include <semaphore.h>

struct Semaphore { sem_t sem; };

Semaphore * NewSemaphore() {
	sem_t s;
	if( sem_init( &s, 0, 0 ) != 0 ) {
		FatalErrno( "sem_init" );
	}

	Semaphore * sem = ALLOC( sys_allocator, Semaphore );
	sem->sem = s;
	return sem;
}

void DeleteSemaphore( Semaphore * sem ) {
	if( sem_destroy( &sem->sem ) != 0 ) {
		FatalErrno( "sem_destroy" );
	}
	FREE( sys_allocator, sem );
}

void Signal( Semaphore * sem, int n ) {
	for( int i = 0; i < n; i++ ) {
		if( sem_post( &sem->sem ) != 0 && errno != EOVERFLOW ) {
			FatalErrno( "sem_post" );
		}
	}
}

void Wait( Semaphore * sem ) {
	while( true ) {
		if( sem_wait( &sem->sem ) == 0 )
			break;
		if( errno == EINTR )
			continue;
		FatalErrno( "sem_wait" );
	}
}

#endif // #ifdef PLATFORM_LINUX
