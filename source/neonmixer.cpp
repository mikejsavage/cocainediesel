#include <stdio.h>
#include <stdint.h>

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/time.h"

#include "sokol/ggaudio.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.h"

static const s16 samples[ 4 ] = { S16_MIN, 0, -5, S16_MAX };
static constexpr int SAMPLE_RATE = 44100;

void ShowErrorMessage( const char * msg, const char * file, int line ) {
	printf( "%s (%s:%d)\n", msg, file, line );
}

#if ARCHITECTURE_X86

#include <immintrin.h>
#include <tmmintrin.h>

using float32x4_t = __m128;
using int16x4_t = __m128i;
using int32x4_t = __m128i;

static float32x4_t BroadcastFloatx4( float x ) { return _mm_set1_ps( x ); }
static float32x4_t SetFloatx4( float a, float b, float c, float d ) { return _mm_setr_ps( a, b, c, d ); }
static float32x4_t LoadFloatx4( const float * x ) { return _mm_load_ps( x ); }
static void StoreFloatx4( float * x, float32x4_t simd ) { _mm_store_ps( x, simd ); }

static float32x4_t AddFloatx4( float32x4_t lhs, float32x4_t rhs ) { return _mm_add_ps( lhs, rhs ); }
static float32x4_t SubFloatx4( float32x4_t lhs, float32x4_t rhs ) { return _mm_sub_ps( lhs, rhs ); }
static float32x4_t MulFloatx4( float32x4_t lhs, float32x4_t rhs ) { return _mm_mul_ps( lhs, rhs ); }

static float32x4_t UnzipOddFloat32x4( float32x4_t a, float32x4_t b ) { return _mm_blend_ps( a, b, 0b0101 ); }
static float32x4_t UnzipEvenFloat32x4( float32x4_t a, float32x4_t b ) { return _mm_blend_ps( a, b, 0b1010 ); }

static float32x4_t MaxFloat32x4( float32x4_t lhs, float32x4_t rhs ) { return _mm_max_ps( lhs, rhs ); }

static int16x4_t LoadInt16x4( const s16 * x ) { return _mm_loadl_epi64( ( const __m128i * ) x ); }
static int32x4_t Int16x4ToInt32x4( int16x4_t x ) { return _mm_cvtepi16_epi32( x ); }
static float32x4_t Int32x4ToFloatx4( int32x4_t x ) { return _mm_cvtepi32_ps( x ); }

static float DotFloat32x4( float32x4_t lhs, float32x4_t rhs ) {
	float32x4_t result = _mm_mul_ps( lhs, rhs );

	// horizontal add
	// result = [ r0, r1, r2, r3 ]

	result = _mm_add_ps( result, _mm_movehdup_ps( result ) );
	// result = [ r0 + r3, r1 + r3, r2 + r1, r3 + r1 ]

	result = _mm_add_ps( result, _mm_movehl_ps( result, result ) );
	// result = [ r0 + r3 + r2 + r1, ... ]

	return _mm_cvtss_f32( result );
}

#else

#include <arm_neon.h>

static float32x4_t BroadcastFloatx4( float x ) { return vmovq_n_f32( x ); }
static float32x4_t SetFloat4( float a, float b, float c, float d ) {
	float32x4_t ret = vmovq_n_f32( a );
	ret = vsetq_lane_f32( b, ret, 1 );
	ret = vsetq_lane_f32( c, ret, 2 );
	ret = vsetq_lane_f32( d, ret, 3 );
	return ret;
}
static float32x4_t LoadFloatx4( const float * x ) { return vld1q_f32( x ); }
static void StoreFloatx4( float * x, float32x4_t simd ) { vst1q_f32( x, simd ); }

static float32x4_t AddFloatx4( float32x4_t lhs, float32x4_t rhs ) { return vaddq_f32( lhs, rhs ); }
static float32x4_t SubFloatx4( float32x4_t lhs, float32x4_t rhs ) { return vsubq_f32( lhs, rhs ); }
static float32x4_t MulFloatx4( float32x4_t lhs, float32x4_t rhs ) { return vmulq_f32( lhs, rhs ); }

static float32x4_t UnzipOddFloat32x4( float32x4_t a, float32x4_t b ) { return vuzp1q_f32( a, b ); }
static float32x4_t UnzipEvenFloat32x4( float32x4_t a, float32x4_t b ) { return vuzp2q_f32( a, b ); }

static float32x4_t MaxFloat32x4( float32x4_t lhs, float32x4_t rhs ) { return vmaxq_f32( lhs, rhs ); }

static int16x4_t LoadInt16x4( const s16 * x ) { return vld1_s16( x ); }
static int32x4_t Int16x4ToInt32x4( int16x4_t x ) { return vmovl_s16( x ); }
static float32x4_t Int32x4ToFloatx4( int32x4_t x ) { return vcvtq_f32_s32( x ); }

static float DotFloat32x4( float32x4_t lhs, float32x4_t rhs ) { return vaddvq_f32( vmulq_f32( lhs, rhs ) ); }

#endif

// precompute a piecewise linear approximation to the left side of
// https://en.wikipedia.org/wiki/Bicubic_interpolation#Bicubic_convolution_algorithm
// i.e the `0.5 * [ 1 t t^2 t^3 ] M` part with t in [0, 1)
struct CubicCoefficients {
	static constexpr size_t NUM_CUBIC_COEFFICIENTS = 16;
	float32x4_t coeffs[ NUM_CUBIC_COEFFICIENTS ];
	float32x4_t deltas[ NUM_CUBIC_COEFFICIENTS ]; // coeffs[ i ] + deltas[ i ] == coeffs[ i + 1 ]
};

static CubicCoefficients InitCubicCoefficients() {
	CubicCoefficients r;

	for( size_t i = 0; i < ARRAY_COUNT( r.coeffs ); i++ ) {
		float t = float( i ) / float( ARRAY_COUNT( r.coeffs ) );
		float t2 = t * t;
		float t3 = t * t * t;

		alignas( 16 ) Vec4 coeff = 0.5f * Vec4(
			0 - 1*t + 2*t2 - 1*t3,
			2 + 0*t - 5*t2 + 3*t3,
			0 + 1*t + 4*t2 - 3*t3,
			0 + 0*t - 1*t2 + 1*t3
		);
		r.coeffs[ i ] = LoadFloatx4( coeff.ptr() );
	}

	for( size_t i = 0; i < ARRAY_COUNT( r.coeffs ); i++ ) {
		float32x4_t next = i == ARRAY_COUNT( r.coeffs ) - 1 ? r.coeffs[ 0 ] : r.coeffs[ i + 1 ];
		r.deltas[ i ] = SubFloatx4( next, r.coeffs[ i ] );
	}

	return r;
}

static float32x4_t SampleCubicCoefficients( const CubicCoefficients & cubic_coefficients, float t ) {
	size_t idx = t * float( ARRAY_COUNT( cubic_coefficients.coeffs ) );
	float remainder = t * float( ARRAY_COUNT( cubic_coefficients.coeffs ) ) - idx;
	return AddFloatx4( cubic_coefficients.coeffs[ idx ], MulFloatx4( cubic_coefficients.deltas[ idx ], BroadcastFloatx4( remainder ) ) );
}

static float32x4_t Dequantize( int32x4_t samples ) {
	// maybe it's faster to do the 1 / S16_MAX once post-mixing
	return MaxFloat32x4( MulFloatx4( Int32x4ToFloatx4( samples ), BroadcastFloatx4( 1.0f / S16_MAX ) ), BroadcastFloatx4( -1.0f ) );
}

static float32x4_t LoadAndDequantize( const s16 * samples ) {
	return Dequantize( Int16x4ToInt32x4( LoadInt16x4( samples ) ) );
}

static float ResampleMono( const CubicCoefficients & cubic_coefficients, const s16 * samples, float t ) {
	return DotFloat32x4( LoadAndDequantize( samples ), SampleCubicCoefficients( cubic_coefficients, t ) );
}

#if ARCHITECTURE_X86
static Vec2 ResampleStereo( const CubicCoefficients & cubic_coefficients, const s16 * samples, float t ) {
	float32x4_t coeffs = SampleCubicCoefficients( cubic_coefficients, t );

	__m128i interleaved = _mm_load_si128( ( const __m128i * ) samples );
	__m128i left_s32 = _mm_srai_epi32( interleaved, 16 );
	__m128i right_s32 = _mm_madd_epi16( interleaved, _mm_set1_epi32( 1 ) );

	return Vec2( DotFloat32x4( Dequantize( left_s32 ), coeffs ), DotFloat32x4( Dequantize( right_s32 ), coeffs ) );
}
#else
static Vec2 ResampleStereo( const CubicCoefficients & cubic_coefficients, const s16 * samples, float t ) {
	float32x4_t coeffs = SampleCubicCoefficients( cubic_coefficients, t );

	float32x4_t simd_samples0 = LoadAndDequantize( samples );
	float32x4_t simd_samples1 = LoadAndDequantize( samples + 4 );

	float32x4_t left = UnzipOddFloat32x4( simd_samples0, simd_samples1 );
	float32x4_t right = UnzipEvenFloat32x4( simd_samples0, simd_samples1 );

	return Vec2( DotFloat32x4( left, coeffs ), DotFloat32x4( right, coeffs ) );
}
#endif

struct Sound {
	Span< s16 > samples;
	int sample_rate;
	bool mono;
};

struct MixContext {
	CubicCoefficients cubic_coefficients;
	ArenaAllocator arena;
	Time time;
	Sound longcovid;
};

static u64 ScaleFixed( u64 x, u64 numer, u64 denom ) {
	u64 a = ( x / denom ) * numer;
	u64 b = ( ( x % denom ) * numer ) / denom;
	return a + b;
}

static void MixStereo( float * buffer, size_t num_frames, const CubicCoefficients & cubic_coefficients, Time t_0, Time dt, Sound sound ) {
	// if( sound.sample_rate == SAMPLE_RATE ) {
	// 	size_t idx = ScaleFixed( t_0.flicks, sound.sample_rate, GGTIME_FLICKS_PER_SECOND );
	// 	memcpy( buffer, ( sound.samples + idx * 2 ).ptr, num_frames * sizeof( float ) * 2 );
	// 	return;
	// }

	for( size_t i = 0; i < num_frames; i++ ) {
		Time t = t_0 + u64( i ) * dt;
		size_t idx = ScaleFixed( t.flicks, sound.sample_rate, GGTIME_FLICKS_PER_SECOND );
		float remainder = ToSeconds( t - Time { idx * ( GGTIME_FLICKS_PER_SECOND / sound.sample_rate ) } ) * sound.sample_rate;

		Vec2 sample = ResampleStereo( cubic_coefficients, ( sound.samples + idx * 2 ).ptr, remainder );
		buffer[ i * 2 + 0 ] = sample.x;
		buffer[ i * 2 + 1 ] = sample.y;
	}
}

static void GenerateSamples( float * buffer, size_t num_frames, int num_channels, void * userdata ) {
	MixContext * ctx = ( MixContext * ) userdata;

	memset( buffer, 0, num_frames * sizeof( float ) * 2 );

	Time dt_44100_hz = Hz( SAMPLE_RATE );
	MixStereo( buffer, num_frames, ctx->cubic_coefficients, ctx->time, dt_44100_hz, ctx->longcovid );
	ctx->time += u64( num_frames ) * dt_44100_hz;
}

static void Print( const char * name, float32x4_t simd ) {
	float xs[ 4 ];
	StoreFloatx4( xs, simd );
	printf( "%s = [%f, %f, %f, %f]\n", name, xs[ 0 ], xs[ 1 ], xs[ 2 ], xs[ 3 ] );
}

static Sound LoadSound( const char * path ) {
	int channels;
	int sample_rate;
	s16 * samples;
	int num_samples = stb_vorbis_decode_filename( path, &channels, &sample_rate, &samples );
	if( num_samples == -1 ) {
		abort();
	}
	return Sound {
		.samples = Span< s16 >( samples, num_samples ),
		.sample_rate = sample_rate,
		.mono = channels == 1,
	};
}

int main() {
	CubicCoefficients cubic_coefficients = InitCubicCoefficients();
	printf( "%f\n", ResampleMono( cubic_coefficients, samples, 0.5f ) );

	float32x4_t a = SetFloatx4( 1, 2, 3, 4 );
	float32x4_t b = SetFloatx4( 5, 6, 7, 8 );
	float32x4_t left = UnzipOddFloat32x4( a, b );
	float32x4_t right = UnzipEvenFloat32x4( a, b );
	Print( "a", a );
	Print( "b", b );
	Print( "left", left );
	Print( "right", right );

	MixContext mix_context;
	mix_context.cubic_coefficients = cubic_coefficients;
	constexpr size_t AUDIO_ARENA_SIZE = 1024 * 64; // 64KB
	void * mixer_arena_memory = sys_allocator->allocate( AUDIO_ARENA_SIZE, 16 );
	mix_context.arena = ArenaAllocator( mixer_arena_memory, AUDIO_ARENA_SIZE );
	mix_context.time = Seconds( 50 );
	mix_context.longcovid = LoadSound( "base/sounds/music/longcovid.ogg" );

	saudio_desc sokol = {
		.sample_rate = SAMPLE_RATE,
		.num_channels = 2,
		.buffer_frames = int( SAMPLE_RATE * 0.02f ), // 20ms
		.callback = GenerateSamples,
		.user_data = &mix_context,
	};
	if( !saudio_setup( sokol ) )
		return 1;

	while( true ) {
		Sys_Sleep( 50 );
	}

	return 0;
}
