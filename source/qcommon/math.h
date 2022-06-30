// include base.h instead of this

#if COMPILER_CLANG
#define MATH_DECLARATION_NOTHROW throw()
#else
#define MATH_DECLARATION_NOTHROW
#endif

// <math.h> is huge so manually declare the stuff we actually use
extern "C" float cosf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float sinf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float tanf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float acosf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float asinf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float atanf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float atan2f( float, float ) MATH_DECLARATION_NOTHROW;

extern "C" float sqrtf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float cbrtf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float powf( float, float ) MATH_DECLARATION_NOTHROW;
extern "C" float expf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float logf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float fmodf( float, float ) MATH_DECLARATION_NOTHROW;
extern "C" double fmod( double, double ) MATH_DECLARATION_NOTHROW;
extern "C" float floorf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float ceilf( float ) MATH_DECLARATION_NOTHROW;
extern "C" float roundf( float ) MATH_DECLARATION_NOTHROW;

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
void sinf( double ) = delete;
void cosf( double ) = delete;
void tanf( double ) = delete;
void asinf( double ) = delete;
void acosf( double ) = delete;
void atanf( double ) = delete;
void atan2f( double, double ) = delete;
void sqrtf( double ) = delete;
