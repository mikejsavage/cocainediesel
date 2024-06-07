// this is a hack, see linear_algebra.h for the explanation

#ifndef KERNEL_MAYBE_INLINE
#define KERNEL_MAYBE_INLINE
#endif

#if !PUBLIC_BUILD

#include "qcommon/types.h"

#include <emmintrin.h>

// https://stackoverflow.com/a/18508113
// need to swap the args because their implementation is row major
KERNEL_MAYBE_INLINE Mat4 operator*( const Mat4 & lhs, const Mat4 & rhs ) {
	Mat4 result;

	__m128 row1 = _mm_load_ps( &lhs.col0.x );
	__m128 row2 = _mm_load_ps( &lhs.col1.x );
	__m128 row3 = _mm_load_ps( &lhs.col2.x );
	__m128 row4 = _mm_load_ps( &lhs.col3.x );

	for( size_t i = 0; i < 4; i++ ) {
		__m128 brod1 = _mm_set1_ps( ( &rhs.col0.x )[ i * 4 + 0 ] );
		__m128 brod2 = _mm_set1_ps( ( &rhs.col0.x )[ i * 4 + 1 ] );
		__m128 brod3 = _mm_set1_ps( ( &rhs.col0.x )[ i * 4 + 2 ] );
		__m128 brod4 = _mm_set1_ps( ( &rhs.col0.x )[ i * 4 + 3 ] );
		__m128 row = _mm_add_ps(
			_mm_add_ps( _mm_mul_ps( brod1, row1 ), _mm_mul_ps( brod2, row2 ) ),
			_mm_add_ps( _mm_mul_ps( brod3, row3 ), _mm_mul_ps( brod4, row4 ) )
		);

		_mm_store_ps( &( &result.col0.x )[ i * 4 ], row );
	}

	return result;
}

#endif
