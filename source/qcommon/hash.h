#pragma once

#include "qcommon/types.h"

// fnv1a
u32 Hash32( const void * data, size_t n, u32 basis = U32( 2166136261 ) );
u64 Hash64( const void * data, size_t n, u64 basis = U64( 14695981039346656037 ) );

u32 Hash32( const char * str );
u64 Hash64( const char * str );

u64 Hash64( u64 x );

template< typename T >
u32 Hash32( Span< const T > data ) {
	return Hash32( data.ptr, data.num_bytes() );
}

template< typename T >
u64 Hash64( Span< const T > data ) {
	return Hash64( data.ptr, data.num_bytes() );
}

// case insensitive hashing
u64 CaseHash64( Span< const char > str );
u64 CaseHash64( const char * str );

// compile time hashing
constexpr u32 Hash32_CT( const char * data, size_t n, u32 hash = U32( 2166136261 ) ) {
	constexpr u32 prime = U32( 16777619 );
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ data[ i ] ) * prime;
	}
	return hash;
}

constexpr u64 Hash64_CT( const char * data, size_t n, u64 hash = U64( 14695981039346656037 ) ) {
	constexpr u64 prime = U64( 1099511628211 );
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ data[ i ] ) * prime;
	}
	return hash;
}

struct StringHash {
	u64 hash;

	StringHash() = default;
	explicit StringHash( const char * s );
	explicit StringHash( Span< const char > s );

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

struct FormatBuffer;
struct FormatOpts;
void format( FormatBuffer * fb, const StringHash & h, const FormatOpts & opts );
