#include "qcommon/base.h"
#include "hash.h"

u32 Hash32( const void * data, size_t n, u32 hash ) {
	const u32 prime = UINT32_C( 16777619 );

	const char * cdata = ( const char * ) data;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ cdata[ i ] ) * prime;
	}
	return hash;
}

u64 Hash64( const void * data, size_t n, u64 hash ) {
	const u64 prime = UINT64_C( 1099511628211 );

	const char * cdata = ( const char * ) data;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ cdata[ i ] ) * prime;
	}
	return hash;
}

u32 Hash32( const char * str ) {
	return Hash32( str, strlen( str ) );
}

u64 Hash64( const char * str ) {
	return Hash64( str, strlen( str ) );
}

u32 Hash32( Span< const char > str ) {
	return Hash32( str.ptr, str.n );
}

u64 Hash64( Span< const char > str ) {
	return Hash64( str.ptr, str.n );
}

#ifdef PUBLIC_BUILD
StringHash::StringHash( const char * s ) {
	hash = Hash64( s );
}
#else
StringHash::StringHash( const char * s ) {
	hash = Hash64( s );
	str = NULL;
}
#endif
