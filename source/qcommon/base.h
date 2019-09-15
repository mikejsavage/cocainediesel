#pragma once

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/platform.h"
#include "qcommon/types.h"
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
 * helpers
 */

template< typename T, size_t N >
constexpr size_t ARRAY_COUNT( const T ( &arr )[ N ] ) {
	return N;
}

#define STATIC_ASSERT( p ) static_assert( p, #p )
#define NONCOPYABLE( T ) T( const T & ) = delete; void operator=( const T & ) = delete

#define CONCAT_HELPER( a, b ) a##b
#define CONCAT( a, b ) CONCAT_HELPER( a, b )
#define COUNTER_NAME( x ) CONCAT( x, __COUNTER__ )
#define LINE_NAME( x ) CONCAT( x, __LINE__ )

template< typename To, typename From >
inline To checked_cast( const From & from ) {
	To result = To( from );
	assert( From( result ) == from );
	return result;
}

template< typename To, typename From >
inline To bit_cast( const From & from ) {
	STATIC_ASSERT( sizeof( To ) == sizeof( From ) );
	To result;
	memcpy( &result, &from, sizeof( result ) );
	return result;
}

template< typename T >
void Swap2( T * a, T * b ) {
	T t = *a;
	*a = *b;
	*b = t;
}

template< typename T >
constexpr T AlignPow2( T x, T alignment ) {
	return ( x + alignment - 1 ) & ~( alignment - 1 );
}

template< typename T >
constexpr bool IsPowerOf2( T x ) {
	return ( x & ( x - 1 ) ) == 0;
}

template< typename T >
constexpr T Min2( const T & a, const T & b ) {
	return a < b ? a : b;
}

template< typename T >
constexpr T Max2( const T & a, const T & b ) {
	return a > b ? a : b;
}

template< typename T >
T Clamp( const T & lo, const T & x, const T & hi ) {
	assert( lo <= hi );
	return Max2( lo, Min2( x, hi ) );
}

template< typename T >
T Clamp01( const T & x ) {
	return Max2( T( 0 ), Min2( x, T( 1 ) ) );
}

/*
 * defer
 */

template< typename F >
struct ScopeExit {
	ScopeExit( F f_ ) : f( f_ ) { }
	~ScopeExit() { f(); }
	F f;
};

struct DeferHelper {
	template< typename F >
		ScopeExit< F > operator+( F f ) { return f; }
};

#define defer const auto & COUNTER_NAME( DEFER_ ) = DeferHelper() + [&]()

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

template< typename... Rest >
const char * TempAllocator::operator()( const char * fmt, const Rest & ... rest ) {
	size_t len = ggformat( NULL, 0, fmt, rest... );
	char * buf = ALLOC_MANY( this, char, len + 1 );
	ggformat( buf, len + 1, fmt, rest... );
	return buf;
}

/*
 * breaks
 */

extern bool break1;
extern bool break2;
extern bool break3;
extern bool break4;

/*
 * colors
 */

constexpr RGBA8 rgba8_white = RGBA8( 255, 255, 255, 255 );
constexpr RGBA8 rgba8_black = RGBA8( 0, 0, 0, 255 );
