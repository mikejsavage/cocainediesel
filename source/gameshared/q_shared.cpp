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

#include <ctype.h>
#include <limits.h>

#include "qcommon/qcommon.h"
#include "qcommon/strtonum.h"

//============================================================================

/*
* COM_SanitizeFilePath
*
* Changes \ character to / characters in the string
* Does NOT validate the string at all
* Must be used before other functions are aplied to the string (or those functions might function improperly)
*/
char *COM_SanitizeFilePath( char *path ) {
	char *p;

	assert( path );

	p = path;
	while( *p && ( p = strchr( p, '\\' ) ) ) {
		*p = '/';
		p++;
	}

	return path;
}

bool COM_ValidateFilename( const char *filename ) {
	assert( filename );

	if( !filename || !filename[0] ) {
		return false;
	}

	// we don't allow \ in filenames, all user inputted \ characters are converted to /
	if( strchr( filename, '\\' ) ) {
		return false;
	}

	return true;
}

bool COM_ValidateRelativeFilename( const char *filename ) {
	// TODO: should probably use PathIsRelative on windows
	// https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathisrelativea?redirectedfrom=MSDN
	if( !COM_ValidateFilename( filename ) ) {
		return false;
	}

	if( strchr( filename, ':' ) || strstr( filename, ".." ) || strstr( filename, "//" ) ) {
		return false;
	}

	if( *filename == '/' || *filename == '.' ) {
		return false;
	}

	return true;
}

static bool IsWhitespace( char c ) {
	return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/*
 * this can return an empty string if it parses empty quotes
 * check for ret.ptr == NULL to see if you hit the end of the string
 */
Span< const char > ParseToken( const char ** ptr, ParseStopOnNewLine stop ) {
	const char * cursor = *ptr;
	if( cursor == NULL ) {
		return MakeSpan( "" );
	}

	// skip leading whitespace
	while( IsWhitespace( *cursor ) ) {
		if( *cursor == '\0' ) {
			*ptr = NULL;
			return Span< const char >( NULL, 0 );
		}

		if( *cursor == '\n' && stop == Parse_StopOnNewLine ) {
			*ptr = cursor;
			return Span< const char >( NULL, 0 );
		}

		cursor++;
	}

	bool quoted = false;
	if( *cursor == '\"' ) {
		quoted = true;
		cursor++;
	}

	Span< const char > span( cursor, 0 );

	if( !quoted ) {
		while( !IsWhitespace( *cursor ) ) {
			cursor++;
			span.n++;
		}
	}
	else {
		while( *cursor != '\0' && *cursor != '\"' ) {
			cursor++;
			span.n++;
		}

		if( *cursor == '\"' )
			cursor++;
	}

	*ptr = cursor;

	return span;
}

/*
 * this can return an empty string if it parses empty quotes
 * check for ret.ptr == NULL to see if you hit the end of the string
 */
Span< const char > ParseToken( Span< const char > * cursor, ParseStopOnNewLine stop ) {
	Span< const char > c = *cursor;

	// skip leading whitespace
	while( c.n == 0 || IsWhitespace( c[ 0 ] ) ) {
		if( c.n == 0 ) {
			*cursor = c;
			return Span< const char >( NULL, 0 );
		}

		if( c[ 0 ] == '\n' && stop == Parse_StopOnNewLine ) {
			*cursor = c;
			return Span< const char >( NULL, 0 );
		}

		c++;
	}

	bool quoted = false;
	if( c[ 0 ] == '\"' ) {
		quoted = true;
		c++;
	}

	Span< const char > token( c.ptr, 0 );

	if( !quoted ) {
		while( c.n > 0 && !IsWhitespace( c[ 0 ] ) ) {
			c++;
			token.n++;
		}
	}
	else {
		while( c.n > 0 && c[ 0 ] != '\"' ) {
			c++;
			token.n++;
		}

		if( c.n > 0 && c[ 0 ] == '\"' )
			c++;
	}

	*cursor = c;

	return token;
}

bool TrySpanToInt( Span< const char > str, int * x ) {
	char buf[ 128 ];
	if( str.n >= sizeof( buf ) )
		return false;

	memcpy( buf, str.ptr, str.n );
	buf[ str.n ] = '\0';

	const char * err = NULL;
	*x = strtonum( buf, INT_MIN, INT_MAX, &err );

	return err == NULL;
}

bool TrySpanToFloat( Span< const char > str, float * x ) {
	char buf[ 128 ];
	if( str.n == 0 || str.n >= sizeof( buf ) )
		return false;

	memcpy( buf, str.ptr, str.n );
	buf[ str.n ] = '\0';

	char * end;
	*x = strtof( buf, &end );

	return end == buf + str.n;
}

bool TryStringToU64( const char * str, u64 * x ) {
	if( strlen( str ) == 0 )
		return false;

	u64 res = 0;
	while( true ) {
		if( *str == '\0' )
			break;

		if( *str < '0' || *str > '9' )
			return false;

		if( U64_MAX / 10 < res )
			return false;

		u64 digit = *str - '0';
		if( U64_MAX - digit < res )
			return false;

		res = res * 10 + digit;
		str++;
	}

	*x = res;
	return true;
}

int SpanToInt( Span< const char > token, int def ) {
	int x;
	return TrySpanToInt( token, &x ) ? x : def;
}

float SpanToFloat( Span< const char > token, float def ) {
	float x;
	return TrySpanToFloat( token, &x ) ? x : def;
}

u64 StringToU64( const char * str, u64 def ) {
	u64 x;
	return TryStringToU64( str, &x ) ? x : def;
}

int ParseInt( Span< const char > * cursor, int def, ParseStopOnNewLine stop ) {
	Span< const char > token = ParseToken( cursor, stop );
	return SpanToInt( token, def );
}

float ParseFloat( Span< const char > * cursor, float def, ParseStopOnNewLine stop ) {
	Span< const char > token = ParseToken( cursor, stop );
	return SpanToFloat( token, def );
}

bool StrEqual( Span< const char > lhs, Span< const char > rhs ) {
	return lhs.n == rhs.n && memcmp( lhs.ptr, rhs.ptr, lhs.n ) == 0;
}

bool StrEqual( Span< const char > lhs, const char * rhs ) {
	return StrEqual( lhs, MakeSpan( rhs ) );
}

bool StrEqual( const char * lhs, Span< const char > rhs ) {
	return StrEqual( MakeSpan( lhs ), rhs );
}

bool StrEqual( const char * lhs, const char * rhs ) {
	return StrEqual( MakeSpan( lhs ), MakeSpan( rhs ) );
}

bool StrCaseEqual( Span< const char > lhs, Span< const char > rhs ) {
	if( lhs.n != rhs.n )
		return false;

	for( size_t i = 0; i < lhs.n; i++ ) {
		if( tolower( lhs[ i ] ) != tolower( rhs[ i ] ) ) {
			return false;
		}
	}

	return true;
}

bool StrCaseEqual( Span< const char > lhs, const char * rhs ) {
	return StrCaseEqual( lhs, MakeSpan( rhs ) );
}

bool StrCaseEqual( const char * lhs, Span< const char > rhs ) {
	return StrCaseEqual( MakeSpan( lhs ), rhs );
}

bool StrCaseEqual( const char * lhs, const char * rhs ) {
	return StrCaseEqual( MakeSpan( lhs ), MakeSpan( rhs ) );
}

bool StartsWith( Span< const char > str, const char * prefix ) {
	if( str.n < strlen( prefix ) )
		return false;
	return memcmp( str.ptr, prefix, strlen( prefix ) ) == 0;
}

bool StartsWith( const char * str, const char * prefix ) {
	return StartsWith( MakeSpan( str ), prefix );
}

bool EndsWith( Span< const char > str, const char * suffix ) {
	if( str.n < strlen( suffix ) )
		return false;
	return memcmp( str.end() - strlen( suffix ), suffix, strlen( suffix ) ) == 0;
}

bool EndsWith( const char * str, const char * suffix ) {
	return EndsWith( MakeSpan( str ), suffix );
}

bool CaseStartsWith( const char * str, const char * prefix ) {
	if( strlen( str ) < strlen( prefix ) )
		return false;
	return StrCaseEqual( Span< const char >( str, strlen( prefix ) ), prefix );
}

bool CaseContains( const char * haystack, const char * needle ) {
	if( strlen( needle ) > strlen( haystack ) )
		return false;

	Span< const char > n = MakeSpan( needle );
	size_t diff = strlen( haystack ) - strlen( needle );
	for( size_t i = 0; i < diff; i++ ) {
		Span< const char > h = Span< const char >( haystack + i, n.n );
		if( StrCaseEqual( h, n ) ) {
			return true;
		}
	}

	return false;
}

static Span< const char > MemRChr( Span< const char > str, char c, bool empty_if_missing ) {
	for( size_t i = 0; i < str.n; i++ ) {
		if( str[ str.n - i - 1 ] == c ) {
			return str + ( str.n - i - 1 );
		}
	}

	return empty_if_missing ? Span< const char >() : str;
}

Span< const char > FileExtension( Span< const char > path ) {
	Span< const char > filename = MemRChr( path, '/', false );
	return MemRChr( filename, '.', true );
}

Span< const char > FileExtension( const char * path ) {
	return FileExtension( MakeSpan( path ) );
}

Span< const char > StripExtension( Span< const char > path ) {
	Span< const char > ext = FileExtension( path );
	return path.slice( 0, path.n - ext.n );
}

Span< const char > StripExtension( const char * path ) {
	return StripExtension( MakeSpan( path ) );
}

Span< const char > FileName( const char * path ) {
	const char * filename = strrchr( path, '/' );
	filename = filename == NULL ? path : filename + 1;
	return MakeSpan( filename );
}

Span< const char > BasePath( const char * path ) {
	const char * slash = strrchr( path, '/' );
	return slash == NULL ? MakeSpan( path ) : Span< const char >( path, slash - path );
}

bool SortCStringsComparator( const char * a, const char * b ) {
	return strcmp( a, b ) < 0;
}

//============================================================================
//
//					LIBRARY REPLACEMENT FUNCTIONS
//
//============================================================================

void Q_strncpyz( char *dest, const char *src, size_t size ) {
	if( size ) {
		while( --size && ( *dest++ = *src++ ) ) ;
		*dest = '\0';
	}
}

void Q_strncatz( char *dest, const char *src, size_t size ) {
	if( size ) {
		while( --size && *dest++ ) ;
		if( size ) {
			dest--; size++;
			while( --size && ( *dest++ = *src++ ) ) ;
		}
		*dest = '\0';
	}
}

char *Q_strupr( char *s ) {
	char *p;

	if( s ) {
		for( p = s; *s; s++ )
			*s = toupper( *s );
		return p;
	}

	return NULL;
}

char *Q_strlwr( char *s ) {
	char *p;

	if( s ) {
		for( p = s; *s; s++ )
			*s = tolower( *s );
		return p;
	}

	return NULL;
}

#define IS_TRIMMED_CHAR( s ) ( ( s ) == ' ' || ( s ) == '\t' || ( s ) == '\r' || ( s ) == '\n' )
char *Q_trim( char *s ) {
	char *t = s;
	size_t len;

	// remove leading whitespace
	while( IS_TRIMMED_CHAR( *t ) ) t++;
	len = strlen( s ) - ( t - s );
	if( s != t ) {
		memmove( s, t, len + 1 );
	}

	// remove trailing whitespace
	while( len && IS_TRIMMED_CHAR( s[len - 1] ) )
		s[--len] = '\0';

	return s;
}

void RemoveTrailingZeroesFloat( char * str ) {
	size_t len = strlen( str );
	if( len == 0 )
		return;

	if( strchr( str, '.' ) == NULL )
		return;

	len--;
	while( true ) {
		char c = str[ len ];
		if( c == '.' )
			len--;
		if( c != '0' )
			break;
		len--;
	}

	str[ len + 1 ] = '\0';
}

#define hex2dec( x ) ( ( ( x ) <= '9' ? ( x ) - '0' : ( ( x ) <= 'F' ) ? ( x ) - 'A' + 10 : ( x ) - 'a' + 10 ) )
size_t Q_urldecode( const char *src, char *dst, size_t dst_size ) {
	char *dst_start = dst, *dst_end = dst + dst_size - 1;
	const char *src_end;

	if( !src || !dst || !dst_size ) {
		return 0;
	}

	src_end = src + strlen( src );
	while( src < src_end ) {
		if( dst == dst_end ) {
			break;
		}
		if( ( *src == '%' ) && ( src + 2 < src_end ) &&
			( isxdigit( src[1] ) && isxdigit( src[2] ) ) ) {
			*dst++ = ( hex2dec( src[1] ) << 4 ) + hex2dec( src[2] );
			src += 3;
		} else {
			*dst++ = *src++;
		}
	}

	*dst = '\0';
	return dst - dst_start;
}

//=====================================================================
//
//  INFO STRINGS
//
//=====================================================================

static bool Info_ValidateValue( const char *value ) {
	assert( value );

	if( !value ) {
		return false;
	}

	if( strlen( value ) >= MAX_INFO_VALUE ) {
		return false;
	}

	if( strchr( value, '\\' ) ) {
		return false;
	}

	if( strchr( value, ';' ) ) {
		return false;
	}

	if( strchr( value, '"' ) ) {
		return false;
	}

	return true;
}

static bool Info_ValidateKey( const char *key ) {
	assert( key );

	if( !key ) {
		return false;
	}

	if( !key[0] ) {
		return false;
	}

	if( strlen( key ) >= MAX_INFO_KEY ) {
		return false;
	}

	if( strchr( key, '\\' ) ) {
		return false;
	}

	if( strchr( key, ';' ) ) {
		return false;
	}

	if( strchr( key, '"' ) ) {
		return false;
	}

	return true;
}

/*
* Info_Validate
*
* Some characters are illegal in info strings because they
* can mess up the server's parsing
*/
bool Info_Validate( const char *info ) {
	const char *p, *start;

	assert( info );

	if( !info ) {
		return false;
	}

	if( strlen( info ) >= MAX_INFO_STRING ) {
		return false;
	}

	if( strchr( info, '\"' ) ) {
		return false;
	}

	if( strchr( info, ';' ) ) {
		return false;
	}

	if( strchr( info, '"' ) ) {
		return false;
	}

	p = info;

	while( p && *p ) {
		if( *p++ != '\\' ) {
			return false;
		}

		start = p;
		p = strchr( start, '\\' );
		if( !p ) { // missing key
			return false;
		}
		if( p - start >= MAX_INFO_KEY ) { // too long
			return false;
		}

		p++; // skip the \ char

		start = p;
		p = strchr( start, '\\' );
		if( ( p && p - start >= MAX_INFO_KEY ) || ( !p && strlen( start ) >= MAX_INFO_KEY ) ) { // too long
			return false;
		}
	}

	return true;
}

/*
* Info_FindKey
*
* Returns the pointer to the \ character if key is found
* Otherwise returns NULL
*/
static char *Info_FindKey( const char *info, const char *key ) {
	const char *p, *start;
	size_t key_len;

	assert( Info_Validate( info ) );
	assert( Info_ValidateKey( key ) );

	if( !Info_Validate( info ) || !Info_ValidateKey( key ) ) {
		return NULL;
	}

	p = info;
	key_len = strlen( key );

	while( p && *p ) {
		start = p;

		p++; // skip the \ char
		if( !strncmp( key, p, key_len ) && p[key_len] == '\\' ) {
			return (char *)start;
		}

		p = strchr( p, '\\' );
		if( !p ) {
			return NULL;
		}

		p++; // skip the \ char
		p = strchr( p, '\\' );
	}

	return NULL;
}

/*
* Info_ValueForKey
*
* Searches the string for the given
* key and returns the associated value, or NULL
*/
char *Info_ValueForKey( const char *info, const char *key ) {
	static char value[2][MAX_INFO_VALUE]; // use two buffers so compares work without stomping on each other
	static int valueindex;
	const char *p, *start;
	size_t len;

	assert( info && Info_Validate( info ) );
	assert( key && Info_ValidateKey( key ) );

	if( !Info_Validate( info ) || !Info_ValidateKey( key ) ) {
		return NULL;
	}

	valueindex ^= 1;

	p = Info_FindKey( info, key );
	if( !p ) {
		return NULL;
	}

	p++; // skip the \ char
	p = strchr( p, '\\' );
	if( !p ) {
		return NULL;
	}

	p++; // skip the \ char
	start = p;
	p = strchr( p, '\\' );
	if( !p ) {
		len = strlen( start );
	} else {
		len = p - start;
	}

	if( len >= MAX_INFO_VALUE ) {
		assert( false );
		return NULL;
	}
	strncpy( value[valueindex], start, len );
	value[valueindex][len] = 0;

	return value[valueindex];
}

void Info_RemoveKey( char *info, const char *key ) {
	assert( info && Info_Validate( info ) );
	assert( key && Info_ValidateKey( key ) );

	if( !Info_Validate( info ) || !Info_ValidateKey( key ) ) {
		return;
	}

	while( 1 ) {
		char *start, *p;

		p = Info_FindKey( info, key );
		if( !p ) {
			return;
		}

		start = p;

		p++; // skip the \ char
		p = strchr( p, '\\' );
		if( p ) {
			p++; // skip the \ char
			p = strchr( p, '\\' );
		}

		if( !p ) {
			*start = 0;
		} else {
			// aiwa : fixed possible source and destination overlap with strcpy()
			memmove( start, p, strlen( p ) + 1 );
		}
	}
}

bool Info_SetValueForKey( char *info, const char *key, const char *value ) {
	char pair[MAX_INFO_KEY + MAX_INFO_VALUE + 1];

	assert( info && Info_Validate( info ) );
	assert( key && Info_ValidateKey( key ) );
	assert( value && Info_ValidateValue( value ) );

	if( !Info_Validate( info ) || !Info_ValidateKey( key ) || !Info_ValidateValue( value ) ) {
		return false;
	}

	Info_RemoveKey( info, key );

	snprintf( pair, sizeof( pair ), "\\%s\\%s", key, value );

	if( strlen( pair ) + strlen( info ) > MAX_INFO_STRING ) {
		return false;
	}

	Q_strncatz( info, pair, MAX_INFO_STRING );

	return true;
}

Span< const char > ParseWorldspawnKey( Span< const char > entities, const char * name ) {
	Span< const char > cursor = entities;

	if( ParseToken( &cursor, Parse_DontStopOnNewLine ) != "{" ) {
		Fatal( "Entity string doesn't start with {" );
	}

	while( true ) {
		Span< const char > key = ParseToken( &cursor, Parse_DontStopOnNewLine );
		Span< const char > value = ParseToken( &cursor, Parse_DontStopOnNewLine );

		if( key == "" || value == "" || key == "}" )
			break;

		if( StrCaseEqual( key, name ) )
			return value;
	}

	return Span< const char >();
}
