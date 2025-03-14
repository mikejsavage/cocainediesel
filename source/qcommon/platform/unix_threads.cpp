#include "qcommon/platform.h"

#if PLATFORM_UNIX

#include "qcommon/base.h"

#include <pthread.h>
#include <unistd.h>

struct Thread { pthread_t thread; };
struct Mutex { pthread_mutex_t mutex; };

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

static void TryPthread( const char * msg, int err ) {
	if( err != 0 ) {
		Fatal( "%s: %s", msg, strerror( err ) );
	}
}

Opaque< Thread > NewThread( void ( *callback )( void * ), void * data ) {
	// can't use sys_allocator because serverlist leaks its threads
	ThreadStartData * tsd = new ThreadStartData;
	tsd->callback = callback;
	tsd->data = data;

	pthread_t t;
	TryPthread( "pthread_create", pthread_create( &t, NULL, ThreadWrapper, tsd ) );

	return Thread { t };
}

void JoinThread( Opaque< Thread > thread ) {
	TryPthread( "pthread_join", pthread_join( thread.unwrap()->thread, NULL ) );
}

void InitMutex( Opaque< Mutex > * mutex ) {
	TryPthread( "pthread_mutex_init", pthread_mutex_init( &mutex->unwrap()->mutex, NULL ) );
}

void DeleteMutex( Opaque< Mutex > * mutex ) {
	TryPthread( "pthread_mutex_destroy", pthread_mutex_destroy( &mutex->unwrap()->mutex );
}

void Lock( Opaque< Mutex > * mutex ) {
	TryPthread( "pthread_mutex_lock", pthread_mutex_lock( &mutex->unwrap()->mutex );
}

void Unlock( Opaque< Mutex > * mutex ) {
	TryPthread( "pthread_mutex_unlock", pthread_mutex_unlock( &mutex->unwrap()->mutex );
}

u32 GetCoreCount() {
	long ok = sysconf( _SC_NPROCESSORS_ONLN );
	if( ok == -1 ) {
		FatalErrno( "sysconf( _SC_NPROCESSORS_ONLN )" );
	}
	return checked_cast< u32 >( ok );
}

#endif // #if PLATFORM_UNIX
