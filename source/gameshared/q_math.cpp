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

#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_collision.h"
#include "qcommon/base.h"
#include "qcommon/rng.h"

static const Vec3 bytedirs[NUMVERTEXNORMALS] = {
	Vec3( -0.525731f, 0.000000f, 0.850651f ),
	Vec3( -0.442863f, 0.238856f, 0.864188f ),
	Vec3( -0.295242f, 0.000000f, 0.955423f ),
	Vec3( -0.309017f, 0.500000f, 0.809017f ),
	Vec3( -0.162460f, 0.262866f, 0.951056f ),
	Vec3( 0.000000f, 0.000000f, 1.000000f ),
	Vec3( 0.000000f, 0.850651f, 0.525731f ),
	Vec3( -0.147621f, 0.716567f, 0.681718f ),
	Vec3( 0.147621f, 0.716567f, 0.681718f ),
	Vec3( 0.000000f, 0.525731f, 0.850651f ),
	Vec3( 0.309017f, 0.500000f, 0.809017f ),
	Vec3( 0.525731f, 0.000000f, 0.850651f ),
	Vec3( 0.295242f, 0.000000f, 0.955423f ),
	Vec3( 0.442863f, 0.238856f, 0.864188f ),
	Vec3( 0.162460f, 0.262866f, 0.951056f ),
	Vec3( -0.681718f, 0.147621f, 0.716567f ),
	Vec3( -0.809017f, 0.309017f, 0.500000f ),
	Vec3( -0.587785f, 0.425325f, 0.688191f ),
	Vec3( -0.850651f, 0.525731f, 0.000000f ),
	Vec3( -0.864188f, 0.442863f, 0.238856f ),
	Vec3( -0.716567f, 0.681718f, 0.147621f ),
	Vec3( -0.688191f, 0.587785f, 0.425325f ),
	Vec3( -0.500000f, 0.809017f, 0.309017f ),
	Vec3( -0.238856f, 0.864188f, 0.442863f ),
	Vec3( -0.425325f, 0.688191f, 0.587785f ),
	Vec3( -0.716567f, 0.681718f, -0.147621f ),
	Vec3( -0.500000f, 0.809017f, -0.309017f ),
	Vec3( -0.525731f, 0.850651f, 0.000000f ),
	Vec3( 0.000000f, 0.850651f, -0.525731f ),
	Vec3( -0.238856f, 0.864188f, -0.442863f ),
	Vec3( 0.000000f, 0.955423f, -0.295242f ),
	Vec3( -0.262866f, 0.951056f, -0.162460f ),
	Vec3( 0.000000f, 1.000000f, 0.000000f ),
	Vec3( 0.000000f, 0.955423f, 0.295242f ),
	Vec3( -0.262866f, 0.951056f, 0.162460f ),
	Vec3( 0.238856f, 0.864188f, 0.442863f ),
	Vec3( 0.262866f, 0.951056f, 0.162460f ),
	Vec3( 0.500000f, 0.809017f, 0.309017f ),
	Vec3( 0.238856f, 0.864188f, -0.442863f ),
	Vec3( 0.262866f, 0.951056f, -0.162460f ),
	Vec3( 0.500000f, 0.809017f, -0.309017f ),
	Vec3( 0.850651f, 0.525731f, 0.000000f ),
	Vec3( 0.716567f, 0.681718f, 0.147621f ),
	Vec3( 0.716567f, 0.681718f, -0.147621f ),
	Vec3( 0.525731f, 0.850651f, 0.000000f ),
	Vec3( 0.425325f, 0.688191f, 0.587785f ),
	Vec3( 0.864188f, 0.442863f, 0.238856f ),
	Vec3( 0.688191f, 0.587785f, 0.425325f ),
	Vec3( 0.809017f, 0.309017f, 0.500000f ),
	Vec3( 0.681718f, 0.147621f, 0.716567f ),
	Vec3( 0.587785f, 0.425325f, 0.688191f ),
	Vec3( 0.955423f, 0.295242f, 0.000000f ),
	Vec3( 1.000000f, 0.000000f, 0.000000f ),
	Vec3( 0.951056f, 0.162460f, 0.262866f ),
	Vec3( 0.850651f, -0.525731f, 0.000000f ),
	Vec3( 0.955423f, -0.295242f, 0.000000f ),
	Vec3( 0.864188f, -0.442863f, 0.238856f ),
	Vec3( 0.951056f, -0.162460f, 0.262866f ),
	Vec3( 0.809017f, -0.309017f, 0.500000f ),
	Vec3( 0.681718f, -0.147621f, 0.716567f ),
	Vec3( 0.850651f, 0.000000f, 0.525731f ),
	Vec3( 0.864188f, 0.442863f, -0.238856f ),
	Vec3( 0.809017f, 0.309017f, -0.500000f ),
	Vec3( 0.951056f, 0.162460f, -0.262866f ),
	Vec3( 0.525731f, 0.000000f, -0.850651f ),
	Vec3( 0.681718f, 0.147621f, -0.716567f ),
	Vec3( 0.681718f, -0.147621f, -0.716567f ),
	Vec3( 0.850651f, 0.000000f, -0.525731f ),
	Vec3( 0.809017f, -0.309017f, -0.500000f ),
	Vec3( 0.864188f, -0.442863f, -0.238856f ),
	Vec3( 0.951056f, -0.162460f, -0.262866f ),
	Vec3( 0.147621f, 0.716567f, -0.681718f ),
	Vec3( 0.309017f, 0.500000f, -0.809017f ),
	Vec3( 0.425325f, 0.688191f, -0.587785f ),
	Vec3( 0.442863f, 0.238856f, -0.864188f ),
	Vec3( 0.587785f, 0.425325f, -0.688191f ),
	Vec3( 0.688191f, 0.587785f, -0.425325f ),
	Vec3( -0.147621f, 0.716567f, -0.681718f ),
	Vec3( -0.309017f, 0.500000f, -0.809017f ),
	Vec3( 0.000000f, 0.525731f, -0.850651f ),
	Vec3( -0.525731f, 0.000000f, -0.850651f ),
	Vec3( -0.442863f, 0.238856f, -0.864188f ),
	Vec3( -0.295242f, 0.000000f, -0.955423f ),
	Vec3( -0.162460f, 0.262866f, -0.951056f ),
	Vec3( 0.000000f, 0.000000f, -1.000000f ),
	Vec3( 0.295242f, 0.000000f, -0.955423f ),
	Vec3( 0.162460f, 0.262866f, -0.951056f ),
	Vec3( -0.442863f, -0.238856f, -0.864188f ),
	Vec3( -0.309017f, -0.500000f, -0.809017f ),
	Vec3( -0.162460f, -0.262866f, -0.951056f ),
	Vec3( 0.000000f, -0.850651f, -0.525731f ),
	Vec3( -0.147621f, -0.716567f, -0.681718f ),
	Vec3( 0.147621f, -0.716567f, -0.681718f ),
	Vec3( 0.000000f, -0.525731f, -0.850651f ),
	Vec3( 0.309017f, -0.500000f, -0.809017f ),
	Vec3( 0.442863f, -0.238856f, -0.864188f ),
	Vec3( 0.162460f, -0.262866f, -0.951056f ),
	Vec3( 0.238856f, -0.864188f, -0.442863f ),
	Vec3( 0.500000f, -0.809017f, -0.309017f ),
	Vec3( 0.425325f, -0.688191f, -0.587785f ),
	Vec3( 0.716567f, -0.681718f, -0.147621f ),
	Vec3( 0.688191f, -0.587785f, -0.425325f ),
	Vec3( 0.587785f, -0.425325f, -0.688191f ),
	Vec3( 0.000000f, -0.955423f, -0.295242f ),
	Vec3( 0.000000f, -1.000000f, 0.000000f ),
	Vec3( 0.262866f, -0.951056f, -0.162460f ),
	Vec3( 0.000000f, -0.850651f, 0.525731f ),
	Vec3( 0.000000f, -0.955423f, 0.295242f ),
	Vec3( 0.238856f, -0.864188f, 0.442863f ),
	Vec3( 0.262866f, -0.951056f, 0.162460f ),
	Vec3( 0.500000f, -0.809017f, 0.309017f ),
	Vec3( 0.716567f, -0.681718f, 0.147621f ),
	Vec3( 0.525731f, -0.850651f, 0.000000f ),
	Vec3( -0.238856f, -0.864188f, -0.442863f ),
	Vec3( -0.500000f, -0.809017f, -0.309017f ),
	Vec3( -0.262866f, -0.951056f, -0.162460f ),
	Vec3( -0.850651f, -0.525731f, 0.000000f ),
	Vec3( -0.716567f, -0.681718f, -0.147621f ),
	Vec3( -0.716567f, -0.681718f, 0.147621f ),
	Vec3( -0.525731f, -0.850651f, 0.000000f ),
	Vec3( -0.500000f, -0.809017f, 0.309017f ),
	Vec3( -0.238856f, -0.864188f, 0.442863f ),
	Vec3( -0.262866f, -0.951056f, 0.162460f ),
	Vec3( -0.864188f, -0.442863f, 0.238856f ),
	Vec3( -0.809017f, -0.309017f, 0.500000f ),
	Vec3( -0.688191f, -0.587785f, 0.425325f ),
	Vec3( -0.681718f, -0.147621f, 0.716567f ),
	Vec3( -0.442863f, -0.238856f, 0.864188f ),
	Vec3( -0.587785f, -0.425325f, 0.688191f ),
	Vec3( -0.309017f, -0.500000f, 0.809017f ),
	Vec3( -0.147621f, -0.716567f, 0.681718f ),
	Vec3( -0.425325f, -0.688191f, 0.587785f ),
	Vec3( -0.162460f, -0.262866f, 0.951056f ),
	Vec3( 0.442863f, -0.238856f, 0.864188f ),
	Vec3( 0.162460f, -0.262866f, 0.951056f ),
	Vec3( 0.309017f, -0.500000f, 0.809017f ),
	Vec3( 0.147621f, -0.716567f, 0.681718f ),
	Vec3( 0.000000f, -0.525731f, 0.850651f ),
	Vec3( 0.425325f, -0.688191f, 0.587785f ),
	Vec3( 0.587785f, -0.425325f, 0.688191f ),
	Vec3( 0.688191f, -0.587785f, 0.425325f ),
	Vec3( -0.955423f, 0.295242f, 0.000000f ),
	Vec3( -0.951056f, 0.162460f, 0.262866f ),
	Vec3( -1.000000f, 0.000000f, 0.000000f ),
	Vec3( -0.850651f, 0.000000f, 0.525731f ),
	Vec3( -0.955423f, -0.295242f, 0.000000f ),
	Vec3( -0.951056f, -0.162460f, 0.262866f ),
	Vec3( -0.864188f, 0.442863f, -0.238856f ),
	Vec3( -0.951056f, 0.162460f, -0.262866f ),
	Vec3( -0.809017f, 0.309017f, -0.500000f ),
	Vec3( -0.864188f, -0.442863f, -0.238856f ),
	Vec3( -0.951056f, -0.162460f, -0.262866f ),
	Vec3( -0.809017f, -0.309017f, -0.500000f ),
	Vec3( -0.681718f, 0.147621f, -0.716567f ),
	Vec3( -0.681718f, -0.147621f, -0.716567f ),
	Vec3( -0.850651f, 0.000000f, -0.525731f ),
	Vec3( -0.688191f, 0.587785f, -0.425325f ),
	Vec3( -0.587785f, 0.425325f, -0.688191f ),
	Vec3( -0.425325f, 0.688191f, -0.587785f ),
	Vec3( -0.425325f, -0.688191f, -0.587785f ),
	Vec3( -0.587785f, -0.425325f, -0.688191f ),
	Vec3( -0.688191f, -0.587785f, -0.425325f ),
};

int DirToByte( Vec3 dir ) {
	if( dir == Vec3( 0.0f ) ) {
		return NUMVERTEXNORMALS;
	}

	bool normalized = Dot( dir, dir ) == 1;

	float bestd = 0;
	int best = 0;
	for( int i = 0; i < NUMVERTEXNORMALS; i++ ) {
		float d = Dot( dir, bytedirs[i] );
		if( d == 1 && normalized ) {
			return i;
		}
		if( d > bestd ) {
			bestd = d;
			best = i;
		}
	}

	return best;
}

Vec3 ByteToDir( int b ) {
	if( b < 0 || b >= NUMVERTEXNORMALS ) {
		return Vec3( 0.0f );
	} else {
		return bytedirs[b];
	}
}

//============================================================================

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
	float s = copysignf( 1.0f, v.z );
	float a = -1.0f / ( s + v.z );
	float b = v.x * v.y * a;

	*tangent = Vec3( 1.0f + s * v.x * v.x * a, s * b, -s * v.x );
	*bitangent = Vec3( b, s + v.y * v.y * a, -v.y );
}

void BuildBoxPoints( Vec3 p[8], Vec3 org, Vec3 mins, Vec3 maxs ) {
	p[0] = org + mins;
	p[1] = org + maxs;
	p[2] = Vec3( p[0].x, p[0].y, p[1].z );
	p[3] = Vec3( p[0].x, p[1].y, p[0].z );
	p[4] = Vec3( p[0].x, p[1].y, p[1].z );
	p[5] = Vec3( p[1].x, p[1].y, p[0].z );
	p[6] = Vec3( p[1].x, p[0].y, p[1].z );
	p[7] = Vec3( p[1].x, p[0].y, p[0].z );
}

/*
* ProjectPointOntoVector
*/
void ProjectPointOntoVector( Vec3 point, Vec3 vStart, Vec3 vDir, Vec3 * vProj ) {
	Vec3 pVec = point - vStart;
	// project onto the directional vector for this segment
	*vProj = vStart + vDir * Dot( pVec, vDir );
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

/*
* AngleNormalize360
*
* returns angle normalized to the range [0 <= angle < 360]
*/
float AngleNormalize360( float angle ) {
	return angle - 360.0f * floorf( angle / 360.0f );
}

/*
* AngleNormalize180
*
* returns angle normalized to the range [-180 < angle <= 180]
*/
float AngleNormalize180( float angle ) {
	angle = AngleNormalize360( angle );
	if( angle > 180.0f ) {
		angle -= 360.0f;
	}
	return angle;
}

/*
* AngleDelta
*
* returns the normalized delta from angle1 to angle2
*/
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

/*
* WidescreenFov
*/
float WidescreenFov( float fov ) {
	return atanf( tanf( fov / 360.0f * PI ) * 0.75f ) * ( 360.0f / PI );
}

/*
* CalcHorizontalFov
*/
float CalcHorizontalFov( float fov_y, float width, float height ) {
	float x;

	if( fov_y < 1 || fov_y > 179 ) {
		Sys_Error( "Bad vertical fov: %f", fov_y );
	}

	x = width;
	x *= tanf( fov_y / 360.0f * PI );
	return atanf( x / height ) * 360.0f / PI;
}

/*
* PlaneFromPoints
*/
bool PlaneFromPoints( Vec3 verts[3], cplane_t *plane ) {
	if( verts[ 0 ] == verts[ 1 ] || verts[ 0 ] == verts[ 2 ] || verts[ 1 ] == verts[ 2 ] )
		return false;

	Vec3 v1 = verts[1] - verts[0];
	Vec3 v2 = verts[2] - verts[0];
	plane->normal = Normalize( Cross( v2, v1 ) );
	plane->dist = Dot( verts[0], plane->normal );

	return true;
}

#define PLANE_NORMAL_EPSILON    0.00001
#define PLANE_DIST_EPSILON  0.01

/*
* ComparePlanes
*/
bool ComparePlanes( Vec3 p1normal, float p1dist, Vec3 p2normal, float p2dist ) {
	if( Abs( p1normal.x - p2normal.x ) < PLANE_NORMAL_EPSILON
		&& Abs( p1normal.y - p2normal.y ) < PLANE_NORMAL_EPSILON
		&& Abs( p1normal.z - p2normal.z ) < PLANE_NORMAL_EPSILON
		&& Abs( p1dist - p2dist ) < PLANE_DIST_EPSILON ) {
		return true;
	}

	return false;
}

/*
* SnapVector
*/
void SnapVector( Vec3 * normal ) {
	for( int i = 0; i < 3; i++ ) {
		if( Abs( normal->ptr()[i] - 1 ) < PLANE_NORMAL_EPSILON ) {
			*normal = Vec3( 0.0f );
			normal->ptr()[i] = 1;
			break;
		}
		if( Abs( normal->ptr()[i] - -1 ) < PLANE_NORMAL_EPSILON ) {
			*normal = Vec3( 0.0f );
			normal->ptr()[i] = -1;
			break;
		}
	}
}

/*
* SnapPlane
*/
void SnapPlane( Vec3 * normal, float *dist ) {
	SnapVector( normal );

	if( Abs( *dist - Q_rint( *dist ) ) < PLANE_DIST_EPSILON ) {
		*dist = Q_rint( *dist );
	}
}

void ClearBounds( Vec3 * mins, Vec3 * maxs ) {
	*mins = Vec3( 999999.0f );
	*maxs = Vec3( -999999.0f );
}

bool BoundsOverlap( const Vec3 & mins1, const Vec3 & maxs1, const Vec3 & mins2, const Vec3 & maxs2 ) {
	return mins1.x <= maxs2.x && mins1.y <= maxs2.y && mins1.z <= maxs2.z &&
		maxs1.x >= mins2.x && maxs1.y >= mins2.y && maxs1.z >= mins2.z;
}

bool BoundsOverlapSphere( Vec3 mins, Vec3 maxs, Vec3 centre, float radius ) {
	float dist_squared = 0;

	for( int i = 0; i < 3; i++ ) {
		float x = Clamp( mins[ i ], centre[ i ], maxs[ i ] );
		float d = centre[ i ] - x;
		dist_squared += d * d;
	}

	return dist_squared <= radius * radius;
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

bool Matrix3_Compare( const mat3_t m1, const mat3_t m2 ) {
	int i;

	for( i = 0; i < 9; i++ )
		if( m1[i] != m2[i] ) {
			return false;
		}
	return true;
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

void Matrix3_Transpose( const mat3_t in, mat3_t out ) {
	out[0] = in[0];
	out[4] = in[4];
	out[8] = in[8];

	out[1] = in[3];
	out[2] = in[6];
	out[3] = in[1];
	out[5] = in[7];
	out[6] = in[2];
	out[7] = in[5];
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

float PositiveMod( float x, float y ) {
	float res = fmodf( x, y );
	if( res < 0 )
		res += y;
	return res;
}

double PositiveMod( double x, double y ) {
	double res = fmod( x, y );
	if( res < 0 )
		res += y;
	return res;
}

Vec3 UniformSampleSphere( RNG * rng ) {
	float z = random_float11( rng );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * PI * random_float01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

Vec3 UniformSampleInsideSphere( RNG * rng ) {
	Vec3 p = UniformSampleSphere( rng );
	float r = cbrtf( random_float01( rng ) );
	return p * r;
}

Vec3 UniformSampleCone( RNG * rng, float theta ) {
	assert( theta >= 0.0f && theta <= PI );
	float z = random_uniform_float( rng, cosf( theta ), 1.0f );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * PI * random_float01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

Vec2 UniformSampleDisk( RNG * rng ) {
	float theta = random_float01( rng ) * 2.0f * PI;
	float r = sqrtf( random_float01( rng ) );
	return Vec2( r * cosf( theta ), r * sinf( theta ) );
}

float SampleNormalDistribution( RNG * rng ) {
	// generate a float in (0, 1). works because prev(1) + FLT_MIN == prev(1)
	float u1 = random_float01( rng ) + FLT_MIN;
	float u2 = random_float01( rng );
	return sqrtf( -2.0f * logf( u1 ) ) * cosf( u2 * 2.0f * PI );
}

Vec3 Project( Vec3 a, Vec3 b ) {
	return Dot( a, b ) / LengthSquared( b ) * b;
}

Vec3 ClosestPointOnLine( Vec3 p0, Vec3 p1, Vec3 p ) {
	return p0 + Project( p - p0, p1 - p0 );
}

Vec3 ClosestPointOnSegment( Vec3 start, Vec3 end, Vec3 p ) {
	Vec3 seg = end - start;
	float t = Dot( p - start, seg ) / Dot( seg, seg );
	return Lerp( start, Clamp01( t ), end );
}

Mat4 TransformKToDir( Vec3 dir ) {
	assert( ( Length( dir ) - 1.0f ) < 0.0001f );

	Vec3 K = Vec3( 0, 0, 1 );

	if( Abs( dir.z ) >= 0.9999f ) {
		return dir.z > 0 ? Mat4::Identity() : -Mat4::Identity();
	}

	Vec3 axis = Normalize( Cross( K, dir ) );
	float c = Dot( K, dir ) / Length( dir );
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
