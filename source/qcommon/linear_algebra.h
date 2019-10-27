// include base.h instead of this

/*
 * Vec2
 */

inline Vec2 operator+( Vec2 v, float x ) { return Vec2( v.x + x, v.y + x ); }
inline Vec2 operator-( Vec2 v, float x ) { return Vec2( v.x - x, v.y - x ); }
inline Vec2 operator-( float x, Vec2 v ) { return Vec2( x - v.x, x - v.y ); }
inline Vec2 operator*( Vec2 v, float scale ) { return Vec2( v.x * scale, v.y * scale ); }
inline Vec2 operator*( float scale, Vec2 v ) { return v * scale; }
inline Vec2 operator/( float x, Vec2 v ) { return Vec2( x / v.x, x / v.y ); }
inline Vec2 operator/( Vec2 v, float scale ) { return v * ( 1.0f / scale ); }

inline Vec2 operator+( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x + rhs.x, lhs.y + rhs.y ); }
inline Vec2 operator-( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x - rhs.x, lhs.y - rhs.y ); }
inline Vec2 operator*( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x * rhs.x, lhs.y * rhs.y ); }
inline Vec2 operator/( Vec2 lhs, Vec2 rhs ) { return Vec2( lhs.x / rhs.x, lhs.y / rhs.y ); }

inline void operator*=( Vec2 & v, float scale ) { v = v * scale; }

inline void operator+=( Vec2 & lhs, Vec2 rhs ) { lhs = lhs + rhs; }
inline void operator*=( Vec2 & lhs, Vec2 rhs ) { lhs = lhs * rhs; }

inline Vec2 operator-( Vec2 v ) { return Vec2( -v.x, -v.y ); }

inline float Dot( Vec2 lhs, Vec2 rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline float Length( Vec2 v ) {
	return sqrtf( v.x * v.x + v.y * v.y );
}

inline Vec2 Normalize( Vec2 v ) {
	return v / Length( v );
}

/*
 * Vec3
 */

inline Vec3 operator+( Vec3 lhs, Vec3 rhs ) {
	return Vec3( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z );
}

inline void operator+=( Vec3 & lhs, Vec3 rhs ) {
	lhs = lhs + rhs;
}

inline Vec3 operator-( Vec3 lhs, Vec3 rhs ) {
	return Vec3( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z );
}

inline void operator-=( Vec3 & lhs, Vec3 rhs ) {
	lhs = lhs - rhs;
}

inline Vec3 operator*( Vec3 v, float scale ) {
	return Vec3( v.x * scale, v.y * scale, v.z * scale );
}

inline Vec3 operator*( float scale, Vec3 v ) {
	return v * scale;
}

inline void operator*=( Vec3 & v, float scale ) {
	v = v * scale;
}

inline Vec3 operator*( Vec3 lhs, Vec3 rhs ) {
	return Vec3( lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z );
}

inline Vec3 operator/( Vec3 v, float scale ) {
	float inv_scale = 1.0f / scale;
	return v * inv_scale;
}

inline void operator/=( Vec3 & v, float scale ) {
	v = v / scale;
}

inline Vec3 operator-( Vec3 v ) {
	return Vec3( -v.x, -v.y, -v.z );
}

inline bool operator==( Vec3 lhs, Vec3 rhs ) {
	return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

inline float Dot( Vec3 lhs, Vec3 rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 Cross( Vec3 lhs, Vec3 rhs ) {
	return Vec3(
		lhs.y * rhs.z - rhs.y * lhs.z,
		rhs.x * lhs.z - lhs.x * rhs.z,
		lhs.x * rhs.y - rhs.x * lhs.y
	);
}

inline float Length( Vec3 v ) {
	return sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
}

inline Vec3 Normalize( Vec3 v ) {
	return v / Length( v );
}

/*
 * Mat3
 */

inline Mat3 operator*( const Mat3 & lhs, const Mat3 & rhs ) {
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

inline Vec3 operator*( const Mat3 & m, Vec3 v ) {
	return Vec3(
		Dot( m.row0(), v ),
		Dot( m.row1(), v ),
		Dot( m.row2(), v )
	);
}

inline Mat3 operator-( const Mat3 & m ) {
	return Mat3( -m.col0, -m.col1, -m.col2 );
}

/*
 * Vec4
 */

inline Vec4 operator*( Vec4 v, float scale ) { return Vec4( v.x * scale, v.y * scale, v.z * scale, v.w * scale ); }
inline Vec4 operator*( float scale, Vec4 v ) { return v * scale; }
inline Vec4 operator/( Vec4 v, float scale ) { return v * ( 1.0f / scale ); }

inline Vec4 operator+( Vec4 lhs, Vec4 rhs ) { return Vec4( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w ); }
inline Vec4 operator-( Vec4 lhs, Vec4 rhs ) { return Vec4( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w ); }
inline Vec4 operator*( Vec4 lhs, Vec4 rhs ) { return Vec4( lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w ); }

inline void operator*=( Vec4 & v, float scale ) { v = v / scale; }
inline void operator/=( Vec4 & v, float scale ) { v = v / scale; }

inline void operator*=( Vec4 & lhs, Vec4 rhs ) { lhs = lhs * rhs; }

inline Vec4 operator-( Vec4 v ) { return Vec4( -v.x, -v.y, -v.z, -v.w ); }

inline float Dot( Vec4 lhs, Vec4 rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

inline float Length( Vec4 v ) {
	return sqrtf( v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w );
}

inline Vec4 Normalize( Vec4 v ) {
	return v / Length( v );
}

/*
 * Mat4
 */

inline Mat4 Mat4Translation( float x, float y, float z ) {
	return Mat4(
		1, 0, 0, x,
		0, 1, 0, y,
		0, 0, 1, z,
		0, 0, 0, 1
	);
}

inline Mat4 Mat4Translation( Vec3 v ) {
	return Mat4Translation( v.x, v.y, v.z );
}

inline Mat4 Mat4Scale( float x, float y, float z ) {
	return Mat4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		0, 0, 0, 1
	);
}

inline Mat4 Mat4Scale( float s ) {
	return Mat4Scale( s, s, s );
}

inline Mat4 operator*( const Mat4 & lhs, const Mat4 & rhs ) {
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

inline void operator*=( Mat4 & lhs, const Mat4 & rhs ) {
	lhs = lhs * rhs;
}

inline Vec4 operator*( const Mat4 & m, Vec4 v ) {
	return Vec4(
		Dot( m.row0(), v ),
		Dot( m.row1(), v ),
		Dot( m.row2(), v ),
		Dot( m.row3(), v )
	);
}

inline Mat4 operator-( const Mat4 & m ) {
	return Mat4( -m.col0, -m.col1, -m.col2, -m.col3 );
}

/*
 * Quaternion
 */

inline Quaternion operator+( Quaternion lhs, Quaternion rhs ) {
	return Quaternion(
		lhs.x + rhs.x,
		lhs.y + rhs.y,
		lhs.z + rhs.z,
		lhs.w + rhs.w
	);
}

inline Quaternion operator*( Quaternion lhs, Quaternion rhs ) {
	return Quaternion(
		lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
		lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
	);
}

inline void operator*=( Quaternion & lhs, Quaternion rhs ) {
	lhs = lhs * rhs;
}

inline Quaternion operator*( Quaternion q, float scale ) {
	return Quaternion(
		q.x * scale,
		q.y * scale,
		q.z * scale,
		q.w * scale
	);
}

inline Quaternion operator*( float scale, Quaternion q ) {
	return q * scale;
}

inline Quaternion operator/( Quaternion q, float scale ) {
	float inv_scale = 1.0f / scale;
	return q * inv_scale;
}

inline float Dot( Quaternion lhs, Quaternion rhs ) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

inline float Length( Quaternion q ) {
	return sqrtf( q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w );
}

inline Quaternion Normalize( Quaternion q ) {
	return q / Length( q );
}

inline Quaternion NLerp( Quaternion from, float t, Quaternion to ) {
	float lt = 1.0f - t;
	float rt = Dot( from, to ) > 0 ? t : -t;
	return Normalize( from * lt + to * rt );
}

/*
 * MinMax3
 */

inline MinMax3 operator*( MinMax3 bounds, float scale ) {
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
