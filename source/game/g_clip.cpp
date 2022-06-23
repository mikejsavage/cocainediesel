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

#include "game/g_local.h"
#include "qcommon/cmodel.h"

//===============================================================================
//
//ENTITY AREA CHECKING
//
//FIXME: this use of "area" is different from the bsp file use
//===============================================================================

#define AREA_GRID       128
#define AREA_GRIDNODES  ( AREA_GRID * AREA_GRID )
#define AREA_GRIDMINSIZE 64.0f  // minimum areagrid cell size, smaller values
// work better for lots of small objects, higher
// values for large objects

typedef struct
{
	link_t grid[AREA_GRIDNODES];
	link_t outside;
	Vec3 bias;
	Vec3 scale;
	Vec3 mins;
	Vec3 maxs;
	Vec3 size;
	int marknumber;

	// since the areagrid can have multiple references to one entity,
	// we should avoid extensive checking on entities already encountered
	int entmarknumber[MAX_EDICTS];
} areagrid_t;

static areagrid_t g_areagrid;

#define CFRAME_UPDATE_BACKUP    64  // copies of SyncEntityState to keep buffered (1 second of backup at 62 fps).
#define CFRAME_UPDATE_MASK  ( CFRAME_UPDATE_BACKUP - 1 )

struct c4clipedict_t {
	SyncEntityState s;
	entity_shared_t r;
};

//backups of all server frames areas and edicts
struct c4frame_t {
	c4clipedict_t clipEdicts[MAX_EDICTS];
	int numedicts;

	int64_t timestamp;
	int64_t framenum;
};

static c4frame_t sv_collisionframes[CFRAME_UPDATE_BACKUP];
static int64_t sv_collisionFrameNum = 0;

void GClip_BackUpCollisionFrame() {
	c4frame_t *cframe;
	edict_t *svedict;
	int i;

	// fixme: should check for any validation here?

	cframe = &sv_collisionframes[sv_collisionFrameNum & CFRAME_UPDATE_MASK];
	cframe->timestamp = svs.gametime;
	cframe->framenum = sv_collisionFrameNum;
	sv_collisionFrameNum++;

	//memset( cframe->clipEdicts, 0, sizeof(cframe->clipEdicts) );

	//backup edicts
	for( i = 0; i < game.numentities; i++ ) {
		svedict = &game.edicts[i];

		cframe->clipEdicts[i].r.inuse = svedict->r.inuse;
		cframe->clipEdicts[i].r.solid = svedict->r.solid;
		if( !svedict->r.inuse || svedict->r.solid == SOLID_NOT
			|| ( svedict->r.solid == SOLID_TRIGGER && !( i >= 1 && i <= server_gs.maxclients ) ) ) {
			continue;
		}

		cframe->clipEdicts[i].r = svedict->r;
		cframe->clipEdicts[i].s = svedict->s;
	}
	cframe->numedicts = game.numentities;
}

static c4clipedict_t *GClip_GetClipEdictForDeltaTime( int entNum, int deltaTime ) {
	static int index = 0;
	static c4clipedict_t clipEnts[8];
	static c4clipedict_t *clipent;
	static c4clipedict_t clipentNewer; // for interpolation
	c4frame_t *cframe = NULL;
	int64_t backTime, cframenum;
	unsigned bf;
	edict_t *ent = game.edicts + entNum;

	// pick one of the 8 slots to prevent overwritings
	clipent = &clipEnts[index];
	index = ( index + 1 ) & 7;

	if( !entNum || deltaTime >= 0 ) { // current time entity
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	if( !ent->r.inuse || ent->r.solid == SOLID_NOT
		|| ( ent->r.solid == SOLID_TRIGGER && !( entNum >= 1 && entNum <= server_gs.maxclients ) ) ) {
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	// always use the latest information about moving world brushes
	if( ent->movetype == MOVETYPE_PUSH ) {
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	// clamp delta time inside the backed up limits
	backTime = Abs( deltaTime );
	if( g_antilag_maxtimedelta->integer ) {
		if( g_antilag_maxtimedelta->integer < 0 ) {
			Cvar_SetInteger( "g_antilag_maxtimedelta", Abs( g_antilag_maxtimedelta->integer ) );
		}
		if( backTime > (int64_t)g_antilag_maxtimedelta->integer ) {
			backTime = (int64_t)g_antilag_maxtimedelta->integer;
		}
	}

	// find the first snap with timestamp < than realtime - backtime
	cframenum = sv_collisionFrameNum;
	for( bf = 1; bf < CFRAME_UPDATE_BACKUP && bf < sv_collisionFrameNum; bf++ ) { // never overpass limits
		cframe = &sv_collisionframes[( cframenum - bf ) & CFRAME_UPDATE_MASK];

		// if solid has changed, we can't keep moving backwards
		if( ent->r.solid != cframe->clipEdicts[entNum].r.solid
			|| ent->r.inuse != cframe->clipEdicts[entNum].r.inuse ) {
			bf--;
			if( bf == 0 ) {
				// we can't step back from first
				cframe = NULL;
			} else {
				cframe = &sv_collisionframes[( cframenum - bf ) & CFRAME_UPDATE_MASK];
			}
			break;
		}

		if( svs.gametime >= cframe->timestamp + backTime ) {
			break;
		}
	}

	if( !cframe ) {
		// current time entity
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	// setup with older for the data that is not interpolated
	*clipent = cframe->clipEdicts[entNum];

	// if we found an older than desired backtime frame, interpolate to find a more precise position.
	if( svs.gametime > cframe->timestamp + backTime ) {
		float lerpFrac;

		if( bf == 1 ) {
			// interpolate from 1st backed up to current
			lerpFrac = (float)( ( svs.gametime - backTime ) - cframe->timestamp )
					   / (float)( svs.gametime - cframe->timestamp );
			clipentNewer.r = ent->r;
			clipentNewer.s = ent->s;
		} else {
			// interpolate between 2 backed up
			c4frame_t *cframeNewer = &sv_collisionframes[( cframenum - ( bf - 1 ) ) & CFRAME_UPDATE_MASK];
			lerpFrac = (float)( ( svs.gametime - backTime ) - cframe->timestamp )
					   / (float)( cframeNewer->timestamp - cframe->timestamp );
			clipentNewer = cframeNewer->clipEdicts[entNum];
		}

		// interpolate
		clipent->s.origin = Lerp( clipent->s.origin, lerpFrac, clipentNewer.s.origin );
		clipent->r.mins = Lerp( clipent->r.mins, lerpFrac, clipentNewer.r.mins );
		clipent->r.maxs = Lerp( clipent->r.maxs, lerpFrac, clipentNewer.r.maxs );
		clipent->s.angles = LerpAngles( clipent->s.angles, lerpFrac, clipentNewer.s.angles );
	}

	// back time entity
	return clipent;
}

// ClearLink is used for new headnodes
static void GClip_ClearLink( link_t *l ) {
	l->prev = l->next = l;
	l->entNum = 0;
}

static void GClip_RemoveLink( link_t *l ) {
	l->next->prev = l->prev;
	l->prev->next = l->next;
	l->entNum = 0;
}

static void GClip_InsertLinkBefore( link_t *l, link_t *before, int entNum ) {
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
	l->entNum = entNum;
}

/*
* GClip_Init_AreaGrid
*/
static void GClip_Init_AreaGrid( areagrid_t *areagrid, Vec3 world_mins, Vec3 world_maxs ) {
	// the areagrid_marknumber is not allowed to be 0
	if( areagrid->marknumber < 1 ) {
		areagrid->marknumber = 1;
	}

	// choose either the world box size, or a larger box to ensure the grid isn't too fine
	areagrid->size.x = Max2( world_maxs.x - world_mins.x, AREA_GRID * AREA_GRIDMINSIZE );
	areagrid->size.y = Max2( world_maxs.y - world_mins.y, AREA_GRID * AREA_GRIDMINSIZE );
	areagrid->size.z = Max2( world_maxs.z - world_mins.z, AREA_GRID * AREA_GRIDMINSIZE );

	// figure out the corners of such a box, centered at the center of the world box
	areagrid->mins.x = ( world_mins.x + world_maxs.x - areagrid->size.x ) * 0.5f;
	areagrid->mins.y = ( world_mins.y + world_maxs.y - areagrid->size.y ) * 0.5f;
	areagrid->mins.z = ( world_mins.z + world_maxs.z - areagrid->size.z ) * 0.5f;
	areagrid->maxs.x = ( world_mins.x + world_maxs.x + areagrid->size.x ) * 0.5f;
	areagrid->maxs.y = ( world_mins.y + world_maxs.y + areagrid->size.y ) * 0.5f;
	areagrid->maxs.z = ( world_mins.z + world_maxs.z + areagrid->size.z ) * 0.5f;

	// now calculate the actual useful info from that
	areagrid->bias = -areagrid->mins;
	areagrid->scale = float( AREA_GRID ) / areagrid->size;

	GClip_ClearLink( &areagrid->outside );
	for( int i = 0; i < AREA_GRIDNODES; i++ ) {
		GClip_ClearLink( &areagrid->grid[i] );
	}

	memset( areagrid->entmarknumber, 0, sizeof( areagrid->entmarknumber ) );
}

/*
* GClip_UnlinkEntity_AreaGrid
*/
static void GClip_UnlinkEntity_AreaGrid( edict_t *ent ) {
	for( int i = 0; i < MAX_ENT_AREAS; i++ ) {
		if( !ent->areagrid[i].prev ) {
			break;
		}
		GClip_RemoveLink( &ent->areagrid[i] );
		ent->areagrid[i].prev = ent->areagrid[i].next = NULL;
	}
}

/*
* GClip_LinkEntity_AreaGrid
*/
static void GClip_LinkEntity_AreaGrid( areagrid_t *areagrid, edict_t *ent ) {
	link_t *grid;
	int igrid[3], igridmins[3], igridmaxs[3], gridnum, entitynumber;

	entitynumber = ENTNUM( ent );
	if( entitynumber <= 0 || entitynumber >= game.maxentities || &game.edicts[ entitynumber ] != ent ) {
		Com_Printf( "GClip_LinkEntity_AreaGrid: invalid edict %p "
					"(edicts is %p, edict compared to prog->edicts is %i)\n",
					(void *)ent, game.edicts, entitynumber );
		return;
	}

	igridmins[0] = (int) floorf( ( ent->r.absmin.x + areagrid->bias.x ) * areagrid->scale.x );
	igridmins[1] = (int) floorf( ( ent->r.absmin.y + areagrid->bias.y ) * areagrid->scale.y );

	//igridmins[2] = (int) floorf( (ent->r.absmin[2] + areagrid->bias[2]) * areagrid->scale[2] );
	igridmaxs[0] = (int) floorf( ( ent->r.absmax.x + areagrid->bias.x ) * areagrid->scale.x ) + 1;
	igridmaxs[1] = (int) floorf( ( ent->r.absmax.y + areagrid->bias.y ) * areagrid->scale.y ) + 1;

	//igridmaxs[2] = (int) floorf( (ent->r.absmax[2] + areagrid->bias[2]) * areagrid->scale[2] ) + 1;
	if( igridmins[0] < 0 || igridmaxs[0] > AREA_GRID
		|| igridmins[1] < 0 || igridmaxs[1] > AREA_GRID
		|| ( ( igridmaxs[0] - igridmins[0] ) * ( igridmaxs[1] - igridmins[1] ) ) > MAX_ENT_AREAS ) {
		// wow, something outside the grid, store it as such
		GClip_InsertLinkBefore( &ent->areagrid[0], &areagrid->outside, entitynumber );
		return;
	}

	gridnum = 0;
	for( igrid[1] = igridmins[1]; igrid[1] < igridmaxs[1]; igrid[1]++ ) {
		grid = areagrid->grid + igrid[1] * AREA_GRID + igridmins[0];
		for( igrid[0] = igridmins[0]; igrid[0] < igridmaxs[0]; igrid[0]++, grid++, gridnum++ )
			GClip_InsertLinkBefore( &ent->areagrid[gridnum], grid, entitynumber );
	}
}

/*
* GClip_EntitiesInBox_AreaGrid
*/
static int GClip_EntitiesInBox_AreaGrid( areagrid_t *areagrid, Vec3 mins, Vec3 maxs, int *list, int maxcount, int areatype, int timeDelta ) {
	int numlist;
	link_t *grid;
	link_t *l;
	c4clipedict_t *clipEnt;
	Vec3 paddedmins, paddedmaxs;
	int igrid[3], igridmins[3], igridmaxs[3];

	paddedmins = mins;
	paddedmaxs = maxs;

	// FIXME: if areagrid_marknumber wraps, all entities need their
	// ent->priv.server->areagridmarknumber reset
	areagrid->marknumber++;

	igridmins[0] = (int) floorf( ( paddedmins.x + areagrid->bias.x ) * areagrid->scale.x );
	igridmins[1] = (int) floorf( ( paddedmins.y + areagrid->bias.y ) * areagrid->scale.y );

	//igridmins[2] = (int) ( (paddedmins[2] + areagrid->bias[2]) * areagrid->scale[2] );
	igridmaxs[0] = (int) floorf( ( paddedmaxs.x + areagrid->bias.x ) * areagrid->scale.x ) + 1;
	igridmaxs[1] = (int) floorf( ( paddedmaxs.y + areagrid->bias.y ) * areagrid->scale.y ) + 1;

	//igridmaxs[2] = (int) ( (paddedmaxs[2] + areagrid->bias[2]) * areagrid->scale[2] ) + 1;
	igridmins[0] = Max2( 0, igridmins[0] );
	igridmins[1] = Max2( 0, igridmins[1] );

	//igridmins[2] = max( 0, igridmins[2] );
	igridmaxs[0] = Min2( AREA_GRID, igridmaxs[0] );
	igridmaxs[1] = Min2( AREA_GRID, igridmaxs[1] );

	//igridmaxs[2] = min( AREA_GRID, igridmaxs[2] );

	// paranoid debugging
	//VectorSet( igridmins, 0, 0, 0 );VectorSet( igridmaxs, AREA_GRID, AREA_GRID, AREA_GRID );

	numlist = 0;

	// add entities not linked into areagrid because they are too big or
	// outside the grid bounds
	if( areagrid->outside.next ) {
		grid = &areagrid->outside;
		for( l = grid->next; l != grid; l = l->next ) {
			clipEnt = GClip_GetClipEdictForDeltaTime( l->entNum, timeDelta );

			if( areagrid->entmarknumber[l->entNum] == areagrid->marknumber ) {
				continue;
			}
			areagrid->entmarknumber[l->entNum] = areagrid->marknumber;

			if( !clipEnt->r.inuse ) {
				continue; // deactivated
			}
			if( areatype == AREA_TRIGGERS && clipEnt->r.solid != SOLID_TRIGGER ) {
				continue;
			}
			if( areatype == AREA_SOLID &&
				( clipEnt->r.solid == SOLID_TRIGGER || clipEnt->r.solid == SOLID_NOT ) ) {
				continue;
			}

			if( BoundsOverlap( paddedmins, paddedmaxs, clipEnt->r.absmin, clipEnt->r.absmax ) ) {
				if( numlist < maxcount ) {
					list[numlist] = l->entNum;
				}
				numlist++;
			}
		}
	}

	// add grid linked entities
	for( igrid[1] = igridmins[1]; igrid[1] < igridmaxs[1]; igrid[1]++ ) {
		grid = areagrid->grid + igrid[1] * AREA_GRID + igridmins[0];

		for( igrid[0] = igridmins[0]; igrid[0] < igridmaxs[0]; igrid[0]++, grid++ ) {
			if( !grid->next ) {
				continue;
			}

			for( l = grid->next; l != grid; l = l->next ) {
				clipEnt = GClip_GetClipEdictForDeltaTime( l->entNum, timeDelta );

				if( areagrid->entmarknumber[l->entNum] == areagrid->marknumber ) {
					continue;
				}
				areagrid->entmarknumber[l->entNum] = areagrid->marknumber;

				if( !clipEnt->r.inuse ) {
					continue; // deactivated
				}
				if( areatype == AREA_TRIGGERS && clipEnt->r.solid != SOLID_TRIGGER ) {
					continue;
				}
				if( areatype == AREA_SOLID &&
					( clipEnt->r.solid == SOLID_TRIGGER || clipEnt->r.solid == SOLID_NOT ) ) {
					continue;
				}

				if( BoundsOverlap( paddedmins, paddedmaxs, clipEnt->r.absmin, clipEnt->r.absmax ) ) {
					if( numlist < maxcount ) {
						list[numlist] = l->entNum;
					}
					numlist++;
				}
			}
		}
	}

	return numlist;
}


/*
* GClip_ClearWorld
* called after the world model has been loaded, before linking any entities
*/
void GClip_ClearWorld() {
	cmodel_t * world_model = CM_FindCModel( CM_Server, StringHash( svs.cms->world_hash ) );

	Vec3 world_mins, world_maxs;
	CM_InlineModelBounds( svs.cms, world_model, &world_mins, &world_maxs );

	GClip_Init_AreaGrid( &g_areagrid, world_mins, world_maxs );
}

/*
* GClip_UnlinkEntity
* call before removing an entity, and before trying to move one,
* so it doesn't clip against itself
*/
void GClip_UnlinkEntity( edict_t *ent ) {
	if( !ent->linked ) {
		return; // not linked in anywhere
	}
	GClip_UnlinkEntity_AreaGrid( ent );
	ent->linked = false;
}

/*
* GClip_LinkEntity
* Needs to be called any time an entity changes origin, mins, maxs,
* or solid.  Automatically unlinks if needed.
* sets ent->v.absmin and ent->v.absmax
* sets ent->clusternums[] for pvs determination even if the entity is not solid
*/
#define MAX_TOTAL_ENT_LEAFS 128
void GClip_LinkEntity( edict_t *ent ) {
	int leafs[MAX_TOTAL_ENT_LEAFS];
	int clusters[MAX_TOTAL_ENT_LEAFS];

	GClip_UnlinkEntity( ent ); // unlink from old position

	if( ent == game.edicts ) {
		return; // don't add the world
	}
	if( !ent->r.inuse ) {
		return;
	}

	// set the size
	ent->r.size = ent->r.maxs - ent->r.mins;

	bool predicted_trigger = ent->r.solid == SOLID_TRIGGER && ( ent->s.type == ET_JUMPPAD || ent->s.type == ET_PAINKILLER_JUMPPAD );
	bool transmit_bounds = ent->r.solid == SOLID_YES || predicted_trigger;
	ent->s.bounds = transmit_bounds ? MinMax3( ent->r.mins, ent->r.maxs ) : MinMax3::Empty();

	// set the abs box
	if( CM_IsBrushModel( CM_Server, ent->s.model ) && ent->s.angles != Vec3( 0.0f ) ) {
		// expand for rotation
		float radius = RadiusFromBounds( ent->r.mins, ent->r.maxs );
		ent->r.absmin = ent->s.origin - Vec3( radius );
		ent->r.absmax = ent->s.origin + Vec3( radius );
	} else {   // axis aligned
		ent->r.absmin = ent->s.origin + ent->r.mins;
		ent->r.absmax = ent->s.origin + ent->r.maxs;
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->r.absmin -= Vec3( 1.0f );
	ent->r.absmax += Vec3( 1.0f );

	// link to PVS leafs
	ent->r.num_clusters = 0;
	ent->r.areanum = ent->r.areanum2 = -1;

	// get all leafs, including solids
	int topnode;
	int num_leafs = CM_BoxLeafnums( svs.cms, ent->r.absmin, ent->r.absmax,
									 leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

	// set areas
	for( int i = 0; i < num_leafs; i++ ) {
		clusters[i] = CM_LeafCluster( svs.cms, leafs[i] );
		int area = CM_LeafArea( svs.cms, leafs[i] );
		if( area > -1 ) {
			// doors may legally straggle two areas,
			// but nothing should ever need more than that
			if( ent->r.areanum > -1 && ent->r.areanum != area ) {
				if( ent->r.areanum2 > -1 && ent->r.areanum2 != area ) {
					Com_Printf( "Object touching 3 areas at %f %f %f\n",
						ent->r.absmin.x, ent->r.absmin.y, ent->r.absmin.z );
				}
				ent->r.areanum2 = area;
			} else {
				ent->r.areanum = area;
			}
		}
	}

	if( num_leafs >= MAX_TOTAL_ENT_LEAFS ) {
		// assume we missed some leafs, and mark by headnode
		ent->r.num_clusters = -1;
		ent->r.headnode = topnode;
	} else {
		ent->r.num_clusters = 0;
		for( int i = 0; i < num_leafs; i++ ) {
			if( clusters[i] == -1 ) {
				continue; // not a visible leaf
			}
			int j;
			for( j = 0; j < i; j++ ) {
				if( clusters[j] == clusters[i] ) {
					break;
				}
			}
			if( j == i ) {
				if( ent->r.num_clusters == MAX_ENT_CLUSTERS ) {
					// assume we missed some leafs, and mark by headnode
					ent->r.num_clusters = -1;
					ent->r.headnode = topnode;
					break;
				}
				ent->r.clusternums[ent->r.num_clusters] = clusters[i];
				ent->r.num_clusters++;
			}
		}
	}

	// if first time, make sure old_origin is valid
	if( !ent->linkcount ) {
		ent->olds = ent->s;
	}
	ent->linkcount++;
	ent->linked = true;

	GClip_LinkEntity_AreaGrid( &g_areagrid, ent );
}

/*
* GClip_SetAreaPortalState
*
* Finds an areaportal leaf entity is connected with,
* and also finds two leafs from different areas connected
* with the same entity.
*/
void GClip_SetAreaPortalState( edict_t *ent, bool open ) {
	// entity must touch at least two areas
	if( ent->r.areanum < 0 || ent->r.areanum2 < 0 ) {
		return;
	}

	// change areaportal's state
	CM_SetAreaPortalState( svs.cms, ent->r.areanum, ent->r.areanum2, open );
}


/*
* GClip_AreaEdicts
* fills in a table of edict ids with edicts that have bounding boxes
* that intersect the given area.  It is possible for a non-axial bmodel
* to be returned that doesn't actually intersect the area on an exact
* test.
* returns the number of pointers filled in
* ??? does this always return the world?
*/
int GClip_AreaEdicts( Vec3 mins, Vec3 maxs, int *list, int maxcount, int areatype, int timeDelta ) {
	int count = GClip_EntitiesInBox_AreaGrid( &g_areagrid, mins, maxs, list, maxcount, areatype, timeDelta );
	return Min2( count, maxcount );
}

/*
* GClip_CollisionModelForEntity
*
* Returns a collision model that can be used for testing or clipping an
* object of mins/maxs size.
*/

static cmodel_t *GClip_CollisionModelForEntity( SyncEntityState *s, entity_shared_t *r ) {
	cmodel_t * model = CM_TryFindCModel( CM_Server, s->model );
	if( model != NULL ) {
		return model;
	}

	// create a temp hull from bounding box sizes
	if( s->type == ET_PLAYER || s->type == ET_CORPSE ) {
		return CM_OctagonModelForBBox( svs.cms, r->mins, r->maxs );
	} else {
		return CM_ModelForBBox( svs.cms, r->mins, r->maxs );
	}
}

//===========================================================================

typedef struct {
	Vec3 boxmins, boxmaxs;    // enclose the test object along entire move
	Vec3 mins, maxs;         // size of the moving object
	Vec3 start, end;
	trace_t *trace;
	int passent;
	int contentmask;
} moveclip_t;

/*
* GClip_ClipMoveToEntities
*/
static void GClip_ClipMoveToEntities( moveclip_t *clip, int timeDelta ) {
	TracyZoneScoped;

	assert( clip->passent == -1 || ( clip->passent >= 0 && clip->passent < ARRAY_COUNT( game.edicts ) ) );

	int touchlist[MAX_EDICTS];
	int num = GClip_AreaEdicts( clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID, timeDelta );

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( int i = 0; i < num; i++ ) {
		c4clipedict_t * touch = GClip_GetClipEdictForDeltaTime( touchlist[i], timeDelta );
		if( clip->passent >= 0 ) {
			// when they are offseted in time, they can be a different pointer but be the same entity
			if( touch->s.number == clip->passent ) {
				continue;
			}
			if( touch->r.owner && ( touch->r.owner->s.number == clip->passent ) ) {
				continue;
			}
			if( game.edicts[clip->passent].r.owner
				&& ( game.edicts[clip->passent].r.owner->s.number == touch->s.number ) ) {
				continue;
			}

			// wsw : jal : never clipmove against SVF_PROJECTILE entities
			if( touch->s.svflags & SVF_PROJECTILE ) {
				continue;
			}
		}

		if( ( touch->s.svflags & SVF_CORPSE ) && !( clip->contentmask & CONTENTS_CORPSE ) ) {
			continue;
		}

		if( touch->r.client != NULL ) {
			int teammask = clip->contentmask & ( CONTENTS_TEAM_ONE | CONTENTS_TEAM_TWO | CONTENTS_TEAM_THREE | CONTENTS_TEAM_FOUR );
			if( teammask != 0 ) {
				Team clip_team = Team_None;
				for( int team = Team_One; team < Team_Count; team++ ) {
					if( teammask == CONTENTS_TEAM_ONE << ( team - Team_One ) ) {
						clip_team = Team( team );
						break;
					}
				}
				assert( clip_team != Team_None );

				if( touch->s.team == clip_team )
					continue;
			}
		}

		// might intersect, so do an exact clip
		cmodel_t * cmodel = GClip_CollisionModelForEntity( &touch->s, &touch->r );

		Vec3 angles;
		if( CM_IsBrushModel( CM_Server, touch->s.model ) ) {
			angles = touch->s.angles;
		} else {
			angles = Vec3( 0.0f ); // boxes don't rotate
		}

		trace_t trace;
		CM_TransformedBoxTrace( CM_Server, svs.cms, &trace, clip->start, clip->end,
									 clip->mins, clip->maxs, cmodel, clip->contentmask,
									 touch->s.origin, angles );

		if( trace.allsolid || trace.fraction < clip->trace->fraction ) {
			trace.ent = touch->s.number;
			*( clip->trace ) = trace;
		} else if( trace.startsolid ) {
			clip->trace->startsolid = true;
		}
		if( clip->trace->allsolid ) {
			return;
		}
	}
}


/*
* GClip_TraceBounds
*/
static void GClip_TraceBounds( Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, Vec3 * boxmins, Vec3 * boxmaxs ) {
	for( int i = 0; i < 3; i++ ) {
		float near = Min2( start[ i ], end[ i ] );
		float far = Max2( start[ i ], end[ i ] );

		boxmins->ptr()[ i ] = near + mins[ i ] - 1.0f;
		boxmaxs->ptr()[ i ] = far + maxs[ i ] + 1.0f;
	}
}

/*
* G_Trace
*
* Moves the given mins/maxs volume through the world from start to end.
*
* Passedict and edicts owned by passedict are explicitly not checked.
* ------------------------------------------------------------------
* mins and maxs are relative

* if the entire move stays in a solid volume, trace.allsolid will be set,
* trace.startsolid will be set, and trace.fraction will be 0

* if the starting point is in a solid, it will be allowed to move out
* to an open area

* passedict is explicitly excluded from clipping checks (normally NULL)
*/
static void GClip_Trace( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs,
						 Vec3 end, edict_t *passedict, int contentmask, int timeDelta ) {
	TracyZoneScoped;

	moveclip_t clip;

	if( !tr ) {
		return;
	}

	if( passedict == world ) {
		memset( tr, 0, sizeof( trace_t ) );
		tr->fraction = 1;
		tr->ent = -1;
	} else {
		// clip to world
		CM_TransformedBoxTrace( CM_Server, svs.cms, tr, start, end, mins, maxs, NULL, contentmask, Vec3( 0.0f ), Vec3( 0.0f ) );
		tr->ent = tr->fraction < 1.0f ? world->s.number : -1;
		if( tr->fraction == 0 ) {
			return; // blocked by the world
		}
	}

	memset( &clip, 0, sizeof( moveclip_t ) );
	clip.trace = tr;
	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passent = passedict ? ENTNUM( passedict ) : -1;

	// create the bounding box of the entire move
	GClip_TraceBounds( start, mins, maxs, end, &clip.boxmins, &clip.boxmaxs );

	// clip to other solid entities
	GClip_ClipMoveToEntities( &clip, timeDelta );
}

void G_Trace( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, edict_t *passedict, int contentmask ) {
	GClip_Trace( tr, start, mins, maxs, end, passedict, contentmask, 0 );
}

void G_Trace4D( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, edict_t *passedict, int contentmask, int timeDelta ) {
	GClip_Trace( tr, start, mins, maxs, end, passedict, contentmask, timeDelta );
}

bool IsHeadshot( int entNum, Vec3 hit, int timeDelta ) {
	const c4clipedict_t * clip = GClip_GetClipEdictForDeltaTime( entNum, timeDelta );
	return clip->r.absmax.z - hit.z <= 16.0f;
}

//===========================================================================


/*
* GClip_SetBrushModel
*
* Also sets mins and maxs for inline bmodels
*/
void GClip_SetBrushModel( edict_t * ent ) {
	cmodel_t * cmodel = CM_TryFindCModel( CM_Server, ent->s.model );
	if( cmodel != NULL ) {
		CM_InlineModelBounds( svs.cms, cmodel, &ent->r.mins, &ent->r.maxs );
	}
}

/*
* GClip_EntityContact
*/
bool GClip_EntityContact( Vec3 mins, Vec3 maxs, edict_t *ent ) {
	cmodel_t * model = CM_TryFindCModel( CM_Server, ent->s.model );
	if( model != NULL ) {
		trace_t tr;
		CM_TransformedBoxTrace( CM_Server, svs.cms, &tr, Vec3( 0.0f ), Vec3( 0.0f ), mins, maxs, model,
									 MASK_ALL, ent->s.origin, ent->s.angles );

		return tr.startsolid || tr.allsolid ? true : false;
	}

	return BoundsOverlap( mins, maxs, ent->r.absmin, ent->r.absmax );
}

static void CallTouches( edict_t * ent, Vec3 mins, Vec3 maxs ) {
	int touch[ MAX_EDICTS ];
	int num = GClip_AreaEdicts( mins, maxs, touch, MAX_EDICTS, AREA_TRIGGERS, 0 );

	for( int i = 0; i < num; i++ ) {
		edict_t * hit = &game.edicts[ touch[ i ] ];

		// G_CallTouch can kill entities so we need to check they still exist
		if( !hit->r.inuse ) {
			continue;
		}

		if( !hit->touch ) {
			continue;
		}

		if( !GClip_EntityContact( mins, maxs, hit ) ) {
			continue;
		}

		G_CallTouch( hit, ent, NULL, 0 );
	}
}

void GClip_TouchTriggers( edict_t *ent ) {
	if( !ent->r.inuse || ( ent->r.client && G_IsDead( ent ) ) ) {
		return;
	}

	CallTouches( ent, ent->r.absmin, ent->r.absmax );
}

void G_PMoveTouchTriggers( pmove_t *pm, Vec3 previous_origin ) {
	if( pm->playerState->POVnum <= 0 || (int)pm->playerState->POVnum > server_gs.maxclients ) {
		return;
	}

	edict_t * ent = game.edicts + pm->playerState->POVnum;
	if( !ent->r.inuse || !ent->r.client || G_IsDead( ent ) ) { // dead things don't activate triggers!
		return;
	}

	// update the entity with the new position
	ent->s.origin = pm->playerState->pmove.origin;
	ent->velocity = pm->playerState->pmove.velocity;
	ent->s.angles = pm->playerState->viewangles;
	ent->viewheight = pm->playerState->viewheight;
	ent->r.mins = pm->mins;
	ent->r.maxs = pm->maxs;

	if( pm->groundentity == -1 ) {
		ent->groundentity = NULL;
	} else {
		ent->groundentity = &game.edicts[pm->groundentity];
		ent->groundentity_linkcount = ent->groundentity->linkcount;
	}

	GClip_LinkEntity( ent );

	// expand the search bounds to include the space between the previous and current origin
	MinMax3 bounds = MinMax3::Empty();
	bounds = Union( bounds, previous_origin + pm->maxs );
	bounds = Union( bounds, previous_origin + pm->mins );
	bounds = Union( bounds, pm->playerState->pmove.origin + pm->maxs );
	bounds = Union( bounds, pm->playerState->pmove.origin + pm->mins );

	CallTouches( ent, bounds.mins, bounds.maxs );
}

/*
* GClip_FindInRadius4D
* Returns entities that have their boxes within a spherical area
*/
int GClip_FindInRadius4D( Vec3 origin, float radius, int * output, int maxcount, int timeDelta ) {
	float aabb_size = radius * sqrtf( 2.0f ) + 1.0f;
	Vec3 mins = origin - Vec3( aabb_size );
	Vec3 maxs = origin + Vec3( aabb_size );

	int candidates[ MAX_EDICTS ];
	int num = GClip_AreaEdicts( mins, maxs, candidates, MAX_EDICTS, AREA_ALL, timeDelta );

	int n = 0;

	for( int i = 0; i < num; i++ ) {
		const edict_t * check = &game.edicts[ candidates[ i ] ];

		// make absolute mins and maxs
		if( !BoundsOverlapSphere( check->r.absmin, check->r.absmax, origin, radius ) ) {
			continue;
		}

		if( n < maxcount ) {
			output[ n ] = candidates[ i ];
		}
		n++;
	}

	return n;
}

/*
* GClip_FindInRadius
*
* Returns entities that have their boxes within a spherical area
*/
int GClip_FindInRadius( Vec3 origin, float radius, int * output, int maxcount ) {
	return GClip_FindInRadius4D( origin, radius, output, maxcount, 0 );
}

void G_SplashFrac4D( const edict_t *ent, Vec3 hitpoint, float maxradius, Vec3 * pushdir, float *frac, int timeDelta, bool selfdamage ) {
	const c4clipedict_t *clipEnt = GClip_GetClipEdictForDeltaTime( ENTNUM( ent ), timeDelta );
	G_SplashFrac( &clipEnt->s, &clipEnt->r, hitpoint, maxradius, pushdir, frac, selfdamage );
}

SyncEntityState *G_GetEntityStateForDeltaTime( int entNum, int deltaTime ) {
	c4clipedict_t *clipEnt;

	if( entNum == -1 ) {
		return NULL;
	}

	assert( entNum >= 0 && entNum < MAX_EDICTS );

	clipEnt = GClip_GetClipEdictForDeltaTime( entNum, deltaTime );

	return &clipEnt->s;
}
