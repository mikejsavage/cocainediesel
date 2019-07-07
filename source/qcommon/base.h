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
 * ints
 */

#define S8_MAX s8( INT8_MAX )
#define S16_MAX s16( INT16_MAX )
#define S32_MAX s32( INT32_MAX )
#define S64_MAX s64( INT64_MAX )
#define S8_MIN s8( INT8_MIN )
#define S16_MIN s16( INT16_MIN )
#define S32_MIN s32( INT32_MIN )
#define S64_MIN s64( INT64_MIN )

#define U8_MAX u8( UINT8_MAX )
#define U16_MAX u16( UINT16_MAX )
#define U32_MAX u32( UINT32_MAX )
#define U64_MAX u64( UINT64_MAX )

#define S8 INT8_C
#define S16 INT16_C
#define S32 INT32_C
#define S64 INT64_C
#define U8 UINT8_C
#define U16 UINT16_C
#define U32 UINT32_C
#define U64 UINT64_C

/*
 * allocators
 */

extern Allocator * sys_allocator;

#if COMPILER_MSVC
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

void * AllocManyHelper( Allocator * a, size_t n, size_t size, size_t alignment, const char * func, const char * file, int line );
void * ReallocManyHelper( Allocator * a, void * ptr, size_t current_n, size_t new_n, size_t size, size_t alignment, const char * func, const char * file, int line );

#define ALLOC( a, size, alignment ) ( a )->allocate( size, alignment, __PRETTY_FUNCTION__, __FILE__, __LINE__ )
#define REALLOC( a, ptr, current_size, new_size, alignment ) ( a )->reallocate( ptr, current_size, new_size, alignment, __PRETTY_FUNCTION__, __FILE__, __LINE__ )
#define FREE( a, p ) a->deallocate( p, __PRETTY_FUNCTION__, __FILE__, __LINE__ )

#define ALLOC_MANY( a, T, n ) ( ( T * ) AllocManyHelper( a, checked_cast< size_t >( n ), sizeof( T ), alignof( T ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ) )
#define REALLOC_MANY( a, T, ptr, current_n, new_n ) ( ( T * ) ReallocManyHelper( a, ptr, checked_cast< size_t >( current_n ), checked_cast< size_t >( new_n ), sizeof( T ), alignof( T ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ) )
#define ALLOC_SPAN( a, T, n ) Span< T >( ALLOC_MANY( a, T, n ), n )

/*
 * breaks
 */

extern bool break1;
extern bool break2;
extern bool break3;
extern bool break4;
