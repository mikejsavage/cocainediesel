/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

#include "gameshared/q_arch.h"
#include "qcommon/base.h"

//==============================================

short ShortSwap( short l );

// little endian
#define BigShort( l ) ShortSwap( l )
#define LittleShort( l ) ( l )
#define LittleLong( l ) ( l )
#define LittleFloat( l ) ( l )

//==============================================

// command line execution flags
#define EXEC_NOW                    0           // don't return until completed
#define EXEC_APPEND                 1           // add to end of the command buffer

//==============================================================
//
//PATHLIB
//
//==============================================================

char *COM_SanitizeFilePath( char *filename );
bool COM_ValidateFilename( const char *filename );
bool COM_ValidateRelativeFilename( const char *filename );
void COM_StripExtension( char *filename );
void COM_DefaultExtension( char *path, const char *extension, size_t size );
const char *COM_FileBase( const char *in );

enum ParseStopOnNewLine {
	Parse_DontStopOnNewLine,
	Parse_StopOnNewLine,
};

Span< const char > ParseToken( const char ** ptr, ParseStopOnNewLine stop );
Span< const char > ParseToken( Span< const char > * cursor, ParseStopOnNewLine stop );

bool TrySpanToInt( Span< const char > str, int * x );
bool TrySpanToFloat( Span< const char > str, float * x );
bool TryStringToU64( const char * str, u64 * x );

int SpanToInt( Span< const char > token, int def );
float SpanToFloat( Span< const char > token, float def );
u64 StringToU64( const char * str, u64 def );

int ParseInt( Span< const char > * cursor, int def, ParseStopOnNewLine stop );
float ParseFloat( Span< const char > * cursor, float def, ParseStopOnNewLine stop );

bool StrEqual( Span< const char > lhs, Span< const char > rhs );
bool StrEqual( Span< const char > lhs, const char * rhs );
bool StrEqual( const char * rhs, Span< const char > lhs );

bool StrCaseEqual( Span< const char > lhs, Span< const char > rhs );
bool StrCaseEqual( Span< const char > lhs, const char * rhs );
bool StrCaseEqual( const char * rhs, Span< const char > lhs );

template< size_t N >
bool operator==( Span< const char > span, const char ( &str )[ N ] ) {
	return StrCaseEqual( span, Span< const char >( str, N - 1 ) );
}

template< size_t N > bool operator==( const char ( &str )[ N ], Span< const char > span ) { return span == str; }
template< size_t N > bool operator!=( Span< const char > span, const char ( &str )[ N ] ) { return !( span == str ); }
template< size_t N > bool operator!=( const char ( &str )[ N ], Span< const char > span ) { return !( span == str ); }

bool StartsWith( Span< const char > str, const char * prefix );
bool StartsWith( const char * str, const char * prefix );

Span< const char > FileExtension( const char * path );
Span< const char > StripExtension( const char * path );
Span< const char > LastFileExtension( const char * path );
Span< const char > FileName( const char * path );
Span< const char > BasePath( const char * path );

bool SortCStringsComparator( const char * a, const char * b );

const char *COM_RemoveJunkChars( const char *in );
bool COM_ValidateConfigstring( const char *string );

char *COM_ListNameForPosition( const char *namesList, int position, const char separator );

//==============================================================
//
// STRINGLIB
//
//==============================================================

#define MAX_QPATH                   64          // max length of a quake game pathname

#define MAX_STRING_CHARS            1024        // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS           256         // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS             1024        // max length of an individual token
#define MAX_CONFIGSTRING_CHARS      MAX_QPATH   // max length of a configstring string

#define MAX_NAME_CHARS              32          // max length of a player name, including trailing \0

#define MAX_CHAT_BYTES              151         // max length of a chat message, including color tokens and trailing \0

#ifndef STR_HELPER
#define STR_HELPER( s )                 # s
#define STR_TOSTR( x )                  STR_HELPER( x )
#endif

//=============================================
// string colors
//=============================================

#define S_COLOR_BLACK   "\x1b\x01\x01\x01\xff"
#define S_COLOR_RED     "\x1b\xff\x01\x01\xff"
#define S_COLOR_GREEN   "\x1b\x01\xff\x01\xff"
#define S_COLOR_YELLOW  "\x1b\xff\xff\x01\xff"
#define S_COLOR_BLUE    "\x1b\x01\x01\xff\xff"
#define S_COLOR_CYAN    "\x1b\x01\xff\xff\xff"
#define S_COLOR_MAGENTA "\x1b\xff\x01\xff\xff"
#define S_COLOR_WHITE   "\x1b\xff\xff\xff\xff"
#define S_COLOR_ORANGE  "\x1b\xff\x80\x01\xff"
#define S_COLOR_GREY    "\x1b\x80\x80\x80\xff"

//=============================================
// strings
//=============================================

void Q_strncpyz( char *dest, const char *src, size_t size );
void Q_strncatz( char *dest, const char *src, size_t size );

char *Q_strupr( char *s );
char *Q_strlwr( char *s );
const char *Q_strrstr( const char *s, const char *substr );
char *Q_trim( char *s );
void RemoveTrailingZeroesFloat( char * str );

/**
 * Converts the given null-terminated string to an URL encoded null-terminated string.
 * Only "unsafe" subset of characters are encoded.
 */
void Q_urlencode_unsafechars( const char *src, char *dst, size_t dst_size );
/**
 * Converts the given URL-encoded string to a null-terminated plain string. Returns
 * total (untruncated) length of the resulting string.
 */
size_t Q_urldecode( const char *src, char *dst, size_t dst_size );

#ifndef _MSC_VER
char *va( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
char *va_r( char *dst, size_t size, const char *format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );
#else
char *va( _Printf_format_string_ const char *format, ... );
char *va_r( char *dst, size_t size, _Printf_format_string_ const char *format, ... );
#endif

//
// key / value info strings
//
#define MAX_INFO_KEY        64
#define MAX_INFO_VALUE      64
#define MAX_INFO_STRING     512

char *Info_ValueForKey( const char *s, const char *key );
void Info_RemoveKey( char *s, const char *key );
bool Info_SetValueForKey( char *s, const char *key, const char *value );
bool Info_Validate( const char *s );

Span< const char > ParseWorldspawnKey( Span< const char > entities, const char * name );

//============================================
// sound
//============================================

constexpr float S_DEFAULT_ATTENUATION_MAXDISTANCE = 8192.0f;
constexpr float S_DEFAULT_ATTENUATION_REFDISTANCE = 250.0f;

//==============================================================
//
//SYSTEM SPECIFIC
//
//==============================================================

enum com_error_code_t {
	ERR_FATAL,      // exit the entire game with a popup window
	ERR_DROP,       // print to console and disconnect from game
};

//==============================================================
//
//FILESYSTEM
//
//==============================================================


#define FS_READ             0
#define FS_WRITE            1
#define FS_APPEND           2
#define FS_GZ               0x100   // compress on write and decompress on read automatically
#define FS_UPDATE           0x200

#define FS_RWA_MASK         ( FS_READ | FS_WRITE | FS_APPEND )

#define FS_SEEK_CUR         0
#define FS_SEEK_SET         1
#define FS_SEEK_END         2

//==============================================================

// connection state of the client in the server
enum sv_client_state_t {
	CS_FREE,            // can be reused for a new connection
	CS_ZOMBIE,          // client has been disconnected, but don't reuse
	                    // connection for a couple seconds
	CS_CONNECTING,      // has send a "new" command, is awaiting for fetching configstrings
	CS_CONNECTED,       // has been assigned to a client_t, but not in game yet
	CS_SPAWNED          // client is fully in game
};

enum keydest_t {
	key_game,
	key_console,
	key_message,
	key_menu,
};
