#pragma once

#include "qcommon/types.h"

typedef void ( *JobCallback )( TempAllocator * temp, void * data );

void InitThreadPool();
void ShutdownThreadPool();

void ThreadPoolDo( JobCallback callback, void * data = NULL );
void ParallelFor( JobCallback callback, void * datum, size_t n, size_t stride );
void ThreadPoolFinish();

template< typename T >
void ParallelFor( JobCallback callback, Span< T > datum ) {
	ParallelFor( callback, datum.ptr, datum.n, sizeof( T ) );
}
