// this is a hack, see linear_algebra.h for the explanation

#ifndef INLINE_IN_RELEASE_BUILDS
#define INLINE_IN_RELEASE_BUILDS
#endif

#if !PUBLIC_BUILD

#include "qcommon/types.h"

#if ARCHITECTURE_X64

#include <emmintrin.h>

// https://stackoverflow.com/a/18508113
// need to swap the args because their implementation is row major
INLINE_IN_RELEASE_BUILDS Mat4 operator*( const Mat4 & lhs, const Mat4 & rhs ) {
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

INLINE_IN_RELEASE_BUILDS Vec4 operator*( const Mat3x4 & m, const Vec4 & v ) {
	float w = v.w;

	__m128 vx = _mm_set1_ps( v.x );
	__m128 vy = _mm_set1_ps( v.y );
	__m128 vz = _mm_set1_ps( v.z );
	__m128 vw = _mm_set1_ps( v.w );

	__m128 col0 = _mm_load_ps( &m.col0.x );
	__m128 col1 = _mm_loadu_ps( &m.col1.x );
	__m128 col2 = _mm_loadu_ps( &m.col2.x );

	// if we load col3.x we run past the end of the struct, so load col3.x - 1 and shuffle
	__m128 col3 = _mm_loadu_ps( &m.col2.z );
	col3 = _mm_shuffle_ps( col3, col3, _MM_SHUFFLE( 0, 3, 2, 1 ) );

	__m128 res128 = _mm_add_ps(
		_mm_add_ps( _mm_mul_ps( col0, vx ), _mm_mul_ps( col1, vy ) ),
		_mm_add_ps( _mm_mul_ps( col2, vz ), _mm_mul_ps( col3, vw ) )
	);

	Vec4 res;
	_mm_store_ps( &res.x, res128 );
	res.w = w;

	return res;
}

#else // #if ARCHITECTURE_X64

#include <arm_neon.h>

INLINE_IN_RELEASE_BUILDS Mat4 operator*( const Mat4 & lhs, const Mat4 & rhs ) {
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

INLINE_IN_RELEASE_BUILDS Vec4 operator*( const Mat3x4 & m, const Vec4 & v ) {
    float w = v.w;

    float32x4_t vx = vdupq_n_f32( v.x );
    float32x4_t vy = vdupq_n_f32( v.y );
    float32x4_t vz = vdupq_n_f32( v.z );
    float32x4_t vw = vdupq_n_f32( v.w );

    float32x4_t col0 = vld1q_f32( &m.col0.x );
    float32x4_t col1 = vld1q_f32( &m.col1.x );
    float32x4_t col2 = vld1q_f32( &m.col2.x );

    float32x2_t col3xy = vld1_f32( &m.col3.x );
    float32x2_t col3z0 = vld1_lane_f32( &m.col3.z, vdup_n_f32( 0.0f ), 0 );
    float32x4_t col3 = vcombine_f32( col3xy, col3z0 );

    float32x4_t res128 = vaddq_f32(
        vfmaq_f32( vmulq_f32( col0, vx ), col1, vy ),
        vfmaq_f32( vmulq_f32( col2, vz ), col3, vw )
    );

    Vec4 res;
    vst1q_f32( &res.x, res128 );
    res.w = w;

    return res;
}

#endif // #if !ARCHITECTURE_X64

#endif // #if !PUBLIC_BUILD
