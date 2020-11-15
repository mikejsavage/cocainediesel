#pragma once

#include "qcommon/types.h"

#define ALLOC_SPAN3D( a, T, w, h, l ) Span3D< T >( ALLOC_MANY( a, T, w * h * l ), w, h, l )

template< typename T >
struct Span3D {
	T * ptr;
	size_t w, h, l, row_stride, layer_stride;

	constexpr Span3D() : ptr( NULL ), w( 0 ), h( 0 ), l( 0 ), row_stride( 0 ), layer_stride( 0 ) { }
	constexpr Span3D( T * ptr_, size_t w_, size_t h_, size_t l_ ) : Span3D( ptr_, w_, h_, l_, w_, w_ * h_ ) { }
	constexpr Span3D( T * ptr_, size_t w_, size_t h_, size_t l_, size_t row_stride_, size_t layer_stride_ ) : ptr( ptr_ ), w( w_ ), h( h_ ), l( l_ ), row_stride( row_stride_ ), layer_stride( layer_stride_ ) { }

	size_t num_bytes() const { return sizeof( T ) * w * h * l; }

	bool in_range( size_t x, size_t y, size_t z ) const {
		return x < w && y < h && z < l;
	}

	T & operator()( size_t x, size_t y, size_t z ) {
		assert( in_range( x, y, z ) );
		return ptr[ z * layer_stride + y * row_stride + x ];
	}

	const T & operator()( size_t x, size_t y, size_t z ) const {
		assert( in_range( x, y, z ) );
		return ptr[ z * layer_stride + y * row_stride + x ];
	}
};
