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

#include "q_arch.h"

#include <emmintrin.h>

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
	FORWARD = 0,
	RIGHT = 1,
	UP = 2
};

enum {
	AXIS_FORWARD = 0,
	AXIS_RIGHT = 3,
	AXIS_UP = 6
};

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

typedef vec_t mat3_t[9];

typedef uint8_t byte_vec4_t[4];

// 0-2 are axial planes
#define PLANE_X     0
#define PLANE_Y     1
#define PLANE_Z     2
#define PLANE_NONAXIAL  3

// cplane_t structure
typedef struct cplane_s {
	vec3_t normal;
	float dist;
	short type;                 // for fast side tests
	short signbits;             // signx + (signy<<1) + (signz<<1)
} cplane_t;

constexpr vec3_t vec3_origin = { 0, 0, 0 };
constexpr mat3_t axis_identity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

constexpr vec4_t colorBlack  = { 0, 0, 0, 1 };
constexpr vec4_t colorRed    = { 1, 0, 0, 1 };
constexpr vec4_t colorGreen  = { 0, 1, 0, 1 };
constexpr vec4_t colorBlue   = { 0, 0, 1, 1 };
constexpr vec4_t colorYellow = { 1, 1, 0, 1 };
constexpr vec4_t colorOrange = { 1, 0.5, 0, 1 };
constexpr vec4_t colorMagenta = { 1, 0, 1, 1 };
constexpr vec4_t colorCyan   = { 0, 1, 1, 1 };
constexpr vec4_t colorWhite  = { 1, 1, 1, 1 };
constexpr vec4_t colorLtGrey = { 0.75, 0.75, 0.75, 1 };
constexpr vec4_t colorMdGrey = { 0.5, 0.5, 0.5, 1 };
constexpr vec4_t colorDkGrey = { 0.25, 0.25, 0.25, 1 };

constexpr vec4_t color_table[] =
{
	{ 0.0, 0.0, 0.0, 1.0 },
	{ 1.0, 0.0, 0.0, 1.0 },
	{ 0.0, 1.0, 0.0, 1.0 },
	{ 1.0, 1.0, 0.0, 1.0 },
	{ 0.0, 0.0, 1.0, 1.0 },
	{ 0.0, 1.0, 1.0, 1.0 },
	{ 1.0, 0.0, 1.0, 1.0 }, // magenta
	{ 1.0, 1.0, 1.0, 1.0 },
	{ 1.0, 0.5, 0.0, 1.0 }, // orange
	{ 0.5, 0.5, 0.5, 1.0 }, // grey
};

#define MAX_S_COLORS ARRAY_COUNT( color_table )

constexpr double M_TWOPI = M_PI * 2.0;

#define DEG2RAD( a ) ( ( a * float( M_PI ) ) / 180.0f )
#define RAD2DEG( a ) ( ( a * 180.0f ) / float( M_PI ) )

#define max( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define min( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define bound( lo, x, hi ) ( ( lo ) >= ( hi ) ? ( lo ) : ( x ) < ( lo ) ? ( lo ) : ( x ) > ( hi ) ? ( hi ) : ( x ) )

#define random()    ( ( rand() & 0x7fff ) / ( (float)0x7fff ) )  // 0..1
#define brandom( a, b )    ( ( a ) + random() * ( ( b ) - ( a ) ) )                // a..b
#define crandom()   brandom( -1, 1 )                           // -1..1

inline float Q_RSqrt( float x ) {
	return _mm_cvtss_f32( _mm_rsqrt_ss( _mm_set_ss( x ) ) );
}

template< typename T >
T Lerp( T a, float t, T b ) {
	return a * ( 1.0f - t ) + b * t;
}

template< typename T >
float Unlerp( T lo, T x, T hi ) {
	return float( x - lo ) / float( hi - lo );
}

#define SQRTFAST( x ) ( ( x ) * Q_RSqrt( x ) ) // jal : //The expression a * rsqrt(b) is intended as a higher performance alternative to a / sqrt(b). The two expressions are comparably accurate, but do not compute exactly the same value in every case. For example, a * rsqrt(a*a + b*b) can be just slightly greater than 1, in rare cases.

#define DotProduct( x, y )     ( ( x )[0] * ( y )[0] + ( x )[1] * ( y )[1] + ( x )[2] * ( y )[2] )
#define CrossProduct( v1, v2, cross ) ( ( cross )[0] = ( v1 )[1] * ( v2 )[2] - ( v1 )[2] * ( v2 )[1], ( cross )[1] = ( v1 )[2] * ( v2 )[0] - ( v1 )[0] * ( v2 )[2], ( cross )[2] = ( v1 )[0] * ( v2 )[1] - ( v1 )[1] * ( v2 )[0] )

#define PlaneDiff( point, plane ) ( ( ( plane )->type < 3 ? ( point )[( plane )->type] : DotProduct( ( point ), ( plane )->normal ) ) - ( plane )->dist )

#define VectorSubtract( a, b, c )   ( ( c )[0] = ( a )[0] - ( b )[0], ( c )[1] = ( a )[1] - ( b )[1], ( c )[2] = ( a )[2] - ( b )[2] )
#define VectorAdd( a, b, c )        ( ( c )[0] = ( a )[0] + ( b )[0], ( c )[1] = ( a )[1] + ( b )[1], ( c )[2] = ( a )[2] + ( b )[2] )
#define VectorCopy( a, b )     ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
#define VectorClear( a )      ( ( a )[0] = ( a )[1] = ( a )[2] = 0 )
#define VectorNegate( a, b )       ( ( b )[0] = -( a )[0], ( b )[1] = -( a )[1], ( b )[2] = -( a )[2] )
#define VectorSet( v, x, y, z )   ( ( v )[0] = ( x ), ( v )[1] = ( y ), ( v )[2] = ( z ) )
#define VectorAvg( a, b, c )        ( ( c )[0] = ( ( a )[0] + ( b )[0] ) * 0.5f, ( c )[1] = ( ( a )[1] + ( b )[1] ) * 0.5f, ( c )[2] = ( ( a )[2] + ( b )[2] ) * 0.5f )
#define VectorMA( a, b, c, d )       ( ( d )[0] = ( a )[0] + ( b ) * ( c )[0], ( d )[1] = ( a )[1] + ( b ) * ( c )[1], ( d )[2] = ( a )[2] + ( b ) * ( c )[2] )
#define VectorCompare( v1, v2 )    ( ( v1 )[0] == ( v2 )[0] && ( v1 )[1] == ( v2 )[1] && ( v1 )[2] == ( v2 )[2] )
#define VectorLengthSquared( v )    ( DotProduct( ( v ), ( v ) ) )
#define VectorLength( v )     ( sqrtf( VectorLengthSquared( v ) ) )
#define VectorInverse( v )    ( ( v )[0] = -( v )[0], ( v )[1] = -( v )[1], ( v )[2] = -( v )[2] )
#define VectorLerp( a, c, b, v )     ( ( v )[0] = ( a )[0] + ( c ) * ( ( b )[0] - ( a )[0] ), ( v )[1] = ( a )[1] + ( c ) * ( ( b )[1] - ( a )[1] ), ( v )[2] = ( a )[2] + ( c ) * ( ( b )[2] - ( a )[2] ) )
#define VectorScale( in, scale, out ) ( ( out )[0] = ( in )[0] * ( scale ), ( out )[1] = ( in )[1] * ( scale ), ( out )[2] = ( in )[2] * ( scale ) )

#define DistanceSquared( v1, v2 ) ( ( ( v1 )[0] - ( v2 )[0] ) * ( ( v1 )[0] - ( v2 )[0] ) + ( ( v1 )[1] - ( v2 )[1] ) * ( ( v1 )[1] - ( v2 )[1] ) + ( ( v1 )[2] - ( v2 )[2] ) * ( ( v1 )[2] - ( v2 )[2] ) )
#define Distance( v1, v2 ) ( sqrtf( DistanceSquared( v1, v2 ) ) )

#define VectorLengthFast( v )     ( SQRTFAST( DotProduct( ( v ), ( v ) ) ) )  // jal :  //The expression a * rsqrt(b) is intended as a higher performance alternative to a / sqrt(b). The two expressions are comparably accurate, but do not compute exactly the same value in every case. For example, a * rsqrt(a*a + b*b) can be just slightly greater than 1, in rare cases.
#define DistanceFast( v1, v2 )     ( SQRTFAST( DistanceSquared( v1, v2 ) ) )  // jal :  //The expression a * rsqrt(b) is intended as a higher performance alternative to a / sqrt(b). The two expressions are comparably accurate, but do not compute exactly the same value in every case. For example, a * rsqrt(a*a + b*b) can be just slightly greater than 1, in rare cases.

#define Vector2Set( v, x, y )     ( ( v )[0] = ( x ), ( v )[1] = ( y ) )
#define Vector2Copy( a, b )    ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1] )

#define Vector4Set( v, a, b, c, d )   ( ( v )[0] = ( a ), ( v )[1] = ( b ), ( v )[2] = ( c ), ( v )[3] = ( d ) )
#define Vector4Clear( a )     ( ( a )[0] = ( a )[1] = ( a )[2] = ( a )[3] = 0 )
#define Vector4Copy( a, b )    ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2], ( b )[3] = ( a )[3] )
#define Vector4Scale( in, scale, out )      ( ( out )[0] = ( in )[0] * scale, ( out )[1] = ( in )[1] * scale, ( out )[2] = ( in )[2] * scale, ( out )[3] = ( in )[3] * scale )

vec_t VectorNormalize( vec3_t v );       // returns vector length
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void  VectorNormalizeFast( vec3_t v );

vec_t Vector4Normalize( vec4_t v );      // returns vector length

void ClearBounds( vec3_t mins, vec3_t maxs );
void CopyBounds( const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
bool BoundsOverlap( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2 );
bool BoundsOverlapSphere( const vec3_t mins, const vec3_t maxs, const vec3_t centre, float radius );

#define NUMVERTEXNORMALS    162
int DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );

void OrthonormalBasis( const vec3_t forward, vec3_t right, vec3_t up );
void ViewVectors( const vec3_t forward, vec3_t right, vec3_t up );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const struct cplane_s *plane );
float LerpAngle( float a1, float a2, const float frac );
float AngleNormalize360( float angle );
float AngleNormalize180( float angle );
float AngleDelta( float angle1, float angle2 );
void VecToAngles( const vec3_t vec, vec3_t angles );
void AnglesToAxis( const vec3_t angles, mat3_t axis );
void NormalVectorToAxis( const vec3_t forward, mat3_t axis );
void BuildBoxPoints( vec3_t p[8], const vec3_t org, const vec3_t mins, const vec3_t maxs );

float WidescreenFov( float fov );
float CalcHorizontalFov( float fov_y, float width, float height );

#define Q_rint( x ) ( ( x ) < 0 ? ( (int)( ( x ) - 0.5f ) ) : ( (int)( ( x ) + 0.5f ) ) )

int SignbitsForPlane( const cplane_t *out );
int PlaneTypeForNormal( const vec3_t normal );
void CategorizePlane( cplane_t *plane );
void PlaneFromPoints( vec3_t verts[3], cplane_t *plane );

bool ComparePlanes( const vec3_t p1normal, vec_t p1dist, const vec3_t p2normal, vec_t p2dist );
void SnapVector( vec3_t normal );
void SnapPlane( vec3_t normal, vec_t *dist );

#define BOX_ON_PLANE_SIDE( emins, emaxs, p )  \
	( ( ( p )->type < 3 ) ?                       \
	  (                                       \
		  ( ( p )->dist <= ( emins )[( p )->type] ) ?  \
		  1                               \
		  :                                   \
		  (                                   \
			  ( ( p )->dist >= ( emaxs )[( p )->type] ) ? \
			  2                           \
			  :                               \
			  3                           \
		  )                                   \
	  )                                       \
	  :                                       \
	  BoxOnPlaneSide( ( emins ), ( emaxs ), ( p ) ) )

void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void ProjectPointOntoPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void ProjectPointOntoVector( const vec3_t point, const vec3_t vStart, const vec3_t vDir, vec3_t vProj );

void Matrix3_Identity( mat3_t m );
void Matrix3_Copy( const mat3_t m1, mat3_t m2 );
bool Matrix3_Compare( const mat3_t m1, const mat3_t m2 );
void Matrix3_Multiply( const mat3_t m1, const mat3_t m2, mat3_t out );
void Matrix3_TransformVector( const mat3_t m, const vec3_t v, vec3_t out );
void Matrix3_Transpose( const mat3_t in, mat3_t out );
void Matrix3_FromAngles( const vec3_t angles, mat3_t m );

float PositiveMod( float x, float y );
double PositiveMod( double x, double y );
