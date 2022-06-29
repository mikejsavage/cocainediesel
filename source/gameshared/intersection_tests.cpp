#include "qcommon/base.h"
#include "qcommon/testing.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"

Ray MakeRayOriginDirection( Vec3 origin, Vec3 direction, float length ) {
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	ray.inv_dir = direction == Vec3( 0.0f ) ? Vec3( 0.0f ) : 1.0f / direction;
	ray.length = length;
	return ray;
}

Ray MakeRayStartEnd( Vec3 start, Vec3 end ) {
	return MakeRayOriginDirection( start, SafeNormalize( end - start ), Length( end - start ) );
}

// see PBR
static float Gamma( int n ) {
	constexpr float epsilon = FLT_EPSILON * 0.5f;
	return ( n * epsilon ) / ( 1.0f - n * epsilon );
}

bool RayVsAABB( const Ray & ray, const MinMax3 & aabb, Intersection * enter_out, Intersection * leave_out ) {
	constexpr Vec3 aabb_normals[] = {
		Vec3( 1.0f, 0.0f, 0.0f ),
		Vec3( 0.0f, 1.0f, 0.0f ),
		Vec3( 0.0f, 0.0f, 1.0f ),
	};

	Intersection enter = { 0.0f };
	Intersection leave = { ray.length };

	for( int i = 0; i < 3; i++ ) {
		float near = ( aabb.mins[ i ] - ray.origin[ i ] ) * ray.inv_dir[ i ];
		float far = ( aabb.maxs[ i ] - ray.origin[ i ] ) * ray.inv_dir[ i ];
		if( ray.direction[ i ] < 0.0f ) {
			Swap2( &near, &far );
		}

		far *= 1.0f + 2.0f * Gamma( 3 );

		if( near > enter.t ) {
			// TODO: inline SignedOne
			enter = { near, aabb_normals[ i ] * -SignedOne( ray.direction[ i ] ) };
		}

		if( far < leave.t ) {
			leave = { far, aabb_normals[ i ] * SignedOne( ray.direction[ i ] ) };
		}

		if( enter.t > leave.t ) {
			return false;
		}
	}

	*enter_out = enter;
	*leave_out = leave;
	return true;
}

bool RayVsSphere( const Ray & ray, const Sphere & sphere, float * t ) {
	Vec3 m = ray.origin - sphere.center;
	float b = Dot( m, ray.direction );
	float c = Dot( m, m ) - Square( sphere.radius );

	// start outside and pointing away
	if( c > 0.0f && b > 0.0f )
		return false;

	// start inside
	if( c <= 0.0f ) {
		*t = 0.0f;
		return true;
	}

	float d = b * b - c;
	if( d < 0.0f )
		return false;

	*t = -b - sqrtf( d );
	return *t <= ray.length;
}

// see RTCD
// TODO: treat inside as solid
bool RayVsCapsule( const Ray & ray, const Capsule & capsule, float * t ) {
	Vec3 d = capsule.b - capsule.a;
	Vec3 m = ray.origin - capsule.a;
	Vec3 n = ray.direction * ray.length;

	float md = Dot( m, d );
	float nd = Dot( n, d );
	float dd = Dot( d, d );
	float nn = Dot( n, n );
	float mn = Dot( m, n );

	float a = dd * nn - nd * nd;
	float k = Dot( m, m ) - capsule.radius * capsule.radius;
	float c = dd * k - md * md;

	if( a == 0.0f ) {
		if( c > 0.0f )
			return false;

		if( md >= 0.0f && md <= dd ) {
			*t = 0.0f;
			return true;
		}

		Sphere cap;
		cap.center = md < 0.0f ? capsule.a : capsule.b;
		cap.radius = capsule.radius;
		return RayVsSphere( ray, cap, t );
	}

	float b = dd * mn - nd * md;
	float discr = b * b - a * c;
	if( discr < 0.0f )
		return false;

	*t = ray.length * ( -b - sqrtf( discr ) ) / a;

	float y = md + *t * nd;
	if( y >= 0.0f && y <= dd )
		return *t >= 0.0f && *t <= ray.length;

	Sphere cap;
	cap.center = y < 0.0f ? capsule.a : capsule.b;
	cap.radius = capsule.radius;
	return RayVsSphere( ray, cap, t );
}

/*
float capIntersect( Vec3 ro, Vec3 rd, Vec3 pa, Vec3 pb, float r )
{
	vec3 d = pb - pa;
	vec3 m = ro - pa;
	float dd = dot(d,d);
	float nd = dot(n,d);
	float md = dot(m,d);
	float nm = dot(n,m);
	float mm = dot(m,m);
	float nn = 1;
	float a = dd*nn - nd*nd;
	float b = dd*nm - md*nd;
	float c = dd*mm - md*md - r*r*dd; // = dd * k - md * md, where k = mm - r*r
	float h = b*b - a*c;
	if( h >= 0.0 )
	{
		float t = (-b-sqrt(h))/a;
		float y = md + t*nd;
		if( y>0.0 && y<dd ) return t;
		Vec3 oc = (y <= 0.0) ? m : ro - pb;
		b = dot(n,oc);
		c = dot(oc,oc) - r*r;
		h = b*b - c;
		if( h>0.0 ) return -b - sqrt(h);
	}
	return -1.0;
}
*/

static MinMax3 MinkowskiSum( const MinMax3 & bounds1, const CenterExtents3 & bounds2 ) {
	return MinMax3( bounds1.mins - bounds2.extents, bounds1.maxs + bounds2.extents );
}

// TODO look at center too
static float Support( const CenterExtents3 & aabb, Vec3 dir ) {
	return Abs( aabb.extents.x * dir.x ) + Abs( aabb.extents.y * dir.y ) + Abs( aabb.extents.z * dir.z );
}

static float AxialSupport( const CenterExtents3 & aabb, int axis, bool positive ) {
	return aabb.extents[ axis ];
}

MinMax3 MinkowskiSum( const MinMax3 & bounds, const Shape & shape ) {
	switch( shape.type ) {
		case ShapeType_Ray:
			return bounds;
		case ShapeType_AABB:
			return MinkowskiSum( bounds, shape.aabb );
	}

	assert( false );
	return MinMax3::Empty();
}

static float Support( const Shape & shape, Vec3 dir ) {
	switch( shape.type ) {
		case ShapeType_Ray:
			return 0.0f;
		case ShapeType_AABB:
			return Support( shape.aabb, dir );
	}

	assert( false );
	return 0;
}

static float AxialSupport( const Shape & shape, int axis, bool positive ) {
	switch( shape.type ) {
		case ShapeType_Ray:
			return 0.0f;
		case ShapeType_AABB:
			return AxialSupport( shape.aabb, axis, positive );
	}

	assert( false );
	return 0;
}

static bool Contains( const MinMax3 & aabb, Vec3 p ) {
	for( int i = 0; i < 3; i++ ) {
		if( p[ i ] < aabb.mins[ i ] || p[ i ] > aabb.maxs[ i ] ) {
			return false;
		}
	}

	return true;
}

static bool SweptShapeVsMapBrush( const MapData * map, const MapBrush * brush, Ray ray, const Shape & shape, Intersection * intersection ) {
	Intersection enter, leave;
	if( !RayVsAABB( ray, MinkowskiSum( brush->bounds, shape ), &enter, &leave ) )
		return false;

	for( u32 i = 0; i < brush->num_planes; i++ ) {
		Plane plane = map->brush_planes[ brush->first_plane + i ];
		plane.distance += Support( shape, -plane.normal );

		float dist = plane.distance - Dot( plane.normal, ray.origin );
		float denom = Dot( plane.normal, ray.direction );

		if( denom == 0.0f ) {
			if( dist < 0.0f )
				return false;
			continue;
		}

		float t = dist / denom;

		if( denom < 0.0f ) {
			if( t > enter.t )
				enter = { t, plane.normal };
		}
		else {
			if( t < leave.t )
				leave = { t, plane.normal };
		}

		if( enter.t > leave.t )
			return false;
	}

	*intersection = enter;
	return true;
}

static bool SweptShapeVsMapLeaf( const MapData * map, const MapKDTreeNode * leaf, const Ray & ray, const Shape & shape, Intersection * intersection ) {
	Optional< Intersection > best = NONE;

	for( u32 i = 0; i < leaf->leaf.num_brushes; i++ ) {
		const MapBrush * brush = &map->brushes[ map->brush_indices[ leaf->leaf.first_brush + i ] ];
		Intersection brush_intersection;
		if( SweptShapeVsMapBrush( map, brush, ray, shape, &brush_intersection ) ) {
			if( !best.exists || brush_intersection.t < best.value.t ) {
				best = brush_intersection;
			}
		}
	}

	if( best.exists ) {
		*intersection = best.value;
	}

	return best.exists;
}

struct KDTreeTraversalWork {
	const MapKDTreeNode * node;
	float t_min;
	float t_max;
};

bool SweptShapeVsMapModel( const MapData * map, const MapModel * model, Ray ray, const Shape & shape, Intersection * intersection ) {
	Intersection bounds_enter, bounds_leave;
	if( !RayVsAABB( ray, MinkowskiSum( model->bounds, shape ), &bounds_enter, &bounds_leave ) )
		return false;

	float t_min = bounds_enter.t;
	float t_max = bounds_leave.t;

	KDTreeTraversalWork todo[ 64 ];
	u32 num_todo = 0;

	Optional< Intersection > best = NONE;

	const MapKDTreeNode * node = &map->nodes[ model->root_node ];
	while( true ) {
		if( t_min > ray.length )
			break;

		if( !MapKDTreeNode::is_leaf( *node ) ) {
			u32 axis = node->node.is_leaf_and_splitting_plane_axis;

			float splitting_interval_start = node->node.splitting_plane_distance - AxialSupport( shape, axis, true );
			float splitting_interval_end = node->node.splitting_plane_distance + AxialSupport( shape, axis, false );

			float t_interval_start, t_interval_end;
			bool start_in_first_child, reach_second_child;

			const MapKDTreeNode * first_child = node + 1;
			const MapKDTreeNode * second_child = &map->nodes[ node->node.front_child ];

			if( ray.direction[ axis ] == 0.0f ) {
				start_in_first_child = true;
				if( ray.origin[ axis ] > node->node.splitting_plane_distance ) {
					Swap2( &first_child, &second_child );
				}
				reach_second_child = ray.origin[ axis ] > splitting_interval_start && ray.origin[ axis ] < splitting_interval_end;

				t_interval_start = t_min;
				t_interval_end = t_max;
			}
			else {
				t_interval_start = ( splitting_interval_start - ray.origin[ axis ] ) * ray.inv_dir[ axis ];
				t_interval_end = ( splitting_interval_end - ray.origin[ axis ] ) * ray.inv_dir[ axis ];

				if( ray.direction[ axis ] < 0.0f ) {
					Swap2( &first_child, &second_child );
					Swap2( &t_interval_start, &t_interval_end );
				}

				start_in_first_child = t_interval_end >= t_min;
				reach_second_child = t_interval_start <= t_max;
			}

			if( !start_in_first_child ) {
				node = second_child;
				t_min = t_interval_start;
			}
			else if( !reach_second_child ) {
				node = first_child;
				t_max = t_interval_end;
			}
			else {
				if( num_todo == ARRAY_COUNT( todo ) ) {
					Fatal( "Trace hit max tree depth" );
				}

				todo[ num_todo ] = { second_child, t_interval_start, t_max };
				num_todo++;

				node = first_child;
				t_max = t_interval_end;
			}
		}
		else {
			Intersection leaf_intersection;
			if( SweptShapeVsMapLeaf( map, node, ray, shape, &leaf_intersection ) ) {
				if( !best.exists || leaf_intersection.t < best.value.t ) {
					best = leaf_intersection;
				}
			}

			if( num_todo > 0 ) {
				num_todo--;
				node = todo[ num_todo ].node;
				t_min = todo[ num_todo ].t_min;
				t_max = todo[ num_todo ].t_max;
			}
			else {
				break;
			}
		}
	}

	if( best.exists ) {
		*intersection = best.value;
	}

	return best.exists;
}

static bool Intersecting( const MinMax3 & a, const MinMax3 & b ) {
	for( int i = 0; i < 3; i++ ) {
		if( a.maxs[ i ] < b.mins[ i ] || a.mins[ i ] > b.maxs[ i ] ) {
			return false;
		}
	}

	return true;
}

// see RTCD
bool SweptAABBVsAABB( const MinMax3 & a, Vec3 va, const MinMax3 & b, Vec3 vb, Intersection * intersection ) {
	if( Intersecting( a, b ) ) {
		*intersection = { 0.0f };
		return true;
	}

	Vec3 v = vb - va;

	float t_min = 0.0f;
	float t_max = 1.0f;

	for( int i = 0; i < 3; i++ ) {
		if( v[ i ] < 0.0f ) {
			if( b.maxs[ i ] < a.mins[ i ] )
				return false;
			if( a.maxs[ i ] < b.mins[ i ] )
				t_min = Max2( ( a.maxs[ i ] - b.mins[ i ] ) / v[ i ], t_min );
			if( b.maxs[ i ] > a.mins[ i ] )
				t_max = Min2( ( a.mins[ i ] - b.maxs[ i ] ) / v[ i ], t_max );
		}

		if( v[ i ] > 0.0f ) {
			if( b.mins[ i ] > a.maxs[ i ] )
				return false;
			if( b.maxs[ i ] < a.mins[ i ] )
				t_min = Max2( ( a.mins[ i ] - b.maxs[ i ] ) / v[ i ], t_min );
			if( a.maxs[ i ] > b.mins[ i ] )
				t_max = Min2( ( a.maxs[ i ] - b.mins[ i ] ) / v[ i ], t_max );
		}

		if( t_min > t_max )
			return false;
	}

	return true;
}

SELFTESTS( "Intersection tests", {
	Vec3 a = Vec3( 1, 2, 3 );
	TEST( "a", a == Vec3( 3, 2, 1 ) );
} );