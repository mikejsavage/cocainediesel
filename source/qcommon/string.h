#pragma once

#include "qcommon/base.h"
#include "qcommon/array.h"

template< size_t N >
class String {
	size_t length;
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

	operator const char *() const {
		return buf;
	}

	void clear() {
		buf[ 0 ] = '\0';
		length = 0;
	}

	template< typename T >
	void operator+=( const T & x ) {
		append( "{}", x );
	}

	template< typename... Rest >
	void format( const char * fmt, const Rest & ... rest ) {
		size_t copied = ggformat( buf, N, fmt, rest... );
		length = Min2( copied, N - 1 );
	}

	template< typename... Rest >
	void append( const char * fmt, const Rest & ... rest ) {
		size_t copied = ggformat( buf + length, N - length, fmt, rest... );
		length += Min2( copied, N - length - 1 );
	}

	void append_raw( const char * str, size_t len ) {
		size_t to_copy = Min2( N - length - 1, len );
		memmove( buf + length, str, to_copy );
		length += to_copy;
		buf[ length ] = '\0';
	}

	void remove( size_t start, size_t len ) {
		if( start >= length )
			return;
		size_t to_remove = Min2( length - start, len );
		memmove( buf + start, buf + start + to_remove, length - to_remove );
		length -= to_remove;
		buf[ length ] = '\0';
	}

	void truncate( size_t n ) {
		if( n >= length )
			return;
		buf[ n ] = '\0';
		length = n;
	}

	char & operator[]( size_t i ) {
		assert( i < N );
		return buf[ i ];
	}

	const char & operator[]( size_t i ) const {
		assert( i < N );
		return buf[ i ];
	}

	const char * c_str() const {
		return buf;
	}

	size_t len() const {
		return length;
	}

	bool operator==( const char * rhs ) const {
		return strcmp( buf, rhs ) == 0;
	}

	bool operator!=( const char * rhs ) const {
		return !( *this == rhs );
	}
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

	size_t length() const {
		return buf.size() == 0 ? 0 : buf.size() - 1;
	}

	char * begin() { return buf.begin(); }
	char * end() { return buf.end(); }
	const char * begin() const { return buf.begin(); }
	const char * end() const { return buf.end(); }
};

template< size_t N >
void format( FormatBuffer * fb, const String< N > & buf, const FormatOpts & opts ) {
	format( fb, buf.c_str(), opts );
}

inline void format( FormatBuffer * fb, const DynamicString & str, const FormatOpts & opts ) {
	format( fb, str.c_str(), opts );
}
