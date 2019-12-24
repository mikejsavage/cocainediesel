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

#define DEG2RAD( a ) ( ( ( a ) * PI ) / 180.0f )
#define RAD2DEG( a ) ( ( ( a ) * 180.0f ) / PI )

template< typename T >
T Abs( const T & x ) { return x >= 0 ? x : -x; }

// some stubs to catch accidental double usage
void sinf( double x );
void cosf( double x );
void tanf( double x );
void asinf( double x );
void acosf( double x );
void atanf( double x );
void atan2f( double x );
void sqrtf( double x );
