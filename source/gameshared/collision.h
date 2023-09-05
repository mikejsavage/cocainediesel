#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "gameshared/cdmap.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/gs_synctypes.h"
#include "gameshared/q_collision.h"

struct GLTFCollisionBrush {
	u32 first_plane;
	u32 num_planes;
	u32 first_vertex;
	u32 num_vertices;
	SolidBits solidity;
};

struct GLTFCollisionData {
	MinMax3 bounds;
	SolidBits solidity;
	Span< Vec3 > vertices;
	Span< Plane > planes;
	Span< GLTFCollisionBrush > brushes;
};

struct MapSubModelCollisionData {
	StringHash base_hash;
	u32 sub_model;
};

struct MapSharedCollisionData {
	StringHash base_hash;
	MapData data;
};

struct CollisionModelStorage {
	static constexpr size_t MAX_GLTF_COLLISION_MODELS = 4096;

	GLTFCollisionData gltfs[ MAX_GLTF_COLLISION_MODELS ];
	Hashtable< MAX_GLTF_COLLISION_MODELS * 2 > gltfs_hashtable;

	static constexpr size_t MAX_MAPS = 128;
	static constexpr size_t MAX_MAP_MODELS = 4096;

	MapSharedCollisionData maps[ MAX_MAPS ];
	Hashtable< MAX_MAPS * 2 > maps_hashtable;

	MapSubModelCollisionData map_models[ MAX_MAP_MODELS ];
	Hashtable< MAX_MAP_MODELS * 2 > map_models_hashtable;
};

CollisionModel CollisionModelAABB( const MinMax3 & aabb );
CollisionModel CollisionModelGLTF( StringHash name );

void InitCollisionModelStorage( CollisionModelStorage * storage );
void ShutdownCollisionModelStorage( CollisionModelStorage * storage );

struct cgltf_data;
bool LoadGLTFCollisionData( CollisionModelStorage * storage, const cgltf_data * gltf, Span< const char > path, StringHash name );

const GLTFCollisionData * FindGLTFSharedCollisionData( const CollisionModelStorage * storage, StringHash name );

void LoadMapCollisionData( CollisionModelStorage * storage, const MapData * map, StringHash base_hash );

const MapSharedCollisionData * FindMapSharedCollisionData( const CollisionModelStorage * storage, StringHash name );
const MapSubModelCollisionData * FindMapSubModelCollisionData( const CollisionModelStorage * storage, StringHash name );

CollisionModel EntityCollisionModel( const CollisionModelStorage * storage, const SyncEntityState * ent );
MinMax3 EntityBounds( const CollisionModelStorage * storage, const SyncEntityState * ent );
SolidBits EntitySolidity( const CollisionModelStorage * storage, const SyncEntityState * ent );

trace_t MakeMissedTrace( const Ray & ray );
trace_t TraceVsEnt( const CollisionModelStorage * storage, const Ray & ray, const Shape & shape, const SyncEntityState * ent, SolidBits solid_mask );
bool EntityOverlap( const CollisionModelStorage * storage, const SyncEntityState * ent_a, const SyncEntityState * ent_b, SolidBits solid_mask );


constexpr size_t MAX_PRIMITIVES = MAX_EDICTS;
constexpr size_t SHG_GRID_SIZE = 64 * 64;
constexpr size_t SHG_CELL_DIMENSIONS[] = { 64, 64, 1024 };

struct SpatialHashBounds {
	s32 x1, x2;
	s32 y1, y2;
	s32 z1, z2;
};

struct SpatialHashPrimitive {
	SpatialHashBounds sbounds;
	SolidBits solidity;
};

struct SpatialHashCell {
	u64 active[ ( MAX_PRIMITIVES - 1 ) / 64 + 1 ];
};

struct SpatialHashGrid {
	SpatialHashCell cells[ SHG_GRID_SIZE ];
	SpatialHashPrimitive primitives[ MAX_PRIMITIVES ];
};

void LinkEntity( SpatialHashGrid * grid, const CollisionModelStorage * storage, const SyncEntityState * ent, u64 entity_id );
void UnlinkEntity( SpatialHashGrid * grid, u64 entity_id );
size_t TraverseSpatialHashGrid( const SpatialHashGrid * grid, MinMax3 bounds, int * arr, SolidBits solid_mask );
size_t TraverseSpatialHashGrid( const SpatialHashGrid * a, const SpatialHashGrid * b, MinMax3 bounds, int * touchlist, SolidBits solid_mask );
void ClearSpatialHashGrid( SpatialHashGrid * grid );
