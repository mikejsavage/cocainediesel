// this is a hack, see linear_algebra.h for the explanation

#ifndef KERNEL_MAYBE_INLINE
#define KERNEL_MAYBE_INLINE
#endif

#if !PUBLIC_BUILD

#include "qcommon/types.h"

#if ARCHITECTURE_X64

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

#else // #if ARCHITECTURE_X64

#include <arm_neon.h>

KERNEL_MAYBE_INLINE Mat4 operator*( const Mat4 & lhs, const Mat4 & rhs ) {
	Mat4 result;

	float32x4_t row1 = vld1q_f32( &lhs.col0.x );
	float32x4_t row2 = vld1q_f32( &lhs.col1.x );
	float32x4_t row3 = vld1q_f32( &lhs.col2.x );
	float32x4_t row4 = vld1q_f32( &lhs.col3.x );

	for( size_t i = 0; i < 4; i++ ) {
		float32x4_t brod1 = vmovq_n_f32( ( &rhs.col0.x )[ i * 4 + 0 ] );
		float32x4_t brod2 = vmovq_n_f32( ( &rhs.col0.x )[ i * 4 + 1 ] );
		float32x4_t brod3 = vmovq_n_f32( ( &rhs.col0.x )[ i * 4 + 2 ] );
		float32x4_t brod4 = vmovq_n_f32( ( &rhs.col0.x )[ i * 4 + 3 ] );

		// add( fma( mul ), fma( mul ) ) is higher throughput than than fma( fma( fma( mul ) ) ) on M2
		// unexpected but I didn't investigate
		float32x4_t row = vaddq_f32(
			vfmaq_f32( vmulq_f32( brod1, row1 ), brod2, row2 ),
			vfmaq_f32( vmulq_f32( brod3, row3 ), brod4, row4 )
		);

		vst1q_f32( &( &result.col0.x )[ i * 4 ], row );
	}

	return result;
}

#endif // #if !ARCHITECTURE_X64

#endif // #if !PUBLIC_BUILD
