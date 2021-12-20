#include "qcommon/base.h"

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>

struct Thread { pthread_t thread; };
struct Mutex { pthread_mutex_t mutex; };
struct Semaphore { sem_t sem; };

struct ThreadStartData {
	void ( *callback )( void * );
	void * data;
};

static void * ThreadWrapper( void * data ) {
	ThreadStartData * tsd = ( ThreadStartData * ) data;
	tsd->callback( tsd->data );
	delete tsd;

	return NULL;
}

static void FatalPthread( const char * msg, int err ) {
	Fatal( "%s: %s", msg, strerror( err ) );
}

Thread * NewThread( void ( *callback )( void * ), void * data ) {
	// can't use sys_allocator because serverlist leaks its threads
	ThreadStartData * tsd = new ThreadStartData;
	tsd->callback = callback;
	tsd->data = data;

	pthread_t t;
	int err = pthread_create( &t, NULL, ThreadWrapper, tsd );
	if( err != 0 ) {
		FatalPthread( "pthread_create", err );
	}

	// can't use sys_allocator because serverlist leaks its threads
	Thread * thread = new Thread;
	thread->thread = t;
	return thread;
}

void JoinThread( Thread * thread ) {
	int err = pthread_join( thread->thread, NULL );
	if( err != 0 ) {
		FatalPthread( "pthread_join", err );
	}

	delete thread;
}

Mutex * NewMutex() {
	// can't use sys_allocator because sys_allocator itself calls NewMutex
	Mutex * mutex = new Mutex;
	int err = pthread_mutex_init( &mutex->mutex, NULL );
	if( err != 0 ) {
		FatalPthread( "pthread_mutex_init", err );
	}
	return mutex;
}

void DeleteMutex( Mutex * mutex ) {
	int err = pthread_mutex_destroy( &mutex->mutex );
	if( err != 0 ) {
		FatalPthread( "pthread_mutex_destroy", err );
	}
	delete mutex;
}

void Lock( Mutex * mutex ) {
	int err = pthread_mutex_lock( &mutex->mutex );
	if( err != 0 ) {
		FatalPthread( "pthread_mutex_lock", err );
	}
}

void Unlock( Mutex * mutex ) {
	int err = pthread_mutex_unlock( &mutex->mutex );
	if( err != 0 ) {
		FatalPthread( "pthread_mutex_unlock", err );
	}
}

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

u32 GetCoreCount() {
	long ok = sysconf( _SC_NPROCESSORS_ONLN );
	if( ok == -1 ) {
		FatalErrno( "sysconf( _SC_NPROCESSORS_ONLN )" );
	}
	return checked_cast< u32 >( ok );
}
