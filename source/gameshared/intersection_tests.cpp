#include "qcommon/base.h"
#include "qcommon/fpe.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/collision.h"

Ray MakeRayOriginDirection( Vec3 origin, Vec3 direction, float length ) {
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	ray.length = length;

	for( int i = 0; i < 3; i++ ) {
		ray.inv_dir[ i ] = direction[ i ] == 0.0f ? 0.0f : 1.0f / direction[ i ];
	}

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

static bool Contains( const MinMax3 & aabb, Vec3 p ) {
	for( int i = 0; i < 3; i++ ) {
		if( p[ i ] < aabb.mins[ i ] || p[ i ] > aabb.maxs[ i ] ) {
			return false;
		}
	}

	return true;
}

bool RayVsAABB( const Ray & ray, const MinMax3 & aabb, Intersection * enter_out, Intersection * leave_out ) {
	if( ray.length == 0.0f ) {
		*enter_out = { };
		*leave_out = { };
		return Contains( aabb, ray.origin );
	}

	constexpr Vec3 aabb_normals[] = {
		Vec3( 1.0f, 0.0f, 0.0f ),
		Vec3( 0.0f, 1.0f, 0.0f ),
		Vec3( 0.0f, 0.0f, 1.0f ),
	};

	Intersection enter = { 0.0f };
	Intersection leave = { ray.length };

	for( int i = 0; i < 3; i++ ) {
		if( ray.direction[ i ] == 0.0f ) {
			if( aabb.mins[ i ] < ray.origin[ i ] && ray.origin[ i ] < aabb.maxs[ i ] )
				continue;
			return false;
		}

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

static MinMax3 MinkowskiSum( const MinMax3 & bounds1, const CenterExtents3 & bounds2 ) {
	return MinMax3( bounds1.mins - bounds2.center - bounds2.extents, bounds1.maxs - bounds2.center + bounds2.extents );
}

static MinMax3 MinkowskiSum( const MinMax3 & bounds1, const Sphere & sphere ) {
	return MinMax3( bounds1.mins - sphere.center - sphere.radius, bounds1.maxs - sphere.center + sphere.radius );
}

static float Support( const CenterExtents3 & aabb, Vec3 dir ) {
	float radius = Abs( aabb.extents.x * dir.x ) + Abs( aabb.extents.y * dir.y ) + Abs( aabb.extents.z * dir.z );
	return radius + Dot( aabb.center, dir );
}

static float Support( const Sphere & sphere, Vec3 dir ) {
	return sphere.radius - Dot( sphere.center, dir );
}

static float AxialSupport( const CenterExtents3 & aabb, int axis, bool positive ) {
	return aabb.extents[ axis ] + ( positive ? 1.0f : -1.0f ) * aabb.center[ axis ];
}

static float AxialSupport( const Sphere & sphere, int axis, bool positive ) {
	return sphere.radius + ( positive ? 1.0f : -1.0f ) * sphere.center[ axis ];
}

MinMax3 MinkowskiSum( const MinMax3 & bounds, const Shape & shape ) {
	switch( shape.type ) {
		case ShapeType_Ray:
			return bounds;
		case ShapeType_AABB:
			return MinkowskiSum( bounds, shape.aabb );
		case ShapeType_Sphere:
			return MinkowskiSum( bounds, shape.sphere );
	}

	Assert( false );
	return MinMax3::Empty();
}

float Support( const Shape & shape, Vec3 dir ) {
	switch( shape.type ) {
		case ShapeType_Ray:
			return 0.0f;
		case ShapeType_AABB:
			return Support( shape.aabb, dir );
		case ShapeType_Sphere:
			return Support( shape.sphere, dir );
	}

	Assert( false );
	return 0;
}

static float AxialSupport( const Shape & shape, int axis, bool positive ) {
	switch( shape.type ) {
		case ShapeType_Ray:
			return 0.0f;
		case ShapeType_AABB:
			return AxialSupport( shape.aabb, axis, positive );
		case ShapeType_Sphere:
			return AxialSupport( shape.sphere, axis, positive );
	}

	Assert( false );
	return 0;
}

static bool SweptShapeVsMapBrush( const MapData * map, const MapBrush * brush, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection ) {
	if( ( brush->solidity & solid_mask ) == 0 )
		return false;

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

	enter.solidity = brush->solidity;
	*intersection = enter;
	return true;
}

static bool SweptShapeVsMapLeaf( const MapData * map, const MapKDTreeNode * leaf, const Ray & ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection ) {
	Optional< Intersection > best = NONE;

	for( u32 i = 0; i < leaf->leaf.num_brushes; i++ ) {
		const MapBrush * brush = &map->brushes[ map->brush_indices[ leaf->leaf.first_brush + i ] ];
		Intersection brush_intersection;
		if( SweptShapeVsMapBrush( map, brush, ray, shape, solid_mask, &brush_intersection ) ) {
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

bool SweptShapeVsMapModel( const MapData * map, const MapModel * model, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection ) {
	Intersection bounds_enter, bounds_leave;
	if( !RayVsAABB( ray, MinkowskiSum( model->bounds, shape ), &bounds_enter, &bounds_leave ) )
		return false;

	KDTreeTraversalWork todo[ 64 ];
	u32 num_todo = 0;

	Optional< Intersection > best = NONE;

	KDTreeTraversalWork current = { &map->nodes[ model->root_node ], bounds_enter.t, bounds_leave.t };
	KDTreeTraversalWork next;
	while( true ) {
		if( current.t_min > ray.length )
			break;

		if( !MapKDTreeNode::is_leaf( *current.node ) ) {
			u32 axis = current.node->node.is_leaf_and_splitting_plane_axis;

			const MapKDTreeNode * near_child = current.node + 1;
			const MapKDTreeNode * far_child = &map->nodes[ current.node->node.front_child ];

			// move the near splitting plane ahead by support function
			float splitting_plane_near = current.node->node.splitting_plane_distance + AxialSupport( shape, axis, false );
			// move the far splitting plane closer by support function
			float splitting_plane_far = current.node->node.splitting_plane_distance - AxialSupport( shape, axis, true );

			if( ray.direction[ axis ] < 0.0f ) {
				// moving from far to near
				Swap2( &near_child, &far_child );
				Swap2( &splitting_plane_near, &splitting_plane_far );
			}

			if( ray.direction[ axis ] == 0.0f ) {
				// moving parallel to the splitting plane
				if( ray.origin[ axis ] <= splitting_plane_near ) {
					// we start in the near child
					next = { near_child, current.t_min, current.t_max };
					if( ray.origin[ axis ] >= splitting_plane_far ) {
						// we also reach the far child
						todo[ num_todo++ ] = { far_child, current.t_min, current.t_max };
					}
				}
				else {
					// we start in the far child
					next = { far_child, current.t_min, current.t_max };
				}
			}
			else {
				// not moving parallel to the splitting plane
				float t_at_near = ( splitting_plane_near - ray.origin[ axis ] ) * ray.inv_dir[ axis ];
				float t_at_far = ( splitting_plane_far - ray.origin[ axis ] ) * ray.inv_dir[ axis ];

				if( current.t_min <= t_at_near ) {
					// we start in the near child
					next = { near_child, current.t_min, Min2( current.t_max, t_at_near ) };
					if( current.t_max >= t_at_far ) {
						// we also reach the far child
						todo[ num_todo++ ] = { far_child, Max2( current.t_min, t_at_far ), current.t_max };
					}
				}
				else {
					// we start in the far child
					next = { far_child, Max2( current.t_min, t_at_far ), current.t_max };
				}
			}
			if( num_todo == ARRAY_COUNT( todo ) )
				Fatal( "Trace hit max tree depth" );
			current = next;
		}
		else {
			Intersection leaf_intersection;
			if( SweptShapeVsMapLeaf( map, current.node, ray, shape, solid_mask, &leaf_intersection ) ) {
				if( !best.exists || leaf_intersection.t < best.value.t ) {
					best = leaf_intersection;
				}
			}

			if( num_todo == 0 )
				break;

 			num_todo--;
			current = todo[ num_todo ];
		}
	}

	if( best.exists ) {
		*intersection = best.value;
	}

	return best.exists;
}

static Vec3 MakeNormal( int axis, bool positive ) {
	Vec3 n = Vec3( 0.0f );
	n[ axis ] = positive ? 1.0f : -1.0f;
	return n;
}

// see RTCD
bool SweptAABBVsAABB( const MinMax3 & a, Vec3 va, const MinMax3 & b, Vec3 vb, Intersection * intersection ) {
	// Com_GGPrint( "aabb vs aabb {} {} vs {} {}\n", a.mins, a.maxs, b.mins, b.maxs );
	if( BoundsOverlap( a, b ) ) {
		*intersection = { };
		return true;
	}

	Vec3 v = vb - va;
	if( v == Vec3( 0.0f ) )
		return false;

	float t_min = 0.0f;
	float t_max = 1.0f;
	Vec3 intersection_normal = Vec3( 0.0f );

	for( int i = 0; i < 3; i++ ) {
		if( v[ i ] == 0.0f ) {
			if( b.maxs[ i ] < a.mins[ i ] || b.mins[ i ] > a.maxs[ i ] )
				return false;
		}

		if( v[ i ] < 0.0f ) {
			if( b.maxs[ i ] < a.mins[ i ] )
				return false;
			if( a.maxs[ i ] < b.mins[ i ] ) {
				float t = ( a.maxs[ i ] - b.mins[ i ] ) / v[ i ];
				if( t > t_min ) {
					t_min = t;
					intersection_normal = MakeNormal( i, false );
				}
			}
			if( b.maxs[ i ] > a.mins[ i ] ) {
				float t = ( a.mins[ i ] - b.maxs[ i ] ) / v[ i ];
				t_max = Min2( t, t_max );
			}
		}

		if( v[ i ] > 0.0f ) {
			if( b.mins[ i ] > a.maxs[ i ] )
				return false;
			if( b.maxs[ i ] < a.mins[ i ] ) {
				float t = ( a.mins[ i ] - b.maxs[ i ] ) / v[ i ];
				if( t > t_min ) {
					t_min = t;
					intersection_normal = MakeNormal( i, true );
				}
			}
			if( a.maxs[ i ] > b.mins[ i ] ) {
				float t = ( a.maxs[ i ] - b.mins[ i ] ) / v[ i ];
				t_max = Min2( t, t_max );
			}
		}

	if( t_min > t_max )
		return false;
	}

	*intersection = { t_min, intersection_normal };

	return true;
}

static bool SweptShapeVsGLTFBrush( const GLTFCollisionData * gltf, GLTFCollisionBrush & brush, Mat4 transform, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection ) {
	constexpr Vec3 bevel_axes[] = {
		Vec3( 1, 0, 0 ),
		Vec3( 0, 1, 0 ),
		Vec3( 0, 0, 1 ),
	};

	Intersection enter = { 0.0f };
	Intersection leave = { ray.length };

	auto CheckPlane = [&]( Plane plane ) -> bool {
		plane.distance += Support( shape, -plane.normal );

		float dist = plane.distance - Dot( plane.normal, ray.origin );
		float denom = Dot( plane.normal, ray.direction );

		if( denom == 0.0f ) {
			if( dist < 0.0f )
				return false;
			return true;
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

		return true;
	};

	if( ( brush.solidity & solid_mask ) == 0 )
		return false;

	MinMax1 bevel_bounds[ ARRAY_COUNT( bevel_axes ) ];

	for( MinMax1 & bevel_bound : bevel_bounds ) {
		bevel_bound = { FLT_MAX, -FLT_MAX };
	}

	// check non-bevel planes
	Span< Plane > brush_planes = gltf->planes.slice( brush.first_plane, brush.first_plane + brush.num_planes );
	for( Plane plane : brush_planes ) {
		Vec3 p = ( transform * Vec4( plane.normal * plane.distance, 1.0f ) ).xyz();
		plane.normal = SafeNormalize( ( transform * Vec4( plane.normal, 0.0f ) ).xyz() );
		plane.distance = Dot( p, plane.normal );

		bool is_bevel_axis = false;
		for( const Vec3 & bevel_axis : bevel_axes ) {
			if( Abs( Dot( plane.normal, bevel_axis ) ) >= 0.99999f ) {
				is_bevel_axis = true;
				break;
			}
		}

		if( is_bevel_axis )
			continue;

		if( !CheckPlane( plane ) )
			return false;
	}

	// check bevel planes
	Span< Vec3 > brush_vertices = gltf->vertices.slice( brush.first_vertex, brush.first_vertex + brush.num_vertices );
	for( Vec3 vert : brush_vertices ) {
		vert = ( transform * Vec4( vert, 1.0f ) ).xyz();
		for( size_t j = 0; j < ARRAY_COUNT( bevel_axes ); j++ ) {
			float axis = Dot( vert, bevel_axes[ j ] );
			bevel_bounds[ j ] = Union( bevel_bounds[ j ], axis );
		}
	}

	for( size_t i = 0; i < ARRAY_COUNT( bevel_axes ); i++ ) {
		Plane pos = { bevel_axes[ i ], bevel_bounds[ i ].hi };
		Plane neg = { -bevel_axes[ i ], -bevel_bounds[ i ].lo };

		if( !CheckPlane( pos ) )
			return false;
		if( !CheckPlane( neg ) )
			return false;
	}

	enter.solidity = brush.solidity;
	*intersection = enter;
	return true;
}

bool SweptShapeVsGLTF( const GLTFCollisionData * gltf, Mat4 transform, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection ) {
	Optional< Intersection > best = NONE;

	for( GLTFCollisionBrush & brush : gltf->brushes ) {
		Intersection brush_intersection;
		if( SweptShapeVsGLTFBrush( gltf, brush, transform, ray, shape, solid_mask, &brush_intersection ) ) {
			if( !best.exists || brush_intersection.t < best.value.t ) {
				brush_intersection.solidity = brush.solidity;
				best = brush_intersection;
			}
		}
	}

	if( best.exists )
		*intersection = best.value;

	return best.exists;
}
