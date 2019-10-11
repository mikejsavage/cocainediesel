#pragma once

#include <assert.h>
#include <errno.h>
#define _USE_MATH_DEFINES
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

#define CONCAT_HELPER( a, b ) a##b
#define CONCAT( a, b ) CONCAT_HELPER( a, b )
#define COUNTER_NAME( x ) CONCAT( x, __COUNTER__ )
#define LINE_NAME( x ) CONCAT( x, __LINE__ )

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
constexpr T Max3( const T & a, const T & b, const T & c ) {
	return Max2( Max2( a, b ), c );
}

template< typename T >
T Clamp( const T & lo, const T & x, const T & hi ) {
	assert( lo <= hi );
	return Max2( lo, Min2( x, hi ) );
}

inline float Clamp01( float x ) {
	return Clamp( 0.0f, x, 1.0f );
}

inline Vec4 Clamp01( Vec4 v ) {
	return Vec4( Clamp01( v.x ), Clamp01( v.y ), Clamp01( v.z ), Clamp01( v.w ) );
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

template< typename... Rest >
const char * TempAllocator::operator()( const char * fmt, const Rest & ... rest ) {
	size_t len = ggformat( NULL, 0, fmt, rest... );
	char * buf = ALLOC_MANY( this, char, len + 1 );
	ggformat( buf, len + 1, fmt, rest... );
	return buf;
}

/*
 * Span< const char >
 */

void format( FormatBuffer * fb, Span< const char > arr, const FormatOpts & opts );

template< size_t N >
bool operator==( Span< const char > span, const char ( &str )[ N ] ) {
	return span.n == N && memcmp( span.ptr, str, N ) == 0;
}

template< size_t N > bool operator==( const char ( &str )[ N ], Span< const char > span ) { return span == str; }
template< size_t N > bool operator!=( Span< const char > span, const char ( &str )[ N ] ) { return !( span == str ); }
template< size_t N > bool operator!=( const char ( &str )[ N ], Span< const char > span ) { return !( span == str ); }

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

constexpr Vec4 vec4_white = Vec4( 1, 1, 1, 1 );
constexpr Vec4 vec4_black = Vec4( 0, 0, 0, 1 );
constexpr Vec4 vec4_red = Vec4( 1, 0, 0, 1 );

constexpr RGBA8 rgba8_white = RGBA8( 255, 255, 255, 255 );
constexpr RGBA8 rgba8_black = RGBA8( 0, 0, 0, 255 );
