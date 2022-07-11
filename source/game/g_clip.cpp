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

#include "qcommon/base.h"
#include "game/g_local.h"
#include "game/g_maps.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"

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

// FIXME: eliminate AREA_ distinction?
#define AREA_ALL       -1
#define AREA_SOLID      1
#define AREA_TRIGGERS   2

typedef struct
{
	link_t grid[AREA_GRIDNODES];
	link_t outside;
	Vec3 bias;
	Vec3 scale;
	MinMax3 bounds;
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

static void GClip_Init_AreaGrid( areagrid_t *areagrid, MinMax3 bounds ) {
	// the areagrid_marknumber is not allowed to be 0
	if( areagrid->marknumber < 1 ) {
		areagrid->marknumber = 1;
	}

	// choose either the world box size, or a larger box to ensure the grid isn't too fine
	areagrid->size.x = Max2( bounds.maxs.x - bounds.mins.x, AREA_GRID * AREA_GRIDMINSIZE );
	areagrid->size.y = Max2( bounds.maxs.y - bounds.mins.y, AREA_GRID * AREA_GRIDMINSIZE );
	areagrid->size.z = Max2( bounds.maxs.z - bounds.mins.z, AREA_GRID * AREA_GRIDMINSIZE );

	// figure out the corners of such a box, centered at the center of the world box
	areagrid->bounds.mins = ( bounds.mins + bounds.maxs - areagrid->size ) * 0.5f;
	areagrid->bounds.maxs = ( bounds.mins + bounds.maxs + areagrid->size ) * 0.5f;

	// now calculate the actual useful info from that
	areagrid->bias = -areagrid->bounds.mins;
	areagrid->scale = float( AREA_GRID ) / areagrid->size;

	GClip_ClearLink( &areagrid->outside );
	for( int i = 0; i < AREA_GRIDNODES; i++ ) {
		GClip_ClearLink( &areagrid->grid[i] );
	}

	memset( areagrid->entmarknumber, 0, sizeof( areagrid->entmarknumber ) );
}

static void GClip_UnlinkEntity_AreaGrid( edict_t *ent ) {
	for( int i = 0; i < MAX_ENT_AREAS; i++ ) {
		if( !ent->areagrid[i].prev ) {
			break;
		}
		GClip_RemoveLink( &ent->areagrid[i] );
		ent->areagrid[i].prev = ent->areagrid[i].next = NULL;
	}
}

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

	igridmaxs[0] = (int) floorf( ( ent->r.absmax.x + areagrid->bias.x ) * areagrid->scale.x ) + 1;
	igridmaxs[1] = (int) floorf( ( ent->r.absmax.y + areagrid->bias.y ) * areagrid->scale.y ) + 1;

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

static int GClip_EntitiesInBox_AreaGrid( areagrid_t *areagrid, const MinMax3 & bounds, int *list, int maxcount, int areatype, int timeDelta ) {
	assert( maxcount > 0 );

	Vec3 paddedmins = bounds.mins;
	Vec3 paddedmaxs = bounds.maxs;

	// FIXME: if areagrid_marknumber wraps, all entities need their
	// ent->priv.server->areagridmarknumber reset
	areagrid->marknumber++;

	int igridmins[2];
	igridmins[0] = int( Max2( 0.0f, ( paddedmins.x + areagrid->bias.x ) * areagrid->scale.x ) );
	igridmins[1] = int( Max2( 0.0f, ( paddedmins.y + areagrid->bias.y ) * areagrid->scale.y ) );

	int igridmaxs[2];
	igridmaxs[0] = int( Min2( float( AREA_GRID ), ( paddedmaxs.x + areagrid->bias.x ) * areagrid->scale.x ) ) + 1;
	igridmaxs[1] = int( Min2( float( AREA_GRID ), ( paddedmaxs.y + areagrid->bias.y ) * areagrid->scale.y ) ) + 1;

	int numlist = 0;

	// add entities not linked into areagrid because they are too big or
	// outside the grid bounds
	if( areagrid->outside.next ) {
		const link_t * grid = &areagrid->outside;
		for( const link_t * l = grid->next; l != grid; l = l->next ) {
			const c4clipedict_t * clipEnt = GClip_GetClipEdictForDeltaTime( l->entNum, timeDelta );

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

	// always add the world
	list[numlist] = 0;
	numlist++;

	// add grid linked entities
	for( int y = igridmins[1]; y < igridmaxs[1]; y++ ) {
		for( int x = igridmins[0]; x < igridmaxs[0]; x++ ) {
			const link_t * grid = areagrid->grid + y * AREA_GRID + x;
			if( !grid->next ) {
				continue;
			}

			for( const link_t * l = grid->next; l != grid; l = l->next ) {
				const c4clipedict_t * clipEnt = GClip_GetClipEdictForDeltaTime( l->entNum, timeDelta );

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
	const MapData * map = FindServerMap( server_gs.gameState.map );
	GClip_Init_AreaGrid( &g_areagrid, map->models[ 0 ].bounds );
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
*/
void GClip_LinkEntity( edict_t *ent ) {
	GClip_UnlinkEntity( ent ); // unlink from old position

	if( ent == game.edicts ) {
		return; // don't add the world
	}
	if( !ent->r.inuse ) {
		return;
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
* GClip_AreaEdicts
* fills in a table of edict ids with edicts that have bounding boxes
* that intersect the given area.  It is possible for a non-axial bmodel
* to be returned that doesn't actually intersect the area on an exact
* test.
* returns the number of pointers filled in
* ??? does this always return the world?
*/
static int GClip_AreaEdicts( const MinMax3 bounds, int *list, int maxcount, int areatype, int timeDelta ) {
	int count = GClip_EntitiesInBox_AreaGrid( &g_areagrid, bounds, list, maxcount, areatype, timeDelta );
	return Min2( count, maxcount );
}

//===========================================================================

static trace_t GClip_ClipMoveToEntities( const Ray & ray, const Shape & shape, int passent, int contentmask, int timeDelta ) {
	TracyZoneScoped;

	assert( passent == -1 || ( passent >= 0 && passent < ARRAY_COUNT( game.edicts ) ) );

	trace_t best = MakeMissedTrace( ray );

	int touchlist[MAX_EDICTS];

	MinMax3 ray_bounds = Union( Union( MinMax3::Empty(), ray.origin ), ray.origin + ray.direction * ray.length );
	MinMax3 broadphase_bounds = MinkowskiSum( ray_bounds, shape );

	int num = GClip_AreaEdicts( broadphase_bounds, touchlist, MAX_EDICTS, AREA_SOLID, timeDelta );

	// be careful, it is possible to have an entity in this list removed before we get to it
	for( int i = 0; i < num; i++ ) {
		const c4clipedict_t * touch = GClip_GetClipEdictForDeltaTime( touchlist[i], timeDelta );

		if( passent >= 0 ) {
			// when they are offseted in time, they can be a different pointer but be the same entity
			if( touch->s.number == passent ) {
				continue;
			}
			if( touch->r.owner && ( touch->r.owner->s.number == passent ) ) {
				continue;
			}
			if( game.edicts[passent].r.owner && ( game.edicts[passent].r.owner->s.number == touch->s.number ) ) {
				continue;
			}

			// wsw : jal : never clipmove against SVF_PROJECTILE entities
			if( touch->s.svflags & SVF_PROJECTILE ) {
				continue;
			}
		}

		if( ( touch->s.svflags & SVF_CORPSE ) && !( contentmask & CONTENTS_CORPSE ) ) {
			continue;
		}

		if( touch->r.client != NULL ) {
			int teammask = contentmask & ( CONTENTS_TEAM_ONE | CONTENTS_TEAM_TWO | CONTENTS_TEAM_THREE | CONTENTS_TEAM_FOUR );
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

		trace_t trace = TraceVsEnt( ServerCollisionModelStorage(), ray, shape, &touch->s );
		if( trace.fraction < best.fraction ) {
			best = trace;
		}
	}

	return best;
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
static trace_t GClip_Trace( Vec3 start, Vec3 end, const MinMax3 & bounds, const edict_t * passedict, int contentmask, int timeDelta ) {
	TracyZoneScoped;

	Ray ray = MakeRayStartEnd( start, end );
	int passent = passedict == NULL ? -1 : ENTNUM( passedict );

	Shape shape;
	if( bounds.mins == bounds.maxs ) {
		assert( bounds.mins == Vec3( 0.0f ) );
		shape.type = ShapeType_Ray;
	}
	else {
		shape.type = ShapeType_AABB;
		shape.aabb = ToCenterExtents( bounds );
	}

	return GClip_ClipMoveToEntities( ray, shape, passent, contentmask, timeDelta );
}

void G_Trace( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, const edict_t * passedict, int contentmask ) {
	*tr = GClip_Trace( start, end, MinMax3( mins, maxs ), passedict, contentmask, 0 );
}

void G_Trace4D( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, const edict_t * passedict, int contentmask, int timeDelta ) {
	*tr = GClip_Trace( start, end, MinMax3( mins, maxs ), passedict, contentmask, timeDelta );
}

bool IsHeadshot( int entNum, Vec3 hit, int timeDelta ) {
	const c4clipedict_t * clip = GClip_GetClipEdictForDeltaTime( entNum, timeDelta );
	return clip->r.absmax.z - hit.z <= 16.0f;
}

static bool EntityOverlapsAABB( const edict_t * ent, const CenterExtents3 & aabb ) {
	Ray ray = MakeRayStartEnd( Vec3( 0.0f ), Vec3( 0.0f ) );

	Shape shape = { };
	shape.type = ShapeType_AABB;
	shape.aabb = aabb;

	trace_t trace = TraceVsEnt( ServerCollisionModelStorage(), ray, shape, &ent->s );

	return trace.fraction == 0.0f;
}

static void CallTouches( edict_t * ent, const MinMax3 & bounds ) {
	int touch[ MAX_EDICTS ];
	int num = GClip_AreaEdicts( bounds, touch, MAX_EDICTS, AREA_TRIGGERS, 0 );

	CenterExtents3 aabb = ToCenterExtents( bounds );

	for( int i = 0; i < num; i++ ) {
		edict_t * hit = &game.edicts[ touch[ i ] ];

		// G_CallTouch can kill entities so we need to check they still exist
		if( !hit->r.inuse ) {
			continue;
		}

		if( !hit->touch ) {
			continue;
		}

		// TODO: this isn't right if ent isn't aabb, but good enough for now
		if( !EntityOverlapsAABB( hit, aabb ) ) {
			continue;
		}

		Plane dummy = { };
		G_CallTouch( hit, ent, dummy, 0 );
	}
}

void GClip_TouchTriggers( edict_t * ent ) {
	if( !ent->r.inuse || ( ent->r.client && G_IsDead( ent ) ) ) {
		return;
	}

	CollisionModel collision_model = { };
	if( ent->s.override_collision_model.exists ) {
		collision_model = ent->s.override_collision_model.value;
	}

	assert( collision_model.type == CollisionModelType_Point || collision_model.type == CollisionModelType_AABB );

	MinMax3 bounds;
	if( collision_model.type == CollisionModelType_Point ) {
		bounds = MinMax3( Vec3( 0.0f ), Vec3( 0.0f ) );
	}
	else {
		bounds = collision_model.aabb;
	}

	CallTouches( ent, bounds );
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

	// TODO: this is very wrong, these are meant to be broadphase bounds
	// but they get used as the actual trace bounds lol
	// expand the search bounds to include the space between the previous and current origin
	MinMax3 bounds = MinMax3::Empty();
	bounds = Union( bounds, previous_origin + pm->maxs );
	bounds = Union( bounds, previous_origin + pm->mins );
	bounds = Union( bounds, pm->playerState->pmove.origin + pm->maxs );
	bounds = Union( bounds, pm->playerState->pmove.origin + pm->mins );

	CallTouches( ent, bounds );
}

static bool BoundsOverlapSphere( const MinMax3 & bounds, const Sphere & sphere ) {
	float dist_squared = 0;

	for( int i = 0; i < 3; i++ ) {
		float x = Clamp( bounds.mins[ i ], sphere.center[ i ], bounds.maxs[ i ] );
		float d = sphere.center[ i ] - x;
		dist_squared += d * d;
	}

	return dist_squared <= Square( sphere.radius );
}

/*
* GClip_FindInRadius4D
* Returns entities that have their boxes within a spherical area
*/
int GClip_FindInRadius4D( Vec3 origin, float radius, int * output, int maxcount, int timeDelta ) {
	float aabb_size = radius * sqrtf( 2.0f ) + 1.0f;
	MinMax3 bounds = MinMax3( origin - aabb_size, origin + aabb_size );
	Sphere sphere = { origin, radius };

	int candidates[ MAX_EDICTS ];
	int num = GClip_AreaEdicts( bounds, candidates, MAX_EDICTS, AREA_ALL, timeDelta );

	int n = 0;

	for( int i = 0; i < num; i++ ) {
		const edict_t * other = &game.edicts[ candidates[ i ] ];
		MinMax3 other_bounds = ServerEntityBounds( &other->s );

		if( !BoundsOverlapSphere( other_bounds, sphere ) ) {
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
