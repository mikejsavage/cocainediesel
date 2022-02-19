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
extern "C" float logf( float );
extern "C" float fmodf( float, float );
extern "C" double fmod( double, double );
extern "C" float floorf( float );
extern "C" float ceilf( float );
extern "C" float roundf( float );

constexpr float PI = 3.14159265358979323846f;

inline constexpr float Radians( float d ) { return d * PI / 180.0f; }
inline constexpr float Degrees( float r ) { return r * 180.0f / PI; }

template< typename T >
T Abs( const T & x ) {
	return x >= 0 ? x : -x;
}

inline float Square( float x ) {
	return x * x;
}

inline float Cube( float x ) {
	return x * x * x;
}

template< typename T >
T Lerp( T a, float t, T b ) {
	return a * ( 1.0f - t ) + b * t;
}

template< typename T >
float Unlerp( T lo, T x, T hi ) {
	return float( x - lo ) / float( hi - lo );
}

template< typename T >
float Unlerp01( T lo, T x, T hi ) {
	return Clamp01( Unlerp( lo, x, hi ) );
}

// some stubs to catch accidental double usage
void sinf( double );
void cosf( double );
void tanf( double );
void asinf( double );
void acosf( double );
void atanf( double );
void atan2f( double );
void sqrtf( double );
