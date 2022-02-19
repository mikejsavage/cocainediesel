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

#include "gameshared/q_math.h"
#include "gameshared/q_collision.h"
#include "qcommon/qfiles.h"
#include "qcommon/hash.h"

#define MAX_CM_LEAFS        ( MAX_MAP_LEAFS )

struct cshaderref_t {
	int contents;
	int flags;
	char *name;
};

struct cnode_t {
	int children[2];            // negative numbers are leafs
	Plane *plane;
};

struct cbrushside_t {
	int surfFlags;
	Plane plane;
};

struct cbrush_t {
	int contents;
	int numsides;

	Vec3 mins, maxs;

	cbrushside_t *brushsides;
};

struct cface_t {
	int contents;
	int numfacets;

	Vec3 mins, maxs;

	cbrush_t *facets;
};

struct cleaf_t {
	int contents;
	int cluster;

	int area;

	int nummarkbrushes;
	int nummarkfaces;

	int *markbrushes;
	int *markfaces;
};

struct cmodel_t {
	u64 hash;

	bool builtin;

	int nummarkfaces;
	int nummarkbrushes;

	Vec3 cyl_offset;

	Vec3 mins, maxs;

	cbrush_t *brushes;
	cface_t *faces;

	// dummy iterators for the tracing code
	// which treats brush models as leafs
	int *markfaces;
	int *markbrushes;
};

struct carea_t {
	int floodnum;               // if two areas have equal floodnums, they are connected
	int floodvalid;
};

struct CollisionModel {
	u64 base_hash;
	u64 world_hash;

	int checkcount;
	int floodvalid;

	u32 checksum;

	int numbrushsides;
	cbrushside_t *map_brushsides;

	int numshaderrefs;
	cshaderref_t *map_shaderrefs;

	int numplanes;
	Plane *map_planes;

	int numnodes;
	cnode_t *map_nodes;

	int numleafs;                   // = 1
	cleaf_t map_leaf_empty;         // allow leaf funcs to be called without a map
	cleaf_t *map_leafs;             // = &map_leaf_empty;

	int nummarkbrushes;
	int *map_markbrushes;

	u32 num_models;
	Vec3 world_mins, world_maxs;

	int numbrushes;
	cbrush_t *map_brushes;

	int numfaces;
	cface_t *map_faces;

	int nummarkfaces;
	int *map_markfaces;

	Vec3 *map_verts;              // this will be freed
	int numvertexes;

	// each area has a list of portals that lead into other areas
	// when portals are closed, other areas may not be visible or
	// hearable even if the vis info says that it should be
	int numareas;                   // = 1
	carea_t map_area_empty;
	carea_t *map_areas;             // = &map_area_empty;
	int *map_areaportals;

	dvis_t *map_pvs;
	int map_visdatasize;

	uint8_t nullrow[MAX_CM_LEAFS / 8];

	int numentitychars;
	char map_entitystring_empty;
	char *map_entitystring;         // = &map_entitystring_empty;

	const u8 *cmod_base;

	// cm_trace.c
	cbrushside_t box_brushsides[6];
	cbrush_t box_brush[1];
	int box_markbrushes[1];
	cmodel_t box_cmodel[1];
	int box_checkcount;

	cbrushside_t oct_brushsides[10];
	cbrush_t oct_brush[1];
	int oct_markbrushes[1];
	cmodel_t oct_cmodel[1];
	int oct_checkcount;

	int *map_brush_checkcheckouts;
	int *map_face_checkcheckouts;
};

enum CModelServerOrClient {
	CM_Client,
	CM_Server,
};

CollisionModel * CM_LoadMap( CModelServerOrClient soc, Span< const u8 > data, u64 base_hash );
void CM_Free( CModelServerOrClient soc,  CollisionModel * cms );

cmodel_t * CM_FindCModel( CModelServerOrClient soc, StringHash hash );
cmodel_t * CM_TryFindCModel( CModelServerOrClient soc, StringHash hash );

bool CM_IsBrushModel( CModelServerOrClient soc, StringHash hash );

int CM_NumClusters( const CollisionModel *cms );
int CM_NumAreas( const CollisionModel *cms );
const char * CM_EntityString( const CollisionModel *cms );
size_t CM_EntityStringLen( const CollisionModel *cms );

// creates a clipping hull for an arbitrary bounding box
cmodel_t *CM_ModelForBBox( CollisionModel *cms, Vec3 mins, Vec3 maxs );
cmodel_t *CM_OctagonModelForBBox( CollisionModel *cms, Vec3 mins, Vec3 maxs );
void CM_InlineModelBounds( const CollisionModel *cms, const cmodel_t *cmodel, Vec3 * mins, Vec3 * maxs );

// returns an ORed contents mask
int CM_TransformedPointContents( CModelServerOrClient soc, CollisionModel * cms, Vec3 p, cmodel_t *cmodel, Vec3 origin, Vec3 angles );

void CM_TransformedBoxTrace( CModelServerOrClient soc, CollisionModel * cms, trace_t * tr, Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs,
							 const cmodel_t *cmodel, int brushmask, Vec3 origin, Vec3 angles );

int CM_ClusterRowSize( const CollisionModel *cms );
int CM_AreaRowSize( const CollisionModel *cms );
int CM_PointLeafnum( const CollisionModel *cms, Vec3 p );

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int CM_BoxLeafnums( CollisionModel *cms, Vec3 mins, Vec3 maxs, int *list, int listsize, int *topnode );

int CM_LeafCluster( const CollisionModel *cms, int leafnum );
int CM_LeafArea( const CollisionModel *cms, int leafnum );

void CM_SetAreaPortalState( CollisionModel *cms, int area1, int area2, bool open );
bool CM_AreasConnected( const CollisionModel *cms, int area1, int area2 );

void CM_WriteAreaBits( CollisionModel *cms, uint8_t *buffer );
bool CM_HeadnodeVisible( CollisionModel *cms, int headnode, uint8_t *visbits );

void CM_MergePVS( CollisionModel *cms, Vec3 org, uint8_t *out );
