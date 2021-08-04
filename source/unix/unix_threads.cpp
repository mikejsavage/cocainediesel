#include "qcommon/base.h"
#include "qcommon/qcommon.h"

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

Thread * NewThread( void ( *callback )( void * ), void * data ) {
	// can't use sys_allocator because serverlist leaks its threads
	ThreadStartData * tsd = new ThreadStartData;
	tsd->callback = callback;
	tsd->data = data;

	pthread_t t;
	if( pthread_create( &t, NULL, ThreadWrapper, tsd ) == -1 ) {
		Fatal( "pthread_create" );
	}

	// can't use sys_allocator because serverlist leaks its threads
	Thread * thread = new Thread;
	thread->thread = t;
	return thread;
}

void JoinThread( Thread * thread ) {
	if( pthread_join( thread->thread, NULL ) == -1 ) {
		Fatal( "pthread_join" );
	}

	delete thread;
}

Mutex * NewMutex() {
	// can't use sys_allocator because sys_allocator itself calls NewMutex
	Mutex * mutex = new Mutex;
	if( pthread_mutex_init( &mutex->mutex, NULL ) != 0 ) {
		Fatal( "pthread_mutex_init" );
	}
	return mutex;
}

void DeleteMutex( Mutex * mutex ) {
	if( pthread_mutex_destroy( &mutex->mutex ) != 0 ) {
		Fatal( "pthread_mutex_destroy" );
	}
	delete mutex;
}

void Lock( Mutex * mutex ) {
	if( pthread_mutex_lock( &mutex->mutex ) != 0 ) {
		Fatal( "pthread_mutex_lock" );
	}
}

void Unlock( Mutex * mutex ) {
	if( pthread_mutex_unlock( &mutex->mutex ) != 0 ) {
		Fatal( "pthread_mutex_unlock" );
	}
}

Semaphore * NewSemaphore() {
	sem_t s;
	if( sem_init( &s, 0, 0 ) != 0 ) {
		Fatal( "sem_init" );
	}

	Semaphore * sem = ALLOC( sys_allocator, Semaphore );
	sem->sem = s;
	return sem;
}

void DeleteSemaphore( Semaphore * sem ) {
	if( sem_destroy( &sem->sem ) != 0 ) {
		Fatal( "sem_destroy" );
	}
	FREE( sys_allocator, sem );
}

void Signal( Semaphore * sem, int n ) {
	for( int i = 0; i < n; i++ ) {
		if( sem_post( &sem->sem ) != 0 && errno != EOVERFLOW ) {
			Fatal( "sem_post" );
		}
	}
}

void Wait( Semaphore * sem ) {
	while( true ) {
		if( sem_wait( &sem->sem ) == 0 )
			break;
		if( errno == EINTR )
			continue;
		Fatal( "sem_wait" );
	}
}

u32 GetCoreCount() {
	long ok = sysconf( _SC_NPROCESSORS_ONLN );
	if( ok == -1 ) {
		Fatal( "sysconf( _SC_NPROCESSORS_ONLN )" );
	}
	return checked_cast< u32 >( ok );
}
