#pragma once

#include <vector>

#include "qcommon/types.h"
#include "qcommon/array.h"

#include "gameshared/q_shared.h"

template< typename T, size_t N >
struct StaticArray {
	T elems[ N ];
	size_t n;

	bool full() const {
		return n == N;
	}

	void add( const T & x ) {
		if( full() ) {
			Fatal( "StaticArray::add" );
		}
		elems[ n ] = x;
		n++;
	}

	Span< T > span() {
		return Span< T >( elems, n );
	}

	Span< const T > span() const {
		return Span< const T >( elems, n );
	}
};

struct ParsedKeyValue {
	Span< const char > key;
	Span< const char > value;
};

struct ParsedBrushFace {
	Vec3 plane[ 3 ];
	Span< const char > material;
	u64 material_hash;
	Mat3 texcoords_transform;
};

struct ParsedBrush {
	int id;
	int line_number;

	StaticArray< ParsedBrushFace, 32 > faces;
};

struct ParsedControlPoint {
	Vec3 position;
	Vec2 uv;
};

struct ParsedPatch {
	int id;
	int line_number;

	Span< const char > material;
	u64 material_hash;
	int w, h;
	ParsedControlPoint control_points[ 1024 ];
};

struct ParsedEntity {
	int id;

	StaticArray< ParsedKeyValue, 16 > kvs;
	std::vector< ParsedBrush > brushes;
	std::vector< ParsedPatch > patches;
	u32 model_id;
};

std::vector< ParsedEntity > ParseEntities( Span< char > map );

// helpers below

constexpr Span< const char > NullSpan( NULL, 0 );

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
Span< const char > CaptureNOrMore( std::vector< T > * array, Span< const char > str, size_t n, F parser ) {
	Span< const char > res = PEGNOrMore( str, n, [ &array, &parser ]( Span< const char > str ) {
		T elem;
		Span< const char > res = parser( &elem, str );
		if( res.ptr != NULL ) {
			array->push_back( elem );
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

template< typename T, size_t N, typename F >
Span< const char > ParseUpToN( StaticArray< T, N > * array, Span< const char > str, F parser ) {
	return ParseUpToN( array->elems, &array->n, N, str, parser );
}

constexpr const char * whitespace_chars = " \r\n\t";

inline Span< const char > SkipWhitespace( Span< const char > str ) {
	return PEGNOrMore( str, 0, []( Span< const char > str ) {
		return PEGSet( str, whitespace_chars );
	} );
}

inline Span< const char > SkipLiteral( Span< const char > str, const char * lit ) {
	// TODO: startswith? should move string parsing shit from gameshared to common
	size_t lit_len = strlen( lit );
	return str.n >= lit_len && memcmp( str.ptr, lit, lit_len ) == 0 ? str + lit_len : NullSpan;
}

static Span< const char > SkipToken( Span< const char > str, const char * token ) {
	str = SkipWhitespace( str );
	str = SkipLiteral( str, token );
	return str;
}

inline Span< const char > ParseWord( Span< const char > * capture, Span< const char > str ) {
	str = SkipWhitespace( str );
	return PEGCapture( capture, str, []( Span< const char > str ) {
		return PEGNOrMore( str, 1, []( Span< const char > str ) {
			return PEGNotSet( str, whitespace_chars );
		} );
	} );
}
