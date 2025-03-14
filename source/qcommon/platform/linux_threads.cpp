#include "qcommon/platform.h"

#if PLATFORM_LINUX

#include "qcommon/base.h"
#include "qcommon/threads.h"

#include <errno.h>
#include <semaphore.h>

struct Semaphore { sem_t sem; };

void InitSemaphore( Opaque< Semaphore > * sem ) {
	if( sem_init( &sem->unwrap()->sem, 0, 0 ) != 0 ) {
		FatalErrno( "sem_init" );
	}
}

void DeleteSemaphore( Opaque< Semaphore > * sem ) {
	if( sem_destroy( &sem->unwrap()->sem ) != 0 ) {
		FatalErrno( "sem_destroy" );
	}
}

void Signal( Opaque< Semaphore > * sem, int n ) {
	for( int i = 0; i < n; i++ ) {
		if( sem_post( &sem->unwrap()->sem ) != 0 && errno != EOVERFLOW ) {
			FatalErrno( "sem_post" );
		}
	}
}

void Wait( Opaque< Semaphore > * sem ) {
	while( true ) {
		if( sem_wait( &sem->unwrap()->sem ) == 0 )
			break;
		if( errno == EINTR )
			continue;
		FatalErrno( "sem_wait" );
	}
}

#endif // #if PLATFORM_LINUX
