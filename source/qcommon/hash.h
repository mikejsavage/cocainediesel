#pragma once

#include "qcommon/types.h"

// fnv1a
u32 Hash32( const void * data, size_t n, u32 basis = U32( 2166136261 ) );
u64 Hash64( const void * data, size_t n, u64 basis = U64( 14695981039346656037 ) );

u32 Hash32( const char * str );
u64 Hash64( const char * str );

template< typename T >
u32 Hash32( Span< const T > data ) {
	return Hash32( data.ptr, data.num_bytes() );
}

template< typename T >
u64 Hash64( Span< const T > data ) {
	return Hash64( data.ptr, data.num_bytes() );
}

// compile time hashing
constexpr u32 Hash32_CT( const char * str, size_t n, u32 basis = U32( 2166136261 ) ) {
	return n == 0 ? basis : Hash32_CT( str + 1, n - 1, ( basis ^ str[ 0 ] ) * U32( 16777619 ) );
}

constexpr u64 Hash64_CT( const char * str, size_t n, u64 basis = U64( 14695981039346656037 ) ) {
	return n == 0 ? basis : Hash64_CT( str + 1, n - 1, ( basis ^ str[ 0 ] ) * U64( 1099511628211 ) );
}

struct StringHash {
	u64 hash;

	StringHash() = default;
	explicit StringHash( const char * s );

#ifdef PUBLIC_BUILD
	template< size_t N >
	constexpr StringHash( const char ( &s )[ N ] ) : hash( Hash64_CT( s, N - 1 ) ) { }

	constexpr explicit StringHash( u64 h ) : hash( h ) { }
#else
	const char * str;

	template< size_t N >
	constexpr StringHash( const char ( &s )[ N ] ) : hash( Hash64_CT( s, N - 1 ) ), str( s ) { }

	constexpr explicit StringHash( u64 h ) : hash( h ), str( NULL ) { }
#endif

};

inline bool operator==( StringHash a, StringHash b ) { return a.hash == b.hash; }
inline bool operator!=( StringHash a, StringHash b ) { return !( a == b ); }

// define this as hash = 0 so memset( 0 ) can be used to reset StringHashes
constexpr StringHash EMPTY_HASH = StringHash( U64( 0 ) );
