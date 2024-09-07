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
#include "gameshared/q_math.h"

#include "cgltf/cgltf.h"

CollisionModel CollisionModelAABB( const MinMax3 & aabb ) {
	return CollisionModel {
		.type = CollisionModelType_AABB,
		.aabb = aabb,
	};
}

static void DeleteGLTFCollisionData( GLTFCollisionData data ) {
	Free( sys_allocator, data.vertices.ptr );
	Free( sys_allocator, data.planes.ptr );
	Free( sys_allocator, data.brushes.ptr );
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
			if( Dot( plane.normal, v ) - plane.distance > 0.1f ) {
				return false;
			}
		}
	}

	return true;
}

static Span< const u8 > AccessorToSpan( const cgltf_accessor * accessor ) {
	cgltf_size offset = accessor->offset + accessor->buffer_view->offset;
	return Span< const u8 >( ( const u8 * ) accessor->buffer_view->buffer->data + offset, accessor->count * accessor->stride );
}

static bool PlaneFrom3Points( Plane * plane, Vec3 a, Vec3 b, Vec3 c ) {
	Vec3 ab = b - a;
	Vec3 ac = c - a;

	Vec3 normal = SafeNormalize( Cross( ac, ab ) );
	if( normal == Vec3( 0.0f ) )
		return false;

	plane->normal = normal;
	plane->distance = Dot( a, normal );

	return true;
}

// like cgltf_load_buffers, but doesn't try to load URIs
bool LoadGLBBuffers( cgltf_data * data ) {
	if( data->buffers_count && data->buffers[0].data == NULL && data->buffers[0].uri == NULL && data->bin ) {
		if( data->bin_size < data->buffers[0].size )
			return false;
		data->buffers[0].data = const_cast< void * >( data->bin );
	}

	for( cgltf_size i = 0; i < data->buffers_count; i++ ) {
		if( data->buffers[i].data == NULL )
			return false;
	}

	return true;
}

bool LoadGLTFCollisionData( CollisionModelStorage * storage, const cgltf_data * gltf, Span< const char > path, StringHash name ) {
	NonRAIIDynamicArray< Vec3 > vertices( sys_allocator );
	NonRAIIDynamicArray< Plane > planes( sys_allocator );
	NonRAIIDynamicArray< GLTFCollisionBrush > brushes( sys_allocator );
	DynamicArray< size_t > brush_to_node_idx( sys_allocator );
	GLTFCollisionData data = { };

	for( size_t i = 0; i < gltf->nodes_count; i++ ) {
		const cgltf_node * node = &gltf->nodes[ i ];
		if( node->mesh == NULL )
			continue;

		const cgltf_primitive & prim = node->mesh->primitives[ 0 ];
		if( prim.material == NULL )
			continue;

		const EditorMaterial * material = FindEditorMaterial( StringHash( prim.material->name ) );
		if( material == NULL )
			continue;

		data.broadphase_solidity |= material->solidity;

		Mat4 transform;
		cgltf_node_transform_world( node, transform.ptr() );

		GLTFCollisionBrush brush = { };
		brush.first_plane = planes.size();
		brush.first_vertex = vertices.size();
		brush.solidity = material->solidity;

		Span< const Vec3 > gltf_verts;
		for( size_t j = 0; j < prim.attributes_count; j++ ) {
			const cgltf_attribute & attr = prim.attributes[ j ];
			if( attr.type == cgltf_attribute_type_position ) {
				gltf_verts = AccessorToSpan( attr.data ).cast< const Vec3 >();

				Vec3 min, max;
				for( int k = 0; k < 3; k++ ) {
					min[ k ] = attr.data->min[ k ];
					max[ k ] = attr.data->max[ k ];
				}
				data.bounds = Union( data.bounds, ( transform * Vec4( min, 1.0f ) ).xyz() );
				data.bounds = Union( data.bounds, ( transform * Vec4( max, 1.0f ) ).xyz() );
			}
		}

		constexpr Mat4 y_up_to_z_up(
			1, 0, 0, 0,
			0, 0, -1, 0,
			0, 1, 0, 0,
			0, 0, 0, 1
		);

		transform = y_up_to_z_up * transform;

		DynamicArray< Vec3 > brush_vertices( sys_allocator );
		for( Vec3 gltf_vert : gltf_verts ) {
			gltf_vert = ( transform * Vec4( gltf_vert, 1.0f ) ).xyz();
			bool found = false;
			for( Vec3 & vert : brush_vertices ) {
				if( Length( gltf_vert - vert ) < 0.01f ) {
					found = true;
					break;
				}
			}
			if( found ) {
				continue;
			}
			brush_vertices.add( gltf_vert );
			vertices.add( gltf_vert );
		}

		Span< const u8 > indices_data = AccessorToSpan( prim.indices );
		Assert( prim.indices->count % 3 == 0 );

		DynamicArray< Plane > brush_planes( sys_allocator );
		for( size_t j = 0; j < prim.indices->count; j += 3 ) {
			Vec3 a, b, c;
			if( prim.indices->component_type == cgltf_component_type_r_16u ) {
				a = gltf_verts[ indices_data.cast< const u16 >()[ j + 0 ] ];
				b = gltf_verts[ indices_data.cast< const u16 >()[ j + 1 ] ];
				c = gltf_verts[ indices_data.cast< const u16 >()[ j + 2 ] ];
			}
			else {
				a = gltf_verts[ indices_data.cast< const u32 >()[ j + 0 ] ];
				b = gltf_verts[ indices_data.cast< const u32 >()[ j + 1 ] ];
				c = gltf_verts[ indices_data.cast< const u32 >()[ j + 2 ] ];
			}

			a = ( transform * Vec4( a, 1.0f ) ).xyz();
			b = ( transform * Vec4( b, 1.0f ) ).xyz();
			c = ( transform * Vec4( c, 1.0f ) ).xyz();

			Plane plane;
			if( !PlaneFrom3Points( &plane, a, c, b ) ) {
				return false;
			}

			bool found = false;
			for( Plane & other_plane : brush_planes ) {
				if( Abs( plane.distance - other_plane.distance ) < 1.0f && Dot( plane.normal, other_plane.normal ) >= 0.9f ) {
					found = true;
					break;
				}
			}
			if( found ) {
				continue;
			}

			brush_planes.add( plane );
			planes.add( plane );
		}
		brush.num_planes = planes.size() - brush.first_plane;
		brush.num_vertices = vertices.size() - brush.first_vertex;

		brushes.add( brush );
		brush_to_node_idx.add( i );
	}

	if( brushes.size() == 0 ) {
		return false;
	}

	data.vertices = vertices.span();
	data.planes = planes.span();
	data.brushes = brushes.span();

	for( size_t i = 0; i < data.brushes.n; i++ ) {
		const GLTFCollisionBrush & brush = data.brushes[ i ];
		bool ok = true;
		if( !IsConvex( data, brush ) ) {
			Com_GGPrint( S_COLOR_YELLOW "{} has a concave collision brush: {}", path, gltf->nodes[ brush_to_node_idx[ i ] ].name );
			ok = false;
		}

		if( !ok ) {
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

static MinMax3 GLTFBounds( const GLTFCollisionData * gltf, const Mat3x4 & transform ) {
	MinMax3 bounds = MinMax3::Empty();
	for( const Vec3 & vert : gltf->vertices ) {
		bounds = Union( bounds, ( transform * Vec4( vert, 1.0f ) ).xyz() );
	}
	return bounds;
}

const GLTFCollisionData * FindGLTFSharedCollisionData( const CollisionModelStorage * storage, StringHash name ) {
	u64 idx;
	if( !storage->gltfs_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &storage->gltfs[ idx ];
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

CollisionModel EntityCollisionModel( const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	if( ent->override_collision_model.exists ) {
		return ent->override_collision_model.value;
	}

	if( ent->model == EMPTY_HASH ) {
		return { };
	}

	CollisionModel model = { };

	if( FindGLTFSharedCollisionData( storage, ent->model ) != NULL ) {
		model.type = CollisionModelType_GLTF;
		model.gltf_model = ent->model;
		return model;
	}

	if( FindMapSubModelCollisionData( storage, ent->model ) != NULL ) {
		model.type = CollisionModelType_MapModel;
		model.map_model = ent->model;
		return model;
	}

	return { };
}

MinMax3 EntityBounds( const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	CollisionModel model = EntityCollisionModel( storage, ent );

	if( model.type == CollisionModelType_MapModel ) {
		const MapSubModelCollisionData * map_model = FindMapSubModelCollisionData( storage, model.map_model );
		if( map_model == NULL )
			return MinMax3::Empty();
		const MapSharedCollisionData * map = FindMapSharedCollisionData( storage, map_model->base_hash );
		return map->data.models[ map_model->sub_model ].bounds;
	}

	if( model.type == CollisionModelType_GLTF ) {
		const GLTFCollisionData * gltf = FindGLTFSharedCollisionData( storage, model.gltf_model );
		if( gltf == NULL )
			return MinMax3::Empty();
		// Mat3x4 transform = Mat4Translation( ent->origin ) * Mat4Rotation( EulerDegrees3( ent->angles ) ) * Mat4Scale( ent->scale );
		Mat3x4 transform = Mat4Rotation( EulerDegrees3( ent->angles ) ) * Mat4Scale( ent->scale );
		return GLTFBounds( gltf, transform );
	}

	CollisionModelType type = model.type;
	Assert( type == CollisionModelType_Point || type == CollisionModelType_AABB );

	if( type == CollisionModelType_Point ) {
		return MinMax3( 0.0f );
	}

	return model.aabb;
}

SolidBits EntitySolidity( const CollisionModelStorage * storage, const SyncEntityState * ent ) {
	if( ent->solidity.exists ) {
		return ent->solidity.value;
	}

	CollisionModel model = EntityCollisionModel( storage, ent );
	if( model.type == CollisionModelType_MapModel ) {
		const MapSubModelCollisionData * map_model_data = FindMapSubModelCollisionData( storage, model.map_model );
		if( map_model_data == NULL )
			return Solid_NotSolid;
		const MapSharedCollisionData * map = FindMapSharedCollisionData( storage, map_model_data->base_hash );
		const MapModel * map_model = &map->data.models[ map_model_data->sub_model ];
		return map_model == NULL ? Solid_NotSolid : map_model->solidity;
	}
	else if( model.type == CollisionModelType_GLTF ) {
		const GLTFCollisionData * gltf = FindGLTFSharedCollisionData( storage, model.gltf_model );
		return gltf == NULL ? Solid_NotSolid : gltf->broadphase_solidity;
	}

	return Solid_NotSolid;
}

trace_t MakeMissedTrace( const Ray & ray ) {
	trace_t trace = { };
	trace.fraction = 1.0f;
	trace.endpos = ray.origin + ray.direction * ray.length;
	trace.ent = -1;
	trace.solidity = Solid_NotSolid;
	return trace;
}

static trace_t MakeTrace( const Ray & ray, const Shape & shape, const Intersection & intersection, const SyncEntityState * ent ) {
	trace_t trace = { };
	trace.fraction = ray.length == 0.0f ? 0.0f : intersection.t / ray.length;
	trace.normal = intersection.normal;
	trace.ent = ent->number;
	trace.solidity = ent->solidity.exists ? ent->solidity.value : intersection.solidity;
	trace.contact = ray.origin + ray.direction * intersection.t - trace.normal * Support( shape, -trace.normal );

	// step back endpos slightly so objects don't get stuck inside each other
	constexpr float epsilon = 1.0f / 32.0f;
	float stepped_back_t = intersection.t;
	if( intersection.normal != Vec3( 0.0f ) ) {
		stepped_back_t += epsilon / Dot( ray.direction, intersection.normal );
		stepped_back_t = Max2( stepped_back_t, 0.0f );
	}

	trace.endpos = ray.origin + ray.direction * stepped_back_t;

	return trace;
}

trace_t TraceVsEnt( const CollisionModelStorage * storage, const Ray & ray, const Shape & shape, const SyncEntityState * ent, SolidBits solid_mask ) {
	trace_t trace = MakeMissedTrace( ray );

	CollisionModel collision_model = EntityCollisionModel( storage, ent );

	SolidBits solidity = EntitySolidity( storage, ent );
	if( collision_model.type != CollisionModelType_MapModel && ( solidity & solid_mask ) == 0 )
		return trace;

	// TODO: what to do about points?
	if( collision_model.type == CollisionModelType_Point )
		return trace;

	// TODO: accomodate angles!!!!!!!!!
	Vec3 object_space_origin = ( ray.origin - ent->origin ) / ent->scale;
	Vec3 object_space_translation = ( ray.direction * ray.length ) / ent->scale;
	Ray object_space_ray = MakeRayOriginDirection( object_space_origin, SafeNormalize( object_space_translation ), Length( object_space_translation ) );

	if( collision_model.type == CollisionModelType_MapModel ) {
		const MapSubModelCollisionData * map_model = FindMapSubModelCollisionData( storage, collision_model.map_model );
		if( map_model == NULL )
			return trace;
		const MapSharedCollisionData * map = FindMapSharedCollisionData( storage, map_model->base_hash );

		Intersection intersection;
		if( SweptShapeVsMapModel( &map->data, &map->data.models[ map_model->sub_model ], object_space_ray, shape, solid_mask, &intersection ) ) {
			trace = MakeTrace( ray, shape, intersection, ent );
		}
	}
	else if( collision_model.type == CollisionModelType_GLTF ) {
		const GLTFCollisionData * gltf = FindGLTFSharedCollisionData( storage, collision_model.gltf_model );
		if( gltf == NULL )
			return trace;

		Mat3x4 transform = Mat4Translation( ent->origin ) * Mat4Rotation( EulerDegrees3( ent->angles ) ) * Mat4Scale( ent->scale );

		Intersection intersection;
		if( SweptShapeVsGLTF( gltf, transform, ray, shape, solid_mask, &intersection ) ) {
			trace = MakeTrace( ray, shape, intersection, ent );
		}
	}
	else if( shape.type == ShapeType_AABB ) {
		Assert( collision_model.type == CollisionModelType_AABB );

		MinMax3 object_space_aabb = ToMinMax( shape.aabb );
		object_space_aabb += object_space_origin;

		Intersection intersection;
		if( SweptAABBVsAABB( object_space_aabb, ray.direction * ray.length, collision_model.aabb, Vec3( 0.0f ), &intersection ) ) {
			intersection.t *= ray.length; // TODO: make this consistent with the rest...
			trace = MakeTrace( ray, shape, intersection, ent );
		}
	}
	else {
		Assert( shape.type == ShapeType_Ray );

		switch( collision_model.type ) {
			case CollisionModelType_Point:
				break;

			case CollisionModelType_AABB: {
				Intersection enter, leave;
				if( RayVsAABB( object_space_ray, collision_model.aabb, &enter, &leave ) ) {
					enter.solidity = solidity;
					trace = MakeTrace( ray, shape, enter, ent );
				}
			} break;

			case CollisionModelType_Sphere: {
				float t;
				if( RayVsSphere( object_space_ray, collision_model.sphere, &t ) ) {
					trace = MakeTrace( ray, shape, { t }, ent );
				}
			} break;

			case CollisionModelType_Capsule: {
				float t;
				if( RayVsCapsule( object_space_ray, collision_model.capsule, &t ) ) {
					trace = MakeTrace( ray, shape, { t }, ent );
				}
			} break;
		}
	}

	return trace;
}

bool EntityOverlap( const CollisionModelStorage * storage, const SyncEntityState * ent_a, const SyncEntityState * ent_b, SolidBits solid_mask ) {
	CollisionModel collision_model_a = EntityCollisionModel( storage, ent_a );
	CollisionModel collision_model_b = EntityCollisionModel( storage, ent_b );

	// TODO: super hacky, please fix
	if( collision_model_a.type > collision_model_b.type ) {
		Swap2( &collision_model_a, &collision_model_b );
		Swap2( &ent_a, &ent_b );
	}

	Ray ray = MakeRayStartEnd( ent_a->origin, ent_a->origin );
	Shape shape = { };
	if( collision_model_a.type == CollisionModelType_Point ) {
		shape.type = ShapeType_Ray;
	}
	else if( collision_model_a.type == CollisionModelType_AABB ) {
		shape.type = ShapeType_AABB;
		shape.aabb = ToCenterExtents( collision_model_a.aabb );
	}
	else {
		Assert( false );
	}

	Vec3 object_space_origin = ( ent_a->origin - ent_b->origin ) / ent_b->scale;
	Vec3 object_space_translation = ( ray.direction * ray.length ) / ent_b->scale;
	Ray object_space_ray = MakeRayOriginDirection( object_space_origin, SafeNormalize( object_space_translation ), Length( object_space_translation ) );

	if( collision_model_b.type == CollisionModelType_MapModel ) {
		const MapSubModelCollisionData * map_model = FindMapSubModelCollisionData( storage, collision_model_b.map_model );
		if( map_model == NULL )
			return false;
		const MapSharedCollisionData * map = FindMapSharedCollisionData( storage, map_model->base_hash );

		Intersection intersection;
		return SweptShapeVsMapModel( &map->data, &map->data.models[ map_model->sub_model ], object_space_ray, shape, solid_mask, &intersection );
	}
	else if( collision_model_b.type == CollisionModelType_GLTF ) {
		const GLTFCollisionData * gltf = FindGLTFSharedCollisionData( storage, collision_model_b.gltf_model );
		if( gltf == NULL )
			return false;

		Mat3x4 transform = Mat4Translation( ent_b->origin ) * Mat4Rotation( EulerDegrees3( ent_b->angles ) ) * Mat4Scale( ent_b->scale );

		Intersection intersection;
		return SweptShapeVsGLTF( gltf, transform, ray, shape, solid_mask, &intersection );
	}
	else if( collision_model_b.type == CollisionModelType_AABB ) {
		Assert( shape.type == ShapeType_AABB );

		MinMax3 object_space_aabb = ToMinMax( shape.aabb );
		object_space_aabb += object_space_origin;

		Intersection intersection;
		return SweptAABBVsAABB( object_space_aabb, ray.direction * ray.length, collision_model_b.aabb, Vec3( 0.0f ), &intersection );
	}
	else {
		Assert( false );
	}

	return false;
}
