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

#include "q_arch.h"
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
#define EXEC_INSERT                 1           // insert at current position, but don't run yet
#define EXEC_APPEND                 2           // add to end of the command buffer

//=============================================
// fonts
//=============================================

#define SYSTEM_FONT_TINY_SIZE       8
#define SYSTEM_FONT_CONSOLE_SIZE    12
#define SYSTEM_FONT_SMALL_SIZE      14
#define SYSTEM_FONT_MEDIUM_SIZE     16
#define SYSTEM_FONT_BIG_SIZE        24

//==============================================================
//
//PATHLIB
//
//==============================================================

char *COM_SanitizeFilePath( char *filename );
bool COM_ValidateFilename( const char *filename );
bool COM_ValidateRelativeFilename( const char *filename );
void COM_StripExtension( char *filename );
const char *COM_FileExtension( const char *in );
void COM_DefaultExtension( char *path, const char *extension, size_t size );
void COM_ReplaceExtension( char *path, const char *extension, size_t size );
const char *COM_FileBase( const char *in );
void COM_StripFilename( char *filename );
int COM_FilePathLength( const char *in );

Span< char > ParseSpan( char ** ptr, bool stop_on_newline );
Span< const char > ParseSpan( const char ** ptr, bool stop_on_newline );

Span< const char > ParseSpan( Span< const char > * cursor, bool stop_on_newline );

// data is an in/out parm, returns a parsed out token
char *COM_ParseExt2_r( char *token, size_t token_size, const char **data_p, bool nl, bool sq );
#define COM_ParseExt_r( token, token_size, data_p, nl ) COM_ParseExt2_r( token, token_size, (const char **)data_p, nl, true )
#define COM_Parse_r( token, token_size, data_p )   COM_ParseExt_r( token, token_size, data_p, true )

char *COM_ParseExt2( const char **data_p, bool nl, bool sq );
#define COM_ParseExt( data_p, nl ) COM_ParseExt2( (const char **)data_p, nl, true )
#define COM_Parse( data_p )   COM_ParseExt( data_p, true )

int COM_Compress( char *data_p );
const char *COM_RemoveJunkChars( const char *in );
int COM_ReadColorRGBString( const char *in );
int COM_ReadColorRGBAString( const char *in );
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

#define MAX_NAME_BYTES              65          // max length of a player name, including trailing \0
#define MAX_NAME_CHARS              32          // max visible characters in a name (color tokens and \0 not counted)

#define MAX_CHAT_BYTES              151         // max length of a chat message, including color tokens and trailing \0

#ifndef STR_HELPER
#define STR_HELPER( s )                 # s
#define STR_TOSTR( x )                  STR_HELPER( x )
#endif

//=============================================
// string colors
//=============================================

#define Q_COLOR_ESCAPE  '^'
#define S_COLOR_ESCAPE  "^"

#define COLOR_BLACK     '0'
#define COLOR_RED       '1'
#define COLOR_GREEN     '2'
#define COLOR_YELLOW    '3'
#define COLOR_BLUE      '4'
#define COLOR_CYAN      '5'
#define COLOR_MAGENTA   '6'
#define COLOR_WHITE     '7'
#define COLOR_ORANGE    '8'
#define COLOR_GREY      '9'
#define ColorIndex( c )   ( ( ( ( c ) - '0' ) < MAX_S_COLORS ) && ( ( ( c ) - '0' ) >= 0 ) ? ( ( c ) - '0' ) : 7 )

#define S_COLOR_BLACK   "^0"
#define S_COLOR_RED     "^1"
#define S_COLOR_GREEN   "^2"
#define S_COLOR_YELLOW  "^3"
#define S_COLOR_BLUE    "^4"
#define S_COLOR_CYAN    "^5"
#define S_COLOR_MAGENTA "^6"
#define S_COLOR_WHITE   "^7"
#define S_COLOR_ORANGE  "^8"
#define S_COLOR_GREY    "^9"

#define COLOR_R( rgba )       ( ( rgba ) & 0xFF )
#define COLOR_G( rgba )       ( ( ( rgba ) >> 8 ) & 0xFF )
#define COLOR_B( rgba )       ( ( ( rgba ) >> 16 ) & 0xFF )
#define COLOR_A( rgba )       ( ( ( rgba ) >> 24 ) & 0xFF )
#define COLOR_RGB( r, g, b )    ( ( ( r ) << 0 ) | ( ( g ) << 8 ) | ( ( b ) << 16 ) )
#define COLOR_RGBA( r, g, b, a ) ( ( ( r ) << 0 ) | ( ( g ) << 8 ) | ( ( b ) << 16 ) | ( ( a ) << 24 ) )

//=============================================
// strings
//=============================================

void Q_strncpyz( char *dest, const char *src, size_t size );
void Q_strncatz( char *dest, const char *src, size_t size );

int Q_vsnprintfz( char *dest, size_t size, const char *format, va_list argptr );

#ifndef _MSC_VER
int Q_snprintfz( char *dest, size_t size, const char *format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );
#else
int Q_snprintfz( char *dest, size_t size, _Printf_format_string_ const char *format, ... );
#endif

char *Q_strupr( char *s );
char *Q_strlwr( char *s );
const char *Q_strrstr( const char *s, const char *substr );
bool Q_isdigit( const char *str );
char *Q_trim( char *s );
char *Q_chrreplace( char *s, const char subj, const char repl );
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

// color string functions ("^1text" etc)
#define GRABCHAR_END    0
#define GRABCHAR_CHAR   1
#define GRABCHAR_COLOR  2
int Q_GrabCharFromColorString( const char **pstr, char *c, int *colorindex );
const char *COM_RemoveColorTokensExt( const char *str, bool draw );
#define COM_RemoveColorTokens( in ) COM_RemoveColorTokensExt( in,false )
int COM_SanitizeColorString( const char *str, char *buf, int bufsize, int maxprintablechars, int startcolor );
const char *Q_ColorStringTerminator( const char *str, int finalcolor );
int Q_ColorStrLastColor( int previous, const char *s, int maxlen );

size_t Q_WCharUtf8Length( wchar_t wc );
size_t Q_WCharToUtf8( wchar_t wc, char *dest, size_t bufsize );
char *Q_WCharToUtf8Char( wchar_t wc );
wchar_t Q_GrabWCharFromUtf8String( const char **pstr );
int Q_GrabWCharFromColorString( const char **pstr, wchar_t *wc, int *colorindex );
#define UTF8SYNC_LEFT 0
#define UTF8SYNC_RIGHT 1
int Q_Utf8SyncPos( const char *str, int pos, int dir );
bool Q_IsBreakingSpace( const char *str );
bool Q_IsBreakingSpaceChar( wchar_t c );

float *tv( float x, float y, float z );
char *vtos( float v[3] );

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

//==============================================

//
// per-level limits
//
#define MAX_CLIENTS                 256         // absolute limit
#define MAX_EDICTS                  4096        // must change protocol to increase more
#define MAX_MODELS                  1024        // these are sent over the net as shorts
#define MAX_SOUNDS                  1024        // so they cannot be blindly increased
#define MAX_IMAGES                  256
#define MAX_ITEMS                   64          // 16x4
#define MAX_GENERAL                 128         // general config strings

//============================================
// HTTP
//============================================
#define HTTP_CODE_OK                        200
#define HTTP_CODE_PARTIAL_CONTENT           206

//============================================
// sound
//============================================

//#define S_DEFAULT_ATTENUATION_MODEL		1
#define S_DEFAULT_ATTENUATION_MODEL         3
#define S_DEFAULT_ATTENUATION_MAXDISTANCE   8000
#define S_DEFAULT_ATTENUATION_REFDISTANCE   125

float Q_GainForAttenuation( int model, float maxdistance, float refdistance, float dist, float attenuation );

//=============================================

constexpr const char *SOUND_EXTENSIONS[] = { ".ogg" };
constexpr size_t NUM_SOUND_EXTENSIONS = ARRAY_COUNT( SOUND_EXTENSIONS );

constexpr const char *IMAGE_EXTENSIONS[] = { ".jpg", ".png" };
constexpr size_t NUM_IMAGE_EXTENSIONS = ARRAY_COUNT( IMAGE_EXTENSIONS );

//==============================================================
//
//SYSTEM SPECIFIC
//
//==============================================================

typedef enum {
	ERR_FATAL,      // exit the entire game with a popup window
	ERR_DROP,       // print to console and disconnect from game
} com_error_code_t;

// this is only here so the functions in q_shared.c and q_math.c can link

#ifndef _MSC_VER
void Sys_Error( const char *error, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute__( ( noreturn ) );
void Com_Printf( const char *msg, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
void Com_Error( com_error_code_t code, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) void Sys_Error( _Printf_format_string_ const char *error, ... );
void Com_Printf( _Printf_format_string_ const char *msg, ... );
__declspec( noreturn ) void Com_Error( com_error_code_t code, _Printf_format_string_ const char *format, ... );
#endif

//==============================================================
//
//FILESYSTEM
//
//==============================================================


#define FS_READ             0
#define FS_WRITE            1
#define FS_APPEND           2
#define FS_GZ               0x100   // compress on write and decompress on read automatically. doesn't work on pk3 files
#define FS_UPDATE           0x200
#define FS_CACHE            0x800

#define FS_RWA_MASK         ( FS_READ | FS_WRITE | FS_APPEND )

#define FS_SEEK_CUR         0
#define FS_SEEK_SET         1
#define FS_SEEK_END         2

typedef enum {
	FS_MEDIA_IMAGES,

	FS_MEDIA_NUM_TYPES
} fs_mediatype_t;

//==============================================================

// connection state of the client in the server
typedef enum {
	CS_FREE,            // can be reused for a new connection
	CS_ZOMBIE,          // client has been disconnected, but don't reuse
	                    // connection for a couple seconds
	CS_CONNECTING,      // has send a "new" command, is awaiting for fetching configstrings
	CS_CONNECTED,       // has been assigned to a client_t, but not in game yet
	CS_SPAWNED          // client is fully in game
} sv_client_state_t;

typedef enum {
	key_game,
	key_console,
	key_message,
	key_menu,
} keydest_t;
