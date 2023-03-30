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

#define CONCAT_HELPER( a, b ) a##b
#define CONCAT( a, b ) CONCAT_HELPER( a, b )
#define COUNTER_NAME( x ) CONCAT( x, __COUNTER__ )

template< typename To, typename From >
To bit_cast( const From & from ) {
	STATIC_ASSERT( sizeof( To ) == sizeof( From ) );
	To result;
	memcpy( &result, &from, sizeof( result ) );
	return result;
}

#define Fatal( format, ... ) FatalImpl( __FILE__, __LINE__, format, ##__VA_ARGS__ )
#define FatalErrno( msg ) FatalErrnoImpl( msg, __FILE__, __LINE__ )

#if COMPILER_MSVC
void FatalImpl( const char * file, int line, _Printf_format_string_ const char * format, ... );
#else
void FatalImpl( const char * file, int line, const char * format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );
#endif
void FatalErrnoImpl( const char * msg, const char * file, int line );

template< typename T >
constexpr bool HasBit( T bits, T bit ) {
	return ( bits & bit ) != 0;
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

#define defer [[maybe_unused]] const auto & COUNTER_NAME( DEFER_ ) = DeferHelper() + [&]()

/*
 * Span
 */

Span< char > MakeSpan( char * str );
Span< const char > MakeSpan( const char * str );
void format( FormatBuffer * fb, Span< const char > arr, const FormatOpts & opts );

template< typename T, size_t N >
constexpr Span< T > StaticSpan( T ( &arr )[ N ] ) {
	return Span< T >( arr, N );
}

/*
 * Optional
 */

template< typename T >
Optional< T > MakeOptional( const T & x ) {
	return x;
}

// this is a template so implicit casts won't happen, i.e. we can't rely on an
// implicit cast from Optional< T > to Optional< const T >
template< typename T >
bool operator==( Optional< T > & opt, const T & x ) {
	return opt.exists && opt.value == x;
}

template< typename T >
bool operator==( const T & x, Optional< T > & opt ) {
	return opt == x;
}

/*
 * debug stuff
 */

extern bool break1;
extern bool break2;
extern bool break3;
extern bool break4;
