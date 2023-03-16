#include "qcommon/base.h"
#include "game/g_local.h"
#include "game/g_maps.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"

#include <algorithm>

void G_Trace( trace_t * tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, const edict_t * passedict, SolidBits solid_mask ) {
	TracyZoneScoped;

	Ray ray = MakeRayStartEnd( start, end );
	int passent = passedict == NULL ? -1 : ENTNUM( passedict );

	Assert( passent == -1 || ( passent >= 0 && passent < ARRAY_COUNT( game.edicts ) ) );

	MinMax3 bounds = MinMax3( mins, maxs );
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

	u32 bvh_total = 0;
	u32 bvh_tested = 0;

	TraverseBVH( broadphase_bounds, [&]( u32 entity, u32 total ) {
		bvh_total = total;
		bvh_tested++;
		edict_t * touch = &game.edicts[ entity ];
		if( touch->s.number == passent )
			return;
		if( touch->r.owner && ( touch->r.owner->s.number == passent ) )
			return;
		if( game.edicts[passent].r.owner && ( game.edicts[passent].r.owner->s.number == touch->s.number ) ) 
			return;
		// wsw : jal : never clipmove against SVF_PROJECTILE entities
		if( touch->s.svflags & SVF_PROJECTILE )
			return;
		if( touch->r.client != NULL && touch->s.team == game.edicts[ passent ].s.team )
			return;

		trace_t trace = TraceVsEnt( ServerCollisionModelStorage(), ray, shape, &touch->s, solid_mask );
		if( trace.fraction < tr->fraction ) {
			*tr = trace;
		}
	} );

	Com_GGPrint( "bounds: {} {}, tested {} out of {}", broadphase_bounds.mins, broadphase_bounds.maxs, bvh_tested, bvh_total );
}

void G_Trace4D( trace_t * tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, const edict_t * passedict, SolidBits solid_mask, int timeDelta ) {
	G_Trace( tr, start, mins, maxs, end, passedict, solid_mask );
}

void GClip_BackUpCollisionFrame() {}
int GClip_FindInRadius4D( Vec3 org, float rad, int *list, int maxcount, int timeDelta ) {
	return 0;
}
void G_SplashFrac4D( const edict_t * ent, Vec3 hitpoint, float maxradius, Vec3 * pushdir, float *frac, int timeDelta, bool selfdamage ) {}
void GClip_ClearWorld() {}

void GClip_LinkEntity( edict_t * ent ) {
	LinkEntity( ServerCollisionModelStorage(), &ent->s );
}

void GClip_UnlinkEntity( edict_t * ent ) {
	UnlinkEntity( ServerCollisionModelStorage(), &ent->s );
}

void GClip_TouchTriggers( edict_t * ent ) {}
void G_PMoveTouchTriggers( pmove_t *pm, Vec3 previous_origin ) {}
int GClip_FindInRadius( Vec3 org, float rad, int *list, int maxcount ) {
	return 0;
}
bool IsHeadshot( int entNum, Vec3 hit, int timeDelta ) {
	return false;
}
