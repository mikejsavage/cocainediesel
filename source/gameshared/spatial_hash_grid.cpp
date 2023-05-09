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

void TraverseSpatialHashGrid( SpatialHashGrid * grid, MinMax3 bounds, std::function< void ( u32 entity ) > callback ) {
	TracyZoneScoped;

	callback( 0 );
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
				callback( i * 64 + j );
			}
		}
	}
}

size_t TraverseSpatialHashGridArr( SpatialHashGrid * grid, MinMax3 bounds, u64 * touchlist ) {
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
				touchlist[ num++ ] = i * 64 + j;
			}
		}
	}
	return num;
}

void UnlinkEntity( SpatialHashGrid * grid, const CollisionModelStorage * storage, const SyncEntityState * ent, const u64 entity_id ) {
	TracyZoneScoped;

	SpatialHashBounds sbounds = grid->sbounds[ entity_id ];
	grid->sbounds[ entity_id ] = { 0 };

	for( s32 x = sbounds.x1; x <= sbounds.x2; x++ ) {
		for( s32 y = sbounds.y1; y <= sbounds.y2; y++ ) {
			for( s32 z = sbounds.z1; z <= sbounds.z2; z++ ) {
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

	UnlinkEntity( grid, storage, ent, entity_id );

	MinMax3 bounds = EntityBounds( storage, ent );
	if( bounds.mins == MinMax3::Empty().mins && bounds.maxs == MinMax3::Empty().maxs )
		return;

	bounds.mins += ent->origin;
	bounds.maxs += ent->origin;

	SpatialHashBounds sbounds = GetSpatialHashBounds( bounds );
	grid->sbounds[ entity_id ] = sbounds;

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
