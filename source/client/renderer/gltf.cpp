#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/hash.h"
#include "client/renderer/renderer.h"
#include "cgame/ref.h"

#include "cgltf/cgltf.h"

// like cgltf_load_buffers, but doesn't try to load URIs
static bool LoadBinaryBuffers( cgltf_data * data ) {
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

static Span< const u8 > AccessorToSpan( const cgltf_accessor * accessor ) {
	cgltf_size offset = accessor->offset + accessor->buffer_view->offset;
	return Span< const u8 >( ( const u8 * ) accessor->buffer_view->buffer->data + offset, accessor->count * accessor->stride );
}

static u8 GetJointIdx( const cgltf_node * node ) {
	return u8( uintptr_t( node->camera ) - 1 );
}

static void SetJointIdx( cgltf_node * node, u8 joint_idx ) {
	node->camera = ( cgltf_camera * ) uintptr_t( joint_idx + 1 );
}

static void LoadJoint( Model * model, const cgltf_skin * skin, const cgltf_node * node, u8 ** prev ) {
	u8 joint_idx = GetJointIdx( node );
	**prev = joint_idx;
	*prev = &model->joints[ joint_idx ].next;

	Model::Joint & joint = model->joints[ joint_idx ];
	joint.parent = node->parent != NULL ? GetJointIdx( node->parent ) : U8_MAX;
	joint.name = Hash32( node->name );

	cgltf_bool ok = cgltf_accessor_read_float( skin->inverse_bind_matrices, joint_idx, joint.joint_to_bind.ptr(), 16 );
	assert( ok != 0 );

	for( size_t i = 0; i < node->children_count; i++ ) {
		LoadJoint( model, skin, node->children[ i ], prev );
	}

	// TODO: remove with additive animations
	if( node->children_count == 0 ) {
		joint.first_child = U8_MAX;
	}
	else {
		joint.first_child = GetJointIdx( node->children[ 0 ] );

		for( size_t i = 0; i < node->children_count - 1; i++ ) {
			model->joints[ GetJointIdx( node->children[ i ] ) ].sibling = GetJointIdx( node->children[ i + 1 ] );
		}

		model->joints[ GetJointIdx( node->children[ node->children_count - 1 ] ) ].sibling = U8_MAX;
	}
}

static void LoadSkin( Model * model, const cgltf_node * root, const cgltf_skin * skin, Mat4 * transform ) {
	model->num_joints = skin->joints_count;
	if( skin->joints_count == 0 )
		return;

	model->joints = ALLOC_MANY( sys_allocator, Model::Joint, skin->joints_count );
	memset( model->joints, 0, sizeof( Model::Joint ) * skin->joints_count );

	u8 * prev_ptr = &model->root_joint;
	LoadJoint( model, skin, root, &prev_ptr );

	if( root->parent != NULL ) {
		Mat4 root_transform;
		cgltf_node_transform_local( root->parent, root_transform.ptr() );
		*transform = root_transform * *transform;
	}

	// TODO: remove with additive animations
	model->joints[ GetJointIdx( root ) ].sibling = U8_MAX;
}

static bool FindRootJoint( cgltf_skin * skin, cgltf_node ** root ) {
	for( size_t i = 0; i < skin->joints_count; i++ ) {
		SetJointIdx( skin->joints[ i ], i );
	}

	for( size_t i = 0; i < skin->joints_count; i++ ) {
		cgltf_node * joint = skin->joints[ i ];
		if( joint->parent == NULL || GetJointIdx( joint->parent ) == U8_MAX ) {
			if( *root != NULL )
				return false;
			*root = joint;
		}
	}

	return true;
}

static MinMax3 Extend( MinMax3 bounds, Vec3 p ) {
	return MinMax3(
		Vec3( Min2( bounds.mins.x, p.x ), Min2( bounds.mins.y, p.y ), Min2( bounds.mins.z, p.z ) ),
		Vec3( Max2( bounds.maxs.x, p.x ), Max2( bounds.maxs.y, p.y ), Max2( bounds.maxs.z, p.z ) )
	);
}

static void LoadNode( Model * model, const cgltf_node * node, bool animated ) {
	for( size_t i = 0; i < node->children_count; i++ ) {
		LoadNode( model, node->children[ i ], animated );
	}

	if( node->mesh == NULL ) {
		return;
	}

	const cgltf_primitive & prim = node->mesh->primitives[ 0 ];

	MeshConfig mesh_config;

	for( size_t i = 0; i < prim.attributes_count; i++ ) {
		const cgltf_attribute & attr = prim.attributes[ i ];

		if( attr.type == cgltf_attribute_type_position ) {
			Span< const Vec3 > positions = AccessorToSpan( attr.data ).cast< const Vec3 >();
			mesh_config.num_vertices = positions.n;
			if( animated ) {
				mesh_config.positions = NewVertexBuffer( positions );
				for( Vec3 p : positions ) {
					model->bounds = Extend( model->bounds, p );
				}
			}
			else {
				Mat4 node_transform;
				cgltf_node_transform_local( node, node_transform.ptr() );

				Span< Vec3 > transformed = ALLOC_SPAN( sys_allocator, Vec3, positions.n );
				for( size_t j = 0; j < positions.n; j++ ) {
					transformed[ j ] = ( node_transform * Vec4( positions[ j ], 1.0f ) ).xyz();
					model->bounds = Extend( model->bounds, transformed[ j ] );
				}
				mesh_config.positions = NewVertexBuffer( transformed );
				FREE( sys_allocator, transformed.ptr );
			}
		}

		if( attr.type == cgltf_attribute_type_normal ) {
			mesh_config.normals = NewVertexBuffer( AccessorToSpan( attr.data ) );
		}

		if( attr.type == cgltf_attribute_type_texcoord ) {
			mesh_config.tex_coords = NewVertexBuffer( AccessorToSpan( attr.data ) );
		}

		if( attr.type == cgltf_attribute_type_joints ) {
			Span< u16 > joints_u16 = AccessorToSpan( attr.data ).cast< u16 >();
			Span< u8 > joints_u8 = ALLOC_SPAN( sys_allocator, u8, attr.data->count * 4 );
			for( size_t j = 0; j < joints_u16.n; j++ ) {
				joints_u8[ j ] = checked_cast< u8 >( joints_u16[ j ] );
			}
			mesh_config.joints = NewVertexBuffer( joints_u8 );
			mesh_config.joints_format = VertexFormat_U8x4;
			FREE( sys_allocator, joints_u8.ptr );
		}

		if( attr.type == cgltf_attribute_type_weights ) {
			Span< float > weights_float = AccessorToSpan( attr.data ).cast< float >();
			Span< u8 > weights_u8 = ALLOC_SPAN( sys_allocator, u8, attr.data->count * 4 );
			for( size_t k = 0; k < weights_float.n; k++ ) {
				weights_u8[ k ] = weights_float[ k ] * 255;
			}
			mesh_config.weights = NewVertexBuffer( weights_u8 );
			mesh_config.weights_format = VertexFormat_U8x4_Norm;
			FREE( sys_allocator, weights_u8.ptr );
		}
	}

	mesh_config.indices = NewIndexBuffer( AccessorToSpan( prim.indices ) );
	mesh_config.indices_format = prim.indices->component_type == cgltf_component_type_r_16u ? IndexFormat_U16 : IndexFormat_U32;
	mesh_config.num_vertices = prim.indices->count;
	mesh_config.ccw_winding = true;

	Model::Primitive * primitive = &model->primitives[ model->num_primitives ];
	model->num_primitives++;

	primitive->mesh = NewMesh( mesh_config );
	primitive->first_index = 0;
	primitive->num_vertices = 0;

	const char * material_name = prim.material != NULL ? prim.material->name : "***r_notexture***";
	primitive->material = FindMaterial( material_name );
}

template< typename T >
static void LoadChannel( const cgltf_animation_channel * chan, Model::AnimationChannel< T > * out_channel ) {
	constexpr size_t lanes = sizeof( T ) / sizeof( float );
	size_t n = chan->sampler->input->count;

	float * memory = ALLOC_MANY( sys_allocator, float, n * ( lanes + 1 ) );
	out_channel->times = memory;
	out_channel->samples = ( T * ) ( memory + n );
	out_channel->num_samples = n;

	for( size_t i = 0; i < n; i++ ) {
		cgltf_bool ok = cgltf_accessor_read_float( chan->sampler->input, i, &out_channel->times[ i ], 1 );
		ok = ok && cgltf_accessor_read_float( chan->sampler->output, i, out_channel->samples[ i ].ptr(), lanes );
		assert( ok != 0 );
	}
}

static void LoadScaleChannel( const cgltf_animation_channel * chan, Model::AnimationChannel< float > * out_channel ) {
	size_t n = chan->sampler->input->count;

	float * memory = ALLOC_MANY( sys_allocator, float, n * 2 );
	out_channel->times = memory;
	out_channel->samples = memory + n;
	out_channel->num_samples = n;

	for( size_t i = 0; i < n; i++ ) {
		cgltf_accessor_read_float( chan->sampler->input, i, &out_channel->times[ i ], 1 );

		float scale[ 3 ];
		cgltf_accessor_read_float( chan->sampler->output, i, scale, 3 );

		assert( Abs( scale[ 0 ] / scale[ 1 ] - 1.0f ) < 0.001f );
		assert( Abs( scale[ 0 ] / scale[ 2 ] - 1.0f ) < 0.001f );

		out_channel->samples[ i ] = scale[ 0 ];
	}
}

static void LoadAnimation( Model * model, const cgltf_animation * animation ) {
	for( size_t i = 0; i < animation->channels_count; i++ ) {
		const cgltf_animation_channel * chan = &animation->channels[ i ];

		u8 joint_idx = GetJointIdx( chan->target_node );
		assert( joint_idx != U8_MAX );

		if( chan->target_path == cgltf_animation_path_type_translation ) {
			LoadChannel( chan, &model->joints[ joint_idx ].translations );
		}
		else if( chan->target_path == cgltf_animation_path_type_rotation ) {
			LoadChannel( chan, &model->joints[ joint_idx ].rotations );
		}
		else if( chan->target_path == cgltf_animation_path_type_scale ) {
			LoadScaleChannel( chan, &model->joints[ joint_idx ].scales );
		}
	}
}

template< typename T, size_t N >
static void CreateSingleSampleChannel( Model::AnimationChannel< T > * out_channel, const float ( &sample )[ N ] ) {
	constexpr size_t lanes = sizeof( T ) / sizeof( float );
	STATIC_ASSERT( lanes == N );

	float * memory = ALLOC_MANY( sys_allocator, float, N + 1 );
	out_channel->times = memory;
	out_channel->samples = ( T * ) ( memory + 1 );
	out_channel->num_samples = 1;

	out_channel->times[ 0 ] = 0.0f;
	memcpy( &out_channel->samples[ 0 ], sample, N * sizeof( float ) );
}

static void FixupMissingAnimationChannels( Model * model, const cgltf_skin * skin ) {
	for( u8 i = 0; i < model->num_joints; i++ ) {
		const cgltf_node * node = skin->joints[ i ];
		Model::Joint & joint = model->joints[ i ];

		if( joint.rotations.samples == NULL && node->has_rotation ) {
			CreateSingleSampleChannel( &joint.rotations, node->rotation );
		}

		if( joint.translations.samples == NULL && node->has_translation ) {
			CreateSingleSampleChannel( &joint.translations, node->translation );
		}

		if( joint.scales.samples == NULL && node->has_scale ) {
			assert( Abs( node->scale[ 0 ] / node->scale[ 1 ] - 1.0f ) < 0.001f );
			assert( Abs( node->scale[ 0 ] / node->scale[ 2 ] - 1.0f ) < 0.001f );
			float scale[ 1 ] = { node->scale[ 0 ] };
			CreateSingleSampleChannel( &joint.scales, scale );
		}
	}
}

bool LoadGLTFModel( Model * model, const char * path ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	Span< const u8 > data = AssetBinary( path );

	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * gltf;
	if( cgltf_parse( &options, data.ptr, data.num_bytes(), &gltf ) != cgltf_result_success ) {
		Com_Printf( S_COLOR_YELLOW "%s isn't a GLTF file\n", path );
		return false;
	}

	defer { cgltf_free( gltf ); };

	if( !LoadBinaryBuffers( gltf ) ) {
		Com_Printf( S_COLOR_YELLOW "Couldn't load buffers in %s\n", path );
		return false;
	}

	if( cgltf_validate( gltf ) != cgltf_result_success ) {
		Com_Printf( S_COLOR_YELLOW "%s is invalid GLTF\n", path );
		return false;
	}

	if( gltf->scenes_count != 1 || gltf->animations_count > 1 || gltf->skins_count > 1 ) {
		Com_Printf( S_COLOR_YELLOW "Trivial models only please\n" );
		return false;
	}

	if( gltf->animations_count != gltf->skins_count ) {
		Com_Printf( S_COLOR_YELLOW "Animation/skin count must match\n" );
		return false;
	}

	for( size_t i = 0; i < gltf->meshes_count; i++ ) {
		if( gltf->meshes[ i ].primitives_count != 1 ) {
			Com_Printf( S_COLOR_YELLOW "Meshes with multiple primitives are unsupported\n" );
			return false;
		}
	}

	bool animated = gltf->animations_count > 0;
	cgltf_node * root_joint = NULL;

	if( animated ) {
		if( !FindRootJoint( &gltf->skins[ 0 ], &root_joint ) ) {
			Com_Printf( S_COLOR_YELLOW "Model skeleton has multiple root joints" );
			return false;
		}
	}

	*model = { };

	constexpr Mat4 y_up_to_z_up(
		1, 0, 0, 0,
		0, 0, -1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1
	);
	model->transform = y_up_to_z_up;

	model->primitives = ALLOC_MANY( sys_allocator, Model::Primitive, gltf->meshes_count );

	for( size_t i = 0; i < gltf->scene->nodes_count; i++ ) {
		LoadNode( model, gltf->scene->nodes[ i ], animated );
	}

	if( animated ) {
		LoadSkin( model, root_joint, &gltf->skins[ 0 ], &model->transform );
		LoadAnimation( model, &gltf->animations[ 0 ] );
		FixupMissingAnimationChannels( model, &gltf->skins[ 0 ] );
	}

	return true;
}
