#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/hash.h"
#include "client/renderer/renderer.h"
#include "client/assets.h"
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

static u8 GetNodeIdx( const cgltf_node * node ) {
	return u8( uintptr_t( node->camera ) - 1 );
}

static void SetNodeIdx( cgltf_node * node, u8 idx ) {
	node->camera = ( cgltf_camera * ) uintptr_t( idx + 1 );
}

static MinMax3 Extend( MinMax3 bounds, Vec3 p ) {
	return MinMax3(
		Vec3( Min2( bounds.mins.x, p.x ), Min2( bounds.mins.y, p.y ), Min2( bounds.mins.z, p.z ) ),
		Vec3( Max2( bounds.maxs.x, p.x ), Max2( bounds.maxs.y, p.y ), Max2( bounds.maxs.z, p.z ) )
	);
}

static Span< const u8 > AccessorToSpan( const cgltf_accessor * accessor ) {
	cgltf_size offset = accessor->offset + accessor->buffer_view->offset;
	return Span< const u8 >( ( const u8 * ) accessor->buffer_view->buffer->data + offset, accessor->count * accessor->stride );
}

static VertexFormat VertexFormatFromGLTF( cgltf_type dim, cgltf_component_type component, bool normalized ) {
	if( dim == cgltf_type_vec2 ) {
		if( component == cgltf_component_type_r_8u )
			return normalized ? VertexFormat_U8x2_Norm : VertexFormat_U8x2;
		if( component == cgltf_component_type_r_16u )
			return normalized ? VertexFormat_U16x2_Norm : VertexFormat_U16x2;
		if( component == cgltf_component_type_r_32f )
			return VertexFormat_Floatx2;
	}

	if( dim == cgltf_type_vec3 ) {
		if( component == cgltf_component_type_r_8u )
			return normalized ? VertexFormat_U8x3_Norm : VertexFormat_U8x3;
		if( component == cgltf_component_type_r_16u )
			return normalized ? VertexFormat_U16x3_Norm : VertexFormat_U16x3;
		if( component == cgltf_component_type_r_32f )
			return VertexFormat_Floatx3;
	}

	if( dim == cgltf_type_vec4 ) {
		if( component == cgltf_component_type_r_8u )
			return normalized ? VertexFormat_U8x4_Norm : VertexFormat_U8x4;
		if( component == cgltf_component_type_r_16u )
			return normalized ? VertexFormat_U16x4_Norm : VertexFormat_U16x4;
		if( component == cgltf_component_type_r_32f )
			return VertexFormat_Floatx4;
	}

	return VertexFormat_Floatx4; // TODO: actual error handling
}

static void LoadGeometry( Model * model, const cgltf_node * node, const Mat4 & transform ) {
	const cgltf_primitive & prim = node->mesh->primitives[ 0 ];

	MeshConfig mesh_config;

	for( size_t i = 0; i < prim.attributes_count; i++ ) {
		const cgltf_attribute & attr = prim.attributes[ i ];

		if( attr.type == cgltf_attribute_type_position ) {
			mesh_config.num_vertices = attr.data->count;
			mesh_config.positions = NewVertexBuffer( AccessorToSpan( attr.data ) );

			Vec3 min, max;
			for( int j = 0; j < 3; j++ ) {
				min[ j ] = attr.data->min[ j ];
				max[ j ] = attr.data->max[ j ];
			}

			model->bounds = Extend( model->bounds, ( transform * Vec4( min, 1.0f ) ).xyz() );
			model->bounds = Extend( model->bounds, ( transform * Vec4( max, 1.0f ) ).xyz() );
		}

		if( attr.type == cgltf_attribute_type_normal ) {
			mesh_config.normals = NewVertexBuffer( AccessorToSpan( attr.data ) );
		}

		if( attr.type == cgltf_attribute_type_texcoord ) {
			mesh_config.tex_coords = NewVertexBuffer( AccessorToSpan( attr.data ) );
			mesh_config.tex_coords_format = VertexFormatFromGLTF( attr.data->type, attr.data->component_type, attr.data->normalized );
		}

		if( attr.type == cgltf_attribute_type_joints ) {
			mesh_config.joints = NewVertexBuffer( AccessorToSpan( attr.data ) );
			mesh_config.joints_format = VertexFormatFromGLTF( attr.data->type, attr.data->component_type, attr.data->normalized );
		}

		if( attr.type == cgltf_attribute_type_weights ) {
			mesh_config.weights = NewVertexBuffer( AccessorToSpan( attr.data ) );
			mesh_config.weights_format = VertexFormatFromGLTF( attr.data->type, attr.data->component_type, attr.data->normalized );
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

	const char * material_name = prim.material != NULL ? prim.material->name : "";
	primitive->material = FindMaterial( material_name );
}

static void LoadNode( Model * model, cgltf_node * gltf_node, u8 * node_idx ) {
	u8 idx = *node_idx;
	*node_idx += 1;
	SetNodeIdx( gltf_node, idx );

	Model::Node * node = &model->nodes[ idx ];
	node->parent = gltf_node->parent != NULL ? GetNodeIdx( gltf_node->parent ) : U8_MAX;
	node->primitive = U8_MAX;
	node->name = gltf_node->name == NULL ? 0 : Hash32( gltf_node->name );

	cgltf_node_transform_world( gltf_node, node->global_transform.ptr() );

	node->local_transform.rotation = Quaternion::Identity();
	node->local_transform.translation = Vec3( 0.0f );
	node->local_transform.scale = 1.0f;

	if( gltf_node->has_rotation ) {
		node->local_transform.rotation = Quaternion(
			gltf_node->rotation[ 0 ],
			gltf_node->rotation[ 1 ],
			gltf_node->rotation[ 2 ],
			gltf_node->rotation[ 3 ]
		);
	}

	if( gltf_node->has_translation ) {
		node->local_transform.translation = Vec3(
			gltf_node->translation[ 0 ],
			gltf_node->translation[ 1 ],
			gltf_node->translation[ 2 ]
		);
	}

	if( gltf_node->has_scale ) {
		// TODO
		// assert( Abs( gltf_node->scale[ 0 ] / gltf_node->scale[ 1 ] - 1.0f ) < 0.001f );
		// assert( Abs( gltf_node->scale[ 0 ] / gltf_node->scale[ 2 ] - 1.0f ) < 0.001f );
		node->local_transform.scale = gltf_node->scale[ 0 ];
	}

	// TODO: this will break if multiple nodes share a mesh
	if( gltf_node->mesh != NULL ) {
		node->primitive = model->num_primitives;
		LoadGeometry( model, gltf_node, node->global_transform );
	}

	for( size_t i = 0; i < gltf_node->children_count; i++ ) {
		LoadNode( model, gltf_node->children[ i ], node_idx );
	}

	if( gltf_node->children_count == 0 ) {
		node->first_child = U8_MAX;
	}
	else {
		node->first_child = GetNodeIdx( gltf_node->children[ 0 ] );

		for( size_t i = 0; i < gltf_node->children_count - 1; i++ ) {
			model->nodes[ GetNodeIdx( gltf_node->children[ i ] ) ].sibling = GetNodeIdx( gltf_node->children[ i + 1 ] );
		}

		model->nodes[ GetNodeIdx( gltf_node->children[ gltf_node->children_count - 1 ] ) ].sibling = U8_MAX;
	}
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

template< typename T >
static void CreateSingleSampleChannel( Model::AnimationChannel< T > * out_channel, T sample ) {
	constexpr size_t lanes = sizeof( T ) / sizeof( float );

	float * memory = ALLOC_MANY( sys_allocator, float, lanes + 1 );
	out_channel->times = memory;
	out_channel->samples = ( T * ) ( memory + 1 );
	out_channel->num_samples = 1;

	out_channel->times[ 0 ] = 0.0f;
	out_channel->samples[ 0 ] = sample;
}

static void LoadAnimation( Model * model, const cgltf_animation * animation ) {
	for( size_t i = 0; i < animation->channels_count; i++ ) {
		const cgltf_animation_channel * chan = &animation->channels[ i ];

		u8 node_idx = GetNodeIdx( chan->target_node );
		assert( node_idx != U8_MAX );

		if( chan->target_path == cgltf_animation_path_type_translation ) {
			LoadChannel( chan, &model->nodes[ node_idx ].translations );
		}
		else if( chan->target_path == cgltf_animation_path_type_rotation ) {
			LoadChannel( chan, &model->nodes[ node_idx ].rotations );
		}
		else if( chan->target_path == cgltf_animation_path_type_scale ) {
			LoadScaleChannel( chan, &model->nodes[ node_idx ].scales );
		}
	}
}

static void LoadSkin( Model * model, const cgltf_skin * skin ) {
	model->skin = ALLOC_MANY( sys_allocator, Model::Joint, skin->joints_count );
	model->num_joints = skin->joints_count;

	for( size_t i = 0; i < skin->joints_count; i++ ) {
		Model::Joint * joint = &model->skin[ i ];
		joint->node_idx = GetNodeIdx( skin->joints[ i ] );

		cgltf_bool ok = cgltf_accessor_read_float( skin->inverse_bind_matrices, i, joint->joint_to_bind.ptr(), 16 );
		assert( ok != 0 );
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
		Com_Printf( S_COLOR_YELLOW "Trivial models only please (%s)\n", path );
		return false;
	}

	for( size_t i = 0; i < gltf->meshes_count; i++ ) {
		if( gltf->meshes[ i ].primitives_count != 1 ) {
			Com_Printf( S_COLOR_YELLOW "Meshes with multiple primitives are unsupported (%s)\n", path );
			return false;
		}
	}

	*model = { };
	model->bounds = MinMax3::Empty();

	constexpr Mat4 y_up_to_z_up(
		1, 0, 0, 0,
		0, 0, -1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1
	);
	model->transform = y_up_to_z_up;

	model->primitives = ALLOC_MANY( sys_allocator, Model::Primitive, gltf->meshes_count );

	model->nodes = ALLOC_MANY( sys_allocator, Model::Node, gltf->nodes_count );
	memset( model->nodes, 0, sizeof( Model::Node ) * gltf->nodes_count );
	model->num_nodes = gltf->nodes_count;

	u8 node_idx = 0;
	for( size_t i = 0; i < gltf->scene->nodes_count; i++ ) {
		LoadNode( model, gltf->scene->nodes[ i ], &node_idx );
		model->nodes[ GetNodeIdx( gltf->scene->nodes[ i ] ) ].sibling = U8_MAX;
	}

	if( gltf->animations_count > 0 ) {
		if( gltf->skins_count > 0 ) {
			LoadSkin( model, &gltf->skins[ 0 ] );
		}

		LoadAnimation( model, &gltf->animations[ 0 ] );
	}

	return true;
}
