// include base.h instead of this

// <math.h> is huge so manually declare the stuff we actually use
extern "C" float cosf( float );
extern "C" float sinf( float );
extern "C" float tanf( float );
extern "C" float acosf( float );
extern "C" float asinf( float );
extern "C" float atanf( float );
extern "C" float atan2f( float, float );

extern "C" float sqrtf( float );
extern "C" float cbrtf( float );
extern "C" float powf( float, float );
extern "C" float expf( float );
extern "C" float logf( float );
extern "C" float fmodf( float, float );
extern "C" double fmod( double, double );
extern "C" float floorf( float );
extern "C" float ceilf( float );
extern "C" float roundf( float );

constexpr float PI = 3.14159265358979323846f;

constexpr float Radians( float d ) { return d * PI / 180.0f; }
constexpr float Degrees( float r ) { return r * 180.0f / PI; }

template< typename T >
constexpr T Abs( const T & x ) {
	return x >= 0 ? x : -x;
}

constexpr float Square( float x ) {
	return x * x;
}

constexpr float Cube( float x ) {
	return x * x * x;
}

template< typename T >
constexpr T Lerp( T a, float t, T b ) {
	return a * ( 1.0f - t ) + b * t;
}

template< typename T >
constexpr float Unlerp( T lo, T x, T hi ) {
	return float( x - lo ) / float( hi - lo );
}

template< typename T >
constexpr float Unlerp01( T lo, T x, T hi ) {
	return Clamp01( Unlerp( lo, x, hi ) );
}

// some stubs to catch accidental double usage
void sinf( double ) = delete;
void cosf( double ) = delete;
void tanf( double ) = delete;
void asinf( double ) = delete;
void acosf( double ) = delete;
void atanf( double ) = delete;
void atan2f( double, double ) = delete;
void sqrtf( double ) = delete;
