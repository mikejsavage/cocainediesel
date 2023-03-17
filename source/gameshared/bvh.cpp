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

constexpr size_t MAX_PRIMITIVES = MAX_EDICTS;
constexpr size_t BVH_NODE_SIZE = 2;
constexpr size_t BVH_NODES = ( MAX_PRIMITIVES - 1 ) / ( BVH_NODE_SIZE - 1 );

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

struct BVHNode {
	u32 level;
	u32 first_child_index;
	MinMax3 bounds;
};

struct Primitive {
	u32 entity_id;
	u32 morton_id;
	MinMax3 bounds;
	Vec3 center;
};

static BVHNode nodes[ BVH_NODES ];
static Primitive primitives[ MAX_PRIMITIVES ];
static size_t num_primitives;

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

static void BuildBVHLevel( size_t level, size_t num_levels ) {
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
				if( primitive_idx >= num_primitives )
					continue;
				node_bounds = Union( node_bounds, primitives[ primitive_idx ].bounds );
			}
		}
		else {
			first_child_index = FirstNodeIndexOnLevel( level + 1 ) + BVH_NODE_SIZE * i;
			for( size_t j = 0; j < BVH_NODE_SIZE; j++ ) {
				node_bounds = Union( node_bounds, nodes[ first_child_index + j ].bounds );
			}
		}
		nodes[ first_idx + i ].bounds = node_bounds;
		nodes[ first_idx + i ].level = level;
		nodes[ first_idx + i ].first_child_index = first_child_index;
	}
}

static void BuildBVH( const CollisionModelStorage * storage ) {
	TracyZoneScoped;

	MinMax3 global_bounds = MinMax3::Empty();

	for( size_t i = 0; i < num_primitives; i++ ) {
		Primitive & primitive = primitives[ i ];
		global_bounds = Union( global_bounds, primitive.bounds );
	}

	for( size_t i = 0; i < num_primitives; i++ ) {
		Primitive & primitive = primitives[ i ];
		primitive.morton_id = MortonID( primitive.center, global_bounds );
	}

	std::sort( primitives, primitives + num_primitives, []( Primitive a, Primitive b ) {
		return a.morton_id < b.morton_id;
	} );

	size_t num_levels = Log( NextPowerOf2( num_primitives ), BVH_NODE_SIZE );
	for( size_t i = 0; i < num_levels; i++ ) {
		size_t level = num_levels - i - 1;
		BuildBVHLevel( level, num_levels );
	}
}

void TraverseBVH( MinMax3 bounds, std::function< void ( u32 entity, u32 total ) > callback ) {
	TracyZoneScoped;

	callback( 0, num_primitives );

	if( num_primitives == 0 ) return;

	size_t todo[ 1024 ];
	size_t num_todo = 0;
	size_t num_levels = Log( NextPowerOf2( num_primitives ), BVH_NODE_SIZE );

	todo[ num_todo++ ] = 0;
	while( num_todo > 0 ) {
		size_t current_node = todo[ --num_todo ];
		BVHNode & node = nodes[ current_node ];

		bool is_leaf = node.level + 1 >= num_levels;
		if( is_leaf ) {
			for( size_t i = 0; i < BVH_NODE_SIZE; i++ ) {
				size_t primitive_idx = node.first_child_index + i;
				if( primitive_idx >= num_primitives )
					continue;
				Primitive & primitive = primitives[ primitive_idx ];
				if( Intersecting( bounds, primitive.bounds ) ) {
					callback( primitive.entity_id, num_primitives + 1 );
				}
			}
		}
		else {
			for( size_t i = 0; i < BVH_NODE_SIZE; i++ ) {
				size_t child_idx = node.first_child_index + i;
				BVHNode & child_node = nodes[ child_idx ];
				if( Intersecting( bounds, child_node.bounds ) ) {
					todo[ num_todo++ ] = child_idx;
				}
			}
		}
	}
}

static Primitive & GetPrimitive( u32 entity_id ) {
	for( size_t i = 0; i < num_primitives; i++ ) {
		if( primitives[ i ].entity_id == entity_id )
			return primitives[ i ];
	}
	Assert( num_primitives <= MAX_PRIMITIVES );
	Primitive & primitive = primitives[ num_primitives ];
	num_primitives++;
	return primitive;
}

static void DeletePrimitive( u32 entity_id ) {
	for( size_t i = 0; i < num_primitives; i++ ) {
		if( primitives[ i ].entity_id == entity_id ) {
			Swap2( &primitives[ i ], &primitives[ num_primitives - 1 ] );
			num_primitives--;
			return;
		}
	}
}

void LinkEntity( const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	MinMax3 bounds = EntityBounds( storage, ent );
	if( bounds.mins == MinMax3::Empty().mins && bounds.maxs == MinMax3::Empty().maxs )
		return;

	bounds.mins += ent->origin;
	bounds.maxs += ent->origin;

	Primitive & primitive = GetPrimitive( ent->number );
	primitive.entity_id = ent->number;
	primitive.bounds = bounds;
	primitive.center = ( bounds.maxs + bounds.mins ) * 0.5f;

	BuildBVH( storage );
}

void UnlinkEntity( const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	DeletePrimitive( ent->number );

	BuildBVH( storage );
}
