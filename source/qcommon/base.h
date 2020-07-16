#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/platform.h"
#include "qcommon/types.h"
#include "qcommon/math.h"
#include "qcommon/ggformat.h"
#include "qcommon/linear_algebra.h"

#include "tracy/Tracy.hpp"

/*
 * helpers
 */

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
constexpr T Max3( const T & a, const T & b, const T & c ) {
	return Max2( Max2( a, b ), c );
}

template< typename T >
T Lerp( T a, float t, T b ) {
	return a * ( 1.0f - t ) + b * t;
}

template< typename T >
float Unlerp( T lo, T x, T hi ) {
	return float( x - lo ) / float( hi - lo );
}

template< typename T >
float Unlerp01( T lo, T x, T hi ) {
	return Clamp01( Unlerp( lo, x, hi ) );
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
char * Allocator::operator()( const char * fmt, const Rest & ... rest ) {
	size_t len = ggformat( NULL, 0, fmt, rest... );
	char * buf = ALLOC_MANY( this, char, len + 1 );
	ggformat( buf, len + 1, fmt, rest... );
	return buf;
}

char * CopyString( Allocator * a, const char * str );

/*
 * Span< const char >
 */

Span< const char > MakeSpan( const char * str );
void format( FormatBuffer * fb, Span< const char > arr, const FormatOpts & opts );

/*
 * debug stuff
 */

extern bool break1;
extern bool break2;
extern bool break3;
extern bool break4;

void EnableFPE();
void DisableFPE();

#if PUBLIC_BUILD
#define DisableFPEScoped
#else
#define DisableFPEScoped DisableFPE(); defer { EnableFPE(); }
#endif
