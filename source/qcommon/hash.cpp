#include "qcommon/base.h"
#include "qcommon/hash.h"
#include "gameshared/q_shared.h"

u32 Hash32( const void * data, size_t n, u32 hash ) {
	return Hash32_CT( ( const char * ) data, n, hash );
}

u64 Hash64( const void * data, size_t n, u64 hash ) {
	return Hash64_CT( ( const char * ) data, n, hash );
}

u32 Hash32( const char * str ) {
	return Hash32( str, strlen( str ) );
}

u64 Hash64( const char * str ) {
	return Hash64( str, strlen( str ) );
}

u64 Hash64( u64 x ) {
	x = ( x ^ ( x >> 30 ) ) * 0xbf58476d1ce4e5b9_u64;
	x = ( x ^ ( x >> 27 ) ) * 0x94d049bb133111eb_u64;
	x = x ^ ( x >> 31 );
	return x;
}

u64 CaseHash64( Span< const char > str ) {
	u64 hash = Hash64( "" );
	for( char c : str ) {
		c = ToLowerASCII( c );
		hash = Hash64( &c, 1, hash );
	}
	return hash;
}

u64 CaseHash64( const char * str ) {
	return CaseHash64( MakeSpan( str ) );
}

#if PUBLIC_BUILD
StringHash::StringHash( const char * s ) {
	hash = Hash64( s );
}

StringHash::StringHash( Span< const char > s ) {
	hash = Hash64( s );
}
#else
StringHash::StringHash( const char * s ) {
	hash = Hash64( s );
	str = NULL;
}

StringHash::StringHash( Span< const char > s ) {
	hash = Hash64( s );
	str = NULL;
}
#endif

void format( FormatBuffer * fb, const StringHash & v, const FormatOpts & opts ) {
#if PUBLIC_BUILD
	ggformat_impl( fb, "0x{08x}", v.hash );
#else
	ggformat_impl( fb, "{} (0x{08x})", v.str == NULL ? "NULL" : v.str, v.hash );
#endif
}
