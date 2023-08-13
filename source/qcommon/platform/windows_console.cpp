#include "qcommon/platform.h"

#if PLATFORM_WINDOWS

#include "qcommon/platform/windows_mini_windows_h.h"

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
			FatalGLE( "GetNumberOfConsoleInputEvents" );
		}

		if( numevents <= 0 ) {
			break;
		}

		if( !ReadConsoleInput( hinput, &rec, 1, &numread ) ) {
			FatalGLE( "ReadConsoleInput" );
		}

		if( numread != 1 ) {
			FatalGLE( "ReadConsoleInput numread" );
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

#endif // #if PLATFORM_WINDOWS
