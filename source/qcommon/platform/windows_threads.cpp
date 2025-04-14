#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/platform/windows_mini_windows_h.h"
#include <process.h>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/threads.h"

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

Opaque< Thread > NewThread( void ( *callback )( void * ), void * data ) {
	// can't use sys_allocator because serverlist leaks its threads
	ThreadStartData * tsd = new ThreadStartData;
	tsd->callback = callback;
	tsd->data = data;

	HANDLE handle = CreateThread( NULL, 0, ThreadWrapper, tsd, 0, NULL );
	if( handle == NULL ) {
		FatalGLE( "CreateThread" );
	}

	return Thread { handle };
}

void JoinThread( Opaque< Thread > thread ) {
	WaitForSingleObject( thread.unwrap()->handle, INFINITE );
	CloseHandle( thread.unwrap()->handle );
}

void InitMutex( Opaque< Mutex > * mutex ) {
	InitializeSRWLock( &mutex->unwrap()->lock );
}

void DeleteMutex( Opaque< Mutex > * mutex ) {
}

void Lock( Opaque< Mutex > * mutex ) {
	AcquireSRWLockExclusive( &mutex->unwrap()->lock );
}

void Unlock( Opaque< Mutex > * mutex ) {
	ReleaseSRWLockExclusive( &mutex->unwrap()->lock );
}

void InitSemaphore( Opaque< Semaphore > * sem ) {
	LONG max = 8192;
	HANDLE handle = CreateSemaphoreA( NULL, 0, max, NULL );
	if( handle == NULL ) {
		FatalGLE( "CreateSemaphoreA" );
	}

	sem->unwrap()->handle = handle;
}

void DeleteSemaphore( Opaque< Semaphore > * sem ) {
	CloseHandle( sem->unwrap()->handle );
}

void Signal( Opaque< Semaphore > * sem, int n ) {
	if( n == 0 )
		return;

	if( ReleaseSemaphore( sem->unwrap()->handle, n, NULL ) == 0 ) {
		DWORD error = GetLastError();
		if( error != ERROR_TOO_MANY_POSTS ) {
			FatalGLE( "ReleaseSemaphore" );
		}
	}
}

void Wait( Opaque< Semaphore > * sem ) {
	WaitForSingleObject( sem->unwrap()->handle, INFINITE );
}

u32 GetCoreCount() {
	SYSTEM_INFO info;
	GetSystemInfo( &info );
	return info.dwNumberOfProcessors;
}

#endif // #if PLATFORM_WINDOWS
