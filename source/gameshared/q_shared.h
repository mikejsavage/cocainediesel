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

#include "qcommon/base.h"

char *COM_SanitizeFilePath( char *filename );
bool COM_ValidateFilename( const char *filename );
bool COM_ValidateRelativeFilename( const char *filename );

enum ParseStopOnNewLine {
	Parse_DontStopOnNewLine,
	Parse_StopOnNewLine,
};

Span< const char > ParseToken( Span< const char > * cursor, ParseStopOnNewLine stop );

bool TrySpanToU64( Span< const char > str, u64 * x );
bool TrySpanToS64( Span< const char > str, s64 * x );
bool TrySpanToInt( Span< const char > str, int * x );
bool TrySpanToFloat( Span< const char > str, float * x );

u64 SpanToU64( Span< const char > str, u64 def );
int SpanToInt( Span< const char > token, int def );
float SpanToFloat( Span< const char > token, float def );

int ParseInt( Span< const char > * cursor, int def, ParseStopOnNewLine stop );
float ParseFloat( Span< const char > * cursor, float def, ParseStopOnNewLine stop );

char ToLowerASCII( char c );
char ToUpperASCII( char c );

bool StrEqual( Span< const char > lhs, Span< const char > rhs );
bool StrEqual( Span< const char > lhs, const char * rhs );
bool StrEqual( const char * lhs, Span< const char > rhs );
bool StrEqual( const char * lhs, const char * rhs );

bool StrCaseEqual( Span< const char > lhs, Span< const char > rhs );
bool StrCaseEqual( Span< const char > lhs, const char * rhs );
bool StrCaseEqual( const char * lhs, Span< const char > rhs );
bool StrCaseEqual( const char * lhs, const char * rhs );

template< size_t N >
bool operator==( Span< const char > span, const char ( &str )[ N ] ) {
	return StrCaseEqual( span, Span< const char >( str, N - 1 ) );
}

template< size_t N > bool operator==( const char ( &str )[ N ], Span< const char > span ) { return span == str; }
template< size_t N > bool operator!=( Span< const char > span, const char ( &str )[ N ] ) { return !( span == str ); }
template< size_t N > bool operator!=( const char ( &str )[ N ], Span< const char > span ) { return !( span == str ); }

bool StartsWith( Span< const char > str, Span< const char > prefix );
bool StartsWith( Span< const char > str, const char * prefix );
bool StartsWith( const char * str, const char * prefix );
bool EndsWith( Span< const char > str, const char * suffix );
bool EndsWith( const char * str, const char * suffix );

bool CaseStartsWith( const char * str, const char * prefix );

Span< const char > StripPrefix( Span< const char > str, const char * prefix );

bool CaseContains( const char * haystack, const char * needle );

Span< const char > FileExtension( Span< const char > path );
Span< const char > FileExtension( const char * path );
Span< const char > StripExtension( Span< const char > path );
Span< const char > StripExtension( const char * path );
Span< const char > FileName( Span< const char > path );
Span< const char > FileName( const char * path );
Span< const char > BasePath( const char * path );

bool SortCStringsComparator( const char * a, const char * b );

void SafeStrCpy( char * dst, const char * src, size_t dst_size );
void SafeStrCat( char * dst, const char * src, size_t dst_size );
void RemoveTrailingZeroesFloat( char * str );

//==============================================================
//
// STRINGLIB
//
//==============================================================

#define MAX_STRING_CHARS            1024        // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS           256         // max tokens resulting from Cmd_TokenizeString
#define MAX_NAME_CHARS              32          // max length of a player name, not including trailing \0

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

// connection state of the client in the server
enum sv_client_state_t {
	CS_FREE,            // can be reused for a new connection
	CS_ZOMBIE,          // client has been disconnected, but don't reuse
	                    // connection for a couple seconds
	CS_CONNECTING,      // has send a "new" command, is awaiting for fetching baselines
	CS_CONNECTED,       // has been assigned to a client_t, but not in game yet
	CS_SPAWNED          // client is fully in game
};
