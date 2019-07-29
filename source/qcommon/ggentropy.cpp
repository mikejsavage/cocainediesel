#include "qcommon/base.h"

#if PLATFORM_LINUX || PLATFORM_OSX

static bool try_urandom( void * buf, size_t n ) {
	assert( n <= 256 );

	FILE * f = fopen( "/dev/urandom", "r" );
	if( f == NULL )
		return false;

	size_t ok = fread( buf, 1, n, f );
	fclose( f );

	return ok == n;
}

#endif

#if PLATFORM_WINDOWS

#include <wincrypt.h>

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );

	HCRYPTPROV provider;
	if( CryptAcquireContext( &provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) == 0 )
		return false;

	int ok = CryptGenRandom( provider, n, ( BYTE * ) buf );
	CryptReleaseContext( provider, 0 );

	return ok != 0;
}

#elif PLATFORM_LINUX

#include <sys/random.h>

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );

	ssize_t ok = getrandom( buf, n, 0 );
	if( ok >= 0 && size_t( ok ) == n )
		return true;

	if( errno != ENOSYS )
		return false;

	return try_urandom( buf, n );
}

#elif PLATFORM_OSX

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );
	return try_urandom( buf, n );
}

#elif PLATFORM_OPENBSD

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );
	arc4random_buf( buf, n );
	return true;
}

#else
#error new platform
#endif
