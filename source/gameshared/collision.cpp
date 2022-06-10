#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/string.h"
#include "gameshared/cdmap.h"
#include "gameshared/collision.h"
#include "gameshared/editor_materials.h"
#include "gameshared/q_collision.h"

#include "cgltf/cgltf.h"

static void DeleteGLTFCollisionData( GLTFCollisionData data ) {
	FREE( sys_allocator, data.vertices.ptr );
	FREE( sys_allocator, data.planes.ptr );
	FREE( sys_allocator, data.brushes.ptr );
}

void InitCollisionModelStorage( CollisionModelStorage * storage ) {
	storage->gltfs_hashtable.clear();
	storage->maps_hashtable.clear();
	storage->map_models_hashtable.clear();
}

void ShutdownCollisionModelStorage( CollisionModelStorage * storage ) {
	for( size_t i = 0; i < storage->gltfs_hashtable.size(); i++ ) {
		DeleteGLTFCollisionData( storage->gltfs[ i ] );
	}
}

static bool IsConvex( GLTFCollisionData data, GLTFCollisionBrush brush ) {
	for( u32 i = 0; i < brush.num_planes; i++ ) {
		Plane plane = data.planes[ brush.first_plane + i ];
		for( u32 j = 0; j < brush.num_vertices; j++ ) {
			Vec3 v = data.vertices[ brush.first_vertex + j ];
			// TODO: probably needs epsilon
			if( Dot( plane.normal, v ) - plane.distance > 0 ) {
				return false;
			}
		}
	}

	return true;
}

bool LoadGLTFCollisionData( CollisionModelStorage * storage, const cgltf_data * gltf, const char * path, u64 hash ) {
	NonRAIIDynamicArray< Vec3 > vertices( sys_allocator );
	NonRAIIDynamicArray< Plane > planes( sys_allocator );
	NonRAIIDynamicArray< GLTFCollisionBrush > brushes( sys_allocator );

	for( size_t i = 0; i < gltf->nodes_count; i++ ) {
		const cgltf_node * node = &gltf->nodes[ i ];
		if( node->mesh == NULL )
			continue;

		const cgltf_primitive * prim = &node->mesh->primitives[ 0 ];
		if( prim->material == NULL )
			continue;

		const EditorMaterial * material = FindEditorMaterial( StringHash( prim->material->name ) );
		if( material == NULL )
			continue;

		Mat4 transform;
		cgltf_node_transform_world( node, transform.ptr() );

		// TODO: make post-transform hull (deduped verts)
		// TODO: make planes from post-transform triangles
		// TODO: sort by planes and merge similar
	}

	if( brushes.size() == 0 ) {
		return true;
	}

	GLTFCollisionData data;
	data.vertices = vertices.span();
	data.planes = planes.span();
	data.brushes = brushes.span();

	for( GLTFCollisionBrush brush : data.brushes ) {
		if( !IsConvex( data, brush ) ) {
			DeleteGLTFCollisionData( data );
			return false;
		}
	}

	u64 idx = storage->gltfs_hashtable.size();
	if( !storage->gltfs_hashtable.get( hash, &idx ) ) {
		storage->gltfs_hashtable.add( hash, storage->gltfs_hashtable.size() );
	}
	else {
		DeleteGLTFCollisionData( storage->gltfs[ idx ] );
	}

	storage->gltfs[ idx ] = data;

	return true;
}

static void FillMapModelsHashtable( CollisionModelStorage * storage ) {
	TracyZoneScoped;

	storage->map_models_hashtable.clear();

	for( u32 i = 0; i < storage->maps_hashtable.size(); i++ ) {
		const MapSharedCollisionData * map = &storage->maps[ i ];
		for( size_t j = 0; j < map->data.models.n; j++ ) {
			String< 16 > suffix( "*{}", j );
			u64 hash = Hash64( suffix.c_str(), suffix.length(), map->base_hash );

			assert( storage->map_models_hashtable.size() < ARRAY_COUNT( storage->map_models ) ); // TODO: check in release builds too
			storage->map_models[ storage->map_models_hashtable.size() ] = {
				StringHash( map->base_hash ),
				checked_cast< u32 >( j )
			};
			storage->map_models_hashtable.add( hash, storage->map_models_hashtable.size() );
		}
	}
}

void LoadMapCollisionData( CollisionModelStorage * storage, const MapData * data, u64 hash ) {
	TracyZoneScoped;

	u64 idx = storage->maps_hashtable.size();
	if( !storage->maps_hashtable.get( hash, &idx ) ) {
		storage->maps_hashtable.add( hash, storage->maps_hashtable.size() );
	}

	MapSharedCollisionData map;
	map.base_hash = hash;
	map.data = *data;
	storage->maps[ idx ] = map;

	FillMapModelsHashtable( storage );
}
