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

#include "q_math.h"
#include "q_shared.h"
#include "q_collision.h"
#include "qcommon/rng.h"

//============================================================================

vec3_t bytedirs[NUMVERTEXNORMALS] =
{
#include "anorms.h"
};

int DirToByte( vec3_t dir ) {
	int i, best;
	float d, bestd;
	bool normalized;

	if( !dir || VectorCompare( dir, vec3_origin ) ) {
		return NUMVERTEXNORMALS;
	}

	if( DotProduct( dir, dir ) == 1 ) {
		normalized = true;
	} else {
		normalized = false;
	}

	bestd = 0;
	best = 0;
	for( i = 0; i < NUMVERTEXNORMALS; i++ ) {
		d = DotProduct( dir, bytedirs[i] );
		if( ( d == 1 ) && normalized ) {
			return i;
		}
		if( d > bestd ) {
			bestd = d;
			best = i;
		}
	}

	return best;
}

void ByteToDir( int b, vec3_t dir ) {
	if( b < 0 || b >= NUMVERTEXNORMALS ) {
		VectorSet( dir, 0, 0, 0 );
	} else {
		VectorCopy( bytedirs[b], dir );
	}
}

//============================================================================

void OrthonormalBasis( const vec3_t forward, vec3_t right, vec3_t up ) {
	float d;

	// this rotate and negate guarantees a vector not colinear with the original
	VectorSet( right, forward[2], -forward[0], forward[1] );
	d = DotProduct( right, forward );
	VectorMA( right, -d, forward, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
}

void ViewVectors( const vec3_t forward, vec3_t right, vec3_t up ) {
	vec3_t world_up = { 0, 0, 1 };

	CrossProduct( forward, world_up, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
	VectorNormalize( up );
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees ) {
	float t0, t1;
	float c, s;
	vec3_t vr, vu, vf;

	s = DEG2RAD( degrees );
	c = cos( s );
	s = sin( s );

	VectorCopy( dir, vf );
	OrthonormalBasis( vf, vr, vu );

	t0 = vr[0] * c + vu[0] * -s;
	t1 = vr[0] * s + vu[0] *  c;
	dst[0] = ( t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0] ) * point[0]
			 + ( t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1] ) * point[1]
			 + ( t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2] ) * point[2];

	t0 = vr[1] * c + vu[1] * -s;
	t1 = vr[1] * s + vu[1] *  c;
	dst[1] = ( t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0] ) * point[0]
			 + ( t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1] ) * point[1]
			 + ( t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2] ) * point[2];

	t0 = vr[2] * c + vu[2] * -s;
	t1 = vr[2] * s + vu[2] *  c;
	dst[2] = ( t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0] ) * point[0]
			 + ( t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1] ) * point[1]
			 + ( t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2] ) * point[2];
}

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up ) {
	float yaw = DEG2RAD( angles[YAW] );
	float sy = sinf( yaw );
	float cy = cosf( yaw );
	float pitch = DEG2RAD( angles[PITCH] );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	float roll = DEG2RAD( angles[ROLL] );
	float sr = sinf( roll );
	float cr = cosf( roll );

	if( forward ) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if( right ) {
		float t = sr * sp;
		right[0] = ( -1 * t * cy + -1 * cr * -sy );
		right[1] = ( -1 * t * sy + -1 * cr * cy );
		right[2] = -1 * sr * cp;
	}
	if( up ) {
		float t = cr * sp;
		up[0] = ( t * cy + -sr * -sy );
		up[1] = ( t * sy + -sr * cy );
		up[2] = cr * cp;
	}
}

void VecToAngles( const vec3_t vec, vec3_t angles ) {
	float forward;
	float yaw, pitch;

	if( vec[1] == 0 && vec[0] == 0 ) {
		yaw = 0;
		if( vec[2] > 0 ) {
			pitch = 90;
		} else {
			pitch = 270;
		}
	} else {
		if( vec[0] ) {
			yaw = RAD2DEG( atan2( vec[1], vec[0] ) );
		} else if( vec[1] > 0 ) {
			yaw = 90;
		} else {
			yaw = -90;
		}
		if( yaw < 0 ) {
			yaw += 360;
		}

		forward = sqrt( vec[0] * vec[0] + vec[1] * vec[1] );
		pitch = RAD2DEG( atan2( vec[2], forward ) );
		if( pitch < 0 ) {
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

void AnglesToAxis( const vec3_t angles, mat3_t axis ) {
	AngleVectors( angles, &axis[0], &axis[3], &axis[6] );
	VectorInverse( &axis[3] );
}

// similar to MakeNormalVectors but for rotational matrices
// (FIXME: weird, what's the diff between this and MakeNormalVectors?)
void NormalVectorToAxis( const vec3_t forward, mat3_t axis ) {
	VectorCopy( forward, &axis[AXIS_FORWARD] );
	PerpendicularVector( &axis[AXIS_RIGHT], &axis[AXIS_FORWARD] );
	CrossProduct( &axis[AXIS_FORWARD], &axis[AXIS_RIGHT], &axis[AXIS_UP] );
}

void BuildBoxPoints( vec3_t p[8], const vec3_t org, const vec3_t mins, const vec3_t maxs ) {
	VectorAdd( org, mins, p[0] );
	VectorAdd( org, maxs, p[1] );
	VectorSet( p[2], p[0][0], p[0][1], p[1][2] );
	VectorSet( p[3], p[0][0], p[1][1], p[0][2] );
	VectorSet( p[4], p[0][0], p[1][1], p[1][2] );
	VectorSet( p[5], p[1][0], p[1][1], p[0][2] );
	VectorSet( p[6], p[1][0], p[0][1], p[1][2] );
	VectorSet( p[7], p[1][0], p[0][1], p[0][2] );
}

//
// assumes "src" is normalized
//
void PerpendicularVector( vec3_t dst, const vec3_t src ) {
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	//
	// find the smallest magnitude axially aligned vector
	//
	for( pos = 0, i = 0; i < 3; i++ ) {
		if( fabs( src[i] ) < minelem ) {
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	//
	// project the point onto the plane defined by src
	//
	ProjectPointOntoPlane( dst, tempvec, src );

	//
	// normalize the result
	//
	VectorNormalize( dst );
}

void ProjectPointOntoPlane( vec3_t dst, const vec3_t p, const vec3_t normal ) {
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
* ProjectPointOntoVector
*/
void ProjectPointOntoVector( const vec3_t point, const vec3_t vStart, const vec3_t vDir, vec3_t vProj ) {
	vec3_t pVec;

	VectorSubtract( point, vStart, pVec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vDir ), vDir, vProj );
}

//============================================================================

/*
* LerpAngle
*
*/
float LerpAngle( float a2, float a1, const float frac ) {
	if( a1 - a2 > 180 ) {
		a1 -= 360;
	}
	if( a1 - a2 < -180 ) {
		a1 += 360;
	}
	return Lerp( a2, frac, a1 );
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
	if( angle > 180.0 ) {
		angle -= 360.0;
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

/*
* WidescreenFov
*/
float WidescreenFov( float fov ) {
	return atan( tan( (fov) / 360.0 * M_PI ) * (3.0 / 4.0) ) * (360.0 / M_PI);
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
	x *= tan( fov_y / 360.0 * M_PI );
	return atan( x / height ) * 360.0 / M_PI;
}

/*
* BoxOnPlaneSide
*
* Returns 1, 2, or 1 + 2
*/
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const struct cplane_s *p ) {
	float dist1, dist2;
	int sides;

	// general case
	switch( p->signbits ) {
		case 0:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;
		case 1:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;
		case 2:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 3:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 4:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 5:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 6:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;
		case 7:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;
		default:
			dist1 = dist2 = 0; // shut up compiler
			assert( 0 );
			break;
	}

	sides = 0;
	if( dist1 >= p->dist ) {
		sides = 1;
	}
	if( dist2 < p->dist ) {
		sides |= 2;
	}

	return sides;
}

/*
* SignbitsForPlane
*/
int SignbitsForPlane( const cplane_t *out ) {
	int bits, j;

	// for fast box on planeside test

	bits = 0;
	for( j = 0; j < 3; j++ ) {
		if( out->normal[j] < 0 ) {
			bits |= 1 << j;
		}
	}
	return bits;
}

/*
* PlaneTypeForNormal
*/
int PlaneTypeForNormal( const vec3_t normal ) {
	// NOTE: should these have an epsilon around 1.0?
	if( normal[0] >= 1.0 ) {
		return PLANE_X;
	}
	if( normal[1] >= 1.0 ) {
		return PLANE_Y;
	}
	if( normal[2] >= 1.0 ) {
		return PLANE_Z;
	}

	return PLANE_NONAXIAL;
}

/*
* CategorizePlane
*
* A slightly more complex version of SignbitsForPlane and PlaneTypeForNormal,
* which also tries to fix possible floating point glitches (like -0.00000 cases)
*/
void CategorizePlane( cplane_t *plane ) {
	int i;

	plane->signbits = 0;
	plane->type = PLANE_NONAXIAL;
	for( i = 0; i < 3; i++ ) {
		if( plane->normal[i] < 0 ) {
			plane->signbits |= 1 << i;
			if( plane->normal[i] == -1.0f ) {
				plane->signbits = ( 1 << i );
				VectorClear( plane->normal );
				plane->normal[i] = -1.0f;
				break;
			}
		} else if( plane->normal[i] == 1.0f ) {
			plane->type = i;
			plane->signbits = 0;
			VectorClear( plane->normal );
			plane->normal[i] = 1.0f;
			break;
		}
	}
}

/*
* PlaneFromPoints
*/
void PlaneFromPoints( vec3_t verts[3], cplane_t *plane ) {
	vec3_t v1, v2;

	VectorSubtract( verts[1], verts[0], v1 );
	VectorSubtract( verts[2], verts[0], v2 );
	CrossProduct( v2, v1, plane->normal );
	VectorNormalize( plane->normal );
	plane->dist = DotProduct( verts[0], plane->normal );
	plane->type = PLANE_NONAXIAL;
}

#define PLANE_NORMAL_EPSILON    0.00001
#define PLANE_DIST_EPSILON  0.01

/*
* ComparePlanes
*/
bool ComparePlanes( const vec3_t p1normal, vec_t p1dist, const vec3_t p2normal, vec_t p2dist ) {
	if( fabs( p1normal[0] - p2normal[0] ) < PLANE_NORMAL_EPSILON
		&& fabs( p1normal[1] - p2normal[1] ) < PLANE_NORMAL_EPSILON
		&& fabs( p1normal[2] - p2normal[2] ) < PLANE_NORMAL_EPSILON
		&& fabs( p1dist - p2dist ) < PLANE_DIST_EPSILON ) {
		return true;
	}

	return false;
}

/*
* SnapVector
*/
void SnapVector( vec3_t normal ) {
	int i;

	for( i = 0; i < 3; i++ ) {
		if( fabs( normal[i] - 1 ) < PLANE_NORMAL_EPSILON ) {
			VectorClear( normal );
			normal[i] = 1;
			break;
		}
		if( fabs( normal[i] - -1 ) < PLANE_NORMAL_EPSILON ) {
			VectorClear( normal );
			normal[i] = -1;
			break;
		}
	}
}

/*
* SnapPlane
*/
void SnapPlane( vec3_t normal, vec_t *dist ) {
	SnapVector( normal );

	if( fabs( *dist - Q_rint( *dist ) ) < PLANE_DIST_EPSILON ) {
		*dist = Q_rint( *dist );
	}
}

void ClearBounds( vec3_t mins, vec3_t maxs ) {
	mins[0] = mins[1] = mins[2] = 999999;
	maxs[0] = maxs[1] = maxs[2] = -999999;
}

void CopyBounds( const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs ) {
	VectorCopy( inmins, outmins );
	VectorCopy( inmaxs, outmaxs );
}

bool BoundsOverlap( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2 ) {
	return ( mins1[0] <= maxs2[0] && mins1[1] <= maxs2[1] && mins1[2] <= maxs2[2] &&
		maxs1[0] >= mins2[0] && maxs1[1] >= mins2[1] && maxs1[2] >= mins2[2] );
}

bool BoundsOverlapSphere( const vec3_t mins, const vec3_t maxs, const vec3_t centre, float radius ) {
	float dmin = 0;
	for( int i = 0; i < 3; i++ ) {
		if( centre[i] < mins[i] ) {
			dmin += ( centre[i] - mins[i] ) * ( centre[i] - mins[i] );
		} else if( centre[i] > maxs[i] ) {
			dmin += ( centre[i] - maxs[i] ) * ( centre[i] - maxs[i] );
		}
	}

	return dmin <= radius * radius;
}

void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	vec_t val;

	for( int i = 0; i < 3; i++ ) {
		val = v[i];
		if( val < mins[i] ) {
			mins[i] = val;
		}
		if( val > maxs[i] ) {
			maxs[i] = val;
		}
	}
}

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs ) {
	vec3_t corner;

	for( int i = 0; i < 3; i++ ) {
		corner[i] = fabs( mins[i] ) > fabs( maxs[i] ) ? fabs( mins[i] ) : fabs( maxs[i] );
	}

	return VectorLength( corner );
}

vec_t VectorNormalize( vec3_t v ) {
	float length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	if( length ) {
		length = sqrtf( length ); // FIXME
		float ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

vec_t VectorNormalize2( const vec3_t v, vec3_t out ) {
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	if( length ) {
		length = sqrtf( length ); // FIXME
		ilength = 1.0 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	} else {
		VectorClear( out );
	}

	return length;
}

vec_t Vector4Normalize( vec4_t v ) {
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];

	if( length ) {
		length = sqrtf( length ); // FIXME
		ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
		v[3] *= ilength;
	}

	return length;
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
void VectorNormalizeFast( vec3_t v ) {
	float ilength = Q_RSqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
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

void Matrix3_TransformVector( const mat3_t m, const vec3_t v, vec3_t out ) {
	out[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
	out[1] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
	out[2] = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
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

void Matrix3_FromAngles( const vec3_t angles, mat3_t m ) {
	AngleVectors( angles, &m[AXIS_FORWARD], &m[AXIS_RIGHT], &m[AXIS_UP] );
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
	float phi = 2.0f * float( M_PI ) * random_float01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

Vec3 UniformSampleInsideSphere( RNG * rng ) {
	Vec3 p = UniformSampleSphere( rng );
	float r = powf( random_float01( rng ), 1.0f / 3.0f );
	return p * r;
}

Vec3 UniformSampleCone( RNG * rng, float theta ) {
	assert( theta >= 0.0f && theta <= float( M_PI ) );
	float z = random_uniform_float( rng, cosf( theta ), 1.0f );
	float r = sqrtf( Max2( 0.0f, 1.0f - z * z ) );
	float phi = 2.0f * float( M_PI ) * random_float01( rng );
	return Vec3( r * cosf( phi ), r * sinf( phi ), z );
}

Vec2 UniformSampleDisk( RNG * rng ) {
	float theta = random_float01( rng ) * 2.0f * float( M_PI );
	float r = sqrtf( random_float01( rng ) );
	return Vec2( r * cosf( theta ), r * sinf( theta ) );
}

float SampleNormalDistribution( RNG * rng ) {
	// generate a float in (0, 1). works because prev(1) + FLT_MIN == prev(1)
	float u1 = random_float01( rng ) + FLT_MIN;
	float u2 = random_float01( rng );
	return sqrtf( -2.0f * logf( u1 ) ) * cosf( u2 * 2.0f * float( M_PI ) );
}

Vec3 Project( Vec3 a, Vec3 b ) {
	return Dot( a, b ) / Dot( b, b ) * b;
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

	if( fabsf( dir.z ) >= 0.9999f ) {
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
