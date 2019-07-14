#include "qcommon/base.h"
#include "qalgo/hash.h"
#include "r_local.h"

#include "cgltf/cgltf.h"

// TODO: lots of memory leaks in error paths. fix with new asset system

struct GLTFMesh {
	// redundant but qf needs these
	drawSurfaceType_t type;
	int num_verts;
	int num_elems;

	mesh_vbo_t * vbo;
	shader_t * shader;
};

struct GLTFModel {
	GLTFMesh meshes[ 16 ];
	u32 num_meshes;

	template< typename T >
	struct AnimationChannel {
		T * samples;
		float * times;
		u32 num_samples;
	};

	struct Joint {
		Mat4 joint_to_bind;
		u8 parent;
		u8 next;

		// TODO: remove this with additive animations
		u32 name;
		u8 first_child;
		u8 sibling;

		AnimationChannel< Quaternion > rotations;
		AnimationChannel< Vec3 > translations;
		AnimationChannel< float > scales;
	};

	// struct AnimationClip {
	//         float start_time;
	//         float duration;
	//         bool loop;
	// };

	Joint * joints;
	u8 num_joints;
	u8 root_joint;

	// AnimationClip * clips;
	// u32 num_clips;
};

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

static void TouchGLTFModel( model_t * mod ) {
	mod->registrationSequence = rsh.registrationSequence;

	GLTFModel * gltf = ( GLTFModel * ) mod->extradata;
	for( u32 i = 0; i < gltf->num_meshes; i++ ) {
		R_TouchMeshVBO( gltf->meshes[ i ].vbo );
		R_TouchShader( gltf->meshes[ i ].shader );
	}
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

static void LoadJoint( GLTFModel * gltf, const cgltf_skin * skin, cgltf_node * node, u8 ** prev ) {
	u8 joint_idx = GetJointIdx( node );
	**prev = joint_idx;
	*prev = &gltf->joints[ joint_idx ].next;

	GLTFModel::Joint & joint = gltf->joints[ joint_idx ];
	joint.parent = node->parent != NULL ? GetJointIdx( node->parent ) : U8_MAX;
	joint.name = Hash32( node->name );

	cgltf_bool ok = cgltf_accessor_read_float( skin->inverse_bind_matrices, joint_idx, joint.joint_to_bind.ptr(), 16 );
	assert( ok != 0 );

	for( size_t i = 0; i < node->children_count; i++ ) {
		LoadJoint( gltf, skin, node->children[ i ], prev );
	}

	// TODO: remove with additive animations
	if( node->children_count == 0 ) {
		joint.first_child = U8_MAX;
	}
	else {
		joint.first_child = GetJointIdx( node->children[ 0 ] );

		for( size_t i = 0; i < node->children_count - 1; i++ ) {
			gltf->joints[ GetJointIdx( node->children[ i ] ) ].sibling = GetJointIdx( node->children[ i + 1 ] );
		}

		gltf->joints[ GetJointIdx( node->children[ node->children_count - 1 ] ) ].sibling = U8_MAX;
	}
}

static void LoadSkin( GLTFModel * gltf, const cgltf_skin * skin, mat4_t transform ) {
	gltf->joints = ( GLTFModel::Joint * ) malloc( sizeof( GLTFModel::Joint ) * skin->joints_count );
	gltf->num_joints = skin->joints_count;

	if( skin->joints_count == 0 )
		return;

	for( size_t i = 0; i < skin->joints_count; i++ ) {
		SetJointIdx( skin->joints[ i ], i );
	}

	cgltf_node * root = NULL;
	for( size_t i = 0; i < skin->joints_count; i++ ) {
		cgltf_node * joint = skin->joints[ i ];
		if( joint->parent == NULL || GetJointIdx( joint->parent ) == U8_MAX ) {
			if( root != NULL ) {
				ri.Com_Error( ERR_DROP, "Model skin has multiple roots" );
			}
			root = joint;
		}
	}

	u8 * prev_ptr = &gltf->root_joint;
	LoadJoint( gltf, skin, root, &prev_ptr );

	if( root->parent != NULL ) {
		mat4_t root_transform;
		cgltf_node_transform_local( root->parent, root_transform );

		mat4_t t;
		Matrix4_Copy( transform, t );
		Matrix4_Multiply( t, root_transform, transform );
	}

	// TODO: remove with additive animations
	gltf->joints[ GetJointIdx( root ) ].sibling = U8_MAX;
}

static void LoadNode( model_t * mod, GLTFModel * gltf, const cgltf_node * node, bool animated, mat4_t transform ) {
	for( size_t i = 0; i < node->children_count; i++ ) {
		LoadNode( mod, gltf, node->children[ i ], animated, transform );
	}

	if( node->mesh == NULL ) {
		return;
	}

	if( node->skin != NULL ) {
		LoadSkin( gltf, node->skin, transform );
	}

	if( node->mesh->primitives_count != 1 ) {
		ri.Com_Error( ERR_DROP, "Trivial models only please" );
	}

	const cgltf_primitive & prim = node->mesh->primitives[ 0 ];
	if( prim.indices->component_type != cgltf_component_type_r_16u ) {
		ri.Com_Error( ERR_DROP, "16bit indices please" );
	}

	mat4_t node_transform;
	if( !animated ) {
		cgltf_node_transform_local( node, node_transform );
	}

	vec4_t * positions = NULL;
	vec4_t * normals = NULL;
	vec2_t * uvs = NULL;
	u8 * joints = NULL;
	u8 * weights = NULL;

	vattribmask_t attributes = 0;

	for( size_t i = 0; i < prim.attributes_count; i++ ) {
		const cgltf_attribute & attr = prim.attributes[ i ];

		if( attr.type == cgltf_attribute_type_position ) {
			attributes |= VATTRIB_POSITION_BIT;
			positions = ( vec4_t * ) Mod_Malloc( mod, attr.data->count * sizeof( vec4_t ) );
			for( size_t k = 0; k < attr.data->count; k++ ) {
				vec4_t lolqfusion; // TODO: better vector library
				cgltf_accessor_read_float( attr.data, k, lolqfusion, 3 );
				lolqfusion[ 3 ] = 1.0f;

				if( !animated ) {
					Matrix4_Multiply_Vector( node_transform, lolqfusion, positions[ k ] );
				}
				else {
					Vector4Copy( lolqfusion, positions[ k ] );
				}

				AddPointToBounds( positions[ k ], mod->mins, mod->maxs );
			}
		}

		if( attr.type == cgltf_attribute_type_normal ) {
			attributes |= VATTRIB_NORMAL_BIT;
			normals = ( vec4_t * ) Mod_Malloc( mod, attr.data->count * sizeof( vec4_t ) );
			for( size_t k = 0; k < attr.data->count; k++ ) {
				cgltf_accessor_read_float( attr.data, k, normals[ k ], 3 );
				normals[ k ][ 3 ] = 0.0f;
			}
		}

		if( attr.type == cgltf_attribute_type_texcoord ) {
			attributes |= VATTRIB_TEXCOORDS_BIT;
			uvs = ( vec2_t * ) Mod_Malloc( mod, attr.data->count * sizeof( vec2_t ) );
			for( size_t k = 0; k < attr.data->count; k++ ) {
				cgltf_accessor_read_float( attr.data, k, uvs[ k ], 2 );
			}
		}

		if( attr.type == cgltf_attribute_type_joints ) {
			attributes |= VATTRIB_JOINTSINDICES_BIT;
			joints = ( u8 * ) Mod_Malloc( mod, attr.data->count * 4 * sizeof( u8 ) );
			Span< u16 > joints_u16 = AccessorToSpan( attr.data ).cast< u16 >();
			for( size_t k = 0; k < joints_u16.n; k++ ) {
				joints[ k ] = joints_u16[ k ];
			}
		}

		if( attr.type == cgltf_attribute_type_weights ) {
			attributes |= VATTRIB_JOINTSWEIGHTS_BIT;
			weights = ( u8 * ) Mod_Malloc( mod, attr.data->count * 4 * sizeof( u8 ) );
			Span< float > weights_float = AccessorToSpan( attr.data ).cast< float >();
			for( size_t k = 0; k < weights_float.n; k++ ) {
				weights[ k ] = weights_float[ k ] * 255;
			}
		}
	}

	mesh_t mesh = { };
	mesh.numVerts = prim.attributes[ 0 ].data->count;
	mesh.numElems = prim.indices->count;
	mesh.xyzArray = positions;
	mesh.normalsArray = normals;
	mesh.stArray = uvs;
	mesh.blendIndices = joints;
	mesh.blendWeights = weights;

	cgltf_size offset = prim.indices->offset + prim.indices->buffer_view->offset;
	mesh.elems = ( elem_t * ) ( ( const u8 * ) prim.indices->buffer_view->buffer->data + offset );

	GLTFMesh & gltf_mesh = gltf->meshes[ gltf->num_meshes ];
	gltf->num_meshes++;

	gltf_mesh.vbo = R_CreateMeshVBO( NULL, mesh.numVerts, mesh.numElems, 0, attributes, VBO_TAG_NONE, 0 );
	R_UploadVBOVertexData( gltf_mesh.vbo, 0, attributes, &mesh );
	R_UploadVBOElemData( gltf_mesh.vbo, 0, 0, &mesh );

	Mod_MemFree( positions );
	Mod_MemFree( normals );
	Mod_MemFree( uvs );
	Mod_MemFree( joints );
	Mod_MemFree( weights );

	const char * material_name = prim.material != NULL ? prim.material->name : "***r_notexture***";
	gltf_mesh.shader = R_RegisterShader( material_name, SHADER_TYPE_DIFFUSE );

	gltf_mesh.type = ST_GLTF;
	gltf_mesh.num_verts = mesh.numVerts;
	gltf_mesh.num_elems = mesh.numElems;
}

template< typename T >
static void LoadChannel( const cgltf_animation_channel * chan, GLTFModel::AnimationChannel< T > * out_channel ) {
	constexpr size_t lanes = sizeof( T ) / sizeof( float );
	size_t n = chan->sampler->input->count;

	float * memory = ( float * ) malloc( n * ( lanes + 1 ) * sizeof( float ) );
	out_channel->times = memory;
	out_channel->samples = ( T * ) ( memory + n );
	out_channel->num_samples = n;

	for( size_t i = 0; i < n; i++ ) {
		cgltf_bool ok = cgltf_accessor_read_float( chan->sampler->input, i, &out_channel->times[ i ], 1 );
		ok = ok && cgltf_accessor_read_float( chan->sampler->output, i, out_channel->samples[ i ].ptr(), lanes );
		assert( ok != 0 );
	}
}

static void LoadScaleChannel( const cgltf_animation_channel * chan, GLTFModel::AnimationChannel< float > * out_channel ) {
	size_t n = chan->sampler->input->count;

	float * memory = ( float * ) malloc( n * 2 * sizeof( float ) );
	out_channel->times = memory;
	out_channel->samples = memory + n;
	out_channel->num_samples = n;

	for( size_t i = 0; i < n; i++ ) {
		cgltf_accessor_read_float( chan->sampler->input, i, &out_channel->times[ i ], 1 );

		float scale[ 3 ];
		cgltf_accessor_read_float( chan->sampler->output, i, scale, 3 );

		assert( fabsf( scale[ 0 ] / scale[ 1 ] - 1.0f ) < 0.001f );
		assert( fabsf( scale[ 0 ] / scale[ 2 ] - 1.0f ) < 0.001f );

		out_channel->samples[ i ] = scale[ 0 ];
	}
}

static void LoadAnimation( GLTFModel * gltf, const cgltf_animation * animation ) {
	for( size_t i = 0; i < animation->channels_count; i++ ) {
		const cgltf_animation_channel * chan = &animation->channels[ i ];

		assert( chan->target_node->camera != NULL );
		u8 joint_idx = GetJointIdx( chan->target_node );

		if( chan->target_path == cgltf_animation_path_type_translation ) {
			LoadChannel( chan, &gltf->joints[ joint_idx ].translations );
		}
		else if( chan->target_path == cgltf_animation_path_type_rotation ) {
			LoadChannel( chan, &gltf->joints[ joint_idx ].rotations );
		}
		else if( chan->target_path == cgltf_animation_path_type_scale ) {
			LoadScaleChannel( chan, &gltf->joints[ joint_idx ].scales );
		}
	}
}

void Mod_LoadGLTFModel( model_t * mod, void * buffer, int buffer_size, const bspFormatDesc_t * bsp_format ) {
	MICROPROFILE_SCOPEI( "Assets", "Mod_LoadGLTFModel", 0xffffffff );

	cgltf_options options = { };
	options.type = cgltf_file_type_glb;

	cgltf_data * data;
	if( cgltf_parse( &options, buffer, buffer_size, &data ) != cgltf_result_success ) {
		ri.Com_Error( ERR_DROP, "%s isn't a GLTF file", mod->name );
	}

	if( !LoadBinaryBuffers( data ) ) {
		cgltf_free( data );
		ri.Com_Error( ERR_DROP, "Couldn't load buffers in %s", mod->name );
	}

	if( cgltf_validate( data ) != cgltf_result_success ) {
		cgltf_free( data );
		ri.Com_Error( ERR_DROP, "%s is invalid GLTF", mod->name );
	}

	if( data->scenes_count != 1 || data->animations_count > 1 ) {
		cgltf_free( data );
		ri.Com_Error( ERR_DROP, "Trivial models only please" );
	}

	GLTFModel * gltf = ( GLTFModel * ) Mod_Malloc( mod, sizeof( GLTFModel ) );
	*gltf = { };

	mod->type = ModelType_GLTF;
	mod->extradata = gltf;
	mod->registrationSequence = rsh.registrationSequence;
	mod->touch = &TouchGLTFModel;
	mod->radius = 0;

	constexpr mat4_t y_up_to_z_up = {
		1, 0, 0, 0,
		0, 0, 1, 0,
		0, -1, 0, 0,
		0, 0, 0, 1,
	};
	Matrix4_Copy( y_up_to_z_up, mod->transform );

	ClearBounds( mod->mins, mod->maxs );

	bool animated = data->animations_count > 0;
	for( size_t i = 0; i < data->scene->nodes_count; i++ ) {
		LoadNode( mod, gltf, data->scene->nodes[ i ], animated, mod->transform );
	}

	if( animated ) {
		LoadAnimation( gltf, &data->animations[ 0 ] );
	}

	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );

	cgltf_free( data );
}

void R_AddGLTFModelToDrawList( const entity_t * e ) {
	const GLTFModel * gltf = ( const GLTFModel * ) e->model->extradata;
	for( u32 i = 0; i < gltf->num_meshes; i++ ) {
		const GLTFMesh & mesh = gltf->meshes[ i ];
		int sort_key = R_PackShaderOrder( mesh.shader );
		R_AddSurfToDrawList( rn.meshlist, e, mesh.shader, 0, sort_key, &mesh );
	}
}

void R_DrawGLTFMesh( const entity_t * e, const shader_t * shader, const GLTFMesh * mesh ) {
	RB_BindVBO( mesh->vbo->index, GL_TRIANGLES );
	// TODO: do this once per model rather than per mesh
	RB_SetSkinningMatrices( e->pose.skinning_matrices );
	RB_FlipFrontFace();
	RB_DrawElements( 0, mesh->num_verts, 0, mesh->num_elems );
	RB_FlipFrontFace();
}

void R_CacheGLTFModelEntity( const entity_t * e ) {
	// behold qf's masterpiece renderer
	entSceneCache_t * cache = R_ENTCACHE( e );
	const model_t * mod = e->model;
	cache->rotated = true;
	cache->radius = mod->radius * e->scale;

	// hack but doing this properly is difficult
	if( e->pose.skinning_matrices.ptr != NULL )
		cache->radius *= 2;
}

static void FindSampleAndLerpFrac( const float * times, u32 n, float t, u32 * sample, float * lerp_frac ) {
	t = Clamp( times[ 0 ], t, times[ n - 1 ] );

	*sample = 0;
	for( u32 i = 1; i < n; i++ ) {
		if( times[ i ] >= t ) {
			*sample = i - 1;
			break;
		}
	}

	*lerp_frac = ( t - times[ *sample ] ) / ( times[ *sample + 1 ] - times[ *sample ] );
}

Span< TRS > R_SampleAnimation( ArenaAllocator * a, const model_t * model, float t ) {
	assert( model->type == ModelType_GLTF );
	const GLTFModel * gltf = ( const GLTFModel * ) model->extradata;

	Span< TRS > local_poses = ALLOC_SPAN( a, TRS, gltf->num_joints );

	for( u8 i = 0; i < gltf->num_joints; i++ ) {
		const GLTFModel::Joint & joint = gltf->joints[ i ];

		u32 rotation_sample, translation_sample, scale_sample;
		float rotation_lerp_frac, translation_lerp_frac, scale_lerp_frac;

		FindSampleAndLerpFrac( joint.rotations.times, joint.rotations.num_samples, t, &rotation_sample, &rotation_lerp_frac );
		FindSampleAndLerpFrac( joint.translations.times, joint.translations.num_samples, t, &translation_sample, &translation_lerp_frac );
		FindSampleAndLerpFrac( joint.scales.times, joint.scales.num_samples, t, &scale_sample, &scale_lerp_frac );

		local_poses[ i ].rotation = NLerp( joint.rotations.samples[ rotation_sample ], rotation_lerp_frac, joint.rotations.samples[ ( rotation_sample + 1 ) % joint.rotations.num_samples ] );
		local_poses[ i ].translation = Lerp( joint.translations.samples[ translation_sample ], translation_lerp_frac, joint.translations.samples[ ( translation_sample + 1 ) % joint.translations. num_samples ] );
		local_poses[ i ].scale = Lerp( joint.scales.samples[ scale_sample ], scale_lerp_frac, joint.scales.samples[ ( scale_sample + 1 ) % joint.scales.num_samples ] );
	}

	return local_poses;
}

static Mat4 TRSToMat4( const TRS & trs ) {
        Quaternion q = trs.rotation;
        Vec3 t = trs.translation;
        float s = trs.scale;

        // return t * q * s;
        return Mat4(
                ( 1.0f - 2 * q.y * q.y - 2.0f * q.z * q.z ) * s,
                ( 2.0f * q.x * q.y - 2.0f * q.z * q.w ) * s,
                ( 2.0f * q.x * q.z + 2.0f * q.y * q.w ) * s,
                t.x,

                ( 2.0f * q.x * q.y + 2.0f * q.z * q.w ) * s,
                ( 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z ) * s,
                ( 2.0f * q.y * q.z - 2.0f * q.x * q.w ) * s,
                t.y,

                ( 2.0f * q.x * q.z - 2.0f * q.y * q.w ) * s,
                ( 2.0f * q.y * q.z + 2.0f * q.x * q.w ) * s,
                ( 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y ) * s,
                t.z,

                0.0f, 0.0f, 0.0f, 1.0f
        );
}

MatrixPalettes R_ComputeMatrixPalettes( ArenaAllocator * a, const model_t * model, Span< TRS > local_poses ) {
	assert( model->type == ModelType_GLTF );
	const GLTFModel * gltf = ( const GLTFModel * ) model->extradata;
	assert( local_poses.n == gltf->num_joints );

	MatrixPalettes palettes;
	palettes.joint_poses = ALLOC_SPAN( a, Mat4, gltf->num_joints );
	palettes.skinning_matrices = ALLOC_SPAN( a, Mat4, gltf->num_joints );

	u8 joint_idx = gltf->root_joint;
	palettes.joint_poses[ joint_idx ] = TRSToMat4( local_poses[ joint_idx ] );
	for( u8 i = 0; i < gltf->num_joints - 1; i++ ) {
		joint_idx = gltf->joints[ joint_idx ].next;
		u8 parent = gltf->joints[ joint_idx ].parent;
		palettes.joint_poses[ joint_idx ] = palettes.joint_poses[ parent ] * TRSToMat4( local_poses[ joint_idx ] );
	}

	for( u8 i = 0; i < gltf->num_joints; i++ ) {
		palettes.skinning_matrices[ i ] = palettes.joint_poses[ i ] * gltf->joints[ i ].joint_to_bind;
	}

	return palettes;
}

bool R_FindJointByName( const model_t * model, const char * name, u8 * joint_idx ) {
	assert( model->type == ModelType_GLTF );
	const GLTFModel * gltf = ( const GLTFModel * ) model->extradata;

	u32 hash = Hash32( name );
	for( u8 i = 0; i < gltf->num_joints; i++ ) {
		if( gltf->joints[ i ].name == hash ) {
			*joint_idx = i;
			return true;
		}
	}

	return false;
}

static void MergePosesRecursive( Span< TRS > lower, Span< const TRS > upper, const GLTFModel * gltf, u8 i ) {
	lower[ i ] = upper[ i ];

	const GLTFModel::Joint & joint = gltf->joints[ i ];
	if( joint.sibling != U8_MAX )
		MergePosesRecursive( lower, upper, gltf, joint.sibling );
	if( joint.first_child != U8_MAX )
		MergePosesRecursive( lower, upper, gltf, joint.first_child );
}

void R_MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const model_t * model, u8 upper_root_joint ) {
	assert( model->type == ModelType_GLTF );
	const GLTFModel * gltf = ( const GLTFModel * ) model->extradata;

	lower[ upper_root_joint ] = upper[ upper_root_joint ];

	const GLTFModel::Joint & joint = gltf->joints[ upper_root_joint ];
	if( joint.first_child != U8_MAX )
		MergePosesRecursive( lower, upper, gltf, joint.first_child );
}
