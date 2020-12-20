#pragma once

#include <string.h>

#include "qcommon/types.h"
#include "qcommon/array.h"
#include "gg/ggformat.h"

template< size_t N >
class String {
	size_t len;
	char buf[ N ];

public:
	STATIC_ASSERT( N > 0 );

	String() {
		clear();
	}

	template< typename... Rest >
	String( const char * fmt, const Rest & ... rest ) {
		format( fmt, rest... );
	}

	void clear() {
		buf[ 0 ] = '\0';
		len = 0;
	}

	template< typename T >
	void operator+=( const T & x ) {
		append( "{}", x );
	}

	template< typename... Rest >
	void format( const char * fmt, const Rest & ... rest ) {
		size_t copied = ggformat( buf, N, fmt, rest... );
		len = Min2( copied, N - 1 );
	}

	template< typename... Rest >
	void append( const char * fmt, const Rest & ... rest ) {
		size_t copied = ggformat( buf + len, N - len, fmt, rest... );
		len += Min2( copied, N - len - 1 );
	}

	void append_raw( const char * str, size_t n ) {
		size_t to_copy = Min2( N - len - 1, n );
		memmove( buf + len, str, to_copy );
		len += to_copy;
		buf[ len ] = '\0';
	}

	void remove( size_t start, size_t n ) {
		if( start >= len )
			return;
		size_t to_remove = Min2( len - start, n );
		memmove( buf + start, buf + start + to_remove, len - to_remove );
		len -= to_remove;
		buf[ len ] = '\0';
	}

	void truncate( size_t n ) {
		if( n >= len )
			return;
		buf[ n ] = '\0';
		len = n;
	}

	char & operator[]( size_t i ) {
		assert( i < N );
		return buf[ i ];
	}

	const char & operator[]( size_t i ) const {
		assert( i < N );
		return buf[ i ];
	}

	const char * c_str() const { return buf; }
	size_t length() const { return len; }

	bool operator==( const char * rhs ) const { return strcmp( buf, rhs ) == 0; }
	bool operator!=( const char * rhs ) const { return !( *this == rhs ); }

	Span< const char > span() const { return Span< const char >( buf, len ); }
};

class DynamicString {
	DynamicArray< char > buf;

public:
	DynamicString( Allocator * a ) : buf( a ) { }

	template< typename... Rest >
	DynamicString( Allocator * a, const char * fmt, const Rest & ... rest ) : buf( a ) {
		format( fmt, rest... );
	}

	template< typename T >
	void operator+=( const T & x ) {
		append( "{}", x );
	}

	template< typename... Rest >
	void format( const char * fmt, const Rest & ... rest ) {
		size_t len = ggformat( NULL, 0, fmt, rest... );
		buf.resize( len + 1 );
		ggformat( &buf[ 0 ], len + 1, fmt, rest... );
	}

	template< typename... Rest >
	void append( const char * fmt, const Rest & ... rest ) {
		size_t len = ggformat( NULL, 0, fmt, rest... );
		size_t old_len = length();
		buf.resize( old_len + len + 1 );
		ggformat( &buf[ old_len ], len + 1, fmt, rest... );
	}

	void append_raw( const char * data, size_t n ) {
		size_t old_len = length();
		buf.extend( old_len == 0 ? n + 1 : n );
		memmove( &buf[ old_len ], data, n );
		buf[ length() ] = '\0';
	}

	const char * c_str() const {
		if( buf.size() == 0 )
			return "";
		return buf.ptr();
	}

	void truncate( size_t len ) {
		if( len < length() ) {
			buf.resize( len + 1 );
			buf[ len ] = '\0';
		}
	}

	char & operator[]( size_t i ) { return buf[ i ]; }
	const char & operator[]( size_t i ) const { return buf[ i ]; }

	size_t length() const { return buf.size() == 0 ? 0 : buf.size() - 1; }

	Span< const char > span() const { return buf.span(); }
};

template< size_t N >
void format( FormatBuffer * fb, const String< N > & buf, const FormatOpts & opts ) {
	format( fb, buf.c_str(), opts );
}

inline void format( FormatBuffer * fb, const DynamicString & str, const FormatOpts & opts ) {
	format( fb, str.c_str(), opts );
}
