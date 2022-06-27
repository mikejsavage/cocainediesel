// include base.h instead of this

// <math.h> is huge so manually declare the stuff we actually use
extern "C" float cosf( float ) throw();
extern "C" float sinf( float ) throw();
extern "C" float tanf( float ) throw();
extern "C" float acosf( float ) throw();
extern "C" float asinf( float ) throw();
extern "C" float atanf( float ) throw();
extern "C" float atan2f( float, float ) throw();

extern "C" float sqrtf( float ) throw();
extern "C" float cbrtf( float ) throw();
extern "C" float powf( float, float ) throw();
extern "C" float expf( float ) throw();
extern "C" float logf( float ) throw();
extern "C" float fmodf( float, float ) throw();
extern "C" double fmod( double, double ) throw();
extern "C" float floorf( float ) throw();
extern "C" float ceilf( float ) throw();
extern "C" float roundf( float ) throw();

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
