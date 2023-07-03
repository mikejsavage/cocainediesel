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

#include <limits.h>
#include <type_traits>

#include "qcommon/qcommon.h"

/*
* COM_SanitizeFilePath
*
* Changes \ character to / characters in the string
* Does NOT validate the string at all
* Must be used before other functions are aplied to the string (or those functions might function improperly)
*/
char *COM_SanitizeFilePath( char * path ) {
	Assert( path );

	for( ; *path && ( path = strchr( path, '\\' ) ); path++ ) {
		*path = '/';
	}

	return path;
}

bool COM_ValidateFilename( const char *filename ) {
	Assert( filename );

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

bool TrySpanToU64( Span< const char > str, u64 * x ) {
	if( str.n == 0 )
		return false;

	u64 res = 0;
	for( char c : str ) {
		if( c < '0' || c > '9' )
			return false;

		if( U64_MAX / 10 < res )
			return false;

		u64 digit = c - '0';
		if( U64_MAX - digit < res )
			return false;

		res = res * 10 + digit;
	}

	*x = res;
	return true;
}

bool TrySpanToS64( Span< const char > str, s64 * x ) {
	if( str.n == 0 )
		return false;

	if( str[ 0 ] == '-' ) {
		u64 u;
		if( !TrySpanToU64( str + 1, &u ) || u > u64( S64_MAX ) + 1 )
			return false;
		*x = -s64( u );
		return true;
	}

	u64 u;
	if( !TrySpanToU64( str, &u ) || u > S64_MAX )
		return false;
	*x = u;
	return true;
}

bool TrySpanToInt( Span< const char > str, int * x ) {
	s64 x64;
	if( !TrySpanToS64( str, &x64 ) )
		return false;
	if( x64 < INT_MIN || x64 > INT_MAX )
		return false;
	*x = int( x64 );
	return true;
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

u64 SpanToU64( Span< const char > str, u64 def ) {
	u64 x;
	return TrySpanToU64( str, &x ) ? x : def;
}

int SpanToInt( Span< const char > token, int def ) {
	int x;
	return TrySpanToInt( token, &x ) ? x : def;
}

float SpanToFloat( Span< const char > token, float def ) {
	float x;
	return TrySpanToFloat( token, &x ) ? x : def;
}

int ParseInt( Span< const char > * cursor, int def, ParseStopOnNewLine stop ) {
	Span< const char > token = ParseToken( cursor, stop );
	return SpanToInt( token, def );
}

float ParseFloat( Span< const char > * cursor, float def, ParseStopOnNewLine stop ) {
	Span< const char > token = ParseToken( cursor, stop );
	return SpanToFloat( token, def );
}

char ToLowerASCII( char c ) {
	return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}

char ToUpperASCII( char c ) {
	return c >= 'a' && c <= 'z' ? c + 'A' - 'a' : c;
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
		if( ToLowerASCII( lhs[ i ] ) != ToLowerASCII( rhs[ i ] ) ) {
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

bool StartsWith( Span< const char > str, Span< const char > prefix ) {
	if( str.n < prefix.n )
		return false;
	return memcmp( str.ptr, prefix.ptr, prefix.n ) == 0;
}

bool StartsWith( Span< const char > str, const char * prefix ) {
	return StartsWith( str, MakeSpan( prefix ) );
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

Span< const char > StripPrefix( Span< const char > str, const char * prefix ) {
	return StartsWith( str, prefix ) ? str + strlen( prefix ) : str;
}

bool CaseContains( const char * haystack, const char * needle ) {
	Span< const char > h = MakeSpan( haystack );
	Span< const char > n = MakeSpan( needle );
	if( n.n > h.n )
		return false;

	size_t diff = h.n - n.n;
	for( size_t i = 0; i <= diff; i++ ) {
		if( StrCaseEqual( h.slice( i, i + n.n ), n ) ) {
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

Span< const char > FileName( Span< const char > path ) {
	Span< const char > name = MemRChr( path, '/', true );
	return name == "" ? path : name + 1;
}

Span< const char > FileName( const char * path ) {
	return FileName( MakeSpan( path ) );
}

Span< const char > BasePath( const char * path ) {
	const char * slash = strrchr( path, '/' );
	return slash == NULL ? MakeSpan( path ) : Span< const char >( path, slash - path );
}

bool SortCStringsComparator( const char * a, const char * b ) {
	return strcmp( a, b ) < 0;
}

void SafeStrCpy( char * dst, const char * src, size_t dst_size ) {
	if( dst_size == 0 )
		return;
	size_t src_len = strlen( src );
	size_t to_copy = src_len < dst_size ? src_len : dst_size - 1;
	memcpy( dst, src, to_copy );
	dst[ to_copy ] = '\0';
}

void SafeStrCat( char * dst, const char * src, size_t dst_size ) {
	if( dst_size == 0 )
		return;

	size_t dst_len = strlen( dst );
	if( dst_len >= dst_size - 1 )
		return;

	size_t to_copy = Min2( strlen( src ), dst_size - dst_len - 1 );
	memcpy( dst + dst_len, src, to_copy );
	dst[ dst_len + to_copy ] = '\0';
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

//=====================================================================
//
//  INFO STRINGS
//
//=====================================================================

static bool Info_ValidateValue( const char *value ) {
	Assert( value );

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
	Assert( key );

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

	Assert( info );

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

	Assert( Info_Validate( info ) );
	Assert( Info_ValidateKey( key ) );

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

	Assert( info && Info_Validate( info ) );
	Assert( key && Info_ValidateKey( key ) );

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
		Assert( false );
		return NULL;
	}
	strncpy( value[valueindex], start, len );
	value[valueindex][len] = 0;

	return value[valueindex];
}

void Info_RemoveKey( char *info, const char *key ) {
	Assert( info && Info_Validate( info ) );
	Assert( key && Info_ValidateKey( key ) );

	if( !Info_Validate( info ) || !Info_ValidateKey( key ) ) {
		return;
	}

	while( true ) {
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

	Assert( info && Info_Validate( info ) );
	Assert( key && Info_ValidateKey( key ) );
	Assert( value && Info_ValidateValue( value ) );

	if( !Info_Validate( info ) || !Info_ValidateKey( key ) || !Info_ValidateValue( value ) ) {
		return false;
	}

	Info_RemoveKey( info, key );

	snprintf( pair, sizeof( pair ), "\\%s\\%s", key, value );

	if( strlen( pair ) + strlen( info ) > MAX_INFO_STRING ) {
		return false;
	}

	SafeStrCat( info, pair, MAX_INFO_STRING );

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

void operator++( WeaponType & x, int ) {
	using T = typename std::underlying_type< WeaponType >::type;
	x = WeaponType( T( x ) + 1 );
}

void operator++( GadgetType & x, int ) {
	using T = typename std::underlying_type< GadgetType >::type;
	x = GadgetType( T( x ) + 1 );
}

void operator++( PerkType & x, int ) {
	using T = typename std::underlying_type< PerkType >::type;
	x = PerkType( T( x ) + 1 );
}

void operator|=( UserCommandButton & lhs, UserCommandButton rhs ) {
	lhs = UserCommandButton( lhs | rhs );
}
