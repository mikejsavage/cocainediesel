/*
Copyright (C) 2013 Victor Luchits

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

#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "patch.h"

static int Patch_FlatnessTest( float maxflat2, Vec3 point0, Vec3 point1, Vec3 point2 ) {
	Vec3 t, n;
	Vec3 v1, v2, v3;

	n = point2 - point0;
	if( !Length( n ) ) {
		return 0;
	}
	n = Normalize( n );

	t = point1 - point0;
	float d = -Dot( t, n );
	t = t + n * d;
	if( Dot( t, t ) < maxflat2 ) {
		return 0;
	}

	v1 = ( point1 + point0 ) * 0.5f;
	v2 = ( point2 + point1 ) * 0.5f;
	v3 = ( v1 + v2 ) * 0.5f;

	int ft0 = Patch_FlatnessTest( maxflat2, point0, v1, v3 );
	int ft1 = Patch_FlatnessTest( maxflat2, v3, v2, point2 );

	return 1 + Max2( ft0, ft1 );
}

void Patch_GetFlatness( float maxflat, const Vec3 *points, int comp, const int *patch_cp, int *flat ) {
	int i, p, u, v;
	float maxflat2 = maxflat * maxflat;

	flat[0] = flat[1] = 0;
	for( v = 0; v < patch_cp[1] - 1; v += 2 ) {
		for( u = 0; u < patch_cp[0] - 1; u += 2 ) {
			p = v * patch_cp[0] + u;

			i = Patch_FlatnessTest( maxflat2, points[p * comp], points[( p + 1 ) * comp], points[( p + 2 ) * comp] );
			flat[0] = Max2( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, points[( p + patch_cp[0] ) * comp], points[( p + patch_cp[0] + 1 ) * comp], points[( p + patch_cp[0] + 2 ) * comp] );
			flat[0] = Max2( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, points[( p + 2 * patch_cp[0] ) * comp], points[( p + 2 * patch_cp[0] + 1 ) * comp], points[( p + 2 * patch_cp[0] + 2 ) * comp] );
			flat[0] = Max2( flat[0], i );

			i = Patch_FlatnessTest( maxflat2, points[p * comp], points[( p + patch_cp[0] ) * comp], points[( p + 2 * patch_cp[0] ) * comp] );
			flat[1] = Max2( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, points[( p + 1 ) * comp], points[( p + patch_cp[0] + 1 ) * comp], points[( p + 2 * patch_cp[0] + 1 ) * comp] );
			flat[1] = Max2( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, points[( p + 2 ) * comp], points[( p + patch_cp[0] + 2 ) * comp], points[( p + 2 * patch_cp[0] + 2 ) * comp] );
			flat[1] = Max2( flat[1], i );
		}
	}
}

static void Patch_Evaluate_QuadricBezier( float t, Vec3 point0, Vec3 point1, Vec3 point2, Vec3 * out, int comp ) {
	float qt = t * t;
	float dt = 2.0f * t;

	float tt = 1.0f - dt + qt;
	float tt2 = dt - 2.0f * qt;

	*out = point0 * tt + point1 * tt2 + point2 *qt;
}

void Patch_Evaluate( int comp, Vec3 * p, const int *numcp, const int *tess, Vec3 * dest, int stride ) {
	int num_patches[2], num_tess[2];
	int index[3], dstpitch, i, u, v, x, y;
	float s, t, step[2];
	// float *tvec, *tvec2;
	Vec3 *tvec, *tvec2;
	Vec3 *pv[3][3];
	Vec3 v1( 0.0f );
	Vec3 v2( 0.0f );
	Vec3 v3( 0.0f );

	assert( comp <= 4 );

	if( stride == 0 ) {
		stride = comp;
	}

	num_patches[0] = numcp[0] / 2;
	num_patches[1] = numcp[1] / 2;
	dstpitch = ( num_patches[0] * tess[0] + 1 ) * stride;

	step[0] = 1.0f / (float)tess[0];
	step[1] = 1.0f / (float)tess[1];

	for( v = 0; v < num_patches[1]; v++ ) {
		/* last patch has one more row */
		num_tess[1] = v < num_patches[1] - 1 ? tess[1] : tess[1] + 1;

		for( u = 0; u < num_patches[0]; u++ ) {
			/* last patch has one more column */
			num_tess[0] = u < num_patches[0] - 1 ? tess[0] : tess[0] + 1;

			index[0] = ( v * numcp[0] + u ) * 2;
			index[1] = index[0] + numcp[0];
			index[2] = index[1] + numcp[0];

			/* current 3x3 patch control points */
			for( i = 0; i < 3; i++ ) {
				pv[i][0] = &p[( index[0] + i ) * comp];
				pv[i][1] = &p[( index[1] + i ) * comp];
				pv[i][2] = &p[( index[2] + i ) * comp];
			}

			tvec = dest + v * tess[1] * dstpitch + u * tess[0] * stride;
			for( y = 0, t = 0.0f; y < num_tess[1]; y++, t += step[1], tvec += dstpitch ) {
				Patch_Evaluate_QuadricBezier( t, *pv[0][0], *pv[0][1], *pv[0][2], &v1, comp );
				Patch_Evaluate_QuadricBezier( t, *pv[1][0], *pv[1][1], *pv[1][2], &v2, comp );
				Patch_Evaluate_QuadricBezier( t, *pv[2][0], *pv[2][1], *pv[2][2], &v3, comp );

				for( x = 0, tvec2 = tvec, s = 0.0f; x < num_tess[0]; x++, s += step[0], tvec2 += stride )
					Patch_Evaluate_QuadricBezier( s, v1, v2, v3, tvec2, comp );
			}
		}
	}
}

void Patch_RemoveLinearColumnsRows( Vec3 *verts, int comp, int *pwidth, int *pheight,
									int numattribs, uint8_t * const *attribs, const int *attribsizes ) {
	int i, j, k, l;
	Vec3 v0, v1, v2 = Vec3( 0.0f );
	float len, maxLength;
	int maxWidth = *pwidth;
	int src, dst;
	int width = *pwidth, height = *pheight;
	Vec3 dir, proj;

	for( j = 1; j < width - 1; j++ ) {
		maxLength = 0;
		for( i = 0; i < height; i++ ) {
			v0 = verts[( i * maxWidth + j - 1 ) * comp];
			v1 = verts[( i * maxWidth + j + 1 ) * comp];
			v2 = verts[( i * maxWidth + j ) * comp];
			if( v1 == v0 )
				continue;
			dir = v1 - v0;
			dir = Normalize( dir );
			ProjectPointOntoVector( v2, v0, dir, &proj );
			dir = v2 - proj;
			len = LengthSquared( dir );
			if( len > maxLength ) {
				maxLength = len;
			}
		}
		if( maxLength < 0.01f ) {
			width--;
			for( i = 0; i < height; i++ ) {
				dst = i * maxWidth + j;
				src = dst + 1;
				memmove( &verts[dst * comp], &verts[src * comp], ( width - j ) * sizeof( float ) * comp );
				for( k = 0; k < numattribs; k++ )
					memmove( &attribs[k][dst * attribsizes[k]], &attribs[k][src * attribsizes[k]], ( width - j ) * attribsizes[k] );
			}
			j--;
		}
	}

	for( j = 1; j < height - 1; j++ ) {
		maxLength = 0;
		for( i = 0; i < width; i++ ) {
			v0 = verts[( ( j - 1 ) * maxWidth + i ) * comp];
			v1 = verts[( ( j + 1 ) * maxWidth + i ) * comp];
			v2 = verts[( j * maxWidth + i ) * comp];
			dir = v1 - v0;
			dir = Normalize( dir );
			ProjectPointOntoVector( v2, v0, dir, &proj );
			dir = v2 - proj;
			len = LengthSquared( dir );
			if( len > maxLength ) {
				maxLength = len;
			}
		}
		if( maxLength < 0.01f ) {
			height--;
			for( i = 0; i < width; i++ ) {
				for( k = j; k < height; k++ ) {
					src = ( k + 1 ) * maxWidth + i;
					dst = k * maxWidth + i;
					memcpy( &verts[dst * comp], &verts[src * comp], sizeof( float ) * comp );
					for( l = 0; l < numattribs; l++ )
						memcpy( &attribs[l][dst * attribsizes[l]], &attribs[l][src * attribsizes[l]], attribsizes[l] );
				}
			}
			j--;
		}
	}

	if( maxWidth != width ) {
		for( i = 0; i < height; i++ ) {
			src = i * maxWidth;
			dst = i * width;
			memmove( &verts[dst * comp], &verts[src * comp], width * sizeof( float ) * comp );
			for( j = 0; j < numattribs; j++ )
				memmove( &attribs[j][dst * attribsizes[j]], &attribs[j][src * attribsizes[j]], width * attribsizes[j] );
		}
	}

	*pwidth = width;
	*pheight = height;
}
