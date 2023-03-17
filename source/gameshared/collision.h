#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "gameshared/cdmap.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/gs_synctypes.h"
#include "gameshared/q_collision.h"

#include <functional>

struct GLTFCollisionBrush {
	u32 first_plane;
	u32 num_planes;
	u32 first_vertex;
	u32 num_vertices;
	SolidBits solidity;
};

struct GLTFCollisionData {
	MinMax3 bounds;
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

CollisionModel EntityCollisionModel( const SyncEntityState * ent );
MinMax3 EntityBounds( const CollisionModelStorage * storage, const SyncEntityState * ent );

trace_t MakeMissedTrace( const Ray & ray );
trace_t TraceVsEnt( const CollisionModelStorage * storage, const Ray & ray, const Shape & shape, const SyncEntityState * ent, SolidBits solid_mask );



constexpr size_t MAX_PRIMITIVES = MAX_EDICTS;
constexpr size_t BVH_NODE_SIZE = 2;
constexpr size_t BVH_NODES = ( MAX_PRIMITIVES - 1 ) / ( BVH_NODE_SIZE - 1 );

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

struct BVH {
	BVHNode nodes[ BVH_NODES ];
	Primitive primitives[ MAX_PRIMITIVES ];
	size_t num_primitives;
};

void LinkEntity( BVH * bvh, const CollisionModelStorage * storage, const SyncEntityState * ent );
void PrepareEntity( BVH * bvh, const CollisionModelStorage * storage, const SyncEntityState * ent );
void BuildBVH( BVH * bvh, const CollisionModelStorage * storage );
void UnlinkEntity( BVH * bvh, const CollisionModelStorage * storage, const SyncEntityState * ent );

void TraverseBVH( BVH * bvh, MinMax3 bounds, std::function< void ( u32 entity, u32 total ) > callback );
void ClearBVH( BVH * bvh );
