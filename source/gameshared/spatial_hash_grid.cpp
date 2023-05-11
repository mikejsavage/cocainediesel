#include "qcommon/base.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/q_math.h"

#include "qcommon/hashtable.h"

static inline u64 GetCellHash( s32 x, s32 y, s32 z ) {
	u64 hash;
	hash = Hash64( &x, sizeof( x ) );
	hash = Hash64( &y, sizeof( y ), hash );
	hash = Hash64( &z, sizeof( z ), hash );
	return hash;
}

static SpatialHashBounds GetSpatialHashBounds( MinMax3 bounds ) {
	SpatialHashBounds sbounds;

	sbounds.x1 = s32( bounds.mins.x ) / SHG_CELL_DIMENSIONS[ 0 ];
	sbounds.y1 = s32( bounds.mins.y ) / SHG_CELL_DIMENSIONS[ 1 ];
	sbounds.z1 = s32( bounds.mins.z ) / SHG_CELL_DIMENSIONS[ 2 ];

	sbounds.x2 = ( s32( bounds.maxs.x ) - 1 ) / SHG_CELL_DIMENSIONS[ 0 ] + 1;
	sbounds.y2 = ( s32( bounds.maxs.y ) - 1 ) / SHG_CELL_DIMENSIONS[ 1 ] + 1;
	sbounds.z2 = ( s32( bounds.maxs.z ) - 1 ) / SHG_CELL_DIMENSIONS[ 2 ] + 1;

	return sbounds;
}

size_t TraverseSpatialHashGrid( const SpatialHashGrid * grid, const MinMax3 bounds, int * touchlist, const SolidBits solidity ) {
	TracyZoneScoped;

	size_t num = 0;
	touchlist[ num++ ] = 0;
	SpatialHashBounds sbounds = GetSpatialHashBounds( bounds );
	SpatialHashCell result = { 0 };

	for( s32 x = sbounds.x1; x <= sbounds.x2; x++ ) {
		for( s32 y = sbounds.y1; y <= sbounds.y2; y++ ) {
			for( s32 z = sbounds.z1; z <= sbounds.z2; z++ ) {
				u64 hash = GetCellHash( x, y, 0 );
				u64 cell_idx = hash % SHG_GRID_SIZE;
				for( size_t i = 0; i < ARRAY_COUNT( &SpatialHashCell::active ); i++ ) {
					result.active[ i ] |= grid->cells[ cell_idx ].active[ i ];
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
				if( ( grid->primitives[ entity_id ].solidity & solidity ) != 0 )
					touchlist[ num++ ] = entity_id;
			}
		}
	}
	return num;
}

void UnlinkEntity( SpatialHashGrid * grid, const u64 entity_id ) {
	TracyZoneScoped;

	SpatialHashPrimitive primitive = grid->primitives[ entity_id ];
	grid->primitives[ entity_id ] = { 0 };

	for( s32 x = primitive.sbounds.x1; x <= primitive.sbounds.x2; x++ ) {
		for( s32 y = primitive.sbounds.y1; y <= primitive.sbounds.y2; y++ ) {
			for( s32 z = primitive.sbounds.z1; z <= primitive.sbounds.z2; z++ ) {
				u64 hash = GetCellHash( x, y, 0 );
				u64 cell_idx = hash % SHG_GRID_SIZE;
				SpatialHashCell & cell = grid->cells[ cell_idx ];
				cell.active[ entity_id / 64 ] &= ~( 1ULL << ( entity_id % 64 ) );
			}
		}
	}
}


void LinkEntity( SpatialHashGrid * grid, const CollisionModelStorage * storage, const SyncEntityState * ent, const u64 entity_id ) {
	TracyZoneScoped;

	UnlinkEntity( grid, entity_id );

	SolidBits solidity = EntitySolidity( storage, ent );
	if( solidity == Solid_NotSolid )
		return;

	MinMax3 bounds = EntityBounds( storage, ent );
	if( bounds.mins == MinMax3::Empty().mins && bounds.maxs == MinMax3::Empty().maxs )
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
				u64 cell_idx = hash % SHG_GRID_SIZE;
				SpatialHashCell & cell = grid->cells[ cell_idx ];
				cell.active[ entity_id / 64 ] |= 1ULL << ( entity_id % 64 );
			}
		}
	}
}

void ClearSpatialHashGrid( SpatialHashGrid * grid ) {
	memset( grid->cells, 0, sizeof( grid->cells ) );
}
