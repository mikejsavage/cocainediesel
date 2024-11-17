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

bool BoundsOverlap( const MinMax3 & a, const MinMax3 & b );

CenterExtents3 ToCenterExtents( const MinMax3 & bounds );
MinMax3 ToMinMax( const CenterExtents3 & aabb );
Capsule MakePlayerCapsule( const MinMax3 & bounds );

u64 DirToU64( Vec3 dir );
Vec3 U64ToDir( u64 v );

float SignedOne( float x );

void ViewVectors( Vec3 forward, Vec3 * right, Vec3 * up );
void AngleVectors( EulerDegrees3 angles, Vec3 * forward, Vec3 * right, Vec3 * up );
EulerDegrees3 LerpAngles( EulerDegrees3 a, float t, EulerDegrees3 b );
float AngleNormalize360( float angle );
float AngleNormalize180( float angle );
float AngleDelta( float angle1, float angle2 );
Vec3 AngleDelta( Vec3 angle1, Vec3 angle2 );
EulerDegrees2 AngleDelta( EulerDegrees2 a, EulerDegrees2 b );
EulerDegrees3 VecToAngles( Vec3 vec );
void AnglesToAxis( EulerDegrees3 angles, mat3_t axis );
void OrthonormalBasis( Vec3 v, Vec3 * tangent, Vec3 * bitangent );

void Matrix3_TransformVector( const mat3_t m, Vec3 v, Vec3 * out );
void Matrix3_FromAngles( EulerDegrees3 angles, mat3_t m );

int PositiveMod( int x, int y );
float PositiveMod( float x, float y );
double PositiveMod( double x, double y );

template< typename T, u64 Bits = sizeof( T ) * 8 >
float Dequantize01( T x ) {
	return x / float( ( 1_u64 << Bits ) - 1 );
}

template< typename T, u64 Bits = sizeof( T ) * 8 >
T Quantize01( float x ) {
	Assert( x >= 0.0f && x <= 1.0f );
	return T( x * float( ( 1_u64 << Bits ) - 1 ) + 0.5f );
}

// these map 2^n - 1 and 2^n - 2 to 1.0f so we can exactly represent 0
template< typename T, u64 Bits = sizeof( T ) * 8 >
float Dequantize11( T x ) {
	return Min2( 1.0f, ( x / float( ( 1_u64 << Bits ) - 2 ) - 0.5f ) * 2.0f );
}

template< typename T, u64 Bits = sizeof( T ) * 8 >
T Quantize11( float x ) {
	Assert( x >= -1.0f && x <= 1.0f );
	return T( ( x * 0.5f + 0.5f ) * float( ( 1_u64 << Bits ) - 2 ) + 0.5f );
}

struct RNG;

Vec3 UniformSampleOnSphere( RNG * rng );
Vec3 UniformSampleInsideSphere( RNG * rng );
Vec3 UniformSampleCone( RNG * rng, float theta );
Vec2 UniformSampleInsideCircle( RNG * rng );
float SampleNormalDistribution( RNG * rng );

Vec3 Project( Vec3 a, Vec3 b );
Vec3 ClosestPointOnSegment( Vec3 start, Vec3 end, Vec3 p );

Mat3x4 Mat4Rotation( EulerDegrees3 angles );

MinMax3 Union( const MinMax3 & bounds, Vec3 p );
MinMax3 Union( const MinMax3 & a, const MinMax3 & b );

MinMax1 Union( MinMax1 bounds, float x );
MinMax1 Union( MinMax1 a, MinMax1 b );

u32 Log2( u64 x );
