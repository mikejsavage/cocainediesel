#pragma once

#include "qcommon/types.h"

// fnv1a
constexpr u32 FNV1A_BASIS_32 = 2166136261_u32;
constexpr u64 FNV1A_BASIS_64 = 14695981039346656037_u64;

u32 Hash32( const void * data, size_t n, u32 basis = FNV1A_BASIS_32 );
u64 Hash64( const void * data, size_t n, u64 basis = FNV1A_BASIS_64 );

u32 Hash32( const char * str );
u64 Hash64( const char * str );

u64 Hash64( u64 x );

template< typename T >
u32 Hash32( Span< const T > data, u32 basis = FNV1A_BASIS_32 ) {
	return Hash32( data.ptr, data.num_bytes(), basis );
}

template< typename T >
u64 Hash64( Span< const T > data, u64 basis = FNV1A_BASIS_64 ) {
	return Hash64( data.ptr, data.num_bytes(), basis );
}

// case insensitive hashing
u64 CaseHash64( Span< const char > str );
u64 CaseHash64( const char * str );

// compile time hashing
constexpr u32 Hash32_CT( const char * data, size_t n, u32 hash = FNV1A_BASIS_32 ) {
	constexpr u32 prime = 16777619_u32;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ data[ i ] ) * prime;
	}
	return hash;
}

constexpr u64 Hash64_CT( const char * data, size_t n, u64 hash = FNV1A_BASIS_64 ) {
	constexpr u64 prime = 1099511628211_u64;
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
constexpr StringHash EMPTY_HASH = StringHash( 0_u64 );

struct FormatBuffer;
struct FormatOpts;
void format( FormatBuffer * fb, const StringHash & h, const FormatOpts & opts );
