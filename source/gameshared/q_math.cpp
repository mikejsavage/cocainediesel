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

void AngleVectors( EulerDegrees3 angles, Vec3 * forward, Vec3 * right, Vec3 * up ) {
	float pitch = Radians( angles.pitch );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	float yaw = Radians( angles.yaw );
	float sy = sinf( yaw );
	float cy = cosf( yaw );
	float roll = Radians( angles.roll );
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

EulerDegrees3 VecToAngles( Vec3 vec ) {
	if( vec.xy() == Vec2( 0.0f ) ) {
		if( vec.z == 0.0f ) {
			return EulerDegrees3( 0.0f, 0.0f, 0.0f );
		}
		return vec.z > 0 ? EulerDegrees3( -90.0f, 0.0f, 0.0f ) : EulerDegrees3( -270.0f, 0.0f, 0.0f );
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

	return EulerDegrees3( -pitch, yaw, 0.0f );
}

void AnglesToAxis( EulerDegrees3 angles, mat3_t axis ) {
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

EulerDegrees3 LerpAngles( EulerDegrees3 a, float t, EulerDegrees3 b ) {
	return EulerDegrees3(
		LerpAngle( a.pitch, t, b.pitch ),
		LerpAngle( a.yaw, t, b.yaw ),
		LerpAngle( a.roll, t, b.roll )
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

bool BoundsOverlap( const MinMax3 & a, const MinMax3 & b ) {
	for( int i = 0; i < 3; i++ ) {
		if( a.maxs[ i ] < b.mins[ i ] || a.mins[ i ] > b.maxs[ i ] ) {
			return false;
		}
	}

	return true;
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
	Assert( dim.z >= dim.x && dim.x == dim.y );

	Capsule capsule;
	capsule.radius = bounds.maxs.x;
	capsule.a = Vec3( center.xy(), bounds.mins.z + capsule.radius );
	capsule.b = Vec3( center.xy(), bounds.maxs.z - capsule.radius );

	return capsule;
}

//============================================================================

void Matrix3_TransformVector( const mat3_t m, Vec3 v, Vec3 * out ) {
	out->x = m[0] * v.x + m[1] * v.y + m[2] * v.z;
	out->y = m[3] * v.x + m[4] * v.y + m[5] * v.z;
	out->z = m[6] * v.x + m[7] * v.y + m[8] * v.z;
}

void Matrix3_FromAngles( EulerDegrees3 angles, mat3_t m ) {
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
	Assert( theta >= 0.0f && theta <= PI );
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

Mat3x4 Mat4Rotation( EulerDegrees3 angles ) {
	float pitch = Radians( angles.pitch );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	Mat3x4 rp(
		cp, 0, sp, 0,
		0, 1, 0, 0,
		-sp, 0, cp, 0
	);
	float yaw = Radians( angles.yaw );
	float sy = sinf( yaw );
	float cy = cosf( yaw );
	Mat3x4 ry(
		cy, -sy, 0, 0,
		sy, cy, 0, 0,
		0, 0, 1, 0
	);
	float roll = Radians( angles.roll );
	float sr = sinf( roll );
	float cr = cosf( roll );
	Mat3x4 rr(
		1, 0, 0, 0,
		0, cr, -sr, 0,
		0, sr, cr, 0
	);

	return ry * rp * rr;
}

Quaternion EulerDegrees3ToQuaternion( EulerDegrees3 angles ) {
	float cp = cosf( Radians( angles.pitch ) * 0.5f );
	float sp = sinf( Radians( angles.pitch ) * 0.5f );
	float cy = cosf( Radians( angles.yaw ) * 0.5f );
	float sy = sinf( Radians( angles.yaw ) * 0.5f );
	float cr = cosf( Radians( angles.roll ) * 0.5f );
	float sr = sinf( Radians( angles.roll ) * 0.5f );

	return Quaternion(
		cp * cy * sr - sp * sy * cr,
		sp * cy * cr + cp * sy * sr,
		cp * sy * cr - sp * cy * sr,
		cp * cy * cr + sp * sy * sr
	);
}

Quaternion QuaternionFromAxisAndRadians( Vec3 axis, float radians ) {
	float s = sinf( radians * 0.5f );
	return Quaternion( axis.x * s, axis.y * s, axis.z * s, cosf( radians * 0.5f ) );
}

static Quaternion QuaternionFromAxisAndCosine( Vec3 axis, float cosine ) {
	// using half angle identities, theta is always in [0..pi) so always take the positive
	float c = sqrtf( ( 1.0f + cosine ) * 0.5f );
	float s = sqrtf( ( 1.0f - cosine ) * 0.5f );
	return Quaternion( axis.x * s, axis.y * s, axis.z * s, c );
}

Quaternion QuaternionFromNormalAndRadians( Vec3 normal, float radians ) {
	constexpr Vec3 x = Vec3( 1.0f, 0.0f, 0.0f );
	float d = Dot( normal, x );

	Vec3 axis;

	if( Abs( d ) > 0.999f ) {
		axis = Vec3( 0.0f, 0.0f, 1.0f );
	}
	else {
		axis = Normalize( Cross( x, normal ) );
	}

	return QuaternionFromAxisAndRadians( normal, radians ) * QuaternionFromAxisAndCosine( axis, Dot( x, normal ) );
}

Quaternion BasisToQuaternion( Vec3 normal, Vec3 tangent, Vec3 bitangent ) {
	// https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
	// "Converting a Rotation Matrix to a Quaternion - Mike Day, Insomniac Games"

	Quaternion q;
	float t;
	if( bitangent.z < 0.0f ) {
		if( normal.x > tangent.y ) {
			t = 1.0f + normal.x - tangent.y - bitangent.z;
			q = Quaternion( t, normal.y + tangent.x, bitangent.x + normal.z, tangent.z - bitangent.y );
		}
		else {
			t = 1.0f - normal.x + tangent.y - bitangent.z;
			q = Quaternion( normal.y + tangent.x, t, tangent.z + bitangent.y, bitangent.x - normal.z );
		}
	}
	else {
		if( normal.x < -tangent.y )
		{
			t = 1.0f - normal.x - tangent.y + bitangent.z;
			q = Quaternion( bitangent.x + normal.z, tangent.z + bitangent.y, t, normal.y - tangent.x );
		}
		else {
			t = 1.0f + normal.x + tangent.y + bitangent.z;
			q = Quaternion( tangent.z - bitangent.y, bitangent.x - normal.z, normal.y - tangent.x, t );
		}
	}
	return q * ( 0.5f / sqrtf( t ) );
}

MinMax3 Union( const MinMax3 & bounds, Vec3 p ) {
	return MinMax3(
		Vec3( Min2( bounds.mins.x, p.x ), Min2( bounds.mins.y, p.y ), Min2( bounds.mins.z, p.z ) ),
		Vec3( Max2( bounds.maxs.x, p.x ), Max2( bounds.maxs.y, p.y ), Max2( bounds.maxs.z, p.z ) )
	);
}

MinMax3 Union( const MinMax3 & a, const MinMax3 & b ) {
	return MinMax3(
		Vec3( Min2( a.mins.x, b.mins.x ), Min2( a.mins.y, b.mins.y ), Min2( a.mins.z, b.mins.z ) ),
		Vec3( Max2( a.maxs.x, b.maxs.x ), Max2( a.maxs.y, b.maxs.y ), Max2( a.maxs.z, b.maxs.z ) )
	);
}

MinMax1 Union( MinMax1 a, float x ) {
	return MinMax1( Min2( a.lo, x ), Max2( a.hi, x ) );
}

MinMax1 Union( MinMax1 a, MinMax1 b ) {
	return MinMax1( Min2( a.lo, b.lo ), Max2( a.hi, b.hi ) );
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
