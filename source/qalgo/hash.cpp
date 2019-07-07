#include <string.h>

#include "hash.h"

uint32_t Hash32( const void * data, size_t n, uint32_t hash ) {
	const uint32_t prime = UINT32_C( 16777619 );

	const char * cdata = ( const char * ) data;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ cdata[ i ] ) * prime;
	}
	return hash;
}

uint64_t Hash64( const void * data, size_t n, uint64_t hash ) {
	const uint64_t prime = UINT64_C( 1099511628211 );

	const char * cdata = ( const char * ) data;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ cdata[ i ] ) * prime;
	}
	return hash;
}

uint32_t Hash32( const char * str ) {
	return Hash32( str, strlen( str ) );
}

uint64_t Hash64( const char * str ) {
	return Hash64( str, strlen( str ) );
}
