/*
 * ggentropy
 *
 * Copyright (c) 2019 Michael Savage <mike@mikejsavage.co.uk>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined( _WIN32 )
#  define PLATFORM_WINDOWS 1

#elif defined( __linux__ )
#  define PLATFORM_LINUX 1
#  define PLATFORM_HAS_URANDOM 1

#elif defined( __APPLE__ )
#  define PLATFORM_MACOS 1

#elif defined( __FreeBSD__ )
#  define PLATFORM_HAS_ARC4RANDOM 1

#elif defined( __OpenBSD__ )
#  define PLATFORM_HAS_ARC4RANDOM 1

#elif defined( __NetBSD__ )
#  define PLATFORM_HAS_ARC4RANDOM 1

#elif defined( __sun )
#  define PLATFORM_HAS_URANDOM 1

#else
#  error new platform
#endif

#include <stddef.h>
#include <assert.h>

#if PLATFORM_HAS_URANDOM

#include <stdio.h>

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

#pragma comment( lib, "bcrypt.lib" )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );
	return !FAILED( BCryptGenRandom( NULL, ( PUCHAR ) buf, n, BCRYPT_USE_SYSTEM_PREFERRED_RNG ) );
}

#elif PLATFORM_LINUX

#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );

	int ok = syscall( SYS_getrandom, buf, n, 0 );
	if( ok >= 0 && size_t( ok ) == n )
		return true;

	if( errno != ENOSYS )
		return false;

	return try_urandom( buf, n );
}

#elif PLATFORM_MACOS

#include <sys/random.h>

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );
	return getentropy( buf, n ) == 0;
}

#elif PLATFORM_HAS_URANDOM

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );
	return try_urandom( buf, n );
}

#elif PLATFORM_HAS_ARC4RANDOM

#include <stdlib.h>

bool ggentropy( void * buf, size_t n ) {
	assert( n <= 256 );
	arc4random_buf( buf, n );
	return true;
}

#else
#error new platform
#endif
