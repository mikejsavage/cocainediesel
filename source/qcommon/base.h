#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/platform.h"
#include "qcommon/types.h"
#include "qcommon/math.h"
#include "gg/ggformat.h"
#include "qcommon/linear_algebra.h"

#include "tracy/Tracy.hpp"

/*
 * helpers
 */

#define STRINGIFY_HELPER( a ) #a
#define STRINGIFY( a ) STRINGIFY_HELPER( a )
#define CONCAT_HELPER( a, b ) a##b
#define CONCAT( a, b ) CONCAT_HELPER( a, b )
#define COUNTER_NAME( x ) CONCAT( x, __COUNTER__ )
#define LINE_NAME( x ) CONCAT( x, __LINE__ )

#define IFDEF( x ) ( STRINGIFY( x )[ 0 ] == '1' && STRINGIFY( x )[ 1 ] == '0' )

constexpr bool is_public_build = IFDEF( PUBLIC_BUILD );

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

struct ArenaAllocator;
struct TempAllocator final : public Allocator {
	TempAllocator() = default;
	TempAllocator( const TempAllocator & other );
	~TempAllocator();

	void operator=( const TempAllocator & ) = delete;

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	void deallocate( void * ptr, const char * func, const char * file, int line );

private:
	ArenaAllocator * arena;
	u8 * old_cursor;

	friend struct ArenaAllocator;
};

struct ArenaAllocator final : public Allocator {
	ArenaAllocator() = default;
	ArenaAllocator( void * mem, size_t size );

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	void deallocate( void * ptr, const char * func, const char * file, int line );

	TempAllocator temp();

	void clear();
	void * get_memory();

	float max_utilisation() const;

private:
	u8 * memory;
	u8 * top;
	u8 * cursor;
	u8 * cursor_max;

	u32 num_temp_allocators;

	void * try_temp_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_temp_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );

	friend struct TempAllocator;
};

template< typename... Rest >
char * Allocator::operator()( const char * fmt, const Rest & ... rest ) {
	size_t len = ggformat( NULL, 0, fmt, rest... );
	char * buf = ALLOC_MANY( this, char, len + 1 );
	ggformat( buf, len + 1, fmt, rest... );
	return buf;
}

char * CopyString( Allocator * a, const char * str );

/*
 * Span
 */

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

void EnableFPE();
void DisableFPE();

#if PUBLIC_BUILD
#define DisableFPEScoped
#else
#define DisableFPEScoped DisableFPE(); defer { EnableFPE(); }
#endif
