#pragma once

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/types.h"
#include "qcommon/platform.h"
#include "qcommon/ggformat.h"
#include "qcommon/linear_algebra.h"

/*
 * allocators
 */

extern Allocator * sys_allocator;

#if COMPILER_MSVC
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

void * AllocManyHelper( Allocator * a, size_t n, size_t size, size_t alignment, const char * func, const char * file, int line );

#define ALLOC( a, size, alignment ) ( a )->allocate( size, alignment, __PRETTY_FUNCTION__, __FILE__, __LINE__ )
#define REALLOC( a, ptr, current_size, new_size, alignment ) ( a )->reallocate( ptr, current_size, new_size, alignment, __PRETTY_FUNCTION__, __FILE__, __LINE__ )
#define FREE( a, p ) a->deallocate( p, __PRETTY_FUNCTION__, __FILE__, __LINE__ )

#define ALLOC_MANY( T, a, n ) ( ( T * ) AllocManyHelper( a, checked_cast< size_t >( n ), sizeof( T ), alignof( T ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ) )
