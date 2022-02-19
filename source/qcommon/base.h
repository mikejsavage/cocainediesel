#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/platform.h"
#include "qcommon/types.h"
#include "qcommon/math.h"
#include "gg/ggformat.h"
#include "qcommon/allocators.h"
#include "qcommon/linear_algebra.h"
#include "qcommon/tracy.h"

/*
 * helpers
 */

#define STRINGIFY_HELPER( a ) #a
#define STRINGIFY( a ) STRINGIFY_HELPER( a )
#define CONCAT_HELPER( a, b ) a##b
#define CONCAT( a, b ) CONCAT_HELPER( a, b )
#define COUNTER_NAME( x ) CONCAT( x, __COUNTER__ )
#define LINE_NAME( x ) CONCAT( x, __LINE__ )

#define IFDEF( x ) ( STRINGIFY( x )[ 0 ] == '1' && STRINGIFY( x )[ 1 ] == '\0' )

constexpr bool is_public_build = IFDEF( PUBLIC_BUILD );

template< typename To, typename From >
inline To bit_cast( const From & from ) {
	STATIC_ASSERT( sizeof( To ) == sizeof( From ) );
	To result;
	memcpy( &result, &from, sizeof( result ) );
	return result;
}

#define Fatal( format, ... ) FatalImpl( __FILE__, __LINE__, format, ##__VA_ARGS__ )
#ifndef _MSC_VER
void FatalImpl( const char * file, int line, const char * format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );
#else
void FatalImpl( const char * file, int line, _Printf_format_string_ const char * format, ... );
#endif
void FatalErrno( const char * msg );

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
 * Span
 */

Span< char > MakeSpan( char * str );
Span< const char > MakeSpan( const char * str );
void format( FormatBuffer * fb, Span< const char > arr, const FormatOpts & opts );

template< typename T, size_t N >
Span< T > StaticSpan( T ( &arr )[ N ] ) {
	return Span< T >( arr, N );
}

/*
 * debug stuff
 */

extern bool break1;
extern bool break2;
extern bool break3;
extern bool break4;
