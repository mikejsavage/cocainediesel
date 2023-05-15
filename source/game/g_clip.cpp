#include "qcommon/base.h"
#include "game/g_local.h"
#include "game/g_maps.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"

#include <algorithm>

static SpatialHashGrid g_grid;

void G_Trace( trace_t * tr, Vec3 start, MinMax3 bounds, Vec3 end, const edict_t * passedict, SolidBits solid_mask ) {
	TracyZoneScoped;

	Ray ray = MakeRayStartEnd( start, end );
	int passent = passedict == NULL ? -1 : ENTNUM( passedict );

	Assert( passent == -1 || ( passent >= 0 && size_t( passent ) < ARRAY_COUNT( game.edicts ) ) );

	Shape shape;
	if( bounds.mins == bounds.maxs ) {
		Assert( bounds.mins == Vec3( 0.0f ) );
		shape.type = ShapeType_Ray;
	}
	else {
		shape.type = ShapeType_AABB;
		shape.aabb = ToCenterExtents( bounds );
	}

	MinMax3 ray_bounds = Union( Union( MinMax3::Empty(), ray.origin ), ray.origin + ray.direction * ray.length );
	MinMax3 broadphase_bounds = MinkowskiSum( ray_bounds, shape );

	*tr = MakeMissedTrace( ray );

	int touchlist[ 1024 ];
	size_t num = TraverseSpatialHashGrid( &g_grid, broadphase_bounds, touchlist, solid_mask );
	
	for( size_t i = 0; i < num; i++ ) {
		edict_t * touch = &game.edicts[ touchlist[ i ] ];
		if( touch->s.number == passent )
			continue;
		if( touch->r.owner && ( touch->r.owner->s.number == passent ) )
			continue;
		if( game.edicts[passent].r.owner && ( game.edicts[passent].r.owner->s.number == touch->s.number ) ) 
			continue;
		// wsw : jal : never clipmove against SVF_PROJECTILE entities
		if( touch->s.svflags & SVF_PROJECTILE )
			continue;
		if( touch->r.client != NULL && touch->s.team == game.edicts[ passent ].s.team )
			continue;

		trace_t trace = TraceVsEnt( ServerCollisionModelStorage(), ray, shape, &touch->s, solid_mask );
		if( trace.fraction <= tr->fraction ) {
			*tr = trace;
		}
	}
}

void G_Trace4D( trace_t * tr, Vec3 start, MinMax3 bounds, Vec3 end, const edict_t * passedict, SolidBits solid_mask, int timeDelta ) {
	G_Trace( tr, start, bounds, end, passedict, solid_mask );
}

void GClip_BackUpCollisionFrame() {
}

int GClip_FindInRadius4D( Vec3 org, float rad, int *list, int maxcount, int timeDelta ) {
	// TODO: timedelta
	MinMax3 bounds = MinMax3( org - rad, org + rad );
	int touchlist[ MAX_EDICTS ];
	size_t touchnum = TraverseSpatialHashGrid( &g_grid, bounds, touchlist, SolidMask_AnySolid );

	size_t num = 0;
	for( size_t i = 0; i < touchnum; i++ ) {
		// TODO: actually check radius? whatever
		if( true ) {
			list[ num++ ] = touchlist[ i ];
			if( num == maxcount )
				return num;
		}
	}
	return num;
}

void G_SplashFrac4D( const edict_t * ent, Vec3 hitpoint, float maxradius, Vec3 * pushdir, float *frac, int timeDelta, bool selfdamage ) {
	// TODO: timedelta
	G_SplashFrac( &ent->s, &ent->r, hitpoint, maxradius, pushdir, frac, selfdamage );
}

void GClip_ClearWorld() {
	ClearSpatialHashGrid( &g_grid );
}

void GClip_LinkEntity( edict_t * ent ) {
	LinkEntity( &g_grid, ServerCollisionModelStorage(), &ent->s, ENTNUM( ent ) );
}

void GClip_UnlinkEntity( edict_t * ent ) {
	UnlinkEntity( &g_grid, ENTNUM( ent ) );
}

void GClip_TouchTriggers( edict_t * ent ) {
	// TODO: timedelta
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	if( bounds == MinMax3::Empty() )
		return;

	bounds.mins += ent->s.origin;
	bounds.maxs += ent->s.origin;

	int touchlist[ MAX_EDICTS ];
	size_t touchnum = TraverseSpatialHashGrid( &g_grid, bounds, touchlist, Solid_Trigger );

	for( size_t i = 0; i < touchnum; i++ ) {
		if( !ent->r.inuse )
			break;
		
		edict_t * hit = &game.edicts[ touchlist[ i ] ];
		if( !hit->r.inuse )
			continue;
		if( hit->touch == NULL )
			continue;
		if( !EntityOverlap( ServerCollisionModelStorage(), &ent->s, &hit->s, SolidMask_Everything ) )
			continue;
		
		G_CallTouch( hit, ent, Vec3( 0.0f ), SolidMask_AnySolid );
	}
}
void G_PMoveTouchTriggers( pmove_t *pm, Vec3 previous_origin ) {
	if( pm->playerState->POVnum <= 0 || (int)pm->playerState->POVnum > MAX_CLIENTS )
		return;
	
	edict_t * ent = game.edicts + pm->playerState->POVnum;
	if( !ent->r.client || G_IsDead( ent ) )  // dead things don't activate triggers!
		return;

	ent->s.origin = pm->playerState->pmove.origin;
	ent->velocity = pm->playerState->pmove.velocity;
	ent->s.angles = pm->playerState->viewangles;
	ent->viewheight = pm->playerState->viewheight;
	if( pm->groundentity == -1 ) {
		ent->groundentity = NULL;
	} else {
		ent->groundentity = &game.edicts[ pm->groundentity ];
		// ent->groundentity_linkcount = ent->groundentity->linkcount;
	}

	GClip_LinkEntity( ent );

	MinMax3 bounds = pm->bounds + pm->playerState->pmove.origin;
	bounds = Union( bounds, pm->bounds + previous_origin );

	int touchlist[ MAX_EDICTS ];
	size_t num = TraverseSpatialHashGrid( &g_grid, bounds, touchlist, Solid_Trigger );

	for( size_t i = 0; i < num; i++ ) {
		if( !ent->r.inuse )
			break;
		
		edict_t * hit = &game.edicts[ touchlist[ i ] ];
		MinMax3 hit_bounds = EntityBounds( ServerCollisionModelStorage(), &hit->s );
		hit_bounds += hit->s.origin;

		if( !hit->r.inuse )
			continue;
		if( hit->touch == NULL )
			continue;
		if( !BoundsOverlap( bounds.mins, bounds.maxs, hit_bounds.mins, hit_bounds.maxs ) )
			continue;

		{
			Ray ray = MakeRayStartEnd( ent->s.origin, hit->s.origin );
			Shape shape = { };
			CollisionModel collision_model = EntityCollisionModel( ServerCollisionModelStorage(), &ent->s );
			if( collision_model.type == CollisionModelType_Point ) {
				shape.type = ShapeType_Ray;
			}
			else if( collision_model.type == CollisionModelType_AABB ) {
				shape.type = ShapeType_AABB;
				shape.aabb = ToCenterExtents( collision_model.aabb );
			}
			else {
				Assert( false );
			}

			trace_t trace = TraceVsEnt( ServerCollisionModelStorage(), ray, shape, &hit->s, SolidMask_Everything );
			if( trace.GotSomewhere() )
				continue;
		}

		// if( !EntityOverlap( ServerCollisionModelStorage(), &ent->s, &hit->s, SolidMask_Everything ) )
		// 	continue;
		
		G_CallTouch( hit, ent, Vec3( 0.0f ), SolidMask_AnySolid );
	}
}

int GClip_FindInRadius( Vec3 org, float rad, int *list, int maxcount ) {
	return GClip_FindInRadius4D( org, rad, list, maxcount, 0 );
}
bool IsHeadshot( int entNum, Vec3 hit, int timeDelta ) {
	return false;
}
