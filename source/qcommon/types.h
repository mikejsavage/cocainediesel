#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <float.h>

#include "qcommon/platform.h"

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

constexpr s8 S8_MAX = INT8_MAX;
constexpr s16 S16_MAX = INT16_MAX;
constexpr s32 S32_MAX = INT32_MAX;
constexpr s64 S64_MAX = INT64_MAX;
constexpr s8 S8_MIN = INT8_MIN;
constexpr s16 S16_MIN = INT16_MIN;
constexpr s32 S32_MIN = INT32_MIN;
constexpr s64 S64_MIN = INT64_MIN;

constexpr u8 U8_MAX = UINT8_MAX;
constexpr u16 U16_MAX = UINT16_MAX;
constexpr u32 U32_MAX = UINT32_MAX;
constexpr u64 U64_MAX = UINT64_MAX;

#define S8 INT8_C
#define S16 INT16_C
#define S32 INT32_C
#define S64 INT64_C
#define U8 UINT8_C
#define U16 UINT16_C
#define U32 UINT32_C
#define U64 UINT64_C

/*
 * Allocator
 */

struct Allocator {
	virtual void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) = 0;
	virtual void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) = 0;
	void * allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	virtual void deallocate( void * ptr, const char * func, const char * file, int line ) = 0;

	template< typename... Rest >
	char * operator()( const char * fmt, const Rest & ... rest );
};

struct TempAllocator;

#if COMPILER_MSVC
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

void * AllocManyHelper( Allocator * a, size_t n, size_t size, size_t alignment, const char * func, const char * file, int line );
void * ReallocManyHelper( Allocator * a, void * ptr, size_t current_n, size_t new_n, size_t size, size_t alignment, const char * func, const char * file, int line );

#define ALLOC_SIZE( a, size, alignment ) ( a )->allocate( size, alignment, __PRETTY_FUNCTION__, __FILE__, __LINE__ )
#define REALLOC( a, ptr, current_size, new_size, alignment ) ( a )->reallocate( ptr, current_size, new_size, alignment, __PRETTY_FUNCTION__, __FILE__, __LINE__ )
#define FREE( a, p ) a->deallocate( p, __PRETTY_FUNCTION__, __FILE__, __LINE__ )

#define ALLOC( a, T ) ( ( T * ) ( a )->allocate( sizeof( T ), alignof( T ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ) )
#define ALLOC_MANY( a, T, n ) ( ( T * ) AllocManyHelper( a, checked_cast< size_t >( n ), sizeof( T ), alignof( T ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ) )
#define REALLOC_MANY( a, T, ptr, current_n, new_n ) ( ( T * ) ReallocManyHelper( a, ptr, checked_cast< size_t >( current_n ), checked_cast< size_t >( new_n ), sizeof( T ), alignof( T ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ) )
#define ALLOC_SPAN( a, T, n ) Span< T >( ALLOC_MANY( a, T, n ), n )

/*
 * helper functions that are useful in templates. so headers don't need to include base.h
 */

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
inline To checked_cast( const From & from ) {
	To result = To( from );
	assert( From( result ) == from );
	return result;
}

template< typename T >
constexpr T AlignPow2( T x, T alignment ) {
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
T Clamp( const T & lo, const T & x, const T & hi ) {
	assert( lo <= hi );
	return Max2( lo, Min2( x, hi ) );
}

template< typename T >
T Clamp01( const T & x ) {
	return Clamp( T( 0.0f ), x, T( 1.0f ) );
}

/*
 * Span
 */

template< typename T >
struct Span {
	T * ptr;
	size_t n;

	constexpr Span() : ptr( NULL ), n( 0 ) { }
	constexpr Span( T * ptr_, size_t n_ ) : ptr( ptr_ ), n( n_ ) { }

	// allow implicit conversion to Span< const T >
	operator Span< const T >() const { return Span< const T >( ptr, n ); }

	size_t num_bytes() const { return sizeof( T ) * n; }

	T & operator[]( size_t i ) const {
		assert( i < n );
		return ptr[ i ];
	}

	Span< T > operator+( size_t i ) const {
		assert( i <= n );
		return Span< T >( ptr + i, n - i );
	}

	void operator+=( size_t i ) {
		*this = *this + i;
	}

	void operator++( int ) {
		assert( n > 0 );
		ptr++;
		n--;
	}

	T * begin() const { return ptr; }
	T * end() const { return ptr + n; }

	Span< T > slice( size_t start, size_t one_past_end ) const {
		assert( start <= one_past_end );
		assert( one_past_end <= n );
		return Span< T >( ptr + start, one_past_end - start );
	}

	template< typename S >
	Span< S > cast() const {
		assert( num_bytes() % sizeof( S ) == 0 );
		assert( uintptr_t( ptr ) % alignof( S ) == 0 );
		return Span< S >( ( S * ) ptr, num_bytes() / sizeof( S ) );
	}
};

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

	Optional( NoneType ) { exists = false; }
	Optional( const T & other ) {
		value = other;
		exists = true;
	}

	void operator=( NoneType ) { exists = false; }
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
		assert( i < 2 );
		return ptr()[ i ];
	}

	float operator[]( size_t i ) const {
		assert( i < 2 );
		return ptr()[ i ];
	}
};

struct Vec3 {
	float x, y, z;

	Vec3() = default;
	explicit constexpr Vec3( float xyz ) : x( xyz ), y( xyz ), z( xyz ) { }
	constexpr Vec3( Vec2 xy, float z_ ) : x( xy.x ), y( xy.y ), z( z_ ) { }
	constexpr Vec3( float x_, float y_, float z_ ) : x( x_ ), y( y_ ), z( z_ ) { }

	constexpr Vec2 xy() const { return Vec2( x, y ); }

	float * ptr() { return &x; }
	const float * ptr() const { return &x; }

	float & operator[]( size_t i ) {
		assert( i < 3 );
		return ptr()[ i ];
	}

	float operator[]( size_t i ) const {
		assert( i < 3 );
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
		assert( i < 4 );
		return ptr()[ i ];
	}

	float operator[]( size_t i ) const {
		assert( i < 4 );
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

	Vec3 row0() const { return Vec3( col0.x, col1.x, col2.x ); }
	Vec3 row1() const { return Vec3( col0.y, col1.y, col2.y ); }
	Vec3 row2() const { return Vec3( col0.z, col1.z, col2.z ); }

	float * ptr() { return col0.ptr(); }

	static constexpr Mat3 Identity() {
		return Mat3(
			1, 0, 0,
			0, 1, 0,
			0, 0, 1
		);
	}
};

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

	Vec4 row0() const { return Vec4( col0.x, col1.x, col2.x, col3.x ); }
	Vec4 row1() const { return Vec4( col0.y, col1.y, col2.y, col3.y ); }
	Vec4 row2() const { return Vec4( col0.z, col1.z, col2.z, col3.z ); }
	Vec4 row3() const { return Vec4( col0.w, col1.w, col2.w, col3.w ); }

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
	constexpr Mat3x4(
		float e00, float e01, float e02, float e03,
		float e10, float e11, float e12, float e13,
		float e20, float e21, float e22, float e23
	) : col0( e00, e10, e20 ), col1( e01, e11, e21 ), col2( e02, e12, e22 ), col3( e03, e13, e23 ) { }

	Vec4 row0() const { return Vec4( col0.x, col1.x, col2.x, col3.x ); }
	Vec4 row1() const { return Vec4( col0.y, col1.y, col2.y, col3.y ); }
	Vec4 row2() const { return Vec4( col0.z, col1.z, col2.z, col3.z ); }
	Vec4 row3() const { return Vec4( 0, 0, 0, 1 ); }

	float * ptr() { return col0.ptr(); }

	static constexpr Mat3x4 Identity() {
		return Mat3x4(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0
		);
	}
};

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
};

struct Quaternion {
	float x, y, z, w;

	Quaternion() = default;
	constexpr Quaternion( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }

	float * ptr() { return &x; }

	static constexpr Quaternion Identity() {
		return Quaternion( 0, 0, 0, 1 );
	}
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

	static constexpr MinMax3 Empty() {
		return MinMax3( Vec3( FLT_MAX ), Vec3( -FLT_MAX ) );
	}
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
