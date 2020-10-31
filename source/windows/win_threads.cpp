#include "windows/miniwindows.h"
#include <process.h>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"

struct Thread { HANDLE handle; };
struct Mutex { SRWLOCK lock; };
struct Semaphore { HANDLE handle; };

struct ThreadStartData {
	void ( *callback )( void * );
	void * data;
};

static DWORD WINAPI ThreadWrapper( void * data ) {
	ThreadStartData * tsd = ( ThreadStartData * ) data;
	tsd->callback( tsd->data );
	delete tsd;

	return 0;
}

Thread * NewThread( void ( *callback )( void * ), void * data ) {
	// can't use sys_allocator because serverlist leaks its threads
	ThreadStartData * tsd = new ThreadStartData;
	tsd->callback = callback;
	tsd->data = data;

	DWORD id;
	HANDLE handle = CreateThread( 0, 0, ThreadWrapper, tsd, 0, &id );
	if( handle == NULL ) {
		Com_Error( ERR_FATAL, "CreateThread" );
	}

	// can't use sys_allocator because serverlist leaks its threads
	Thread * thread = new Thread;
	thread->handle = handle;
	return thread;
}

void JoinThread( Thread * thread ) {
	WaitForSingleObject( thread->handle, INFINITE );
	CloseHandle( thread->handle );
	delete thread;
}

Mutex * NewMutex() {
	// can't use sys_allocator because sys_allocator itself calls NewMutex
	Mutex * mutex = new Mutex;
	InitializeSRWLock( &mutex->lock );
	return mutex;
}

void DeleteMutex( Mutex * mutex ) {
	delete mutex;
}

void Lock( Mutex * mutex ) {
	AcquireSRWLockExclusive( &mutex->lock );
}

void Unlock( Mutex * mutex ) {
	ReleaseSRWLockExclusive( &mutex->lock );
}

Semaphore * NewSemaphore() {
	LONG max = 8192;
	HANDLE handle = CreateSemaphoreA( NULL, 0, max, NULL );
	if( handle == NULL ) {
		Com_Error( ERR_FATAL, "CreateSemaphoreA" );
	}

	Semaphore * sem = ALLOC( sys_allocator, Semaphore );
	sem->handle = handle;
	return sem;
}

void DeleteSemaphore( Semaphore * sem ) {
	CloseHandle( sem->handle );
	FREE( sys_allocator, sem );
}

void Signal( Semaphore * sem, int n ) {
	if( ReleaseSemaphore( sem->handle, n, NULL ) == 0 ) {
		DWORD error = GetLastError();
		if( error != ERROR_TOO_MANY_POSTS ) {
			Com_Error( ERR_FATAL, "ReleaseSemaphore" );
		}
	}
}

void Wait( Semaphore * sem ) {
	WaitForSingleObject( sem->handle, INFINITE );
}

u32 GetCoreCount() {
	SYSTEM_INFO info;
	GetSystemInfo( &info );
	return info.dwNumberOfProcessors;
}
