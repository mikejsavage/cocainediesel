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
#include "qcommon/hashmap.h"
#include "qcommon/string.h"

static Hashmap< cmodel_t, 4096 > client_cmodels;
static Hashmap< cmodel_t, 4096 > server_cmodels;

static Hashmap< cmodel_t, 4096 > * GetCModels( CModelServerOrClient soc ) {
	return soc == CM_Client ? &client_cmodels : &server_cmodels;
}

static void CM_AllocateCheckCounts( CollisionModel *cms ) {
	cms->checkcount = 0;
	cms->map_brush_checkcheckouts = AllocMany< int >( sys_allocator, cms->numbrushes );
	cms->map_face_checkcheckouts = AllocMany< int >( sys_allocator, cms->numfaces );
}

static void CM_FreeCheckCounts( CollisionModel *cms ) {
	cms->checkcount = 0;

	if( cms->map_brush_checkcheckouts ) {
		Free( sys_allocator, cms->map_brush_checkcheckouts );
		cms->map_brush_checkcheckouts = NULL;
	}

	if( cms->map_face_checkcheckouts ) {
		Free( sys_allocator, cms->map_face_checkcheckouts );
		cms->map_face_checkcheckouts = NULL;
	}
}

static void CM_Clear( CModelServerOrClient soc, CollisionModel * cms ) {
	if( cms->map_shaderrefs ) {
		Free( sys_allocator, cms->map_shaderrefs[0].name );
		Free( sys_allocator, cms->map_shaderrefs );
		cms->map_shaderrefs = NULL;
		cms->numshaderrefs = 0;
	}

	if( cms->map_faces ) {
		for( int i = 0; i < cms->numfaces; i++ ) {
			Free( sys_allocator, cms->map_faces[i].facets );
		}
		Free( sys_allocator, cms->map_faces );
		cms->map_faces = NULL;
		cms->numfaces = 0;
	}

	for( u32 i = 0; i < cms->num_models; i++ ) {
		String< 16 > suffix( "*{}", i );
		u64 hash = Hash64( suffix.span(), cms->base_hash );
		cmodel_t * model = GetCModels( soc )->get( hash );

		Free( sys_allocator, model->markfaces );
		Free( sys_allocator, model->markbrushes );

		bool ok = GetCModels( soc )->remove( hash );
		Assert( ok );
	}

	if( cms->map_nodes ) {
		Free( sys_allocator, cms->map_nodes );
		cms->map_nodes = NULL;
		cms->numnodes = 0;
	}

	if( cms->map_markfaces ) {
		Free( sys_allocator, cms->map_markfaces );
		cms->map_markfaces = NULL;
		cms->nummarkfaces = 0;
	}

	if( cms->map_leafs != &cms->map_leaf_empty ) {
		Free( sys_allocator, cms->map_leafs );
		cms->map_leafs = &cms->map_leaf_empty;
		cms->numleafs = 0;
	}

	if( cms->map_areas != &cms->map_area_empty ) {
		Free( sys_allocator, cms->map_areas );
		cms->map_areas = &cms->map_area_empty;
		cms->numareas = 0;
	}

	if( cms->map_areaportals ) {
		Free( sys_allocator, cms->map_areaportals );
		cms->map_areaportals = NULL;
	}

	if( cms->map_planes ) {
		Free( sys_allocator, cms->map_planes );
		cms->map_planes = NULL;
		cms->numplanes = 0;
	}

	if( cms->map_markbrushes ) {
		Free( sys_allocator, cms->map_markbrushes );
		cms->map_markbrushes = NULL;
		cms->nummarkbrushes = 0;
	}

	if( cms->map_brushsides ) {
		Free( sys_allocator, cms->map_brushsides );
		cms->map_brushsides = NULL;
		cms->numbrushsides = 0;
	}

	if( cms->map_brushes ) {
		Free( sys_allocator, cms->map_brushes );
		cms->map_brushes = NULL;
		cms->numbrushes = 0;
	}

	if( cms->map_pvs ) {
		Free( sys_allocator, cms->map_pvs );
		cms->map_pvs = NULL;
	}

	if( cms->map_entitystring != &cms->map_entitystring_empty ) {
		Free( sys_allocator, cms->map_entitystring );
		cms->map_entitystring = &cms->map_entitystring_empty;
	}

	CM_FreeCheckCounts( cms );

	ClearBounds( &cms->world_mins, &cms->world_maxs );
}

/*
===============================================================================

MAP LOADING

===============================================================================
*/

/*
* CM_LoadMap
* Loads in the map and all submodels
*/
CollisionModel * CM_LoadMap( CModelServerOrClient soc, Span< const u8 > data, u64 base_hash ) {
	TracyZoneScoped;

	CollisionModel * cms = Alloc< CollisionModel >( sys_allocator );
	*cms = { };

	cms->base_hash = base_hash;

	const char * suffix = "*0";
	cms->world_hash = Hash64( suffix, strlen( suffix ), cms->base_hash );

	CM_InitBoxHull( cms );
	CM_InitOctagonHull( cms );
	CM_Clear( soc, cms );

	CM_LoadQ3BrushModel( soc, cms, data );

	if( cms->numareas ) {
		cms->map_areas = AllocMany< carea_t >( sys_allocator, cms->numareas );
		cms->map_areaportals = AllocMany< int >( sys_allocator, cms->numareas * cms->numareas );

		memset( cms->map_areaportals, 0, cms->numareas * cms->numareas * sizeof( *cms->map_areaportals ) );
		CM_FloodAreaConnections( cms );
	}

	CM_AllocateCheckCounts( cms );

	memset( cms->nullrow, 255, MAX_CM_LEAFS / 8 );

	return cms;
}

void CM_Free( CModelServerOrClient soc, CollisionModel * cms ) {
	CM_Clear( soc, cms );
	Free( sys_allocator, cms );
}

cmodel_t * CM_NewCModel( CModelServerOrClient soc, u64 hash ) {
	return GetCModels( soc )->add( hash );
}

cmodel_t * CM_TryFindCModel( CModelServerOrClient soc, StringHash hash ) {
	return GetCModels( soc )->get( hash.hash );
}

cmodel_t * CM_FindCModel( CModelServerOrClient soc, StringHash hash ) {
	cmodel_t * cmodel = CM_TryFindCModel( soc, hash );
	if( cmodel == NULL ) {
		Fatal( "FindCModel failed" );
	}
	return cmodel;
}

bool CM_IsBrushModel( CModelServerOrClient soc, StringHash hash ) {
	return CM_TryFindCModel( soc, hash ) != NULL;
}

void CM_InlineModelBounds( const CollisionModel *cms, const cmodel_t *cmodel, Vec3 * mins, Vec3 * maxs ) {
	if( cmodel->hash == cms->world_hash ) {
		*mins = cms->world_mins;
		*maxs = cms->world_maxs;
	} else {
		*mins = cmodel->mins;
		*maxs = cmodel->maxs;
	}
}

size_t CM_EntityStringLen( const CollisionModel * cms ) {
	return cms->numentitychars;
}

const char * CM_EntityString( const CollisionModel * cms ) {
	return cms->map_entitystring;
}

int CM_LeafCluster( const CollisionModel *cms, int leafnum ) {
	if( leafnum < 0 || leafnum >= cms->numleafs ) {
		Fatal( "CM_LeafCluster: bad number" );
	}
	return cms->map_leafs[leafnum].cluster;
}

int CM_LeafArea( const CollisionModel *cms, int leafnum ) {
	if( leafnum < 0 || leafnum >= cms->numleafs ) {
		Fatal( "CM_LeafArea: bad number" );
	}
	return cms->map_leafs[leafnum].area;
}

/*
===============================================================================

PVS

===============================================================================
*/

int CM_ClusterRowSize( const CollisionModel *cms ) {
	return cms->map_pvs ? cms->map_pvs->rowsize : MAX_CM_LEAFS / 8;
}

static int CM_ClusterRowLongs( const CollisionModel *cms ) {
	return cms->map_pvs ? ( cms->map_pvs->rowsize + 3 ) / 4 : MAX_CM_LEAFS / 32;
}

int CM_NumClusters( const CollisionModel *cms ) {
	return cms->map_pvs ? cms->map_pvs->numclusters : 0;
}

static const uint8_t *CM_ClusterPVS( const CollisionModel *cms, int cluster ) {
	const dvis_t *vis = cms->map_pvs;

	if( cluster == -1 || !vis ) {
		return cms->nullrow;
	}

	return ( const uint8_t * )vis->data + cluster * vis->rowsize;
}

int CM_NumAreas( const CollisionModel *cms ) {
	return cms->numareas;
}

int CM_AreaRowSize( const CollisionModel *cms ) {
	return ( cms->numareas + 7 ) / 8;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

static void CM_FloodArea_r( CollisionModel *cms, int areanum, int floodnum ) {
	int i;
	carea_t *area;
	int *p;

	area = &cms->map_areas[areanum];
	if( area->floodvalid == cms->floodvalid ) {
		if( area->floodnum == floodnum ) {
			return;
		}
		Fatal( "FloodArea_r: reflooded" );
	}

	area->floodnum = floodnum;
	area->floodvalid = cms->floodvalid;
	p = cms->map_areaportals + areanum * cms->numareas;
	for( i = 0; i < cms->numareas; i++ ) {
		if( p[i] > 0 ) {
			CM_FloodArea_r( cms, i, floodnum );
		}
	}
}

void CM_FloodAreaConnections( CollisionModel *cms ) {
	int i;
	int floodnum;

	// all current floods are now invalid
	cms->floodvalid++;
	floodnum = 0;
	for( i = 0; i < cms->numareas; i++ ) {
		if( cms->map_areas[i].floodvalid == cms->floodvalid ) {
			continue; // already flooded into
		}
		floodnum++;
		CM_FloodArea_r( cms, i, floodnum );
	}
}

void CM_SetAreaPortalState( CollisionModel *cms, int area1, int area2, bool open ) {
	int row1, row2;

	if( area1 == area2 ) {
		return;
	}
	if( area1 < 0 || area2 < 0 ) {
		return;
	}

	row1 = area1 * cms->numareas + area2;
	row2 = area2 * cms->numareas + area1;
	if( open ) {
		cms->map_areaportals[row1]++;
		cms->map_areaportals[row2]++;
	} else {
		if( cms->map_areaportals[row1] > 0 ) {
			cms->map_areaportals[row1]--;
		}
		if( cms->map_areaportals[row2] > 0 ) {
			cms->map_areaportals[row2]--;
		}
	}

	CM_FloodAreaConnections( cms );
}

bool CM_AreasConnected( const CollisionModel *cms, int area1, int area2 ) {
	if( area1 == area2 ) {
		return true;
	}
	if( area1 < 0 || area2 < 0 ) {
		return true;
	}

	if( area1 >= cms->numareas || area2 >= cms->numareas ) {
		Fatal( "CM_AreasConnected: area >= numareas" );
	}

	if( cms->map_areas[area1].floodnum == cms->map_areas[area2].floodnum ) {
		return true;
	}
	return false;
}

static int CM_MergeAreaBits( const CollisionModel *cms, uint8_t *buffer, int area ) {
	int i;

	if( area < 0 ) {
		return CM_AreaRowSize( cms );
	}

	for( i = 0; i < cms->numareas; i++ ) {
		if( CM_AreasConnected( cms, i, area ) ) {
			buffer[i >> 3] |= 1 << ( i & 7 );
		}
	}

	return CM_AreaRowSize( cms );
}

void CM_WriteAreaBits( const CollisionModel *cms, uint8_t *buffer ) {
	int rowsize = CM_AreaRowSize( cms );
	int bytes = rowsize * cms->numareas;

	memset( buffer, 0, bytes );

	for( int i = 0; i < cms->numareas; i++ ) {
		uint8_t * row = buffer + i * rowsize;
		CM_MergeAreaBits( cms, row, i );
	}
}

/*
* CM_HeadnodeVisible
* Returns true if any leaf under headnode has a cluster that
* is potentially visible
*/
bool CM_HeadnodeVisible( const CollisionModel *cms, int nodenum, const uint8_t *visbits ) {
	int cluster;
	cnode_t *node;

	while( nodenum >= 0 ) {
		node = &cms->map_nodes[nodenum];
		if( CM_HeadnodeVisible( cms, node->children[0], visbits ) ) {
			return true;
		}
		nodenum = node->children[1];
	}

	cluster = cms->map_leafs[-1 - nodenum].cluster;
	if( cluster == -1 ) {
		return false;
	}
	if( visbits[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) {
		return true;
	}
	return false;
}


/*
* CM_MergePVS
* Merge PVS at origin into out
*/
void CM_MergePVS( const CollisionModel *cms, Vec3 org, uint8_t *out ) {
	int leafs[128];
	int i, j, count;
	int longs;
	const uint8_t *src;
	Vec3 mins, maxs;

	mins = org - Vec3( 9.0f );
	maxs = org + Vec3( 9.0f );

	count = CM_BoxLeafnums( cms, mins, maxs, leafs, sizeof( leafs ) / sizeof( int ), NULL );
	if( count < 1 ) {
		Fatal( "CM_MergePVS: count < 1" );
	}
	longs = CM_ClusterRowLongs( cms );

	// convert leafs to clusters
	for( i = 0; i < count; i++ )
		leafs[i] = CM_LeafCluster( cms, leafs[i] );

	// or in all the other leaf bits
	for( i = 0; i < count; i++ ) {
		for( j = 0; j < i; j++ ) {
			if( leafs[i] == leafs[j] ) {
				break;
			}
		}
		if( j != i ) {
			continue; // already have the cluster we want
		}
		src = CM_ClusterPVS( cms, leafs[i] );
		for( j = 0; j < longs; j++ )
			align_cast< int >( out )[j] |= align_cast< const int >( src )[ j ];
	}
}

int CM_MergeVisSets( CollisionModel *cms, Vec3 org, uint8_t *pvs, uint8_t *areabits ) {
	int area;

	Assert( pvs || areabits );

	if( pvs ) {
		CM_MergePVS( cms, org, pvs );
	}

	area = CM_PointLeafnum( cms, org );

	area = CM_LeafArea( cms, area );
	if( areabits && area > -1 ) {
		CM_MergeAreaBits( cms, areabits, area );
	}

	return CM_AreaRowSize( cms ); // areabytes
}
