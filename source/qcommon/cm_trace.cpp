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

#include "qcommon/qcommon.h"
#include "qcommon/cm_local.h"

typedef struct {
	int leaf_topnode;
	int leaf_count, leaf_maxcount;
	int *leaf_list;
	Vec3 leaf_mins, leaf_maxs;
} boxLeafsWork_t;

typedef struct {
	bool ispoint;
	int contents;
	int checkcount;

	float realfraction;

	Vec3 extents;

	Vec3 start, end;
	Vec3 mins, maxs;
	Vec3 absmins, absmaxs;

	CollisionModel * cms;

	trace_t *trace;

	int nummarkbrushes;
	cbrush_t *brushes;
	int *markbrushes;

	int nummarkfaces;
	cface_t *faces;
	int *markfaces;

	int *brush_checkcounts;
	int *face_checkcounts;
} traceWork_t;

/*
* CM_InitBoxHull
*
* Set up the planes so that the six floats of a bounding box
* can just be stored out and get a proper clipping hull structure.
*/
void CM_InitBoxHull( CollisionModel *cms ) {
	cms->box_brush->numsides = 6;
	cms->box_brush->brushsides = cms->box_brushsides;
	cms->box_brush->contents = CONTENTS_BODY;

	// Make sure CM_CollideBox() will not reject the brush by its bounds
	ClearBounds( &cms->box_brush->maxs, &cms->box_brush->mins );

	cms->box_markbrushes[0] = 0;

	cms->box_cmodel->brushes = cms->box_brush;
	cms->box_cmodel->builtin = true;
	cms->box_cmodel->nummarkfaces = 0;
	cms->box_cmodel->markfaces = NULL;
	cms->box_cmodel->markbrushes = cms->box_markbrushes;
	cms->box_cmodel->nummarkbrushes = 1;

	for( int i = 0; i < 6; i++ ) {
		// brush sides
		cbrushside_t * s = cms->box_brushsides + i;
		s->surfFlags = 0;

		// planes
		cplane_t * p = &s->plane;
		p->normal = Vec3( 0.0f );

		if( ( i & 1 ) ) {
			p->normal[i >> 1] = -1;
		} else {
			p->normal[i >> 1] = 1;
		}
	}
}

/*
* CM_InitOctagonHull
*
* Set up the planes so that the six floats of a bounding box
* can just be stored out and get a proper clipping hull structure.
*/
void CM_InitOctagonHull( CollisionModel *cms ) {
	const Vec3 oct_dirs[4] = {
		Vec3(  1.0f,  1.0f, 0.0f ),
		Vec3( -1.0f,  1.0f, 0.0f ),
		Vec3( -1.0f, -1.0f, 0.0f ),
		Vec3(  1.0f, -1.0f, 0.0f )
	};

	cms->oct_brush->numsides = 10;
	cms->oct_brush->brushsides = cms->oct_brushsides;
	cms->oct_brush->contents = CONTENTS_BODY;

	// Make sure CM_CollideBox() will not reject the brush by its bounds
	ClearBounds( &cms->oct_brush->maxs, &cms->oct_brush->mins );

	cms->oct_markbrushes[0] = 0;

	cms->oct_cmodel->brushes = cms->oct_brush;
	cms->oct_cmodel->builtin = true;
	cms->oct_cmodel->nummarkfaces = 0;
	cms->oct_cmodel->markfaces = NULL;
	cms->oct_cmodel->markbrushes = cms->oct_markbrushes;
	cms->oct_cmodel->nummarkbrushes = 1;

	// axial planes
	for( int i = 0; i < 6; i++ ) {
		// brush sides
		cbrushside_t * s = cms->oct_brushsides + i;
		s->surfFlags = 0;

		// planes
		cplane_t * p = &s->plane;
		p->normal = Vec3( 0.0f );

		if( ( i & 1 ) ) {
			p->normal[i >> 1] = -1;
		} else {
			p->normal[i >> 1] = 1;
		}
	}

	// non-axial planes
	for( int i = 6; i < 10; i++ ) {
		// brush sides
		cbrushside_t * s = cms->oct_brushsides + i;
		s->surfFlags = 0;

		// planes
		cplane_t * p = &s->plane;
		p->normal = oct_dirs[i - 6];
	}
}

/*
* CM_ModelForBBox
*
* To keep everything totally uniform, bounding boxes are turned into inline models
*/
cmodel_t *CM_ModelForBBox( CollisionModel *cms, Vec3 mins, Vec3 maxs ) {
	cms->box_brushsides[0].plane.dist = maxs.x;
	cms->box_brushsides[1].plane.dist = -mins.x;
	cms->box_brushsides[2].plane.dist = maxs.y;
	cms->box_brushsides[3].plane.dist = -mins.y;
	cms->box_brushsides[4].plane.dist = maxs.z;
	cms->box_brushsides[5].plane.dist = -mins.z;

	cms->box_cmodel->mins = mins;
	cms->box_cmodel->maxs = maxs;

	return cms->box_cmodel;
}

/*
* CM_OctagonModelForBBox
*
* Same as CM_ModelForBBox with 4 additional planes at corners.
* Internally offset to be symmetric on all sides.
*/
cmodel_t *CM_OctagonModelForBBox( CollisionModel *cms, Vec3 mins, Vec3 maxs ) {
	float a, b, d, t;
	float sina, cosa;
	Vec3 offset, size[2];

	offset = ( mins + maxs ) * 0.5f;
	size[0] = mins - offset;
	size[1] = maxs - offset;

	cms->oct_cmodel->cyl_offset = offset;
	cms->oct_cmodel->mins = size[0];
	cms->oct_cmodel->maxs = size[1];

	cms->oct_brushsides[0].plane.dist = size[1].x;
	cms->oct_brushsides[1].plane.dist = -size[0].x;
	cms->oct_brushsides[2].plane.dist = size[1].y;
	cms->oct_brushsides[3].plane.dist = -size[0].y;
	cms->oct_brushsides[4].plane.dist = size[1].z;
	cms->oct_brushsides[5].plane.dist = -size[0].z;

	a = size[1].x; // halfx
	b = size[1].y; // halfy
	d = sqrtf( a * a + b * b ); // hypothenuse

	cosa = a / d;
	sina = b / d;

	// swap sin and cos, which is the same thing as adding pi/2 radians to the original angle
	t = sina;
	sina = cosa;
	cosa = t;

	// elleptical radius
	d = a * b / sqrtf( a * a * cosa * cosa + b * b * sina * sina );
	//d = a * b / sqrtf( a * a  + b * b ); // produces a rectangle, inscribed at middle points

	// the following should match normals set in CM_InitOctagonHull

	cms->oct_brushsides[6].plane.normal = Vec3( cosa, sina, 0 );
	cms->oct_brushsides[6].plane.dist = d;

	cms->oct_brushsides[7].plane.normal = Vec3( -cosa, sina, 0 );
	cms->oct_brushsides[7].plane.dist = d;

	cms->oct_brushsides[8].plane.normal = Vec3( -cosa, -sina, 0 );
	cms->oct_brushsides[8].plane.dist = d;

	cms->oct_brushsides[9].plane.normal = Vec3( cosa, -sina, 0 );
	cms->oct_brushsides[9].plane.dist = d;

	return cms->oct_cmodel;
}

int CM_PointLeafnum( const CollisionModel *cms, Vec3 p ) {
	if( !cms->numplanes ) {
		return 0; // sound may call this without map loaded
	}

	int num = 0;
	do {
		cnode_t * node = cms->map_nodes + num;
		num = node->children[PlaneDiff( p, node->plane ) < 0];
	} while( num >= 0 );

	return -1 - num;
}

enum AABBPlaneResult {
	AABBPlaneResult_Behind,
	AABBPlaneResult_Straddling,
	AABBPlaneResult_InFront,
};

static AABBPlaneResult IntersectAABBPlane( Vec3 mins, Vec3 maxs, const cplane_t * p ) {
	Vec3 back_point, front_point;
	for( int i = 0; i < 3; i++ ) {
		back_point[ i ] = p->normal[ i ] < 0 ? maxs[ i ] : mins[ i ];
		front_point[ i ] = p->normal[ i ] < 0 ? mins[ i ] : maxs[ i ];
	}

	float back_dist = Dot( p->normal, back_point ) - p->dist;
	float front_dist = Dot( p->normal, front_point ) - p->dist;

	if( back_dist > 0 ) {
		return AABBPlaneResult_InFront;
	}

	return front_dist < 0 ? AABBPlaneResult_Behind : AABBPlaneResult_Straddling;
}

/*
* CM_BoxLeafnums
*
* Fills in a list of all the leafs touched
*/
static void CM_BoxLeafnums_r( boxLeafsWork_t *bw, const CollisionModel *cms, int nodenum ) {
	while( nodenum >= 0 ) {
		const cnode_t * node = &cms->map_nodes[nodenum];

		AABBPlaneResult r = IntersectAABBPlane( bw->leaf_mins, bw->leaf_maxs, node->plane );

		if( r == AABBPlaneResult_InFront ) {
			nodenum = node->children[ 0 ];
			continue;
		}

		if( r == AABBPlaneResult_Behind ) {
			nodenum = node->children[ 1 ];
			continue;
		}

		// go down both sides
		if( bw->leaf_topnode == -1 ) {
			bw->leaf_topnode = nodenum;
		}

		CM_BoxLeafnums_r( bw, cms, node->children[0] );
		nodenum = node->children[1];
	}

	if( bw->leaf_count < bw->leaf_maxcount ) {
		bw->leaf_list[bw->leaf_count++] = -1 - nodenum;
	}
}

int CM_BoxLeafnums( CollisionModel *cms, Vec3 mins, Vec3 maxs, int *list, int listsize, int *topnode ) {
	boxLeafsWork_t bw;

	bw.leaf_list = list;
	bw.leaf_count = 0;
	bw.leaf_maxcount = listsize;
	bw.leaf_mins = mins;
	bw.leaf_maxs = maxs;
	bw.leaf_topnode = -1;

	CM_BoxLeafnums_r( &bw, cms, 0 );

	if( topnode ) {
		*topnode = bw.leaf_topnode;
	}

	return bw.leaf_count;
}

static inline int CM_BrushContents( cbrush_t *brush, Vec3 p ) {
	int i;
	cbrushside_t *brushside;

	for( i = 0, brushside = brush->brushsides; i < brush->numsides; i++, brushside++ ) {
		if( PlaneDiff( p, &brushside->plane ) > 0 ) {
			return 0;
		}
	}

	return brush->contents;
}

static inline int CM_PatchContents( cface_t *patch, Vec3 p ) {
	int i, c;
	cbrush_t *facet;

	for( i = 0, facet = patch->facets; i < patch->numfacets; i++, facet++ ) {
		if( ( c = CM_BrushContents( facet, p ) ) ) {
			return c;
		}
	}

	return 0;
}

static int CM_PointContents( CollisionModel *cms, Vec3 p, cmodel_t *cmodel ) {
	ZoneScoped;

	int superContents;
	int nummarkfaces, nummarkbrushes;
	int *markface;
	int *markbrush;

	if( cmodel->hash == cms->world_hash ) {
		cleaf_t *leaf;

		leaf = &cms->map_leafs[CM_PointLeafnum( cms, p )];
		superContents = leaf->contents;

		markbrush = leaf->markbrushes;
		nummarkbrushes = leaf->nummarkbrushes;

		markface = leaf->markfaces;
		nummarkfaces = leaf->nummarkfaces;
	}
	else {
		superContents = ~0;

		markbrush = cmodel->markbrushes;
		nummarkbrushes = cmodel->nummarkbrushes;

		markface = cmodel->markfaces;
		nummarkfaces = cmodel->nummarkfaces;
	}

	int contents = superContents;
	cbrush_t * brushes = cmodel->brushes;
	cface_t * faces = cmodel->faces;

	for( int i = 0; i < nummarkbrushes; i++ ) {
		cbrush_t *brush = brushes + markbrush[i];

		// check if brush adds something to contents
		if( contents & brush->contents ) {
			if( !( contents &= ~CM_BrushContents( brush, p ) ) ) {
				return superContents;
			}
		}
	}

	for( int i = 0; i < nummarkfaces; i++ ) {
		cface_t *patch = faces + markface[i];

		// check if patch adds something to contents
		if( contents & patch->contents ) {
			if( BoundsOverlap( p, p, patch->mins, patch->maxs ) ) {
				if( !( contents &= ~CM_PatchContents( patch, p ) ) ) {
					return superContents;
				}
			}
		}
	}

	return ~contents & superContents;
}

/*
* CM_TransformedPointContents
*
* Handles offseting and rotation of the end points for moving and
* rotating entities
*/
int CM_TransformedPointContents( CModelServerOrClient soc, CollisionModel * cms, Vec3 p, cmodel_t *cmodel, Vec3 origin, Vec3 angles ) {
	if( !cms->numnodes ) { // map not loaded
		return 0;
	}

	if( !cmodel || cmodel->hash == cms->world_hash ) {
		cmodel = CM_FindCModel( soc, StringHash( cms->world_hash ) );
		origin = Vec3( 0.0f );
		angles = Vec3( 0.0f );
	}

	Vec3 p_l = p - origin;

	// rotate start and end into the models frame of reference
	if( angles != Vec3( 0.0f ) && !cmodel->builtin ) {
		mat3_t axis;

		AnglesToAxis( angles, axis );
		Vec3 temp = p_l;
		Matrix3_TransformVector( axis, temp, &p_l );
	}

	return CM_PointContents( cms, p_l, cmodel );
}

/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON    ( 1.0f / 32.0f )

static void CM_ClipBoxToBrush( traceWork_t *tw, const cbrush_t *brush ) {
	ZoneScoped;

	if( !brush->numsides ) {
		return;
	}

	float enterfrac = -1.0f;
	float leavefrac = 1.0f;
	float enterfrac2 = -1.0f;

	const cplane_t * clipplane = NULL;

	bool getout = false;
	bool startout = false;
	const cbrushside_t * leadside = NULL;
	const cbrushside_t * side = brush->brushsides;

	for( int i = 0; i < brush->numsides; i++, side++ ) {
		const cplane_t * p = &side->plane;

		Vec3 offset;
		for( int j = 0; j < 3; j++ ) {
			offset[ j ] = p->normal[ j ] < 0 ? tw->maxs[ j ] : tw->mins[ j ];
		}

		Vec3 start_offset, end_offset;
		start_offset = tw->start + offset;
		end_offset = tw->end + offset;

		float d1 = Dot( p->normal, start_offset ) - p->dist;
		float d2 = Dot( p->normal, end_offset ) - p->dist;

		if( d2 > 0 ) {
			getout = true; // endpoint is not in solid
		}
		if( d1 > 0 ) {
			startout = true;
		}

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 ) {
			return;
		}

		if( d1 <= 0 && d2 <= 0 ) {
			continue;
		}

		// crosses face
		float f = d1 - d2;
		if( f > 0 ) {       // enter
			f = d1 / f;
			if( f > enterfrac ) {
				enterfrac = f;
				clipplane = p;
				leadside = side;
				enterfrac2 = ( d1 - DIST_EPSILON ) / ( d1 - d2 ); // nudged fraction
			}
		}
		else if( f < 0 ) {   // leave
			f = d1 / f;
			if( f < leavefrac ) {
				leavefrac = f;
			}
		}
	}

	if( !startout ) {
		// original point was inside brush
		tw->trace->startsolid = true;
		tw->contents = brush->contents;
		if( !getout ) {
			tw->realfraction = 0;
			tw->trace->allsolid = true;
			tw->trace->fraction = 0;
		}
		return;
	}

	if( enterfrac <= -1 || enterfrac > leavefrac ) {
		return;
	}

	// check if this will reduce the collision time range
	if( enterfrac < tw->realfraction ) {
		if( enterfrac2 < tw->trace->fraction ) {
			tw->realfraction = enterfrac;
			tw->trace->plane = *clipplane;
			tw->trace->surfFlags = leadside->surfFlags;
			tw->trace->contents = brush->contents;
			tw->trace->fraction = enterfrac2;
		}
	}
}

static void CM_TestBoxInBrush( traceWork_t *tw, const cbrush_t *brush ) {
	ZoneScoped;

	if( !brush->numsides ) {
		return;
	}

	const cbrushside_t * side = brush->brushsides;
	for( int i = 0; i < brush->numsides; i++, side++ ) {
		const cplane_t * p = &side->plane;

		Vec3 offset;
		for( int j = 0; j < 3; j++ ) {
			offset[ j ] = p->normal[ j ] < 0 ? tw->maxs[ j ] : tw->mins[ j ];
		}

		Vec3 start_offset = tw->start + offset;

		if( Dot( p->normal, start_offset ) > p->dist ) {
			return;
		}
	}

	// inside this brush
	tw->trace->startsolid = tw->trace->allsolid = true;
	tw->trace->fraction = 0;
	tw->trace->contents = brush->contents;
}

static void CM_CollideBox( traceWork_t *tw, const int *markbrushes, int nummarkbrushes, const int *markfaces, int nummarkfaces, void ( *func )( traceWork_t *, const cbrush_t *b ) ) {
	ZoneScoped;

	const cbrush_t *brushes = tw->brushes;
	const cface_t *faces = tw->faces;
	int checkcount = tw->checkcount;

	// trace line against all brushes
	for( int i = 0; i < nummarkbrushes; i++ ) {
		int mb = markbrushes[i];
		const cbrush_t *b = brushes + mb;

		if( tw->brush_checkcounts[mb] == checkcount ) {
			continue; // already checked this brush
		}
		tw->brush_checkcounts[mb] = checkcount;

		if( !( b->contents & tw->contents ) ) {
			continue;
		}
		if( !BoundsOverlap( b->mins, b->maxs, tw->absmins, tw->absmaxs ) ) {
			continue;
		}
		func( tw, b );
		if( !tw->trace->fraction ) {
			return;
		}
	}

	if( !nummarkfaces ) {
		return;
	}

	// trace line against all patches
	for( int i = 0; i < nummarkfaces; i++ ) {
		int mf = markfaces[i];
		const cface_t *patch = faces + mf;

		if( tw->face_checkcounts[mf] == checkcount ) {
			continue; // already checked this brush
		}
		tw->face_checkcounts[mf] = checkcount;

		if( !( patch->contents & tw->contents ) ) {
			continue;
		}
		if( !BoundsOverlap( patch->mins, patch->maxs, tw->absmins, tw->absmaxs ) ) {
			continue;
		}
		const cbrush_t * facet = patch->facets;
		int j;
		for( j = 0; j < patch->numfacets; j++, facet++ ) {
			if( !BoundsOverlap( facet->mins, facet->maxs, tw->absmins, tw->absmaxs ) ) {
				continue;
			}
			func( tw, facet );
			if( !tw->trace->fraction ) {
				return;
			}
		}
	}
}

static inline void CM_ClipBox( traceWork_t *tw, const int *markbrushes, int nummarkbrushes, const int *markfaces, int nummarkfaces ) {
	CM_CollideBox( tw, markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_ClipBoxToBrush );
}

static inline void CM_TestBox( traceWork_t *tw, const int *markbrushes, int nummarkbrushes, const int *markfaces, int nummarkfaces ) {
	CM_CollideBox( tw, markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_TestBoxInBrush );
}

static void CM_RecursiveHullCheck( traceWork_t *tw, int num, float p1f, float p2f, Vec3 p1, Vec3 p2 ) {
	ZoneScoped;

	const CollisionModel *cms = tw->cms;
	const cnode_t *node;
	const cplane_t *plane;
	int side;
	float t1, t2, radius;
	float frac, frac2;
	float idist, midf;
	Vec3 mid;

loc0:
	if( tw->realfraction <= p1f ) {
		return; // already hit something nearer
	}

	// if < 0, we are in a leaf node
	if( num < 0 ) {
		cleaf_t *leaf;

		leaf = &cms->map_leafs[-1 - num];
		if( leaf->contents & tw->contents ) {
			CM_ClipBox( tw, leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
		}
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the radius for the size of the box
	//
	node = cms->map_nodes + num;
	plane = node->plane;

	t1 = Dot( plane->normal, p1 ) - plane->dist;
	t2 = Dot( plane->normal, p2 ) - plane->dist;
	if( tw->ispoint ) {
		radius = 0;
	}
	else {
		radius = Abs( tw->extents.x * plane->normal.x ) +
			Abs( tw->extents.y * plane->normal.y ) +
			Abs( tw->extents.z * plane->normal.z );
	}

	// see which sides we need to consider
	if( t1 >= radius && t2 >= radius ) {
		num = node->children[0];
		goto loc0;
	}
	if( t1 < -radius && t2 < -radius ) {
		num = node->children[1];
		goto loc0;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( t1 < t2 ) {
		idist = 1.0 / ( t1 - t2 );
		side = 1;
		frac2 = ( t1 + radius ) * idist;
		frac = ( t1 - radius ) * idist;
	} else if( t1 > t2 ) {
		idist = 1.0 / ( t1 - t2 );
		side = 0;
		frac2 = ( t1 - radius ) * idist;
		frac = ( t1 + radius ) * idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = Clamp01( frac );
	midf = p1f + ( p2f - p1f ) * frac;
	mid = Lerp( p1, frac, p2 );

	CM_RecursiveHullCheck( tw, node->children[side], p1f, midf, p1, mid );

	// go past the node
	frac2 = Clamp01( frac2 );
	midf = p1f + ( p2f - p1f ) * frac2;
	mid = Lerp( p1, frac2, p2 );

	CM_RecursiveHullCheck( tw, node->children[side ^ 1], midf, p2f, mid, p2 );
}

static void CM_BoxTrace( traceWork_t *tw, CollisionModel *cms, trace_t *tr,
	Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs,
	cmodel_t *cmodel, Vec3 origin, int brushmask ) {

	ZoneScoped;

	bool world = cmodel->hash == cms->world_hash;

	// fill in a default trace
	memset( tr, 0, sizeof( *tr ) );
	tr->fraction = 1;

	if( !cms->numnodes ) { // map not loaded
		return;
	}

	cms->checkcount++;  // for multi-check avoidance

	memset( tw, 0, sizeof( *tw ) );
	// the epsilon considers blockers with realfraction == 1 and nudged fraction < 1
	tw->realfraction = 1 + DIST_EPSILON;
	tw->checkcount = cms->checkcount;
	tw->trace = tr;
	tw->contents = brushmask;
	tw->cms = cms;
	tw->start = start;
	tw->end = end;
	tw->mins = mins;
	tw->maxs = maxs;

	// build a bounding box of the entire move
	Vec3 startmins = start + tw->mins;
	Vec3 startmaxs = start + tw->maxs;
	Vec3 endmins = end + tw->mins;
	Vec3 endmaxs = end + tw->maxs;

	ClearBounds( &tw->absmins, &tw->absmaxs );
	AddPointToBounds( startmins, &tw->absmins, &tw->absmaxs );
	AddPointToBounds( startmaxs, &tw->absmins, &tw->absmaxs );
	AddPointToBounds( endmins, &tw->absmins, &tw->absmaxs );
	AddPointToBounds( endmaxs, &tw->absmins, &tw->absmaxs );

	tw->brushes = cmodel->brushes;
	tw->faces = cmodel->faces;

	if( cmodel == cms->oct_cmodel ) {
		tw->brush_checkcounts = &cms->oct_checkcount;
		tw->face_checkcounts = NULL;
	} else if( cmodel == cms->box_cmodel ) {
		tw->brush_checkcounts = &cms->box_checkcount;
		tw->face_checkcounts = NULL;
	} else {
		tw->brush_checkcounts = cms->map_brush_checkcheckouts;
		tw->face_checkcounts = cms->map_face_checkcheckouts;
	}

	//
	// check for position test special case
	//
	if( start == end ) {
		int leafs[1024];
		int i, numleafs;
		Vec3 c1, c2;
		cleaf_t *leaf;

		if( world ) {
			c1 = start + mins - Vec3( 1.0f );
			c2 = start + maxs + Vec3( 1.0f );

			numleafs = CM_BoxLeafnums( cms, c1, c2, leafs, 1024, NULL );
			for( i = 0; i < numleafs; i++ ) {
				leaf = &cms->map_leafs[leafs[i]];

				if( leaf->contents & brushmask ) {
					CM_TestBox( tw, leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
					if( tr->allsolid ) {
						break;
					}
				}
			}
		}
		else {
			if( BoundsOverlap( cmodel->mins, cmodel->maxs, tw->absmins, tw->absmaxs ) ) {
				CM_TestBox( tw, cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
			}
		}

		tr->endpos = start;
		return;
	}

	//
	// check for point special case
	//
	if( mins == Vec3( 0.0f ) && maxs == Vec3( 0.0f ) ) {
		tw->ispoint = true;
		tw->extents = Vec3( 0.0f );
	} else {
		tw->ispoint = false;
		for( int i = 0; i < 3; i++ ) {
			tw->extents[ i ] = Max2( Abs( mins[ i ] ), Abs( maxs[ i ] ) );
		}
	}

	//
	// general sweeping through world
	//
	if( world ) {
		CM_RecursiveHullCheck( tw, 0, 0, 1, start, end );
	}
	else if( BoundsOverlap( cmodel->mins, cmodel->maxs, tw->absmins, tw->absmaxs ) ) {
		CM_ClipBox( tw, cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
	}

	tr->fraction = Clamp01( tr->fraction );
	tr->endpos = Lerp( start, tr->fraction, end );
}

/*
* CM_TransformedBoxTrace
*
* Handles offseting and rotation of the end points for moving and
* rotating entities
*/
void CM_TransformedBoxTrace( CModelServerOrClient soc, CollisionModel * cms, trace_t * tr, Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs,
							 cmodel_t *cmodel, int brushmask, Vec3 origin, Vec3 angles ) {
	ZoneScoped;

	Vec3 start_l, end_l;
	Vec3 a, temp;
	mat3_t axis;
	bool rotated;
	traceWork_t tw;

	if( !tr ) {
		return;
	}

	if( !cmodel || cmodel->hash == cms->world_hash ) {
		cmodel = CM_FindCModel( soc, StringHash( cms->world_hash ) );
		origin = Vec3( 0.0f );
		angles = Vec3( 0.0f );
	}

	// cylinder offset
	if( cmodel == cms->oct_cmodel ) {
		start_l = start - cmodel->cyl_offset;
		end_l = end - cmodel->cyl_offset;
	} else {
		start_l = start;
		end_l = end;
	}

	// subtract origin offset
	start_l -= origin;
	end_l -= origin;

	// ch : here we could try back-rotate the vector for aabb to get
	// 'cylinder-like' shape, ie width of the aabb is constant for all directions
	// in this case, the orientation of vector would be ( normalize(origin-start), cross(x,z), up )

	// rotate start and end into the models frame of reference
	if( angles != Vec3( 0.0f ) && !cmodel->builtin ) {
		rotated = true;
	} else {
		rotated = false;
	}

	if( rotated ) {
		AnglesToAxis( angles, axis );

		temp = start_l;
		Matrix3_TransformVector( axis, temp, &start_l );

		temp = end_l;
		Matrix3_TransformVector( axis, temp, &end_l );
	}

	// sweep the box through the model
	CM_BoxTrace( &tw, cms, tr, start_l, end_l, mins, maxs, cmodel, origin, brushmask );

	if( rotated && tr->fraction != 1.0 ) {
		a = -angles;
		AnglesToAxis( a, axis );

		temp = tr->plane.normal;
		Matrix3_TransformVector( axis, temp, &tr->plane.normal );
	}

	tr->endpos = Lerp( start, tr->fraction, end );
}
