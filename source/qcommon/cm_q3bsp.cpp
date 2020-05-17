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

#include <emmintrin.h>

#include "qcommon/qcommon.h"
#include "qcommon/hash.h"
#include "qcommon/string.h"
#include "qcommon/cm_local.h"
#include "qcommon/patch.h"

#define MAX_FACET_PLANES 32

static int CM_CreateFacetFromPoints( CollisionModel *cms, cbrush_t *facet, Vec3 *verts, int numverts, cshaderref_t *shaderref, cplane_t *brushplanes ) {
	Vec3 normal;
	float dist;
	cplane_t mainplane;
	Vec3 vec, vec2;
	int numbrushplanes;

	// set default values for brush
	facet->numsides = 0;
	facet->brushsides = NULL;
	facet->contents = shaderref->contents;

	MinMax3 bounds = MinMax3::Empty();

	// calculate plane for this triangle
	if( !PlaneFromPoints( verts, &mainplane ) ) {
		return 0;
	}

	// test a quad case
	if( numverts > 3 ) {
		float d = Dot( verts[3], mainplane.normal ) - mainplane.dist;
		if( d < -0.1f || d > 0.1f ) {
			return 0;
		}
	}

	numbrushplanes = 0;

	// add front plane
	SnapPlane( &mainplane.normal, &mainplane.dist );
	brushplanes[numbrushplanes].normal = mainplane.normal;
	brushplanes[numbrushplanes].dist = mainplane.dist; numbrushplanes++;

	// calculate mins & maxs
	for( int i = 0; i < numverts; i++ ) {
		bounds = Extend( bounds, verts[ i ] );
	}

	// add the axial planes
	for( int axis = 0; axis < 3; axis++ ) {
		for( int dir = -1; dir <= 1; dir += 2 ) {
			int i;
			for( i = 0; i < numbrushplanes; i++ ) {
				if( brushplanes[i].normal[axis] == dir ) {
					break;
				}
			}

			if( i == numbrushplanes ) {
				normal = Vec3( 0.0f );
				normal[axis] = dir;
				if( dir == 1 ) {
					dist = bounds.maxs[axis];
				} else {
					dist = -bounds.mins[axis];
				}

				brushplanes[numbrushplanes].normal = normal;
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	// add the edge bevels
	for( int i = 0; i < numverts; i++ ) {
		int j = ( i + 1 ) % numverts;

		vec = verts[i] - verts[j];
		if( Length( vec ) < 0.5 ) {
			continue;
		}
		vec = Normalize( vec );

		SnapVector( &vec );
		for( j = 0; j < 3; j++ ) {
			if( vec[j] == 1 || vec[j] == -1 ) {
				break; // axial
			}
		}
		if( j != 3 ) {
			continue; // only test non-axial edges

		}
		// try the six possible slanted axials from this edge
		for( int axis = 0; axis < 3; axis++ ) {
			for( int dir = -1; dir <= 1; dir += 2 ) {
				// construct a plane
				vec2 = Vec3( 0.0f );
				vec2[axis] = dir;
				normal = Cross( vec, vec2 );
				if( Length( normal ) < 0.5f ) {
					continue;
				}
				normal = Normalize( normal );
				dist = Dot( verts[i], normal );

				for( j = 0; j < numbrushplanes; j++ ) {
					// if this plane has already been used, skip it
					if( ComparePlanes( brushplanes[j].normal, brushplanes[j].dist, normal, dist ) ) {
						break;
					}
				}
				if( j != numbrushplanes ) {
					continue;
				}

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < numverts; j++ ) {
					if( j != i ) {
						float d = Dot( verts[j], normal ) - dist;
						if( d > 0.1f ) {
							break; // point in front: this plane isn't part of the outer hull
						}
					}
				}
				if( j != numverts ) {
					continue;
				}

				// add this plane
				brushplanes[numbrushplanes].normal = normal;
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
				if( numbrushplanes == MAX_FACET_PLANES ) {
					break;
				}
			}
		}
	}

	// spread facet mins/maxs by a unit
	bounds.mins -= 1.0f;
	bounds.maxs += 1.0f;

	facet->mins = ToSSE( bounds.mins );
	facet->maxs = ToSSE( bounds.maxs );

	return ( facet->numsides = numbrushplanes );
}

static void CM_CreatePatch( CollisionModel *cms, cface_t *patch, cshaderref_t *shaderref, Vec3 *verts, int *patch_cp ) {
	int step[2], size[2], flat[2];

	// find the degree of subdivision in the u and v directions
	Patch_GetFlatness( CM_SUBDIV_LEVEL, verts, 1, patch_cp, flat );

	step[0] = 1 << flat[0];
	step[1] = 1 << flat[1];
	size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
	size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
	if( size[0] <= 0 || size[1] <= 0 ) {
		return;
	}

	Vec3 * points = ALLOC_MANY( sys_allocator, Vec3, size[ 0 ] * size[ 1 ] );
	Patch_Evaluate( 1, &verts[0], patch_cp, step, points, 0 );
	Patch_RemoveLinearColumnsRows( points, 1, &size[0], &size[1], 0, NULL, NULL );

	cbrush_t * facets = ALLOC_MANY( sys_allocator, cbrush_t, ( size[0] - 1 ) * ( size[1] - 1 ) * 2 );
	cplane_t * brushplanes = ALLOC_MANY( sys_allocator, cplane_t, ( size[0] - 1 ) * ( size[1] - 1 ) * 2 * MAX_FACET_PLANES );

	defer {
		FREE( sys_allocator, points );
		FREE( sys_allocator, facets );
		FREE( sys_allocator, brushplanes );
	};

	int totalsides = 0;
	patch->numfacets = 0;
	patch->facets = NULL;
	MinMax3 bounds = MinMax3::Empty();

	// create a set of facets
	for( int v = 0; v < size[1] - 1; v++ ) {
		for( int u = 0; u < size[0] - 1; u++ ) {
			int i = v * size[0] + u;

			Vec3 tverts[4];
			tverts[0] = points[i];
			tverts[1] = points[i + size[0]];
			tverts[2] = points[i + size[0] + 1];
			tverts[3] = points[i + 1];

			for( i = 0; i < 4; i++ ) {
				bounds = Extend( bounds, tverts[ i ] );
			}

			// try to create one facet from a quad
			int numsides = CM_CreateFacetFromPoints( cms, &facets[patch->numfacets], tverts, 4, shaderref, brushplanes + totalsides );
			if( !numsides ) { // create two facets from triangles
				tverts[2] = tverts[3];
				numsides = CM_CreateFacetFromPoints( cms, &facets[patch->numfacets], tverts, 3, shaderref, brushplanes + totalsides );
				if( numsides ) {
					totalsides += numsides;
					patch->numfacets++;
				}

				tverts[0] = tverts[2];
				tverts[2] = points[v * size[0] + u + size[0] + 1];
				numsides = CM_CreateFacetFromPoints( cms, &facets[patch->numfacets], tverts, 3, shaderref, brushplanes + totalsides );
			}

			if( numsides ) {
				totalsides += numsides;
				patch->numfacets++;
			}
		}
	}

	if( patch->numfacets ) {
		patch->facets = ALLOC_MANY( sys_allocator, cbrush_t, patch->numfacets );
		memcpy( patch->facets, facets, patch->numfacets * sizeof( cbrush_t ) );

		cbrushside_t * brushsides = ALLOC_MANY( sys_allocator, cbrushside_t, totalsides );

		int k = 0;
		for( int i = 0; i < patch->numfacets; i++ ) {
			cbrush_t * facet = &patch->facets[ i ];
			facet->brushsides = brushsides;
			brushsides += facet->numsides;

			for( int j = 0; j < facet->numsides; j++ ) {
				cbrushside_t * s = &facet->brushsides[ j ];
				s->plane = brushplanes[k++];
				SnapPlane( &s->plane.normal, &s->plane.dist );
				s->surfFlags = shaderref->flags;
			}
		}

		patch->contents = shaderref->contents;

		bounds.mins -= 1.0f;
		bounds.maxs += 1.0f;
	}

	patch->mins = ToSSE( bounds.mins );
	patch->maxs = ToSSE( bounds.maxs );
}

static void CMod_LoadSurfaces( CollisionModel *cms, lump_t *l ) {
	int i;
	int count;
	char *buffer;
	size_t len, bufLen, bufSize;
	dshaderref_t *in;
	cshaderref_t *out;

	in = ( dshaderref_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadSurfaces: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "CMod_LoadSurfaces: map with no shaders" );
	}

	out = cms->map_shaderrefs = ( cshaderref_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numshaderrefs = count;

	buffer = NULL;
	bufLen = bufSize = 0;

	for( i = 0; i < count; i++, in++, out++, bufLen += len + 1 ) {
		len = strlen( in->name );
		if( bufLen + len >= bufSize ) {
			bufSize = bufLen + len + 128;
			if( buffer ) {
				buffer = ( char * ) Mem_Realloc( buffer, bufSize );
			} else {
				buffer = ( char * ) Mem_Alloc( cmap_mempool, bufSize );
			}
		}

		// Vic: ZOMG, this is so nasty, perfectly valid in C though
		out->name = ( char * )( ( void * )bufLen );
		strcpy( buffer + bufLen, in->name );
		out->flags = LittleLong( in->flags );
		out->contents = LittleLong( in->contents );
	}

	for( i = 0; i < count; i++ )
		cms->map_shaderrefs[i].name = buffer + ( size_t )( ( void * )cms->map_shaderrefs[i].name );
}

static void CMod_LoadVertexes( CollisionModel *cms, lump_t *l ) {
	int i;
	int count;
	dvertex_t *in;
	Vec3 *out;

	in = ( dvertex_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMOD_LoadVertexes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no vertexes" );
	}

	out = cms->map_verts = ( Vec3 * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numvertexes = count;

	for( i = 0; i < count; i++, in++ ) {
		out[i].x = LittleFloat( in->point[0] );
		out[i].y = LittleFloat( in->point[1] );
		out[i].z = LittleFloat( in->point[2] );
	}
}

static void CMod_LoadVertexes_RBSP( CollisionModel *cms, lump_t *l ) {
	int i;
	int count;
	rdvertex_t *in;
	Vec3 *out;

	in = ( rdvertex_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadVertexes_RBSP: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no vertexes" );
	}

	out = cms->map_verts = ( Vec3 * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numvertexes = count;

	for( i = 0; i < count; i++, in++ ) {
		out[i].x = LittleFloat( in->point[0] );
		out[i].y = LittleFloat( in->point[1] );
		out[i].z = LittleFloat( in->point[2] );
	}
}

static inline void CMod_LoadFace( CollisionModel *cms, cface_t *out, int shadernum, int firstvert, int numverts, int *patch_cp ) {
	cshaderref_t *shaderref;

	shadernum = LittleLong( shadernum );
	if( shadernum < 0 || shadernum >= cms->numshaderrefs ) {
		return;
	}

	shaderref = &cms->map_shaderrefs[shadernum];
	if( !shaderref->contents || ( shaderref->flags & SURF_NONSOLID ) ) {
		return;
	}

	patch_cp[0] = LittleLong( patch_cp[0] );
	patch_cp[1] = LittleLong( patch_cp[1] );
	if( patch_cp[0] <= 0 || patch_cp[1] <= 0 ) {
		return;
	}

	firstvert = LittleLong( firstvert );
	if( numverts <= 0 || firstvert < 0 || firstvert >= cms->numvertexes ) {
		return;
	}

	CM_CreatePatch( cms, out, shaderref, cms->map_verts + firstvert, patch_cp );
}

static void CMod_LoadFaces( CollisionModel *cms, lump_t *l ) {
	int i, count;
	dface_t *in;
	cface_t *out;

	in = ( dface_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadFaces: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no faces" );
	}

	out = cms->map_faces = ( cface_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numfaces = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = 0;
		out->numfacets = 0;
		out->facets = NULL;
		if( LittleLong( in->facetype ) != FaceType_Patch ) {
			continue;
		}
		CMod_LoadFace( cms, out, in->shadernum, in->firstvert, in->numverts, in->patch_cp );
	}
}

static void CMod_LoadFaces_RBSP( CollisionModel *cms, lump_t *l ) {
	int i, count;
	rdface_t *in;
	cface_t *out;

	in = ( rdface_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadFaces_RBSP: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no faces" );
	}

	out = cms->map_faces = ( cface_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numfaces = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = 0;
		out->numfacets = 0;
		out->facets = NULL;
		if( LittleLong( in->facetype ) != FaceType_Patch ) {
			continue;
		}
		CMod_LoadFace( cms, out, in->shadernum, in->firstvert, in->numverts, in->patch_cp );
	}
}

static void CMod_LoadSubmodels( CModelServerOrClient soc, CollisionModel *cms, lump_t *l ) {
	const dmodel_t * in = ( dmodel_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadSubmodels: funny lump size" );
	}
	int count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no models" );
	}

	cms->num_models = count;

	for( int i = 0; i < count; i++, in++ ) {
		String< 16 > suffix( "*{}", i );
		u64 hash = Hash64( suffix.c_str(), suffix.len(), cms->base_hash );

		cmodel_t * model = CM_NewCModel( soc, hash );

		model->faces = cms->map_faces;
		model->nummarkfaces = LittleLong( in->numfaces );
		model->markfaces = ( int * ) Mem_Alloc( cmap_mempool, model->nummarkfaces * sizeof( *model->markfaces ) );

		model->brushes = cms->map_brushes;
		model->nummarkbrushes = LittleLong( in->numbrushes );
		model->markbrushes = ( int * ) Mem_Alloc( cmap_mempool, model->nummarkbrushes * sizeof( *model->markbrushes ) );

		if( model->nummarkfaces ) {
			int firstface = LittleLong( in->firstface );
			for( int j = 0; j < model->nummarkfaces; j++ )
				model->markfaces[j] = firstface + j;
		}

		if( model->nummarkbrushes ) {
			int firstbrush = LittleLong( in->firstbrush );
			for( int j = 0; j < model->nummarkbrushes; j++ ) {
				model->markbrushes[j] = firstbrush + j;
			}
		}

		for( int j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			model->mins[j] = LittleFloat( in->mins[j] ) - 1;
			model->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}
	}
}

static void CMod_LoadNodes( CollisionModel *cms, lump_t *l ) {
	int i;
	int count;
	dnode_t *in;
	cnode_t *out;

	in = ( dnode_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadNodes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map has no nodes" );
	}

	out = cms->map_nodes = ( cnode_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numnodes = count;

	for( i = 0; i < 3; i++ ) {
		cms->world_mins[i] = LittleFloat( in->mins[i] );
		cms->world_maxs[i] = LittleFloat( in->maxs[i] );
	}

	for( i = 0; i < count; i++, out++, in++ ) {
		out->plane = cms->map_planes + LittleLong( in->planenum );
		out->children[0] = LittleLong( in->children[0] );
		out->children[1] = LittleLong( in->children[1] );
	}
}

static void CMod_LoadMarkFaces( CollisionModel *cms, lump_t *l ) {
	int i, j;
	int count;
	int *out;
	int *in;

	in = ( int * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadMarkFaces: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no leaffaces" );
	}

	out = cms->map_markfaces = ( int * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->nummarkfaces = count;

	for( i = 0; i < count; i++ ) {
		j = LittleLong( in[i] );
		if( j < 0 || j >= cms->numfaces ) {
			Com_Error( ERR_DROP, "CMod_LoadMarkFaces: bad surface number" );
		}
		out[i] = j;
	}
}

static void CMod_LoadLeafs( CollisionModel *cms, lump_t *l ) {
	int i, j, k;
	int count;
	cleaf_t *out;
	dleaf_t *in;

	in = ( dleaf_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadLeafs: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no leafs" );
	}

	out = cms->map_leafs = ( cleaf_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = 0;
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );
		out->markbrushes = cms->map_markbrushes + LittleLong( in->firstleafbrush );
		out->nummarkbrushes = LittleLong( in->numleafbrushes );
		out->markfaces = cms->map_markfaces + LittleLong( in->firstleafface );
		out->nummarkfaces = LittleLong( in->numleaffaces );

		// OR brushes' contents
		for( j = 0; j < out->nummarkbrushes; j++ )
			out->contents |= cms->map_brushes[out->markbrushes[j]].contents;

		// exclude markfaces that have no facets
		// so we don't perform this check at runtime
		for( j = 0; j < out->nummarkfaces; ) {
			k = j;
			if( !cms->map_faces[out->markfaces[j]].facets ) {
				for(; ( ++j < out->nummarkfaces ) && !cms->map_faces[out->markfaces[j]].facets; ) ;
				if( j < out->nummarkfaces ) {
					memmove( &out->markfaces[k], &out->markfaces[j], ( out->nummarkfaces - j ) * sizeof( *out->markfaces ) );
				}
				out->nummarkfaces -= j - k;
			}
			j = k + 1;
		}

		// OR patches' contents
		for( j = 0; j < out->nummarkfaces; j++ )
			out->contents |= cms->map_faces[out->markfaces[j]].contents;

		if( out->area >= cms->numareas ) {
			cms->numareas = out->area + 1;
		}
	}
}

static void CMod_LoadPlanes( CollisionModel *cms, lump_t *l ) {
	dplane_t * in = ( dplane_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadPlanes: funny lump size" );
	}
	int count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no planes" );
	}

	cplane_t * out = cms->map_planes = ( cplane_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numplanes = count;

	for( int i = 0; i < count; i++, in++, out++ ) {
		for( int j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
		}

		out->dist = LittleFloat( in->dist );
	}
}

static void CMod_LoadMarkBrushes( CollisionModel *cms, lump_t *l ) {
	int i;
	int count;
	int *out;
	int *in;

	in = ( int * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadMarkBrushes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no leafbrushes" );
	}

	out = cms->map_markbrushes = ( int * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->nummarkbrushes = count;

	for( i = 0; i < count; i++, in++ )
		out[i] = LittleLong( *in );
}

static void CMod_LoadBrushSides( CollisionModel *cms, lump_t *l ) {
	dbrushside_t * in = ( dbrushside_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadBrushSides: funny lump size" );
	}
	int count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no brushsides" );
	}

	cbrushside_t * out = cms->map_brushsides = ( cbrushside_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numbrushsides = count;

	for( int i = 0; i < count; i++, in++, out++ ) {
		cplane_t *plane = cms->map_planes + LittleLong( in->planenum );
		int j = LittleLong( in->shadernum );
		if( j >= cms->numshaderrefs ) {
			Com_Error( ERR_DROP, "Bad brushside texinfo" );
		}
		out->plane = *plane;
		out->surfFlags = cms->map_shaderrefs[j].flags;
	}
}

static void CMod_LoadBrushSides_RBSP( CollisionModel *cms, lump_t *l ) {
	int i, j;
	int count;
	cbrushside_t *out;
	rdbrushside_t *in;

	in = ( rdbrushside_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadBrushSides_RBSP: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no brushsides" );
	}

	out = cms->map_brushsides = ( cbrushside_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		cplane_t *plane = cms->map_planes + LittleLong( in->planenum );
		j = LittleLong( in->shadernum );
		if( j >= cms->numshaderrefs ) {
			Com_Error( ERR_DROP, "Bad brushside texinfo" );
		}
		out->plane = *plane;
		out->surfFlags = cms->map_shaderrefs[j].flags;
	}
}

static void CM_BoundBrush( cbrush_t *brush ) {
	Vec3 mins, maxs;
	for( int i = 0; i < 3; i++ ) {
		mins[ i ] = -brush->brushsides[ i * 2 + 0 ].plane.dist;
		maxs[ i ] = +brush->brushsides[ i * 2 + 1 ].plane.dist;
	}

	brush->mins = ToSSE( mins );
	brush->maxs = ToSSE( maxs );
}

static void CMod_LoadBrushes( CollisionModel *cms, lump_t *l ) {
	int i;
	int count;
	dbrush_t *in;
	cbrush_t *out;
	int shaderref;

	in = ( dbrush_t * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadBrushes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no brushes" );
	}

	out = cms->map_brushes = ( cbrush_t * ) Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	cms->numbrushes = count;

	for( i = 0; i < count; i++, out++, in++ ) {
		shaderref = LittleLong( in->shadernum );
		out->contents = cms->map_shaderrefs[shaderref].contents;
		out->numsides = LittleLong( in->numsides );
		out->brushsides = cms->map_brushsides + LittleLong( in->firstside );
		CM_BoundBrush( out );
	}
}

static void CMod_LoadVisibility( CollisionModel *cms, lump_t *l ) {
	cms->map_visdatasize = l->filelen;
	if( !cms->map_visdatasize ) {
		cms->map_pvs = NULL;
		return;
	}

	cms->map_pvs = ( dvis_t * ) Mem_Alloc( cmap_mempool, cms->map_visdatasize );
	memcpy( cms->map_pvs, cms->cmod_base + l->fileofs, cms->map_visdatasize );

	cms->map_pvs->numclusters = LittleLong( cms->map_pvs->numclusters );
	cms->map_pvs->rowsize = LittleLong( cms->map_pvs->rowsize );
}

static void CMod_LoadEntityString( CollisionModel *cms, lump_t *l ) {
	cms->numentitychars = l->filelen;
	if( !l->filelen ) {
		return;
	}

	cms->map_entitystring = ( char * ) Mem_Alloc( cmap_mempool, cms->numentitychars );
	memcpy( cms->map_entitystring, cms->cmod_base + l->fileofs, l->filelen );
}

void CM_LoadQ3BrushModel( CModelServerOrClient soc, CollisionModel * cms, Span< const u8 > data ) {
	dheader_t header;
	memcpy( &header, data.ptr, sizeof( header ) );

	cms->checksum = Hash32( data );

	for( size_t i = 0; i < sizeof( dheader_t ) / 4; i++ )
		( (int *)&header )[i] = LittleLong( ( (int *)&header )[i] );
	cms->cmod_base = data.ptr;

	bool idbsp = memcmp( &header.ident, IDBSPHEADER, sizeof( header.ident ) ) == 0;

	// load into heap
	CMod_LoadSurfaces( cms, &header.lumps[LUMP_SHADERREFS] );
	CMod_LoadPlanes( cms, &header.lumps[LUMP_PLANES] );
	if( idbsp ) {
		CMod_LoadBrushSides( cms, &header.lumps[LUMP_BRUSHSIDES] );
	}
	else {
		CMod_LoadBrushSides_RBSP( cms, &header.lumps[LUMP_BRUSHSIDES] );
	}
	CMod_LoadBrushes( cms, &header.lumps[LUMP_BRUSHES] );
	CMod_LoadMarkBrushes( cms, &header.lumps[LUMP_LEAFBRUSHES] );
	if( idbsp ) {
		CMod_LoadVertexes( cms, &header.lumps[LUMP_VERTEXES] );
		CMod_LoadFaces( cms, &header.lumps[LUMP_FACES] );
	}
	else {
		CMod_LoadVertexes_RBSP( cms, &header.lumps[LUMP_VERTEXES] );
		CMod_LoadFaces_RBSP( cms, &header.lumps[LUMP_FACES] );
	}
	CMod_LoadMarkFaces( cms, &header.lumps[LUMP_LEAFFACES] );
	CMod_LoadLeafs( cms, &header.lumps[LUMP_LEAFS] );
	CMod_LoadNodes( cms, &header.lumps[LUMP_NODES] );
	CMod_LoadSubmodels( soc, cms, &header.lumps[LUMP_MODELS] );
	CMod_LoadVisibility( cms, &header.lumps[LUMP_VISIBILITY] );
	CMod_LoadEntityString( cms, &header.lumps[LUMP_ENTITIES] );

	if( cms->numvertexes ) {
		Mem_Free( cms->map_verts );
	}
}
