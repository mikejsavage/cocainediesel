#include "qcommon/base.h"
#include "gameshared/collision.h"

static inline u64 GetCellHash( s32 x, s32 y, s32 z ) {
	u64 hash;
	hash = Hash64( &x, sizeof( x ) );
	hash = Hash64( &y, sizeof( y ), hash );
	hash = Hash64( &z, sizeof( z ), hash );
	return hash;
}

static SpatialHashBounds GetSpatialHashBounds( MinMax3 bounds ) {
	constexpr s32 dimensions[] = { 64, 64, 1024 };

	SpatialHashBounds sbounds;

	sbounds.x1 = s32( bounds.mins.x ) / dimensions[ 0 ];
	sbounds.y1 = s32( bounds.mins.y ) / dimensions[ 1 ];
	sbounds.z1 = s32( bounds.mins.z ) / dimensions[ 2 ];

	sbounds.x2 = ( s32( bounds.maxs.x ) - 1 ) / dimensions[ 0 ] + 1;
	sbounds.y2 = ( s32( bounds.maxs.y ) - 1 ) / dimensions[ 1 ] + 1;
	sbounds.z2 = ( s32( bounds.maxs.z ) - 1 ) / dimensions[ 2 ] + 1;

	return sbounds;
}

template< bool Union >
static size_t TraverseSpatialHashGridGeneric( const SpatialHashGrid * a, const SpatialHashGrid * b, MinMax3 bounds, int * touchlist, SolidBits solid_mask ) {
	TracyZoneScoped;

	size_t num = 0;
	touchlist[ num++ ] = 0;
	SpatialHashBounds sbounds = GetSpatialHashBounds( bounds );
	SpatialHashCell result = { };

	for( s32 x = sbounds.x1; x <= sbounds.x2; x++ ) {
		for( s32 y = sbounds.y1; y <= sbounds.y2; y++ ) {
			for( s32 z = sbounds.z1; z <= sbounds.z2; z++ ) {
				u64 hash = GetCellHash( x, y, z );
				u64 cell_idx = hash % ARRAY_COUNT( a->cells );
				for( size_t i = 0; i < ARRAY_COUNT( &SpatialHashCell::active ); i++ ) {
					result.active[ i ] |= a->cells[ cell_idx ].active[ i ];
					if constexpr( Union ) {
						result.active[ i ] |= b->cells[ cell_idx ].active[ i ];
					}
				}
			}
		}
	}

	for( size_t i = 0; i < ARRAY_COUNT( &SpatialHashCell::active ); i++ ) {
		if( result.active[ i ] == 0 ) {
			continue;
		}
		for( size_t j = 0; j < 64; j++ ) {
			if( ( result.active[ i ] & ( 1ULL << j ) ) != 0 ) {
				size_t entity_id = i * 64 + j;
				bool touching = HasAnyBit( a->primitives[ entity_id ].solidity, solid_mask );
				if constexpr( Union ) {
					touching = touching || HasAnyBit( b->primitives[ entity_id ].solidity, solid_mask );
				}
				if( touching ) {
					touchlist[ num++ ] = entity_id;
				}
			}
		}
	}

	return num;
}

size_t TraverseSpatialHashGrid( const SpatialHashGrid * a, const SpatialHashGrid * b, MinMax3 bounds, int * touchlist, SolidBits solid_mask ) {
	return TraverseSpatialHashGridGeneric< true >( a, b, bounds, touchlist, solid_mask );
}

size_t TraverseSpatialHashGrid( const SpatialHashGrid * grid, const MinMax3 bounds, int * touchlist, const SolidBits solid_mask ) {
	return TraverseSpatialHashGridGeneric< false >( grid, NULL, bounds, touchlist, solid_mask );
}

void UnlinkEntity( SpatialHashGrid * grid, u64 entity_id ) {
	TracyZoneScoped;

	SpatialHashPrimitive primitive = grid->primitives[ entity_id ];
	grid->primitives[ entity_id ] = { };

	for( s32 x = primitive.sbounds.x1; x <= primitive.sbounds.x2; x++ ) {
		for( s32 y = primitive.sbounds.y1; y <= primitive.sbounds.y2; y++ ) {
			for( s32 z = primitive.sbounds.z1; z <= primitive.sbounds.z2; z++ ) {
				u64 hash = GetCellHash( x, y, z );
				u64 cell_idx = hash % ARRAY_COUNT( grid->cells );
				SpatialHashCell & cell = grid->cells[ cell_idx ];
				cell.active[ entity_id / 64 ] &= ~( 1ULL << ( entity_id % 64 ) );
			}
		}
	}
}

void LinkEntity( SpatialHashGrid * grid, const CollisionModelStorage * storage, const SyncEntityState * ent, u64 entity_id ) {
	TracyZoneScoped;

	UnlinkEntity( grid, entity_id );

	SolidBits solidity = EntitySolidity( storage, ent );
	if( solidity == Solid_NotSolid )
		return;

	MinMax3 bounds = EntityBounds( storage, ent );
	if( bounds == MinMax3::Empty() )
		return;

	bounds.mins += ent->origin;
	bounds.maxs += ent->origin;

	SpatialHashBounds sbounds = GetSpatialHashBounds( bounds );
	grid->primitives[ entity_id ].solidity = solidity;
	grid->primitives[ entity_id ].sbounds = sbounds;

	for( s32 x = sbounds.x1; x <= sbounds.x2; x++ ) {
		for( s32 y = sbounds.y1; y <= sbounds.y2; y++ ) {
			for( s32 z = sbounds.z1; z <= sbounds.z2; z++ ) {
				u64 hash = GetCellHash( x, y, 0 );
				u64 cell_idx = hash % ARRAY_COUNT( grid->cells );
				SpatialHashCell & cell = grid->cells[ cell_idx ];
				cell.active[ entity_id / 64 ] |= 1ULL << ( entity_id % 64 );
			}
		}
	}
}

void ClearSpatialHashGrid( SpatialHashGrid * grid ) {
	memset( grid->cells, 0, sizeof( grid->cells ) );
}
