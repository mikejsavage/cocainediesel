#pragma once

#include "qcommon/types.h"
#include "qcommon/array.h"

constexpr Span< const char > NullSpan( NULL, 0 );

inline Span< const char > PEGLiteral( Span< const char > str, const char * lit ) {
	// TODO: startswith? should move string parsing shit from gameshared to common
	size_t lit_len = strlen( lit );
	return str.n >= lit_len && memcmp( str.ptr, lit, lit_len ) == 0 ? str + lit_len : NullSpan;
}

inline Span< const char > PEGRange( Span< const char > str, char lo, char hi ) {
	return str.n > 0 && str[ 0 ] >= lo && str[ 0 ] <= hi ? str + 1 : NullSpan;
}

inline Span< const char > PEGSet( Span< const char > str, const char * set ) {
	if( str.n == 0 )
		return NullSpan;

	for( size_t i = 0; i < strlen( set ); i++ ) {
		if( str[ 0 ] == set[ i ] ) {
			return str + 1;
		}
	}

	return NullSpan;
}

inline Span< const char > PEGNotSet( Span< const char > str, const char * set ) {
	if( str.n == 0 )
		return NullSpan;

	for( size_t i = 0; i < strlen( set ); i++ ) {
		if( str[ 0 ] == set[ i ] ) {
			return NullSpan;
		}
	}

	return str + 1;
}

template< typename F >
Span< const char > PEGCapture( Span< const char > * capture, Span< const char > str, F f ) {
	Span< const char > res = f( str );
	if( res.ptr != NULL ) {
		*capture = Span< const char >( str.ptr, res.ptr - str.ptr );
	}
	return res;
}

template< typename F1, typename F2 >
Span< const char > PEGOr( Span< const char > str, F1 parser1, F2 parser2 ) {
	Span< const char > res1 = parser1( str );
	return res1.ptr == NULL ? parser2( str ) : res1;
}

template< typename F1, typename F2, typename F3 >
Span< const char > PEGOr( Span< const char > str, F1 parser1, F2 parser2, F3 parser3 ) {
	Span< const char > res1 = parser1( str );
	if( res1.ptr != NULL )
		return res1;

	Span< const char > res2 = parser2( str );
	if( res2.ptr != NULL )
		return res2;

	return parser3( str );
}

template< typename F >
Span< const char > PEGOptional( Span< const char > str, F parser ) {
	Span< const char > res = parser( str );
	return res.ptr == NULL ? str : res;
}

template< typename F >
Span< const char > PEGNOrMore( Span< const char > str, size_t n, F parser ) {
	size_t parsed = 0;

	while( true ) {
		Span< const char > next = parser( str );
		if( next.ptr == NULL ) {
			return parsed >= n ? str : NullSpan;
		}
		str = next;
		parsed++;
	}
}

template< typename T, typename F >
Span< const char > CaptureNOrMore( NonRAIIDynamicArray< T > * array, Span< const char > str, size_t n, F parser ) {
	Span< const char > res = PEGNOrMore( str, n, [ &array, &parser ]( Span< const char > str ) {
		T elem;
		Span< const char > res = parser( &elem, str );
		if( res.ptr != NULL ) {
			array->add( elem );
		}
		return res;
	} );

	if( res.ptr == NULL ) {
		array->clear();
	}

	return res;
}

template< typename T, typename F >
Span< const char > ParseUpToN( T * array, size_t * num_parsed, size_t n, Span< const char > str, F parser ) {
	*num_parsed = 0;

	while( true ) {
		T elem;
		Span< const char > res = parser( &elem, str );
		if( res.ptr == NULL )
			return str;

		if( *num_parsed == n )
			return NullSpan;

		array[ *num_parsed ] = elem;
		*num_parsed += 1;
		str = res;
	}
}
