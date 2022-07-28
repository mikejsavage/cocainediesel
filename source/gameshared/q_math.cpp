/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <math.h>

#include "qcommon/qcommon.h"
#include "qcommon/rng.h"

struct PackedVec3 {
	u64 x : 31;
	u64 y : 31;
	u64 zsign : 1;
	u64 zero : 1;
};

u64 DirToU64( Vec3 v ) {
	PackedVec3 packed = { };
	if( v == Vec3( 0.0f ) ) {
		packed.zero = 1;
	}
	else {
		packed.x = bit_cast< u32 >( v.x ) >> 1;
		packed.y = bit_cast< u32 >( v.y ) >> 1;
		packed.zsign = v.z >= 0.0f ? 0 : 1;
	}
	return bit_cast< u64 >( packed );
}

Vec3 U64ToDir( u64 u ) {
	PackedVec3 packed = bit_cast< PackedVec3 >( u );
	if( packed.zero )
		return Vec3( 0.0f );

	float x = bit_cast< float >( u32( packed.x << 1 ) );
	float y = bit_cast< float >( u32( packed.y << 1 ) );

	float sign = packed.zsign == 0 ? 1.0f : -1.0f;
	float z = sqrtf( Max2( 1.0f - x * x - y * y, 0.0f ) ) * sign;

	return Vec3( x, y, z );
}

float SignedOne( float x ) {
	return copysignf( 1.0f, x );
}

void ViewVectors( Vec3 forward, Vec3 * right, Vec3 * up ) {
	constexpr Vec3 world_up = Vec3( 0, 0, 1 );

	*right = Normalize( Cross( forward, world_up ) );
	*up = Normalize( Cross( *right, forward ) );
}

void AngleVectors( Vec3 angles, Vec3 * forward, Vec3 * right, Vec3 * up ) {
	float pitch = Radians( angles.x );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	float yaw = Radians( angles.y );
	float sy = sinf( yaw );
	float cy = cosf( yaw );
	float roll = Radians( angles.z );
	float sr = sinf( roll );
	float cr = cosf( roll );

	if( forward != NULL ) {
		*forward = Vec3( cp * cy, cp * sy, -sp );
	}

	if( right != NULL ) {
		float t = -sr * sp;
		*right = Vec3( t * cy + cr * sy, t * sy - cr * cy, -sr * cp );
	}

	if( up ) {
		float t = cr * sp;
		*up = Vec3( t * cy + sr * sy, t * sy - sr * cy, cr * cp );
	}
}

Vec3 VecToAngles( Vec3 vec ) {
	if( vec.xy() == Vec2( 0.0f ) ) {
		if( vec.z == 0.0f )
			return Vec3( 0.0f );
		return vec.z > 0 ? Vec3( -90.0f, 0.0f, 0.0f ) : Vec3( -270.0f, 0.0f, 0.0f );
	}

	float yaw;
	if( vec.x != 0.0f ) {
		yaw = Degrees( atan2f( vec.y, vec.x ) );
	}
	else if( vec.y > 0 ) {
		yaw = 90;
	}
	else {
		yaw = -90;
	}

	float forward = Length( vec.xy() );
	float pitch = Degrees( atan2f( vec.z, forward ) );

	if( yaw < 0 ) {
		yaw += 360;
	}
	if( pitch < 0 ) {
		pitch += 360;
	}

	return Vec3( -pitch, yaw, 0.0f );
}

void AnglesToAxis( Vec3 angles, mat3_t axis ) {
	Vec3 forward, right, up;
	AngleVectors( angles, &forward, &right, &up );
	axis[0] = forward.x;
	axis[1] = forward.y;
	axis[2] = forward.z;
	axis[3] = -right.x;
	axis[4] = -right.y;
	axis[5] = -right.z;
	axis[6] = up.x;
	axis[7] = up.y;
	axis[8] = up.z;
}

// must match the GLSL OrthonormalBasis
void OrthonormalBasis( Vec3 v, Vec3 * tangent, Vec3 * bitangent ) {
	float s = SignedOne( v.z );
	float a = -1.0f / ( s + v.z );
	float b = v.x * v.y * a;

	*tangent = Vec3( 1.0f + s * v.x * v.x * a, s * b, -s * v.x );
	*bitangent = Vec3( b, s + v.y * v.y * a, -v.y );
}

//============================================================================

static float LerpAngle( float a, float t, float b ) {
	if( b - a > 180 ) {
		b -= 360;
	}
	if( b - a < -180 ) {
		b += 360;
	}
	return Lerp( a, t, b );
}

Vec3 LerpAngles( Vec3 a, float t, Vec3 b ) {
	return Vec3(
		LerpAngle( a.x, t, b.x ),
		LerpAngle( a.y, t, b.y ),
		LerpAngle( a.z, t, b.z )
	);
}

float AngleNormalize360( float angle ) {
	return PositiveMod( angle, 360.0f );
}

float AngleNormalize180( float angle ) {
	angle = AngleNormalize360( angle );
	return angle > 180.0f ? angle - 360.0f : angle;
}

float AngleDelta( float angle1, float angle2 ) {
	return AngleNormalize180( angle1 - angle2 );
}

Vec3 AngleDelta( Vec3 angle1, Vec3 angle2 ) {
	return Vec3(
		AngleDelta( angle1.x, angle2.x ),
		AngleDelta( angle1.y, angle2.y ),
		AngleDelta( angle1.z, angle2.z )
	);
}

EulerDegrees2 AngleDelta( EulerDegrees2 a, EulerDegrees2 b ) {
	return EulerDegrees2( AngleDelta( a.pitch, b.pitch ), AngleDelta( a.yaw, b.yaw ) );
}

void ClearBounds( Vec3 * mins, Vec3 * maxs ) {
	*mins = Vec3( 999999.0f );
	*maxs = Vec3( -999999.0f );
}

bool BoundsOverlap( const Vec3 & mins1, const Vec3 & maxs1, const Vec3 & mins2, const Vec3 & maxs2 ) {
	return mins1.x <= maxs2.x && mins1.y <= maxs2.y && mins1.z <= maxs2.z &&
		maxs1.x >= mins2.x && maxs1.y >= mins2.y && maxs1.z >= mins2.z;
}

void AddPointToBounds( Vec3 v, Vec3 * mins, Vec3 * maxs ) {
	for( int i = 0; i < 3; i++ ) {
		float x = v[ i ];
		mins->ptr()[ i ] = Min2( x, mins->ptr()[ i ] );
		maxs->ptr()[ i ] = Max2( x, maxs->ptr()[ i ] );
	}
}

float RadiusFromBounds( Vec3 mins, Vec3 maxs ) {
	Vec3 corner;

	for( int i = 0; i < 3; i++ ) {
		corner[ i ] = Max2( Abs( mins[ i ] ), Abs( maxs[ i ] ) );
	}

	return Length( corner );
}

CenterExtents3 ToCenterExtents( const MinMax3 & bounds ) {
	CenterExtents3 aabb;
	aabb.center = ( bounds.maxs + bounds.mins ) * 0.5f;
	aabb.extents = ( bounds.maxs - bounds.mins ) * 0.5f;
	return aabb;
}

MinMax3 ToMinMax( const CenterExtents3 & aabb ) {
	return MinMax3( aabb.center - aabb.extents, aabb.center + aabb.extents );
}

Capsule MakePlayerCapsule( const MinMax3 & bounds ) {
	Vec3 center = ( bounds.maxs + bounds.mins ) * 0.5f;
	Vec3 dim = bounds.maxs - bounds.mins;
	assert( dim.z >= dim.x && dim.x == dim.y );

	Capsule capsule;
	capsule.radius = bounds.maxs.x;
	capsule.a = Vec3( center.xy(), bounds.mins.z + capsule.radius );
	capsule.b = Vec3( center.xy(), bounds.maxs.z - capsule.radius );

	return capsule;
}

//============================================================================

void Matrix3_Identity( mat3_t m ) {
	int i, j;

	for( i = 0; i < 3; i++ )
		for( j = 0; j < 3; j++ )
			if( i == j ) {
				m[i * 3 + j] = 1.0;
			} else {
				m[i * 3 + j] = 0.0;
			}
}

void Matrix3_Copy( const mat3_t m1, mat3_t m2 ) {
	memcpy( m2, m1, sizeof( mat3_t ) );
}

void Matrix3_Multiply( const mat3_t m1, const mat3_t m2, mat3_t out ) {
	out[0] = m1[0] * m2[0] + m1[1] * m2[3] + m1[2] * m2[6];
	out[1] = m1[0] * m2[1] + m1[1] * m2[4] + m1[2] * m2[7];
	out[2] = m1[0] * m2[2] + m1[1] * m2[5] + m1[2] * m2[8];
	out[3] = m1[3] * m2[0] + m1[4] * m2[3] + m1[5] * m2[6];
	out[4] = m1[3] * m2[1] + m1[4] * m2[4] + m1[5] * m2[7];
	out[5] = m1[3] * m2[2] + m1[4] * m2[5] + m1[5] * m2[8];
	out[6] = m1[6] * m2[0] + m1[7] * m2[3] + m1[8] * m2[6];
	out[7] = m1[6] * m2[1] + m1[7] * m2[4] + m1[8] * m2[7];
	out[8] = m1[6] * m2[2] + m1[7] * m2[5] + m1[8] * m2[8];
}

void Matrix3_TransformVector( const mat3_t m, Vec3 v, Vec3 * out ) {
	out->x = m[0] * v.x + m[1] * v.y + m[2] * v.z;
	out->y = m[3] * v.x + m[4] * v.y + m[5] * v.z;
	out->z = m[6] * v.x + m[7] * v.y + m[8] * v.z;
}

void Matrix3_FromAngles( Vec3 angles, mat3_t m ) {
	Vec3 forward, right, up;
	AngleVectors( angles, &forward, &right, &up );
	m[0] = forward.x;
	m[1] = forward.y;
	m[2] = forward.z;
	m[3] = right.x;
	m[4] = right.y;
	m[5] = right.z;
	m[6] = up.x;
	m[7] = up.y;
	m[8] = up.z;
}

int PositiveMod( int x, int y ) {
	int res = x % y;
	return res < 0 ? res + y : res;
}

float PositiveMod( float x, float y ) {
	float res = fmodf( x, y );
	return res < 0 ? res + y : res;
}

double PositiveMod( double x, double y ) {
	double res = fmod( x, y );
	return res < 0 ? res + y : res;
}

Vec3 UniformSampleOnSphere( RNG * rng ) {
	float z = RandomFloat11( rng );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * PI * RandomFloat01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

Vec3 UniformSampleInsideSphere( RNG * rng ) {
	Vec3 p = UniformSampleOnSphere( rng );
	float r = cbrtf( RandomFloat01( rng ) );
	return p * r;
}

Vec3 UniformSampleCone( RNG * rng, float theta ) {
	assert( theta >= 0.0f && theta <= PI );
	float z = RandomUniformFloat( rng, cosf( theta ), 1.0f );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * PI * RandomFloat01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

Vec2 UniformSampleInsideCircle( RNG * rng ) {
	float theta = RandomFloat01( rng ) * 2.0f * PI;
	float r = sqrtf( RandomFloat01( rng ) );
	return Vec2( r * cosf( theta ), r * sinf( theta ) );
}

float SampleNormalDistribution( RNG * rng ) {
	// generate a float in (0, 1). works because prev(1) + FLT_MIN == prev(1)
	float u1 = RandomFloat01( rng ) + FLT_MIN;
	float u2 = RandomFloat01( rng );
	return sqrtf( -2.0f * logf( u1 ) ) * cosf( u2 * 2.0f * PI );
}

Vec3 Project( Vec3 a, Vec3 b ) {
	return Dot( a, b ) / LengthSquared( b ) * b;
}

Vec3 ClosestPointOnSegment( Vec3 start, Vec3 end, Vec3 p ) {
	Vec3 seg = end - start;
	float t = Dot( p - start, seg ) / Dot( seg, seg );
	return Lerp( start, Clamp01( t ), end );
}

Mat4 TransformKToDir( Vec3 dir ) {
	dir = Normalize( dir );

	Vec3 K = Vec3( 0, 0, 1 );

	Vec3 axis;
	if( Abs( dir.z ) < 0.9999f ) {
		axis = Normalize( Cross( K, dir ) );
	}
	else {
		axis = Vec3( 1.0f, 0.0f, 0.0f );
	}

	float c = Dot( K, dir );
	float s = sqrtf( 1.0f - c * c );

	Mat4 rotation = Mat4(
		c + axis.x * axis.x * ( 1.0f - c ),
		axis.x * axis.y * ( 1.0f - c ) - axis.z * s,
		axis.x * axis.z * ( 1.0f - c ) + axis.y * s,
		0.0f,

		axis.y * axis.x * ( 1.0f - c ) + axis.z * s,
		c + axis.y * axis.y * ( 1.0f - c ),
		axis.y * axis.z * ( 1.0f - c ) - axis.x * s,
		0.0f,

		axis.z * axis.x * ( 1.0f - c ) - axis.y * s,
		axis.z * axis.y * ( 1.0f - c ) + axis.x * s,
		c + axis.z * axis.z * ( 1.0f - c ),
		0.0f,

		0.0f, 0.0f, 0.0f, 1.0f
	);

	return rotation;
}

MinMax3 Union( MinMax3 bounds, Vec3 p ) {
	return MinMax3(
		Vec3( Min2( bounds.mins.x, p.x ), Min2( bounds.mins.y, p.y ), Min2( bounds.mins.z, p.z ) ),
		Vec3( Max2( bounds.maxs.x, p.x ), Max2( bounds.maxs.y, p.y ), Max2( bounds.maxs.z, p.z ) )
	);
}

MinMax3 Union( MinMax3 a, MinMax3 b ) {
	return MinMax3(
		Vec3( Min2( a.mins.x, b.mins.x ), Min2( a.mins.y, b.mins.y ), Min2( a.mins.z, b.mins.z ) ),
		Vec3( Max2( a.maxs.x, b.maxs.x ), Max2( a.maxs.y, b.maxs.y ), Max2( a.maxs.z, b.maxs.z ) )
	);
}

u32 Log2( u64 x ) {
	u32 log = 0;
	x >>= 1;

	while( x > 0 ) {
		x >>= 1;
		log++;
	}

	return log;
}

u16 Bswap( u16 x ) {
	return ( x >> 8 ) | ( x << 8 );
}
