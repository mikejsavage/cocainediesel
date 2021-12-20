#pragma once

#include "qcommon/threads.h"

template< typename T >
struct Locked {
	Mutex * mutex;
	T data;
};

template< typename T, typename F >
void DoUnderLock( Locked< T > * locked, F f ) {
	Lock( locked->mutex );
	f( &locked->data );
	Unlock( locked->mutex );
}
