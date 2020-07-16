#pragma once

#include "qcommon/types.h"

#define ALLOC_SPAN2D( a, T, w, h ) Span2D< T >( ALLOC_MANY( a, T, w * h ), w, h )

template< typename T >
struct Span2D {
	T * ptr;
	size_t w, h, row_stride;

	constexpr Span2D() : ptr( NULL ), w( 0 ), h( 0 ), row_stride( 0 ) { }
	constexpr Span2D( T * ptr_, size_t w_, size_t h_ ) : Span2D( ptr_, w_, h_, w_ ) { }
	constexpr Span2D( T * ptr_, size_t w_, size_t h_, size_t row_stride_ ) : ptr( ptr_ ), w( w_ ), h( h_ ), row_stride( row_stride_ ) { }

	size_t num_bytes() const { return sizeof( T ) * w * h; }

	bool in_range( size_t x, size_t y ) const {
		return x < w && y < h;
	}

	T & operator()( size_t x, size_t y ) {
		assert( in_range( x, y ) );
		return ptr[ y * row_stride + x ];
	}

	const T & operator()( size_t x, size_t y ) const {
		assert( in_range( x, y ) );
		return ptr[ y * row_stride + x ];
	}
};
