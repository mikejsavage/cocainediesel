#include "gameshared/trace.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"

// TODO copied from pbr
static float gamma( int n ) {
	constexpr float epsilon = FLT_EPSILON * 0.5f;
	return ( n * epsilon ) / ( 1.0f - n * epsilon );
}

static bool IntersectAABB( const MinMax3 & aabb, const Ray & ray, Intersection * enter_out, Intersection * leave_out ) {
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

		far *= 1.0f + 2.0f * gamma( 3 );

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

static bool Contains( const MinMax3 & aabb, Vec3 p ) {
	for( int i = 0; i < 3; i++ ) {
		if( p[ i ] < aabb.mins[ i ] || p[ i ] > aabb.maxs[ i ] ) {
			return false;
		}
	}

	return true;
}

static bool IntersectBrush( const MapData * map, const MapBrush * brush, Ray ray, Intersection * intersection ) {
	Intersection best, bounds_leave;
	if( !IntersectAABB( brush->bounds, ray, &best, &bounds_leave ) )
		return false;

	bool any_outside = !Contains( brush->bounds, ray.origin );
	for( u32 i = 0; i < brush->num_planes; i++ ) {
		const Plane * plane = &map->brush_planes[ brush->first_plane + i ];

		float start_dist = Dot( plane->normal, ray.origin ) - plane->distance;
		float end_dist = Dot( plane->normal, ray.origin + ray.direction * ray.length ) - plane->distance;

		if( start_dist > 0.0f ) { // start outside
			any_outside = true;
			if( end_dist > 0.0f ) // fully outside
				return false;
		}

		// fully inside
		if( start_dist <= 0.0f && end_dist <= 0.0f ) {
			continue;
		}

		// hit backface
		if( end_dist >= start_dist ) {
			continue;
		}

		// end_dist - start_dist = n.(o + ld) - n.o = n.o - n.o + l(n.d) = l(n.d)
		float t = -start_dist / ( end_dist - start_dist );
		if( t > best.t ) {
			best = { t, plane->normal };
		}
	}

	constexpr Intersection fully_inside_intersection = { 0.0f, Vec3( 0.0f ) };
	*intersection = any_outside ? best : fully_inside_intersection;
	return true;
}

static bool IntersectLeaf( const MapData * map, const MapKDTreeNode * leaf, const Ray & ray, Intersection * intersection ) {
	Optional< Intersection > best = NONE;

	for( u32 i = 0; i < leaf->leaf.num_brushes; i++ ) {
		const MapBrush * brush = &map->brushes[ map->brush_indices[ leaf->leaf.first_brush + i ] ];
		Intersection brush_intersection;
		if( IntersectBrush( map, brush, ray, &brush_intersection ) ) {
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

bool Trace( const MapData * map, const MapModel * model, Ray ray, Intersection * intersection ) {
	Intersection bounds_enter, bounds_leave;
	if( !IntersectAABB( model->bounds, ray, &bounds_enter, &bounds_leave ) )
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
			float t_plane = ( node->node.splitting_plane_distance - ray.origin[ axis ] ) * ray.inv_dir[ axis ];

			bool below_first =
				( ray.origin[ axis ] < node->node.splitting_plane_distance ) ||
				( ray.origin[ axis ] == node->node.splitting_plane_distance && ray.direction[ axis ] <= 0.0f );
			const MapKDTreeNode * first_child = node + 1;
			const MapKDTreeNode * second_child = &map->nodes[ node->node.front_child ];
			if( !below_first ) {
				Swap2( &first_child, &second_child );
			}

			if( t_plane > t_max || t_plane <= 0.0f ) {
				node = first_child;
			}
			else if( t_plane < t_min ) {
				node = second_child;
			}
			else {
				if( num_todo == ARRAY_COUNT( todo ) ) {
					Fatal( "shit" ); // TODO
				}

				todo[ num_todo ] = { second_child, t_plane, t_max };
				num_todo++;
				node = first_child;
				t_max = t_plane;
			}
		}
		else {
			Intersection leaf_intersection;
			if( IntersectLeaf( map, node, ray, &leaf_intersection ) ) {
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
