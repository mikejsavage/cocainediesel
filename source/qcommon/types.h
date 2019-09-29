#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <float.h>

/*
 * ints
 */

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/*
 * allocators
 */

struct Allocator {
	virtual ~Allocator() { }
	virtual void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line ) = 0;
	virtual void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line ) = 0;
	void * allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	virtual void deallocate( void * ptr, const char * func, const char * file, int line ) = 0;
};

struct ArenaAllocator;
struct TempAllocator final : public Allocator {
	~TempAllocator();

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	void deallocate( void * ptr, const char * func, const char * file, int line );

	template< typename... Rest >
	const char * operator()( const char * fmt, const Rest & ... rest );

private:
	ArenaAllocator * arena;
	u8 * old_cursor;

	friend struct ArenaAllocator;
};

struct ArenaAllocator final : public Allocator {
	ArenaAllocator();
	ArenaAllocator( void * mem, size_t size );

	void * try_allocate( size_t size, size_t alignment, const char * func, const char * file, int line );
	void * try_reallocate( void * ptr, size_t current_size, size_t new_size, size_t alignment, const char * func, const char * file, int line );
	void deallocate( void * ptr, const char * func, const char * file, int line );

	TempAllocator temp();

	void clear();
	void * get_memory();

private:
	u8 * memory;
	u8 * top;
	u8 * cursor;

	friend struct TempAllocator;
};

/*
 * span
 */

template< typename T >
struct Span {
	T * ptr;
	size_t n;

	constexpr Span() : ptr( NULL ), n( 0 ) { }
	constexpr Span( T * ptr_, size_t n_ ) : ptr( ptr_ ), n( n_ ) { }

	// allow implicit conversion to Span< const T >
	operator Span< const T >() { return Span< const T >( ptr, n ); }
	operator Span< const T >() const { return Span< const T >( ptr, n ); }

	size_t num_bytes() const { return sizeof( T ) * n; }

	T & operator[]( size_t i ) {
		assert( i < n );
		return ptr[ i ];
	}

	const T & operator[]( size_t i ) const {
		assert( i < n );
		return ptr[ i ];
	}

	Span< T > operator+( size_t i ) {
		assert( i <= n );
		return Span< T >( ptr + i, n - i );
	}

	Span< const T > operator+( size_t i ) const {
		assert( i <= n );
		return Span< const T >( ptr + i, n - i );
	}

	void operator++( int ) {
		assert( n > 0 );
		ptr++;
		n--;
	}

	T * begin() { return ptr; }
	T * end() { return ptr + n; }
	const T * begin() const { return ptr; }
	const T * end() const { return ptr + n; }

	Span< T > slice( size_t start, size_t one_past_end ) {
		assert( start <= one_past_end );
		assert( one_past_end <= n );
		return Span< T >( ptr + start, one_past_end - start );
	}

	Span< const T > slice( size_t start, size_t one_past_end ) const {
		assert( start <= one_past_end );
		assert( one_past_end <= n );
		return Span< const T >( ptr + start, one_past_end - start );
	}

	template< typename S >
	Span< S > cast() {
		assert( num_bytes() % sizeof( S ) == 0 );
		return Span< S >( ( S * ) ptr, num_bytes() / sizeof( S ) );
	}
};

/*
 * maths types
 */

struct Vec2 {
	float x, y;

	Vec2() { }
	explicit constexpr Vec2( float xy ) : x( xy ), y( xy ) { }
	constexpr Vec2( float x_, float y_ ) : x( x_ ), y( y_ ) { }

	float * ptr() { return &x; }
};

struct Vec3 {
	float x, y, z;

	Vec3() { }
	explicit constexpr Vec3( float xyz ) : x( xyz ), y( xyz ), z( xyz ) { }
	constexpr Vec3( Vec2 xy, float z_ ) : x( xy.x ), y( xy.y ), z( z_ ) { }
	constexpr Vec3( float x_, float y_, float z_ ) : x( x_ ), y( y_ ), z( z_ ) { }

	Vec2 xy() const { return Vec2( x, y ); }

	float * ptr() { return &x; }
};

struct Vec4 {
	float x, y, z, w;

	Vec4() { }
	explicit constexpr Vec4( float xyzw ) : x( xyzw ), y( xyzw ), z( xyzw ), w( xyzw ) { }
	constexpr Vec4( Vec2 xy, float z_, float w_ ) : x( xy.x ), y( xy.y ), z( z_ ), w( w_ ) { }
	constexpr Vec4( Vec3 xyz, float w_ ) : x( xyz.x ), y( xyz.y ), z( xyz.z ), w( w_ ) { }
	constexpr Vec4( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }

	Vec2 xy() const { return Vec2( x, y ); }
	Vec3 xyz() const { return Vec3( x, y, z ); }

	float * ptr() { return &x; }
};

struct Mat2 {
	Vec2 col0, col1;

	Mat2() { }
	constexpr Mat2( Vec2 c0, Vec2 c1 ) : col0( c0 ), col1( c1 ) { }
	constexpr Mat2( float e00, float e01, float e10, float e11 ) : col0( e00, e10 ), col1( e01, e11 ) { }

	Vec2 row0() const { return Vec2( col0.x, col1.x ); }
	Vec2 row1() const { return Vec2( col0.y, col1.y ); }

	float * ptr() { return col0.ptr(); }

	static constexpr Mat2 Identity() { return Mat2( 1, 0, 0, 1 ); }
};

struct Mat3 {
	Vec3 col0, col1, col2;

	Mat3() { }
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

	Mat4() { }
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

	Mat3x4() { }
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

struct EulerDegrees3 {
	float pitch, yaw, roll;
};

struct Quaternion {
	float x, y, z, w;

	Quaternion() { }
	constexpr Quaternion( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) { }

	float * ptr() { return &x; }

	static constexpr Quaternion Identity() {
		return Quaternion( 0, 0, 0, 1 );
	}
};

struct MinMax1 {
	float lo, hi;

	MinMax1() { }
	constexpr MinMax1( float lo_, float hi_ ) : lo( lo_ ), hi( hi_ ) { }

	static constexpr MinMax1 Empty() {
		return MinMax1( FLT_MAX, -FLT_MAX );
	}
};

struct MinMax2 {
	Vec2 mins, maxs;

	MinMax2() { }
	constexpr MinMax2( Vec2 mins_, Vec2 maxs_ ) : mins( mins_ ), maxs( maxs_ ) { }

	static constexpr MinMax2 Empty() {
		return MinMax2( Vec2( FLT_MAX ), Vec2( -FLT_MAX ) );
	}
};

struct MinMax3 {
	Vec3 mins, maxs;

	MinMax3() { }
	constexpr MinMax3( Vec3 mins_, Vec3 maxs_ ) : mins( mins_ ), maxs( maxs_ ) { }

	static constexpr MinMax3 Empty() {
		return MinMax3( Vec3( FLT_MAX ), Vec3( -FLT_MAX ) );
	}
};

struct RGB8 {
	u8 r, g, b;

	RGB8() { }
	constexpr RGB8( u8 r_, u8 g_, u8 b_ ) : r( r_ ), g( g_ ), b( b_ ) { }
};

struct RGBA8 {
	u8 r, g, b, a;

	RGBA8() { }
	constexpr RGBA8( u8 r_, u8 g_, u8 b_, u8 a_ ) : r( r_ ), g( g_ ), b( b_ ), a( a_ ) { }

	explicit RGBA8( const Vec4 & v ) {
		r = v.x * 255.0f;
		g = v.y * 255.0f;
		b = v.z * 255.0f;
		a = v.w * 255.0f;
	}
};

// TODO: asset types?

struct Model;
struct Material;
struct SoundAsset;
