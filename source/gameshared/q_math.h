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

#pragma once

#include "gameshared/q_arch.h"
#include "qcommon/types.h"

//==============================================================
//
//MATHLIB
//
//==============================================================

enum {
	PITCH = 0,      // up / down
	YAW = 1,        // left / right
	ROLL = 2        // fall over
};

enum {
	AXIS_FORWARD = 0,
	AXIS_RIGHT = 3,
	AXIS_UP = 6
};

typedef float mat3_t[9];

struct cplane_t {
	Vec3 normal;
	float dist;
};

constexpr mat3_t axis_identity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

#define qmin( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

#define PlaneDiff( point, plane ) ( Dot( ( point ), ( plane )->normal ) - ( plane )->dist )

void ClearBounds( Vec3 * mins, Vec3 * maxs );
void AddPointToBounds( Vec3 v, Vec3 * mins, Vec3 * maxs );
float RadiusFromBounds( Vec3 mins, Vec3 maxs );
bool BoundsOverlap( const Vec3 & mins1, const Vec3 & maxs1, const Vec3 & mins2, const Vec3 & maxs2 );
bool BoundsOverlapSphere( Vec3 mins, Vec3 maxs, Vec3 centre, float radius );

u64 DirToU64( Vec3 dir );
Vec3 U64ToDir( u64 v );

void ViewVectors( Vec3 forward, Vec3 * right, Vec3 * up );
void AngleVectors( Vec3 angles, Vec3 * forward, Vec3 * right, Vec3 * up );
Vec3 LerpAngles( Vec3 a, float t, Vec3 b );
float AngleNormalize360( float angle );
float AngleNormalize180( float angle );
float AngleDelta( float angle1, float angle2 );
Vec3 AngleDelta( Vec3 angle1, Vec3 angle2 );
EulerDegrees2 AngleDelta( EulerDegrees2 a, EulerDegrees2 b );
Vec3 VecToAngles( Vec3 vec );
void AnglesToAxis( Vec3 angles, mat3_t axis );
void OrthonormalBasis( Vec3 v, Vec3 * tangent, Vec3 * bitangent );
void BuildBoxPoints( Vec3 p[8], Vec3 org, Vec3 mins, Vec3 maxs );

float WidescreenFov( float fov );
float CalcHorizontalFov( float fov_y, float width, float height );

#define Q_rint( x ) ( ( x ) < 0 ? ( (int)( ( x ) - 0.5f ) ) : ( (int)( ( x ) + 0.5f ) ) )

bool PlaneFromPoints( Vec3 verts[3], cplane_t *plane );

bool ComparePlanes( Vec3 p1normal, float p1dist, Vec3 p2normal, float p2dist );
void SnapVector( Vec3 * normal );
void SnapPlane( Vec3 * normal, float *dist );

void ProjectPointOntoVector( Vec3 point, Vec3 vStart, Vec3 vDir, Vec3 * vProj );

void Matrix3_Identity( mat3_t m );
void Matrix3_Copy( const mat3_t m1, mat3_t m2 );
void Matrix3_Multiply( const mat3_t m1, const mat3_t m2, mat3_t out );
void Matrix3_TransformVector( const mat3_t m, Vec3 v, Vec3 * out );
void Matrix3_FromAngles( Vec3 angles, mat3_t m );

float PositiveMod( float x, float y );
double PositiveMod( double x, double y );

struct RNG;

Vec3 UniformSampleSphere( RNG * rng );
Vec3 UniformSampleInsideSphere( RNG * rng );
Vec3 UniformSampleCone( RNG * rng, float theta );
Vec2 UniformSampleDisk( RNG * rng );
float SampleNormalDistribution( RNG * rng );

Vec3 Project( Vec3 a, Vec3 b );
Vec3 ClosestPointOnLine( Vec3 p0, Vec3 p1, Vec3 p );
Vec3 ClosestPointOnSegment( Vec3 start, Vec3 end, Vec3 p );

Mat4 TransformKToDir( Vec3 dir );

MinMax3 Extend( MinMax3 bounds, Vec3 p );
