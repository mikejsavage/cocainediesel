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
	cms->map_brush_checkcheckouts = ALLOC_MANY( sys_allocator, int, cms->numbrushes );
	cms->map_face_checkcheckouts = ALLOC_MANY( sys_allocator, int, cms->numfaces );
}

static void CM_FreeCheckCounts( CollisionModel *cms ) {
	cms->checkcount = 0;

	if( cms->map_brush_checkcheckouts ) {
		FREE( sys_allocator, cms->map_brush_checkcheckouts );
		cms->map_brush_checkcheckouts = NULL;
	}

	if( cms->map_face_checkcheckouts ) {
		FREE( sys_allocator, cms->map_face_checkcheckouts );
		cms->map_face_checkcheckouts = NULL;
	}
}

static void CM_Clear( CModelServerOrClient soc, CollisionModel * cms ) {
	if( cms->map_shaderrefs ) {
		FREE( sys_allocator, cms->map_shaderrefs[0].name );
		FREE( sys_allocator, cms->map_shaderrefs );
		cms->map_shaderrefs = NULL;
		cms->numshaderrefs = 0;
	}

	if( cms->map_faces ) {
		for( int i = 0; i < cms->numfaces; i++ ) {
			FREE( sys_allocator, cms->map_faces[i].facets );
		}
		FREE( sys_allocator, cms->map_faces );
		cms->map_faces = NULL;
		cms->numfaces = 0;
	}

	for( u32 i = 0; i < cms->num_models; i++ ) {
		String< 16 > suffix( "*{}", i );
		u64 hash = Hash64( suffix.c_str(), suffix.length(), cms->base_hash );
		cmodel_t * model = GetCModels( soc )->get( hash );

		FREE( sys_allocator, model->markfaces );
		FREE( sys_allocator, model->markbrushes );

		bool ok = GetCModels( soc )->remove( hash );
		assert( ok );
	}

	if( cms->map_nodes ) {
		FREE( sys_allocator, cms->map_nodes );
		cms->map_nodes = NULL;
		cms->numnodes = 0;
	}

	if( cms->map_markfaces ) {
		FREE( sys_allocator, cms->map_markfaces );
		cms->map_markfaces = NULL;
		cms->nummarkfaces = 0;
	}

	if( cms->map_leafs != &cms->map_leaf_empty ) {
		FREE( sys_allocator, cms->map_leafs );
		cms->map_leafs = &cms->map_leaf_empty;
		cms->numleafs = 0;
	}

	if( cms->map_planes ) {
		FREE( sys_allocator, cms->map_planes );
		cms->map_planes = NULL;
		cms->numplanes = 0;
	}

	if( cms->map_markbrushes ) {
		FREE( sys_allocator, cms->map_markbrushes );
		cms->map_markbrushes = NULL;
		cms->nummarkbrushes = 0;
	}

	if( cms->map_brushsides ) {
		FREE( sys_allocator, cms->map_brushsides );
		cms->map_brushsides = NULL;
		cms->numbrushsides = 0;
	}

	if( cms->map_brushes ) {
		FREE( sys_allocator, cms->map_brushes );
		cms->map_brushes = NULL;
		cms->numbrushes = 0;
	}

	if( cms->map_entitystring != &cms->map_entitystring_empty ) {
		FREE( sys_allocator, cms->map_entitystring );
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

	CollisionModel * cms = ALLOC( sys_allocator, CollisionModel );
	*cms = { };

	cms->base_hash = base_hash;

	const char * suffix = "*0";
	cms->world_hash = Hash64( suffix, strlen( suffix ), cms->base_hash );

	CM_InitBoxHull( cms );
	CM_InitOctagonHull( cms );
	CM_Clear( soc, cms );

	CM_LoadQ3BrushModel( soc, cms, data );

	CM_AllocateCheckCounts( cms );

	return cms;
}

void CM_Free( CModelServerOrClient soc, CollisionModel * cms ) {
	CM_Clear( soc, cms );
	FREE( sys_allocator, cms );
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
