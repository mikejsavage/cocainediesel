#pragma once

#include <stdint.h>
#include <stddef.h>
#include <float.h>

#include "qcommon/platform.h"
#include "qcommon/source_location.h"
#include "qcommon/initializer_list.h"

/*
 * ints
 */

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

constexpr s8 S8_MIN = INT8_MIN;
constexpr s16 S16_MIN = INT16_MIN;
constexpr s32 S32_MIN = INT32_MIN;
constexpr s64 S64_MIN = INT64_MIN;
constexpr s8 S8_MAX = INT8_MAX;
constexpr s16 S16_MAX = INT16_MAX;
constexpr s32 S32_MAX = INT32_MAX;
constexpr s64 S64_MAX = INT64_MAX;

constexpr u8 U8_MAX = UINT8_MAX;
constexpr u16 U16_MAX = UINT16_MAX;
constexpr u32 U32_MAX = UINT32_MAX;
constexpr u64 U64_MAX = UINT64_MAX;

template< typename T > constexpr T MinInt;
template< typename T > constexpr T MaxInt;

template<> inline constexpr s8  MinInt< s8  > = S8_MIN;
template<> inline constexpr s16 MinInt< s16 > = S16_MIN;
template<> inline constexpr s32 MinInt< s32 > = S32_MIN;
template<> inline constexpr s64 MinInt< s64 > = S64_MIN;
template<> inline constexpr s8  MaxInt< s8  > = S8_MAX;
template<> inline constexpr s16 MaxInt< s16 > = S16_MAX;
template<> inline constexpr s32 MaxInt< s32 > = S32_MAX;
template<> inline constexpr s64 MaxInt< s64 > = S64_MAX;

template<> inline constexpr u8  MaxInt< u8  > = U8_MAX;
template<> inline constexpr u16 MaxInt< u16 > = U16_MAX;
template<> inline constexpr u32 MaxInt< u32 > = U32_MAX;
template<> inline constexpr u64 MaxInt< u64 > = U64_MAX;

template< typename T > constexpr bool IsSigned() { return T( -1 ) < T( 0 ); }

inline void integer_constant_too_big() { }

consteval u16 operator""_u16( unsigned long long value ) {
	if( value > U16_MAX ) {
		integer_constant_too_big();
	}
	return value;
}

consteval u32 operator""_u32( unsigned long long value ) {
	if( value > U32_MAX ) {
		integer_constant_too_big();
	}
	return value;
}

consteval u64 operator""_u64( unsigned long long value ) {
	if( value > U64_MAX ) {
		integer_constant_too_big();
	}
	return value;
}

/*
 * helper functions that are useful in templates. so headers don't need to include base.h
 */

#define STRINGIFY_HELPER( a ) #a
#define IFDEF( x ) ( STRINGIFY_HELPER( x )[ 0 ] == '1' && STRINGIFY_HELPER( x )[ 1 ] == '\0' )

constexpr bool is_public_build = IFDEF( PUBLIC_BUILD );

void AssertFail( const char * str, const char * file, int line );
#define Assert( p ) \
	do { \
		if constexpr ( is_public_build ) { \
			( void ) sizeof( p ); \
		} \
		else { \
			if( !( p ) ) AssertFail( #p, __FILE__, __LINE__ ); \
		} \
	} while( false )

template< typename T, size_t N >
constexpr size_t ARRAY_COUNT( const T ( &arr )[ N ] ) {
	return N;
}

template< typename T, typename M, size_t N >
constexpr size_t ARRAY_COUNT( M ( T::* )[ N ] ) {
	return N;
}

#define STATIC_ASSERT( p ) static_assert( p, #p )
#define NONCOPYABLE( T ) T( const T & ) = delete; void operator=( const T & ) = delete

template< typename To, typename From >
To checked_cast( const From & from ) {
	To result = To( from );
	Assert( From( result ) == from );
	return result;
}

#if COMPILER_MSVC
template< typename To, typename From >
To * align_cast( From * from ) {
	Assert( uintptr_t( from ) % alignof( To ) == 0 );
	return reinterpret_cast< To * >( from );
}
#else
template< typename To, typename From >
To * align_cast( From * from ) {
	Assert( uintptr_t( from ) % alignof( To ) == 0 );
	// error if we cast away const, __builtin_assume_aligned returns void *
	( void ) reinterpret_cast< To * >( from );
	return ( To * ) __builtin_assume_aligned( from, alignof( To ) );
}
#endif

constexpr size_t AlignPow2( size_t x, size_t alignment ) {
	return ( x + alignment - 1 ) & ~( alignment - 1 );
}

template< typename T >
constexpr bool IsPowerOf2( T x ) {
	return ( x & ( x - 1 ) ) == 0;
}

template< typename T >
void Swap2( T * a, T * b ) {
	T t = *a;
	*a = *b;
	*b = t;
}

template< typename T >
constexpr T Min2( const T & a, const T & b ) {
	return a < b ? a : b;
}

template< typename T >
constexpr T Max2( const T & a, const T & b ) {
	return a > b ? a : b;
}

template< typename T >
constexpr T Clamp( const T & lo, const T & x, const T & hi ) {
	Assert( lo <= hi );
	return Max2( lo, Min2( x, hi ) );
}

template< typename T >
constexpr T Clamp01( const T & x ) {
	return Clamp( T( 0.0f ), x, T( 1.0f ) );
}

template< typename T, typename S > constexpr bool SameType = false;
template< typename T > constexpr bool SameType< T, T > = true;

/*
 * Span
 */

template< typename T >
struct Span {
	T * ptr;
	size_t n;

	constexpr Span() : ptr( NULL ), n( 0 ) { }
	constexpr Span( T * ptr_, size_t n_ ) : ptr( ptr_ ), n( n_ ) { }
	constexpr Span( std::initializer_list< T > && elems ) : ptr( elems.begin() ), n( elems.size() ) { }

	/*
	 * require S is explicitly const char so this doesn't compile:
	 *
	 * char buf[ 128 ];
	 * sprintf( buf, "hello" );
	 * ThingThatTakesSpan( buf );
	 */
	template< size_t N, typename S >
	constexpr Span( S ( &str )[ N ] ) requires( SameType< T, const char > && SameType< S, const char > ) : ptr( str ), n( N - 1 ) { }

	// allow implicit conversion to Span< const T >
	operator Span< const T >() const { return Span< const T >( ptr, n ); }

	size_t num_bytes() const { return sizeof( T ) * n; }

	T & operator[]( size_t i ) const {
		Assert( i < n );
		return ptr[ i ];
	}

	Span< T > operator+( size_t i ) const {
		Assert( i <= n );
		return Span< T >( ptr + i, n - i );
	}

	void operator+=( size_t i ) {
		*this = *this + i;
	}

	void operator++( int ) {
		Assert( n > 0 );
		ptr++;
		n--;
	}

	T * begin() const { return ptr; }
	T * end() const { return ptr + n; }

	Span< T > slice( size_t start, size_t one_past_end ) const {
		Assert( start <= one_past_end );
		Assert( one_past_end <= n );
		return Span< T >( ptr + start, one_past_end - start );
	}

	template< typename S >
	Span< S > cast() const {
		Assert( num_bytes() % sizeof( S ) == 0 );
		return Span< S >( align_cast< S >( ptr ), num_bytes() / sizeof( S ) );
	}

	template< typename S >
	Span< S > constcast() const {
		return Span< S >( const_cast< S * >( ptr ), num_bytes() );
	}
};

/*
 * Allocator
 */

struct Allocator {
	virtual void * try_allocate( size_t size, size_t alignment, SourceLocation src = CurrentSourceLocation() ) = 0;
	virtual void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, SourceLocation src = CurrentSourceLocation() ) = 0;
	void * allocate( size_t size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	void * reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, SourceLocation src = CurrentSourceLocation() );
	virtual void deallocate( void * ptr, SourceLocation src = CurrentSourceLocation() ) = 0;

	template< typename... Rest >
	char * operator()( SourceLocation src, const char * fmt, const Rest & ... rest );

	template< typename... Rest >
	char * operator()( const char * fmt, const Rest & ... rest );

	template< typename... Rest >
	Span< char > sv( const char * fmt, const Rest & ... rest );
};

struct TempAllocator;

void * AllocManyHelper( Allocator * a, size_t n, size_t size, size_t alignment, SourceLocation src );
void * ReallocManyHelper( Allocator * a, void * ptr, size_t current_n, size_t new_n, size_t size, size_t alignment, SourceLocation src );

template< typename T >
T * Alloc( Allocator * a, SourceLocation src = CurrentSourceLocation() ) {
	return ( T * ) a->allocate( sizeof( T ), alignof( T ), src );
}

template< typename T >
T * AllocMany( Allocator * a, size_t n, SourceLocation src = CurrentSourceLocation() ) {
	return ( T * ) AllocManyHelper( a, n, sizeof( T ), alignof( T ), src );
}

template< typename T >
T * ReallocMany( Allocator * a, T * ptr, size_t current_n, size_t new_n, SourceLocation src = CurrentSourceLocation() ) {
	return ( T * ) ReallocManyHelper( a, ptr, current_n, new_n, sizeof( T ), alignof( T ), src );
}

template< typename T >
Span< T > AllocSpan( Allocator * a, size_t n, SourceLocation src = CurrentSourceLocation() ) {
	return Span< T >( AllocMany< T >( a, n, src ), n );
}

inline void Free( Allocator * a, void * p, SourceLocation src = CurrentSourceLocation() ) {
	a->deallocate( p, src );
}

/*
 * Optional
 */

struct NoneType { };
constexpr NoneType NONE = { };

template< typename T >
struct Optional {
	T value;
	bool exists;

	Optional() = default;

	constexpr Optional( NoneType ) {
		value = { };
		exists = false;
	}
	constexpr Optional( const T & other ) {
		value = other;
		exists = true;
	}

	void operator=( NoneType ) {
		value = { };
		exists = false;
	}
	void operator=( const T & other ) {
		value = other;
		exists = true;
	}
};

/*
 * maths types
 */

struct Time {
	u64 flicks;
};

struct Vec2 {
	float x, y;

	Vec2() = default;
	explicit constexpr Vec2( float xy ) : x( xy ), y( xy ) { }
	constexpr Vec2( float x_, float y_ ) : x( x_ ), y( y_ ) { }

	float * ptr() { return &x; }
	const float * ptr() const { return &x; }

	float & operator[]( size_t i ) {
		Assert( i < 2 );
		return ptr()[ i ];
	}

	float operator[]( size_t i ) const {
		Assert( i < 2 );
		return ptr()[ i ];
	}
};

struct Vec3 {
	float x, y, z;

	Vec3() = default;
	explicit constexpr Vec3( float xyz ) : x( xyz ), y( xyz ), z( xyz ) { }
	constexpr Vec3( Vec2 xy, float z_ ) : x( xy.x ), y( xy.y ), z( z_ ) { }
	constexpr Vec3( float x_, float y_, float z_ ) : x( x_ ), y( y_ ), z( z_ ) { }

	static constexpr Vec3 Z( float z ) { return Vec3( 0.0f, 0.0f, z ); }

	constexpr Vec2 xy() const { return Vec2( x, y ); }

	float * ptr() { return &x; }
	const float * ptr() const { return &x; }

	float & operator[]( size_t i ) {
		Assert( i < 3 );
		return ptr()[ i ];
	}

	float operator[]( size_t i ) const {
		Assert( i < 3 );
		return ptr()[ i ];
	}
};

struct Vec4 {
	float x, y, z, w;

	Vec4() = default;
	explicit constexpr Vec4( float xyzw ) : x( xyzw ), y( xyzw ), z( xyzw ), w( xyzw ) { }
	constexpr Vec4( Vec2 xy, float z_, float w_ ) : x( xy.x ), y( xy.y ), z( z_ ), w( w_ ) { }
	constexpr Vec4( Vec3 xyz, float w_ ) : x( xyz.x ), y( xyz.y ), z( xyz.z ), w( w_ ) { }
	constexpr Vec4( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }

	constexpr Vec2 xy() const { return Vec2( x, y ); }
	constexpr Vec3 xyz() const { return Vec3( x, y, z ); }

	float * ptr() { return &x; }
	const float * ptr() const { return &x; }

	float & operator[]( size_t i ) {
		Assert( i < 4 );
		return ptr()[ i ];
	}

	float operator[]( size_t i ) const {
		Assert( i < 4 );
		return ptr()[ i ];
	}
};

struct Mat3 {
	Vec3 col0, col1, col2;

	Mat3() = default;
	constexpr Mat3( Vec3 c0, Vec3 c1, Vec3 c2 ) : col0( c0 ), col1( c1 ), col2( c2 ) { }
	constexpr Mat3(
		float e00, float e01, float e02,
		float e10, float e11, float e12,
		float e20, float e21, float e22
	) : col0( e00, e10, e20 ), col1( e01, e11, e21 ), col2( e02, e12, e22 ) { }

	constexpr Vec3 row0() const { return Vec3( col0.x, col1.x, col2.x ); }
	constexpr Vec3 row1() const { return Vec3( col0.y, col1.y, col2.y ); }
	constexpr Vec3 row2() const { return Vec3( col0.z, col1.z, col2.z ); }

	float * ptr() { return col0.ptr(); }

	static constexpr Mat3 Identity() {
		return Mat3(
			1, 0, 0,
			0, 1, 0,
			0, 0, 1
		);
	}
};

struct Mat3x4;
struct alignas( 16 ) Mat4 {
	Vec4 col0, col1, col2, col3;

	Mat4() = default;
	constexpr Mat4( Vec4 c0, Vec4 c1, Vec4 c2, Vec4 c3 ) : col0( c0 ), col1( c1 ), col2( c2 ), col3( c3 ) { }
	constexpr Mat4(
		float e00, float e01, float e02, float e03,
		float e10, float e11, float e12, float e13,
		float e20, float e21, float e22, float e23,
		float e30, float e31, float e32, float e33
	) : col0( e00, e10, e20, e30 ), col1( e01, e11, e21, e31 ), col2( e02, e12, e22, e32 ), col3( e03, e13, e23, e33 ) { }

	constexpr explicit Mat4( const Mat3x4 & m34 );

	constexpr Vec4 row0() const { return Vec4( col0.x, col1.x, col2.x, col3.x ); }
	constexpr Vec4 row1() const { return Vec4( col0.y, col1.y, col2.y, col3.y ); }
	constexpr Vec4 row2() const { return Vec4( col0.z, col1.z, col2.z, col3.z ); }
	constexpr Vec4 row3() const { return Vec4( col0.w, col1.w, col2.w, col3.w ); }

	float * ptr() { return col0.ptr(); }

	static constexpr Mat4 Identity() {
		return Mat4(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);
	}
};

struct alignas( 16 ) Mat3x4 {
	Vec3 col0, col1, col2, col3;

	Mat3x4() = default;
	constexpr Mat3x4( Vec3 c0, Vec3 c1, Vec3 c2, Vec3 c3 ) : col0( c0 ), col1( c1 ), col2( c2 ), col3( c3 ) { }
	constexpr Mat3x4(
		float e00, float e01, float e02, float e03,
		float e10, float e11, float e12, float e13,
		float e20, float e21, float e22, float e23
	) : col0( e00, e10, e20 ), col1( e01, e11, e21 ), col2( e02, e12, e22 ), col3( e03, e13, e23 ) { }

	explicit Mat3x4( const Mat4 & m4 ) {
		Assert( m4.col0.w == 0.0f && m4.col1.w == 0.0f && m4.col2.w == 0.0f );
		col0 = m4.col0.xyz();
		col1 = m4.col1.xyz();
		col2 = m4.col2.xyz();
		col3 = m4.col3.xyz();
	}

	constexpr Vec4 row0() const { return Vec4( col0.x, col1.x, col2.x, col3.x ); }
	constexpr Vec4 row1() const { return Vec4( col0.y, col1.y, col2.y, col3.y ); }
	constexpr Vec4 row2() const { return Vec4( col0.z, col1.z, col2.z, col3.z ); }
	constexpr Vec4 row3() const { return Vec4( 0.0f, 0.0f, 0.0f, 1.0f ); }

	float * ptr() { return col0.ptr(); }

	static constexpr Mat3x4 Identity() {
		return Mat3x4(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0
		);
	}
};

constexpr Mat4::Mat4( const Mat3x4 & m34 ) :
		col0( Vec4( m34.col0, 0.0f ) ),
		col1( Vec4( m34.col1, 0.0f ) ),
		col2( Vec4( m34.col2, 0.0f ) ),
		col3( Vec4( m34.col3, 1.0f ) ) { }

struct EulerDegrees2 {
	float pitch, yaw;

	EulerDegrees2() = default;
	constexpr EulerDegrees2( float p, float y ) : pitch( p ), yaw( y ) { }
	explicit constexpr EulerDegrees2( Vec2 v ) : pitch( v.x ), yaw( v.y ) { }
};

struct EulerDegrees3 {
	float pitch, yaw, roll;

	EulerDegrees3() = default;
	constexpr EulerDegrees3( float p, float y, float r ) : pitch( p ), yaw( y ), roll( r ) { }
	explicit constexpr EulerDegrees3( Vec3 v ) : pitch( v.x ), yaw( v.y ), roll( v.z ) { }
	explicit constexpr EulerDegrees3( EulerDegrees2 a ) : pitch( a.pitch ), yaw( a.yaw ), roll( 0.0f ) { }

	constexpr EulerDegrees3 yaw_only() const { return EulerDegrees3( 0.0f, yaw, 0.0f ); }
};

struct Quaternion {
	float x, y, z, w;

	Quaternion() = default;
	constexpr Quaternion( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }

	Vec3 im() const { return Vec3( x, y, z ); }

	float * ptr() { return &x; }

	static constexpr Quaternion Identity() {
		return Quaternion( 0, 0, 0, 1 );
	}
};

struct Plane {
	Vec3 normal;
	float distance;
};

struct MinMax1 {
	float lo, hi;

	MinMax1() = default;
	constexpr MinMax1( float lo_, float hi_ ) : lo( lo_ ), hi( hi_ ) { }

	static constexpr MinMax1 Empty() {
		return MinMax1( FLT_MAX, -FLT_MAX );
	}
};

struct MinMax2 {
	Vec2 mins, maxs;

	MinMax2() = default;
	constexpr MinMax2( Vec2 mins_, Vec2 maxs_ ) : mins( mins_ ), maxs( maxs_ ) { }

	static constexpr MinMax2 Empty() {
		return MinMax2( Vec2( FLT_MAX ), Vec2( -FLT_MAX ) );
	}
};

struct MinMax3 {
	Vec3 mins, maxs;

	MinMax3() = default;
	constexpr MinMax3( Vec3 mins_, Vec3 maxs_ ) : mins( mins_ ), maxs( maxs_ ) { }
	explicit constexpr MinMax3( Vec3 half_size ) : mins( Vec3( -half_size.x, -half_size.y, -half_size.z ) ), maxs( half_size ) { }
	explicit constexpr MinMax3( float half_size ) : mins( Vec3( -half_size ) ), maxs( Vec3( half_size ) ) { }

	static constexpr MinMax3 Empty() {
		return MinMax3( Vec3( FLT_MAX ), Vec3( -FLT_MAX ) );
	}
};

struct CenterExtents3 {
	Vec3 center, extents;
};

struct Sphere {
	Vec3 center;
	float radius;
};

struct Capsule {
	Vec3 a, b;
	float radius;
};

struct RGB8 {
	u8 r, g, b;

	RGB8() = default;
	constexpr RGB8( u8 r_, u8 g_, u8 b_ ) : r( r_ ), g( g_ ), b( b_ ) { }
};

struct RGBA8 {
	u8 r, g, b, a;

	RGBA8() = default;
	constexpr RGBA8( u8 r_, u8 g_, u8 b_, u8 a_ ) : r( r_ ), g( g_ ), b( b_ ), a( a_ ) { }
	explicit constexpr RGBA8( RGB8 rgb, u8 a_ = 255 ) : r( rgb.r ), g( rgb.g ), b( rgb.b ), a( a_ ) { }

	constexpr RGB8 rgb() const { return RGB8( r, g, b ); }
};

/*
 * forward declarations
 */

struct Tokenized;

/*
 * enum arithmetic
 */

template< typename E > concept IsEnum = __is_enum( E );
template< typename E > using UnderlyingType = __underlying_type( E );

template< IsEnum E > void operator++( E & x, int ) { x = E( UnderlyingType< E >( x ) + 1 ); }
template< IsEnum E > constexpr E operator&( E lhs, E rhs ) { return E( UnderlyingType< E >( lhs ) & UnderlyingType< E >( rhs ) ); }
template< IsEnum E > constexpr E operator|( E lhs, E rhs ) { return E( UnderlyingType< E >( lhs ) | UnderlyingType< E >( rhs ) ); }
template< IsEnum E > constexpr E operator~( E x ) { return E( ~UnderlyingType< E >( x ) ); }
template< IsEnum E > void operator&=( E & lhs, E rhs ) { lhs = lhs & rhs; }
template< IsEnum E > void operator|=( E & lhs, E rhs ) { lhs = lhs | rhs; }
