#pragma once

#include "qcommon/types.h"

typedef void ( *JobCallback )( TempAllocator * temp, void * data );

void InitThreadPool();
void ShutdownThreadPool();

void ThreadPoolDo( JobCallback callback, void * data = NULL );
void ParallelFor( void * datum, size_t n, size_t stride, JobCallback callback );
void ThreadPoolFinish();

template< typename T >
void ParallelFor( Span< T > datum, JobCallback callback ) {
	ParallelFor( datum.ptr, datum.n, sizeof( T ), callback );
}
