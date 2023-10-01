#include "qcommon/base.h"
#include "game/g_local.h"
#include "game/g_maps.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"

#include <algorithm>

struct CollisionEntity {
	EntityID id;
	Vec3 origin;
	Vec3 scale;
	Vec3 angles;
	Optional< CollisionModel > override_collision_model;
	StringHash model;
	int view_height;
};

struct CollisionFrame {
	s64 timestamp;
	SpatialHashGrid grid;
	CollisionEntity entities[ MAX_EDICTS ];
	size_t num_entities;
};

constexpr size_t NUM_COLLISION_FRAMES = 64;
static CollisionFrame g_collision_frames[ NUM_COLLISION_FRAMES ];
static size_t g_current_collision_frame = 0;

static CollisionEntity GetCollisionEntity( const edict_t * ent ) {
	return CollisionEntity {
		.id = ent->s.id,
		.origin = ent->s.origin,
		.scale = ent->s.scale,
		.angles = ent->s.angles,
		.override_collision_model = ent->s.override_collision_model,
		.model = ent->s.model,
		.view_height = ent->viewheight,
	};
}

static void ApplyCollisionEntity( CollisionEntity cent, edict_t * ent ) {
	ent->s.id = cent.id;
	ent->s.origin = cent.origin;
	ent->s.scale = cent.scale;
	ent->s.angles = cent.angles;
	ent->s.override_collision_model = cent.override_collision_model;
	ent->s.model = cent.model;
	ent->viewheight = cent.view_height;
}

void GClip_BackUpCollisionFrame() {
	CollisionFrame * frame = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	frame->timestamp = svs.gametime;
	frame->num_entities = game.numentities;
	g_current_collision_frame++;

	CollisionFrame * newframe = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	newframe->grid = frame->grid;
	memcpy( newframe->entities, frame->entities, frame->num_entities * sizeof( CollisionEntity ) );
}

static void GetCollisionFrames4D( const CollisionFrame ** older, const CollisionFrame ** newer, int time_delta ) {
	if( time_delta == 0 ) {
		*older = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
		*newer = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
		return;
	}

	s64 time = svs.gametime + time_delta;
	for( size_t i = 1; i < NUM_COLLISION_FRAMES; i++ ) {
		s64 index = ( g_current_collision_frame - i ) % NUM_COLLISION_FRAMES;
		if( index < 0 ) {
			break;
		}
		if( g_collision_frames[ index ].timestamp < time ) {
			*older = &g_collision_frames[ index ];
			*newer = &g_collision_frames[ ( index + 1 ) % NUM_COLLISION_FRAMES ];
			return;
		}
	}

	// timedelta too big, idk return current?
	*older = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	*newer = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	return;
}

static CollisionEntity LerpCollisionEntity4D( CollisionEntity * older, float t, CollisionEntity * newer ) {
	CollisionEntity ent = *newer;

	ent.origin = Lerp( older->origin, t, newer->origin );
	ent.scale = Lerp( older->scale, t, newer->scale );
	ent.angles = LerpAngles( older->angles, t, newer->angles );
	ent.view_height = Lerp( older->view_height, t, newer->view_height );

	switch( ent.override_collision_model.value.type ) {
		case CollisionModelType_AABB: {
			ent.override_collision_model.value.aabb.mins = Lerp( older->override_collision_model.value.aabb.mins, t, newer->override_collision_model.value.aabb.mins );
			ent.override_collision_model.value.aabb.maxs = Lerp( older->override_collision_model.value.aabb.maxs, t, newer->override_collision_model.value.aabb.maxs );
		} break;
		case CollisionModelType_Sphere: {
			ent.override_collision_model.value.sphere.center = Lerp( older->override_collision_model.value.sphere.center, t, newer->override_collision_model.value.sphere.center );
			ent.override_collision_model.value.sphere.radius = Lerp( older->override_collision_model.value.sphere.radius, t, newer->override_collision_model.value.sphere.radius );
		} break;
		case CollisionModelType_Capsule: {
			ent.override_collision_model.value.capsule.a = Lerp( older->override_collision_model.value.capsule.a, t, newer->override_collision_model.value.capsule.a );
			ent.override_collision_model.value.capsule.b = Lerp( older->override_collision_model.value.capsule.b, t, newer->override_collision_model.value.capsule.b );
			ent.override_collision_model.value.capsule.radius = Lerp( older->override_collision_model.value.capsule.radius, t, newer->override_collision_model.value.capsule.radius );
		} break;
		case CollisionModelType_MapModel: {
			Assert( older->override_collision_model.value.map_model == newer->override_collision_model.value.map_model );
		} break;
		case CollisionModelType_GLTF: {
			Assert( older->override_collision_model.value.gltf_model == newer->override_collision_model.value.gltf_model );
		} break;

		default: {
			// Assert( false );
		}
	}

	return ent;
}

static bool CheckSimilarCollisionEntities( CollisionEntity * older, CollisionEntity * newer ) {
	if( older->id.id != newer->id.id )
		return false;

	if( older->override_collision_model.exists != newer->override_collision_model.exists )
		return false;

	if( newer->override_collision_model.exists ) {
		if( older->override_collision_model.value.type != newer->override_collision_model.value.type )
			return false;
	}
	else {
		if( older->model != newer->model )
			return false;
	}

	return true;
}

static bool CollisionEntity4D( int entity_id, int time_delta, edict_t * ent ) {
	*ent = game.edicts[ entity_id ];
	if( time_delta == 0 || entity_id == 0 ) // special case world...
		return true;

	CollisionEntity * newer = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ].entities[ entity_id ];
	s64 newer_time = svs.gametime;
	s64 target_time = svs.gametime + time_delta;
	for( size_t i = 1; i < NUM_COLLISION_FRAMES; i++ ) {
		s64 index = ( g_current_collision_frame - i ) % NUM_COLLISION_FRAMES;
		CollisionEntity * older = &g_collision_frames[ index ].entities[ entity_id ];
		if( !CheckSimilarCollisionEntities( older, newer ) ) {
			// entity changed before this point, use most recent version
			ApplyCollisionEntity( *newer, ent );
			return true;
		}

		s64 older_time = g_collision_frames[ index ].timestamp;

		if( g_collision_frames[ index ].timestamp < target_time ) {
			float t = Unlerp01( older_time, target_time, newer_time );
			CollisionEntity lerped = LerpCollisionEntity4D( older, t, newer );
			ApplyCollisionEntity( lerped, ent );
			return true;
		}

		newer = older;
		newer_time = older_time;
	}

	return false; // time_delta too big, can't find
}

trace_t G_Trace4D( Vec3 start, MinMax3 bounds, Vec3 end, const edict_t * passedict, SolidBits solid_mask, int time_delta ) {
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

	trace_t result = MakeMissedTrace( ray );

	const CollisionFrame * a;
	const CollisionFrame * b;
	GetCollisionFrames4D( &a, &b, time_delta );
	int touchlist[ MAX_EDICTS ];
	size_t num = TraverseSpatialHashGrid( &a->grid, &b->grid, broadphase_bounds, touchlist, solid_mask );

	for( size_t i = 0; i < num; i++ ) {
		edict_t touch;
		if( !CollisionEntity4D( touchlist[ i ], time_delta, &touch ) )
			continue;
		if( touch.s.number == passent )
			continue;
		if( touch.r.owner != NULL && touch.r.owner->s.number == passent )
			continue;
		if( game.edicts[ passent ].r.owner != NULL && game.edicts[ passent ].r.owner->s.number == touch.s.number )
			continue;

		trace_t trace = TraceVsEnt( ServerCollisionModelStorage(), ray, shape, &touch.s, solid_mask );
		if( trace.fraction <= result.fraction ) {
			result = trace;
		}
	}

	return result;
}

trace_t G_Trace( Vec3 start, MinMax3 bounds, Vec3 end, const edict_t * passedict, SolidBits solid_mask ) {
	return G_Trace4D( start, bounds, end, passedict, solid_mask, 0 );
}

int GClip_FindInRadius4D( Vec3 org, float rad, int * list, size_t maxcount, int time_delta ) {
	MinMax3 bounds = MinMax3( org - rad, org + rad );

	const CollisionFrame * a;
	const CollisionFrame * b;
	GetCollisionFrames4D( &a, &b, time_delta );
	int touchlist[ MAX_EDICTS ];
	size_t touchnum = TraverseSpatialHashGrid( &a->grid, &b->grid, bounds, touchlist, SolidMask_AnySolid );

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

void G_SplashFrac4D( const edict_t * ent, Vec3 hitpoint, float maxradius, Vec3 * pushdir, float *frac, int time_delta, bool selfdamage ) {
	edict_t ent4d;
	if( !CollisionEntity4D( ENTNUM( ent ), time_delta, &ent4d ) )
		return;
	G_SplashFrac( &ent4d.s, &ent4d.r, hitpoint, maxradius, pushdir, frac, selfdamage );
}

void GClip_ClearWorld() {
	CollisionFrame * frame = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	ClearSpatialHashGrid( &frame->grid );
}

void GClip_LinkEntity( const edict_t * ent ) {
	CollisionFrame * frame = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	frame->entities[ ENTNUM( ent ) ] = GetCollisionEntity( ent );
	LinkEntity( &frame->grid, ServerCollisionModelStorage(), &ent->s, ENTNUM( ent ) );
}

void GClip_UnlinkEntity( const edict_t * ent ) {
	CollisionFrame * frame = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	UnlinkEntity( &frame->grid, ENTNUM( ent ) );
}

void GClip_TouchTriggers( edict_t * ent ) {
	// TODO: timedelta
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	if( bounds == MinMax3::Empty() )
		return;

	bounds.mins += ent->s.origin;
	bounds.maxs += ent->s.origin;

	int touchlist[ MAX_EDICTS ];
	CollisionFrame * frame = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	size_t touchnum = TraverseSpatialHashGrid( &frame->grid, bounds, touchlist, Solid_Trigger );

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

void G_PMoveTouchTriggers( pmove_t * pm, Vec3 previous_origin ) {
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
	}

	GClip_LinkEntity( ent );

	MinMax3 bounds = pm->bounds + pm->playerState->pmove.origin;
	bounds = Union( bounds, pm->bounds + previous_origin );

	int touchlist[ MAX_EDICTS ];
	CollisionFrame * frame = &g_collision_frames[ g_current_collision_frame % NUM_COLLISION_FRAMES ];
	size_t num = TraverseSpatialHashGrid( &frame->grid, bounds, touchlist, Solid_Trigger );

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
		if( !BoundsOverlap( bounds, hit_bounds ) )
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

		G_CallTouch( hit, ent, Vec3( 0.0f ), SolidMask_AnySolid );
	}
}

int GClip_FindInRadius( Vec3 org, float rad, int * list, size_t maxcount ) {
	return GClip_FindInRadius4D( org, rad, list, maxcount, 0 );
}

bool IsHeadshot( int entNum, Vec3 hit, int timeDelta ) {
	edict_t ent4d;
	if( !CollisionEntity4D( entNum, timeDelta, &ent4d ) )
		return false;
	float top = ent4d.s.origin.z + EntityBounds( ServerCollisionModelStorage(), &ent4d.s ).maxs.z;
	return top - hit.z <= 16.0f;
}
