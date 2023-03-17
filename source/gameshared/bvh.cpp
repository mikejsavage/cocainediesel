#include "qcommon/base.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/q_math.h"

#include <algorithm>

constexpr size_t Log( size_t n, size_t m ) {
	return ( n > 1 ) ? 1 + Log( n / m, m ) : 0;
}
constexpr size_t Pow( size_t n, size_t m ) {
	return ( m > 0 ) ? n * Pow( n, m - 1 ) : 1;
}

constexpr size_t NextPowerOf2( size_t v ) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v++;
	return v;
}
constexpr size_t NumNodesOnLevel( size_t level ) {
	return Pow( BVH_NODE_SIZE, level );
}
constexpr size_t FirstNodeIndexOnLevel( size_t level ) {
	return ( Pow( BVH_NODE_SIZE, level ) + ( BVH_NODE_SIZE - 2 ) ) / ( BVH_NODE_SIZE - 1 ) - 1;
}

constexpr u32 ExpandBits( u32 x ) {
	x = ( x * 0x00010001u ) & 0xFF0000FFu;
	x = ( x * 0x00000101u ) & 0x0F00F00Fu;
	x = ( x * 0x00000011u ) & 0xC30C30C3u;
	x = ( x * 0x00000005u ) & 0x49249249u;
	return x;
}

constexpr u32 MortonID( Vec3 v, MinMax3 bounds ) {
	Vec3 quantized = Vec3(
		Unlerp01( bounds.mins.x, v.x, bounds.maxs.x ),
		Unlerp01( bounds.mins.y, v.y, bounds.maxs.y ),
		Unlerp01( bounds.mins.z, v.z, bounds.maxs.z )
	);

	u32 x = Clamp( 0u, u32( quantized.x * 1024 ), 1023u );
	u32 y = Clamp( 0u, u32( quantized.y * 1024 ), 1023u );
	u32 z = Clamp( 0u, u32( quantized.z * 1024 ), 1023u );

	x = ExpandBits( x );
	y = ExpandBits( y );
	z = ExpandBits( z );

	return ( x << 2 ) | ( y << 1 ) | z;
}

static void BuildBVHLevel( BVH * bvh, size_t level, size_t num_levels ) {
	size_t first_idx = FirstNodeIndexOnLevel( level );
	size_t num = NumNodesOnLevel( level );

	for( size_t i = 0; i < num; i++ ) {
		MinMax3 node_bounds = MinMax3::Empty();
		bool is_leaf = level + 1 == num_levels;
		size_t first_child_index;
		if( is_leaf ) {
			first_child_index = BVH_NODE_SIZE * i;
			for( size_t j = 0; j < BVH_NODE_SIZE; j++ ) {
				size_t primitive_idx = first_child_index + j;
				if( primitive_idx >= bvh->num_primitives )
					continue;
				node_bounds = Union( node_bounds, bvh->primitives[ primitive_idx ].bounds );
			}
		}
		else {
			first_child_index = FirstNodeIndexOnLevel( level + 1 ) + BVH_NODE_SIZE * i;
			for( size_t j = 0; j < BVH_NODE_SIZE; j++ ) {
				node_bounds = Union( node_bounds, bvh->nodes[ first_child_index + j ].bounds );
			}
		}
		bvh->nodes[ first_idx + i ].bounds = node_bounds;
		bvh->nodes[ first_idx + i ].level = level;
		bvh->nodes[ first_idx + i ].first_child_index = first_child_index;
	}
}

void BuildBVH( BVH * bvh, const CollisionModelStorage * storage ) {
	TracyZoneScoped;

	MinMax3 global_bounds = MinMax3::Empty();

	for( size_t i = 0; i < bvh->num_primitives; i++ ) {
		Primitive & primitive = bvh->primitives[ i ];
		global_bounds = Union( global_bounds, primitive.bounds );
	}

	for( size_t i = 0; i < bvh->num_primitives; i++ ) {
		Primitive & primitive = bvh->primitives[ i ];
		primitive.morton_id = MortonID( primitive.center, global_bounds );
	}

	std::sort( bvh->primitives, bvh->primitives + bvh->num_primitives, []( Primitive a, Primitive b ) {
		return a.morton_id < b.morton_id;
	} );

	size_t num_levels = Log( NextPowerOf2( bvh->num_primitives ), BVH_NODE_SIZE );
	for( size_t i = 0; i < num_levels; i++ ) {
		size_t level = num_levels - i - 1;
		BuildBVHLevel( bvh, level, num_levels );
	}
}

void TraverseBVH( BVH * bvh, MinMax3 bounds, std::function< void ( u32 entity, u32 total ) > callback ) {
	TracyZoneScoped;

	callback( 0, bvh->num_primitives );

	if( bvh->num_primitives == 0 ) return;

	size_t todo[ 1024 ];
	size_t num_todo = 0;
	size_t num_levels = Log( NextPowerOf2( bvh->num_primitives ), BVH_NODE_SIZE );

	todo[ num_todo++ ] = 0;
	while( num_todo > 0 ) {
		size_t current_node = todo[ --num_todo ];
		BVHNode & node = bvh->nodes[ current_node ];

		bool is_leaf = node.level + 1 >= num_levels;
		if( is_leaf ) {
			for( size_t i = 0; i < BVH_NODE_SIZE; i++ ) {
				size_t primitive_idx = node.first_child_index + i;
				if( primitive_idx >= bvh->num_primitives )
					continue;
				Primitive & primitive = bvh->primitives[ primitive_idx ];
				if( Intersecting( bounds, primitive.bounds ) ) {
					callback( primitive.entity_id, bvh->num_primitives + 1 );
				}
			}
		}
		else {
			for( size_t i = 0; i < BVH_NODE_SIZE; i++ ) {
				size_t child_idx = node.first_child_index + i;
				BVHNode & child_node = bvh->nodes[ child_idx ];
				if( Intersecting( bounds, child_node.bounds ) ) {
					todo[ num_todo++ ] = child_idx;
				}
			}
		}
	}
}

static Primitive & GetPrimitive( BVH * bvh, u32 entity_id ) {
	for( size_t i = 0; i < bvh->num_primitives; i++ ) {
		if( bvh->primitives[ i ].entity_id == entity_id )
			return bvh->primitives[ i ];
	}
	Assert( bvh->num_primitives <= MAX_PRIMITIVES );
	Primitive & primitive = bvh->primitives[ bvh->num_primitives ];
	bvh->num_primitives++;
	return primitive;
}

static void DeletePrimitive( BVH * bvh, u32 entity_id ) {
	for( size_t i = 0; i < bvh->num_primitives; i++ ) {
		if( bvh->primitives[ i ].entity_id == entity_id ) {
			Swap2( &bvh->primitives[ i ], &bvh->primitives[ bvh->num_primitives - 1 ] );
			bvh->num_primitives--;
			return;
		}
	}
}

void PrepareEntity( BVH * bvh, const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	MinMax3 bounds = EntityBounds( storage, ent );
	if( bounds.mins == MinMax3::Empty().mins && bounds.maxs == MinMax3::Empty().maxs )
		return;

	bounds.mins += ent->origin;
	bounds.maxs += ent->origin;

	Primitive & primitive = GetPrimitive( bvh, ent->number );
	primitive.entity_id = ent->number;
	primitive.bounds = bounds;
	primitive.center = ( bounds.maxs + bounds.mins ) * 0.5f;
}

void LinkEntity( BVH * bvh, const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	PrepareEntity( bvh, storage, ent );

	BuildBVH( bvh, storage );
}

void UnlinkEntity( BVH * bvh, const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	DeletePrimitive( bvh, ent->number );

	BuildBVH( bvh, storage );
}

void ClearBVH( BVH * bvh ) {
	bvh->num_primitives = 0;
}
