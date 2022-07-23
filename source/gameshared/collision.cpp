#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/string.h"
#include "gameshared/cdmap.h"
#include "gameshared/collision.h"
#include "gameshared/editor_materials.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/gs_synctypes.h"
#include "gameshared/q_collision.h"

#include "cgltf/cgltf.h"

CollisionModel CollisionModelAABB( const MinMax3 & aabb ) {
	CollisionModel model = { };
	model.type = CollisionModelType_AABB;
	model.aabb = aabb;
	return model;
}

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

template< typename T >
Span< T > DedupeSorted( Allocator * a, Span< T > sorted ) {
	NonRAIIDynamicArray< T > deduped( a );

	for( size_t i = 1; i < sorted.n; i++ ) {
		if( sorted[ i ] != sorted[ i - 1 ] ) {
			deduped.add( sorted );
		}
	}

	return deduped.span();
}

bool LoadGLTFCollisionData( CollisionModelStorage * storage, const cgltf_data * gltf, const char * path, StringHash name ) {
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
	if( !storage->gltfs_hashtable.get( name.hash, &idx ) ) {
		storage->gltfs_hashtable.add( name.hash, storage->gltfs_hashtable.size() );
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
			u64 hash = Hash64( suffix.c_str(), suffix.length(), map->base_hash.hash );

			if( storage->map_models_hashtable.size() == ARRAY_COUNT( storage->map_models ) ) {
				Fatal( "Too many map submodels" );
			}

			storage->map_models[ storage->map_models_hashtable.size() ] = {
				StringHash( map->base_hash ),
				checked_cast< u32 >( j )
			};
			storage->map_models_hashtable.add( hash, storage->map_models_hashtable.size() );
		}
	}
}

void LoadMapCollisionData( CollisionModelStorage * storage, const MapData * data, StringHash base_hash ) {
	TracyZoneScoped;

	u64 idx = storage->maps_hashtable.size();
	if( !storage->maps_hashtable.get( base_hash.hash, &idx ) ) {
		storage->maps_hashtable.add( base_hash.hash, storage->maps_hashtable.size() );
	}

	if( idx == ARRAY_COUNT( storage->maps ) ) {
		Fatal( "Too many maps" );
	}

	MapSharedCollisionData map;
	map.base_hash = base_hash;
	map.data = *data;
	storage->maps[ idx ] = map;

	FillMapModelsHashtable( storage );
}

const MapSharedCollisionData * FindMapSharedCollisionData( const CollisionModelStorage * storage, StringHash name ) {
	u64 idx;
	if( !storage->maps_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &storage->maps[ idx ];
}

const MapSubModelCollisionData * FindMapSubModelCollisionData( const CollisionModelStorage * storage, StringHash name ) {
	u64 idx;
	if( !storage->map_models_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &storage->map_models[ idx ];
}

CollisionModel EntityCollisionModel( const SyncEntityState * ent ) {
	if( ent->override_collision_model.exists ) {
		return ent->override_collision_model.value;
	}

	CollisionModel model = { };
	model.type = CollisionModelType_MapModel;
	model.map_model = ent->model;
	return model;
}

MinMax3 EntityBounds( const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	CollisionModel model = EntityCollisionModel( ent );

	if( model.type == CollisionModelType_MapModel ) {
		const MapSubModelCollisionData * map_model = FindMapSubModelCollisionData( storage, model.map_model );
		if( map_model == NULL )
			return MinMax3::Empty();
		const MapSharedCollisionData * map = FindMapSharedCollisionData( storage, map_model->base_hash );
		return map->data.models[ map_model->sub_model ].bounds;
	}

	CollisionModelType type = model.type;
	assert( type == CollisionModelType_Point || type == CollisionModelType_AABB );

	if( type == CollisionModelType_Point ) {
		return MinMax3( Vec3( 0.0f ), Vec3( 0.0f ) );
	}

	return model.aabb;
}

trace_t MakeMissedTrace( const Ray & ray ) {
	trace_t trace = { };
	trace.fraction = 1.0f;
	trace.endpos = ray.origin + ray.direction * ray.length;
	trace.ent = -1;
	return trace;
}

static trace_t FUCKING_HELL( const Ray & ray, const Shape & shape, const Intersection & intersection, int ent ) {
	trace_t trace = { };
	trace.fraction = ray.length == 0.0f ? 1.0f : intersection.t / ray.length;
	trace.ent = ent;

	Vec3 plane_reference_point = ray.origin + ray.direction * ray.length * trace.fraction;
	plane_reference_point += Support( shape, -intersection.normal );
	trace.plane = PlaneFromNormalAndPoint( intersection.normal, plane_reference_point );

	constexpr float epsilon = 1.0f / 32.0f;
	if( intersection.normal != Vec3( 0.0f ) ) {
		trace.fraction += epsilon / Dot( ray.direction, intersection.normal );
		trace.fraction = Max2( trace.fraction, 0.0f );
	}
	else {
		// TODO: check this is consistent with the old code
		trace.fraction = Max2( trace.fraction - epsilon, 0.0f );
	}

	trace.endpos = ray.origin + ray.direction * ray.length * trace.fraction;

	return trace;
}

trace_t TraceVsEnt( const CollisionModelStorage * storage, const Ray & ray, const Shape & shape, const SyncEntityState * ent ) {
	trace_t trace = MakeMissedTrace( ray );

	CollisionModel collision_model = EntityCollisionModel( ent );

	if( ent->type != ET_PLAYER ) {
		for( int i = 0; i < 3; i++ ) {
			assert( PositiveMod( ent->angles[ i ], 90.0f ) == 0.0f );
		}
	}

	// TODO: accomodate angles!!!!!!!!!
	Vec3 object_space_origin = ( ray.origin - ent->origin ) / ent->scale;
	Vec3 object_space_translation = ( ray.direction * ray.length ) / ent->scale;
	Ray object_space_ray = MakeRayOriginDirection( object_space_origin, SafeNormalize( object_space_translation ), Length( object_space_translation ) );

	if( collision_model.type == CollisionModelType_MapModel ) {
		const MapSubModelCollisionData * map_model = FindMapSubModelCollisionData( storage, collision_model.map_model );
		if( map_model == NULL )
			return trace;
		const MapSharedCollisionData * map = FindMapSharedCollisionData( storage, map_model->base_hash );

		if( break2 ) __debugbreak();
		Intersection intersection;
		if( SweptShapeVsMapModel( &map->data, &map->data.models[ map_model->sub_model ], object_space_ray, shape, &intersection ) ) {
			trace = FUCKING_HELL( ray, shape, intersection, ent->number );
		}
	}
	else if( shape.type == ShapeType_AABB ) {
		assert( collision_model.type == CollisionModelType_AABB );

		MinMax3 object_space_aabb = ToMinMax( shape.aabb );
		object_space_aabb.mins += object_space_origin;
		object_space_aabb.maxs += object_space_origin;

		Intersection intersection;
		if( SweptAABBVsAABB( object_space_aabb, ray.direction * ray.length, collision_model.aabb, Vec3( 0.0f ), &intersection ) ) {
			intersection.t *= ray.length; // TODO: make this consistent with the rest...
			trace = FUCKING_HELL( ray, shape, intersection, ent->number );
		}
	}
	else {
		assert( shape.type == ShapeType_Ray );

		switch( collision_model.type ) {
			case CollisionModelType_Point:
				break;

			case CollisionModelType_AABB: {
				Intersection enter, leave;
				if( RayVsAABB( object_space_ray, collision_model.aabb, &enter, &leave ) ) {
					trace = FUCKING_HELL( ray, shape, enter, ent->number );
				}
			} break;

			case CollisionModelType_Sphere: {
				float t;
				if( RayVsSphere( object_space_ray, collision_model.sphere, &t ) ) {
					trace = FUCKING_HELL( ray, shape, { t }, ent->number );
				}
			} break;

			case CollisionModelType_Capsule: {
				float t;
				if( RayVsCapsule( object_space_ray, collision_model.capsule, &t ) ) {
					trace = FUCKING_HELL( ray, shape, { t }, ent->number );
				}
			} break;
		}
	}

	return trace;
}
