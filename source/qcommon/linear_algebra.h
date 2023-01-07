// include base.h instead of this

/*
 * Vec2
 */

constexpr Vec2 operator+( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x + rhs.x, lhs.y + rhs.y ); }
constexpr Vec2 operator-( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x - rhs.x, lhs.y - rhs.y ); }
constexpr Vec2 operator*( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x * rhs.x, lhs.y * rhs.y ); }
constexpr Vec2 operator/( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x / rhs.x, lhs.y / rhs.y ); }

constexpr Vec2 operator+( Vec2 v, float x ) { return Vec2( v.x + x, v.y + x ); }
constexpr Vec2 operator-( Vec2 v, float x ) { return Vec2( v.x - x, v.y - x ); }
constexpr Vec2 operator-( float x, Vec2 v ) { return Vec2( x - v.x, x - v.y ); }
constexpr Vec2 operator*( Vec2 v, float scale ) { return Vec2( v.x * scale, v.y * scale ); }
constexpr Vec2 operator*( float scale, Vec2 v ) { return v * scale; }
constexpr Vec2 operator/( Vec2 v, float inv_scale ) { return v * ( 1.0f / inv_scale ); }
constexpr Vec2 operator/( float x, Vec2 v ) { return Vec2( x / v.x, x / v.y ); }

constexpr void operator+=( Vec2 & lhs, Vec2 rhs ) { lhs = lhs + rhs; }
constexpr void operator*=( Vec2 & lhs, Vec2 rhs ) { lhs = lhs * rhs; }

constexpr void operator*=( Vec2 & v, float scale ) { v = v * scale; }

constexpr Vec2 operator-( Vec2 v ) { return Vec2( -v.x, -v.y ); }

constexpr bool operator==( Vec2 lhs, Vec2 rhs ) { return lhs.x == rhs.x && lhs.y == rhs.y; }
constexpr bool operator!=( Vec2 lhs, Vec2 rhs ) { return !( lhs == rhs ); }

constexpr float Dot( Vec2 lhs, Vec2 rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y;
}

constexpr float Length( Vec2 v ) {
	return sqrtf( v.x * v.x + v.y * v.y );
}

constexpr float LengthSquared( Vec2 v ) {
	return v.x * v.x + v.y * v.y;
}

constexpr Vec2 Normalize( Vec2 v ) {
	assert( v != Vec2( 0.0f ) );
	return v / Length( v );
}

constexpr Vec2 SafeNormalize( Vec2 v ) {
	if( v == Vec2( 0.0f ) )
		return v;
	return Normalize( v );
}

constexpr Vec2 Min2( Vec2 a, Vec2 b ) {
	return Vec2( Min2( a.x, b.x ), Min2( a.y, b.y ) );
}

constexpr Vec2 Max2( Vec2 a, Vec2 b ) {
	return Vec2( Max2( a.x, b.x ), Max2( a.y, b.y ) );
}

constexpr Vec2 Clamp( Vec2 lo, Vec2 v, Vec2 hi ) {
	return Vec2(
		Clamp( lo.x, v.x, hi.x ),
		Clamp( lo.y, v.y, hi.y )
	);
}

/*
 * Vec3
 */

constexpr Vec3 operator+( Vec3 lhs, Vec3 rhs ) { return Vec3( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z ); }
constexpr Vec3 operator-( Vec3 lhs, Vec3 rhs ) { return Vec3( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z ); }
constexpr Vec3 operator*( Vec3 lhs, Vec3 rhs ) { return Vec3( lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z ); }

constexpr Vec3 operator+( Vec3 v, float x ) { return Vec3( v.x + x, v.y + x, v.z + x ); }
constexpr Vec3 operator-( Vec3 v, float x ) { return Vec3( v.x - x, v.y - x, v.z - x ); }
constexpr Vec3 operator*( Vec3 v, float scale ) { return Vec3( v.x * scale, v.y * scale, v.z * scale ); }
constexpr Vec3 operator*( float scale, Vec3 v ) { return v * scale; }
constexpr Vec3 operator/( Vec3 v, float inv_scale ) { return v * ( 1.0f / inv_scale ); }
constexpr Vec3 operator/( float x, Vec3 v ) { return Vec3( x / v.x, x / v.y, x / v.z ); }

constexpr void operator+=( Vec3 & lhs, Vec3 rhs ) { lhs = lhs + rhs; }
constexpr void operator-=( Vec3 & lhs, Vec3 rhs ) { lhs = lhs - rhs; }
constexpr void operator*=( Vec3 & lhs, Vec3 rhs ) { lhs = lhs * rhs; }

constexpr void operator+=( Vec3 & v, float x ) { v = v + x; }
constexpr void operator-=( Vec3 & v, float x ) { v = v - x; }
constexpr void operator*=( Vec3 & v, float scale ) { v = v * scale; }
constexpr void operator/=( Vec3 & v, float inv_scale ) { v = v / inv_scale; }

constexpr Vec3 operator-( Vec3 v ) { return Vec3( -v.x, -v.y, -v.z ); }

constexpr bool operator==( Vec3 lhs, Vec3 rhs ) { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z; }
constexpr bool operator!=( Vec3 lhs, Vec3 rhs ) { return !( lhs == rhs ); }

constexpr float Dot( Vec3 lhs, Vec3 rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

constexpr Vec3 Cross( Vec3 lhs, Vec3 rhs ) {
	return Vec3(
		lhs.y * rhs.z - rhs.y * lhs.z,
		rhs.x * lhs.z - lhs.x * rhs.z,
		lhs.x * rhs.y - rhs.x * lhs.y
	);
}

constexpr float Length( Vec3 v ) {
	return sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
}

constexpr float LengthSquared( Vec3 v ) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

constexpr Vec3 Normalize( Vec3 v ) {
	assert( v != Vec3( 0.0f ) );
	return v / Length( v );
}

constexpr Vec3 SafeNormalize( Vec3 v ) {
	if( v == Vec3( 0.0f ) )
		return v;
	return Normalize( v );
}

constexpr Vec3 Floor( Vec3 v ) {
	return Vec3(
		floorf( v.x ),
		floorf( v.y ),
		floorf( v.z )
	);
}

constexpr Vec3 Clamp( Vec3 lo, Vec3 v, Vec3 hi ) {
	return Vec3(
		Clamp( lo.x, v.x, hi.x ),
		Clamp( lo.y, v.y, hi.y ),
		Clamp( lo.z, v.z, hi.z )
	);
}

/*
 * Mat3
 */

constexpr Mat3 operator*( const Mat3 & lhs, const Mat3 & rhs ) {
	return Mat3(
		Dot( lhs.row0(), rhs.col0 ),
		Dot( lhs.row0(), rhs.col1 ),
		Dot( lhs.row0(), rhs.col2 ),

		Dot( lhs.row1(), rhs.col0 ),
		Dot( lhs.row1(), rhs.col1 ),
		Dot( lhs.row1(), rhs.col2 ),

		Dot( lhs.row2(), rhs.col0 ),
		Dot( lhs.row2(), rhs.col1 ),
		Dot( lhs.row2(), rhs.col2 )
	);
}

constexpr Vec3 operator*( const Mat3 & m, Vec3 v ) {
	return Vec3(
		Dot( m.row0(), v ),
		Dot( m.row1(), v ),
		Dot( m.row2(), v )
	);
}

constexpr Mat3 operator-( const Mat3 & m ) {
	return Mat3( -m.col0, -m.col1, -m.col2 );
}

/*
 * Vec4
 */

constexpr Vec4 operator+( Vec4 lhs, Vec4 rhs ) { return Vec4( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w ); }
constexpr Vec4 operator-( Vec4 lhs, Vec4 rhs ) { return Vec4( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w ); }
constexpr Vec4 operator*( Vec4 lhs, Vec4 rhs ) { return Vec4( lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w ); }

constexpr Vec4 operator*( Vec4 v, float scale ) { return Vec4( v.x * scale, v.y * scale, v.z * scale, v.w * scale ); }
constexpr Vec4 operator*( float scale, Vec4 v ) { return v * scale; }
constexpr Vec4 operator/( Vec4 v, float inv_scale ) { return v * ( 1.0f / inv_scale ); }

constexpr void operator*=( Vec4 & lhs, Vec4 rhs ) { lhs = lhs * rhs; }

constexpr void operator*=( Vec4 & v, float scale ) { v = v / scale; }
constexpr void operator/=( Vec4 & v, float inv_scale ) { v = v / inv_scale; }

constexpr Vec4 operator-( Vec4 v ) { return Vec4( -v.x, -v.y, -v.z, -v.w ); }

constexpr bool operator==( Vec4 lhs, Vec4 rhs ) { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w; }
constexpr bool operator!=( Vec4 lhs, Vec4 rhs ) { return !( lhs == rhs ); }

constexpr float Dot( Vec4 lhs, Vec4 rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

constexpr float Length( Vec4 v ) {
	return sqrtf( v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w );
}

constexpr Vec4 Normalize( Vec4 v ) {
	assert( v != Vec4( 0.0f ) );
	return v / Length( v );
}

constexpr Vec4 Clamp( Vec4 lo, Vec4 v, Vec4 hi ) {
	return Vec4(
		Clamp( lo.x, v.x, hi.x ),
		Clamp( lo.y, v.y, hi.y ),
		Clamp( lo.z, v.z, hi.z ),
		Clamp( lo.w, v.w, hi.w )
	);
}

/*
 * Mat4
 */

constexpr Mat4 Mat4Translation( float x, float y, float z ) {
	return Mat4(
		1, 0, 0, x,
		0, 1, 0, y,
		0, 0, 1, z,
		0, 0, 0, 1
	);
}

constexpr Mat4 Mat4Translation( Vec3 v ) {
	return Mat4Translation( v.x, v.y, v.z );
}

constexpr Mat4 Mat4Scale( float x, float y, float z ) {
	return Mat4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		0, 0, 0, 1
	);
}

constexpr Mat4 Mat4Scale( float s ) {
	return Mat4Scale( s, s, s );
}

constexpr Mat4 Mat4Scale( Vec3 v ) {
	return Mat4Scale( v.x, v.y, v.z );
}

constexpr Mat4 operator*( const Mat4 & lhs, const Mat4 & rhs ) {
	return Mat4(
		Dot( lhs.row0(), rhs.col0 ),
		Dot( lhs.row0(), rhs.col1 ),
		Dot( lhs.row0(), rhs.col2 ),
		Dot( lhs.row0(), rhs.col3 ),

		Dot( lhs.row1(), rhs.col0 ),
		Dot( lhs.row1(), rhs.col1 ),
		Dot( lhs.row1(), rhs.col2 ),
		Dot( lhs.row1(), rhs.col3 ),

		Dot( lhs.row2(), rhs.col0 ),
		Dot( lhs.row2(), rhs.col1 ),
		Dot( lhs.row2(), rhs.col2 ),
		Dot( lhs.row2(), rhs.col3 ),

		Dot( lhs.row3(), rhs.col0 ),
		Dot( lhs.row3(), rhs.col1 ),
		Dot( lhs.row3(), rhs.col2 ),
		Dot( lhs.row3(), rhs.col3 )
	);
}

constexpr void operator*=( Mat4 & lhs, const Mat4 & rhs ) {
	lhs = lhs * rhs;
}

constexpr Vec4 operator*( const Mat4 & m, const Vec4 & v ) {
	return Vec4(
		Dot( m.row0(), v ),
		Dot( m.row1(), v ),
		Dot( m.row2(), v ),
		Dot( m.row3(), v )
	);
}

constexpr Mat4 operator-( const Mat4 & m ) {
	return Mat4( -m.col0, -m.col1, -m.col2, -m.col3 );
}

/*
 * Quaternion
 */

constexpr Quaternion operator+( Quaternion lhs, Quaternion rhs ) {
	return Quaternion(
		lhs.x + rhs.x,
		lhs.y + rhs.y,
		lhs.z + rhs.z,
		lhs.w + rhs.w
	);
}

constexpr Quaternion operator*( Quaternion lhs, Quaternion rhs ) {
	return Quaternion(
		lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
		lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
	);
}

constexpr void operator*=( Quaternion & lhs, Quaternion rhs ) {
	lhs = lhs * rhs;
}

constexpr Quaternion operator*( Quaternion q, float scale ) {
	return Quaternion(
		q.x * scale,
		q.y * scale,
		q.z * scale,
		q.w * scale
	);
}

constexpr Quaternion operator*( float scale, Quaternion q ) {
	return q * scale;
}

constexpr Quaternion operator/( Quaternion q, float scale ) {
	float inv_scale = 1.0f / scale;
	return q * inv_scale;
}

constexpr float Dot( Quaternion lhs, Quaternion rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

constexpr float Length( Quaternion q ) {
	return sqrtf( q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w );
}

constexpr Quaternion Normalize( Quaternion q ) {
	return q / Length( q );
}

constexpr Quaternion NLerp( Quaternion from, float t, Quaternion to ) {
	float lt = 1.0f - t;
	float rt = Dot( from, to ) > 0 ? t : -t;
	return Normalize( from * lt + to * rt );
}

/*
 * MinMax3
 */

constexpr MinMax3 operator*( MinMax3 bounds, float scale ) {
	bounds.mins *= scale;
	bounds.maxs *= scale;
	return bounds;
}

/*
 * ggformat
 */

inline void format( FormatBuffer * fb, const Vec2 & v, const FormatOpts & opts ) {
	format( fb, "Vec2(" );
	format( fb, v.x, opts );
	format( fb, ", " );
	format( fb, v.y, opts );
	format( fb, ")" );
}

inline void format( FormatBuffer * fb, const Vec3 & v, const FormatOpts & opts ) {
	format( fb, "Vec3(" );
	format( fb, v.x, opts );
	format( fb, ", " );
	format( fb, v.y, opts );
	format( fb, ", " );
	format( fb, v.z, opts );
	format( fb, ")" );
}

inline void format( FormatBuffer * fb, const Vec4 & v, const FormatOpts & opts ) {
	format( fb, "Vec4(" );
	format( fb, v.x, opts );
	format( fb, ", " );
	format( fb, v.y, opts );
	format( fb, ", " );
	format( fb, v.z, opts );
	format( fb, ", " );
	format( fb, v.w, opts );
	format( fb, ")" );
}

inline void format( FormatBuffer * fb, const Mat3 & m, const FormatOpts & opts ) {
	format( fb, "Mat3(" );
	format( fb, m.row0(), opts );
	format( fb, ", " );
	format( fb, m.row1(), opts );
	format( fb, ", " );
	format( fb, m.row2(), opts );
	format( fb, ")" );
}

inline void format( FormatBuffer * fb, const Mat4 & m, const FormatOpts & opts ) {
	format( fb, "Mat4(" );
	format( fb, m.row0(), opts );
	format( fb, ", " );
	format( fb, m.row1(), opts );
	format( fb, ", " );
	format( fb, m.row2(), opts );
	format( fb, ", " );
	format( fb, m.row3(), opts );
	format( fb, ")" );
}

inline void format( FormatBuffer * fb, const Quaternion & q, const FormatOpts & opts ) {
	format( fb, "Quaternion(" );
	format( fb, q.x, opts );
	format( fb, ", " );
	format( fb, q.y, opts );
	format( fb, ", " );
	format( fb, q.z, opts );
	format( fb, ", " );
	format( fb, q.w, opts );
	format( fb, ")" );
}
