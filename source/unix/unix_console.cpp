#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "qcommon/qcommon.h"

static bool stdin_active = true;

const char * Sys_ConsoleInput() {
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if( !is_dedicated_server ) {
		return NULL;
	}

	if( !stdin_active ) {
		return NULL;
	}

	FD_ZERO( &fdset );
	FD_SET( 0, &fdset ); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if( select( 1, &fdset, NULL, NULL, &timeout ) == -1 || !FD_ISSET( 0, &fdset ) ) {
		return NULL;
	}

	len = read( 0, text, sizeof( text ) );
	if( len == 0 ) { // eof!
		Com_Printf( "EOF from stdin, console input disabled...\n" );
		stdin_active = false;
		return NULL;
	}

	if( len < 1 ) {
		return NULL;
	}

	text[len - 1] = 0; // rip off the /n and terminate

	return text;
}

static RGB8 Get256Color( u8 c ) {
	if( c >= 232 ) {
		u8 greyscale = 8 + 10 * ( c - 6 * 6 * 6 - 16 );
		return RGB8( greyscale, greyscale, greyscale );
	}

	u8 ri = ( ( c - 16 ) / 36 ) % 6;
	u8 gi = ( ( c - 16 ) /  6 ) % 6;
	u8 bi = ( ( c - 16 )      ) % 6;

	return RGB8( 55 + ri * 40, 55 + gi * 40, 55 + bi * 40 );
}

static float ColorDistance( RGB8 a, RGB8 b ) {
	float dr = float( a.r ) - float( b.r );
	float dg = float( a.g ) - float( b.g );
	float db = float( a.b ) - float( b.b );
	return dr * dr + dg * dg + db * db;
}

static int Nearest256Color( RGB8 c ) {
	u8 cube = 16 +
		36 * ( ( u32( c.r ) + 25 ) / 51 ) +
		6 * ( ( u32( c.g ) + 25 ) / 51 ) +
		( ( u32( c.b ) + 25 ) / 51 );

	float average = ( float( c.r ) + float( c.g ) + float( c.b ) ) / 3.0f;
	u8 greyscale = 232 + 24 * Unlerp01( 8.0f, average, 238.0f );

	if( ColorDistance( Get256Color( cube ), c ) < ColorDistance( Get256Color( greyscale ), c ) )
		return cube;
	return greyscale;
}

void Sys_ConsoleOutput( const char * str ) {
	const char * end = str + strlen( str );

	const char * p = str;
	const char * print_from = str;
	while( p < end ) {
		if( p[ 0 ] == '\033' && p[ 1 ] && p[ 2 ] && p[ 3 ] && p[ 4 ] ) {
			const u8 * u = ( const u8 * ) p;
			printf( "\033[38;5;%dm", Nearest256Color( RGB8( u[ 1 ], u[ 2 ], u[ 3 ] ) ) );
			fwrite( print_from, 1, p - print_from, stdout );
			p += 5;
			print_from = p;
			continue;
		}
		p++;
	}

	printf( "%s\033[0m", print_from );
}

void Sys_ShowErrorMessage( const char * msg ) {
	printf( "%s\n", msg );
}
