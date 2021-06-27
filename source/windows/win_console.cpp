#include "windows/miniwindows.h"

#include "qcommon/types.h"
#include "qcommon/qcommon.h"

static HANDLE hinput = NULL;
static HANDLE houtput = NULL;

#define MAX_CONSOLETEXT 256
static char console_text[MAX_CONSOLETEXT];
static int console_textlen;

static char *OEM_to_utf8( const char *str ) {
	WCHAR wstr[MAX_CONSOLETEXT];
	static char utf8str[MAX_CONSOLETEXT * 4]; /* longest valid utf8 sequence is 4 bytes */

	MultiByteToWideChar( CP_OEMCP, 0, str, -1, wstr, sizeof( wstr ) / sizeof( WCHAR ) );
	wstr[sizeof( wstr ) / sizeof( wstr[0] ) - 1] = 0;
	WideCharToMultiByte( CP_UTF8, 0, wstr, -1, utf8str, sizeof( utf8str ), NULL, NULL );
	utf8str[sizeof( utf8str ) - 1] = 0;

	return utf8str;
}

const char *Sys_ConsoleInput() {
	INPUT_RECORD rec;
	int ch;
	DWORD dummy;
	DWORD numread, numevents;

	if( !is_dedicated_server ) {
		return NULL;
	}

	if( !hinput ) {
		hinput = GetStdHandle( STD_INPUT_HANDLE );
	}
	if( !houtput ) {
		houtput = GetStdHandle( STD_OUTPUT_HANDLE );
	}

	while( true ) {
		if( !GetNumberOfConsoleInputEvents( hinput, &numevents ) ) {
			Sys_Error( "Error getting # of console events: %d", GetLastError() );
		}

		if( numevents <= 0 ) {
			break;
		}

		if( !ReadConsoleInput( hinput, &rec, 1, &numread ) ) {
			Sys_Error( "Error reading console input" );
		}

		if( numread != 1 ) {
			Sys_Error( "Couldn't read console input" );
		}

		if( rec.EventType == KEY_EVENT ) {
			if( !rec.Event.KeyEvent.bKeyDown ) {
				ch = rec.Event.KeyEvent.uChar.AsciiChar;

				switch( ch ) {
					case '\r':
						WriteFile( houtput, "\r\n", 2, &dummy, NULL );

						if( console_textlen ) {
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return OEM_to_utf8( console_text );
						}
						break;

					case '\b':
						if( console_textlen ) {
							console_textlen--;
							WriteFile( houtput, "\b \b", 3, &dummy, NULL );
						}
						break;

					default:
						if( ( unsigned char )ch >= ' ' ) {
							if( console_textlen < sizeof( console_text ) - 2 ) {
								WriteFile( houtput, &ch, 1, &dummy, NULL );
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}
						break;
				}
			}
		}
	}

	return NULL;
}

struct WindowsConsoleColor {
	RGB8 color;
	WORD attr;
};

static WindowsConsoleColor console_colors[] = {
	{ RGB8( 128, 0, 0 ), FOREGROUND_RED },
	{ RGB8( 0, 128, 0 ), FOREGROUND_GREEN },
	{ RGB8( 0, 0, 128 ), FOREGROUND_BLUE },
	{ RGB8( 128, 128, 0 ), FOREGROUND_RED | FOREGROUND_GREEN },
	{ RGB8( 128, 0, 128 ), FOREGROUND_RED | FOREGROUND_BLUE },
	{ RGB8( 0, 128, 128 ), FOREGROUND_GREEN | FOREGROUND_BLUE },
	{ RGB8( 128, 128, 128 ), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE },

	{ RGB8( 192, 192, 192 ), FOREGROUND_INTENSITY },

	{ RGB8( 255, 0, 0 ), FOREGROUND_INTENSITY | FOREGROUND_RED },
	{ RGB8( 0, 255, 0 ), FOREGROUND_INTENSITY | FOREGROUND_GREEN },
	{ RGB8( 0, 0, 255 ), FOREGROUND_INTENSITY | FOREGROUND_BLUE },
	{ RGB8( 255, 255, 0 ), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN },
	{ RGB8( 255, 0, 255 ), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE },
	{ RGB8( 0, 255, 255 ), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE },
	{ RGB8( 255, 255, 255 ), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE },
};

static float ColorDistance( RGB8 a, RGB8 b ) {
	float dr = float( a.r ) - float( b.r );
	float dg = float( a.g ) - float( b.g );
	float db = float( a.b ) - float( b.b );
	return dr * dr + dg * dg + db * db;
}

static WORD NearestConsoleColor( RGB8 c ) {
	float best = FLT_MAX;
	WORD attr = 0;
	for( WindowsConsoleColor console : console_colors ) {
		float d = ColorDistance( console.color, c );
		if( d < best ) {
			best = d;
			attr = console.attr;
		}
	}
	return attr;
}

void Sys_ConsoleOutput( const char * str ) {
	HANDLE output = GetStdHandle( STD_OUTPUT_HANDLE );

	const char * end = str + strlen( str );

	const char * p = str;
	const char * print_from = str;
	while( p < end ) {
		if( p[ 0 ] == '\033' && p[ 1 ] && p[ 2 ] && p[ 3 ] && p[ 4 ] ) {
			const u8 * u = ( const u8 * ) p;

			SetConsoleTextAttribute( output, NearestConsoleColor( RGB8( u[ 1 ], u[ 2 ], u[ 3 ] ) ) );

			DWORD written;
			WriteConsole( output, print_from, p - print_from, &written, NULL );

			p += 5;
			print_from = p;
			continue;
		}
		p++;
	}

	DWORD written;
	WriteConsole( output, print_from, strlen( print_from ), &written, NULL );
	SetConsoleTextAttribute( output, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE );
}
