#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/hashtable.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"

constexpr u32 MAX_MODEL_ASSETS = 1024;

static Model models[ MAX_MODEL_ASSETS ];
static u32 num_models;
static Hashtable< MAX_MODEL_ASSETS * 2 > models_hashtable;

void InitModels() {
	ZoneScoped;

	num_models = 0;

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext == ".glb" ) {
			if( !LoadGLTFModel( &models[ num_models ], path ) )
				continue;
			models_hashtable.add( Hash64( path, strlen( path ) - ext.n ), num_models );
			num_models++;
		}
	}
}

static void DeleteModel( Model * model ) {
	for( u32 i = 0; i < model->num_primitives; i++ ) {
		if( model->primitives[ i ].num_vertices == 0 ) {
			DeleteMesh( model->primitives[ i ].mesh );
		}
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		FREE( sys_allocator, model->nodes[ i ].rotations.times );
		FREE( sys_allocator, model->nodes[ i ].translations.times );
		FREE( sys_allocator, model->nodes[ i ].scales.times );
	}

	DeleteMesh( model->mesh );

	FREE( sys_allocator, model->primitives );
	FREE( sys_allocator, model->nodes );
	FREE( sys_allocator, model->skin );
}

void HotloadModels() {
	ZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext != ".glb" )
			continue;

		Model model;
		if( !LoadGLTFModel( &model, path ) )
			continue;

		u64 hash = Hash64( path, strlen( path ) - ext.n );
		u64 idx = num_models;
		if( models_hashtable.get( hash, &idx ) ) {
			DeleteModel( &models[ idx ] );
		}
		else {
			models_hashtable.add( hash, num_models );
			num_models++;
		}

		models[ idx ] = model;
	}
}

void ShutdownModels() {
	for( u32 i = 0; i < num_models; i++ ) {
		DeleteModel( &models[ i ] );
	}
}

Model * NewModel( u64 hash ) {
	Model * model = &models[ num_models ];
	models_hashtable.add( hash, num_models );
	num_models++;
	return model;
}

const Model * FindModel( StringHash name ) {
	u64 idx;
	if( !models_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &models[ idx ];
}

const Model * FindModel( const char * name ) {
	return FindModel( StringHash( name ) );
}

void DrawModelPrimitive( const Model * model, const Model::Primitive * primitive, const PipelineState & pipeline ) {
	if( primitive->num_vertices != 0 ) {
		u32 index_size = model->mesh.indices_format == IndexFormat_U16 ? sizeof( u16 ) : sizeof( u32 );
		DrawMesh( model->mesh, pipeline, primitive->num_vertices, primitive->first_index * index_size );
	}
	else {
		DrawMesh( primitive->mesh, pipeline );
	}
}

template< typename F >
static void RenderNode( const Model * model, u8 node_idx, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes, UniformBlock pose_uniforms, F transform_pipeline ) {
	if( node_idx == U8_MAX )
		return;

	const Model::Node * node = &model->nodes[ node_idx ];

	if( node->primitive != U8_MAX ) {
		bool animated = palettes.node_transforms.ptr != NULL;
		bool skinned = palettes.skinning_matrices.ptr != NULL;

		Mat4 primitive_transform;
		if( animated ) {
			primitive_transform = skinned ? Mat4::Identity() : palettes.node_transforms[ node_idx ];
		}
		else {
			primitive_transform = node->global_transform;
		}

		UniformBlock model_uniforms = UploadModelUniforms( transform * model->transform * primitive_transform );

		PipelineState pipeline = MaterialToPipelineState( model->primitives[ node->primitive ].material, color, skinned );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", model_uniforms );
		if( skinned ) {
			pipeline.set_uniform( "u_Pose", pose_uniforms );
		}
		transform_pipeline( &pipeline, skinned );

		DrawModelPrimitive( model, &model->primitives[ node->primitive ], pipeline );
	}

	RenderNode( model, node->first_child, transform, color, palettes, pose_uniforms, transform_pipeline );
	RenderNode( model, node->sibling, transform, color, palettes, pose_uniforms, transform_pipeline );
}

void DrawModel( const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes ) {
	UniformBlock pose_uniforms;
	if( palettes.skinning_matrices.ptr != NULL ) {
		pose_uniforms = UploadUniforms( palettes.skinning_matrices.ptr, palettes.skinning_matrices.num_bytes() );
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		if( model->nodes[ i ].parent == U8_MAX ) {
			RenderNode( model, i, transform, color, palettes, pose_uniforms, []( PipelineState * pipeline, bool skinned ) { } );
		}
	}
}

static void AddViewWeaponDepthHack( PipelineState * pipeline, bool skinned ) {
	pipeline->view_weapon_depth_hack = true;
}

void DrawViewWeapon( const Model * model, const Mat4 & transform ) {
	for( u8 i = 0; i < model->num_nodes; i++ ) {
		if( model->nodes[ i ].parent == U8_MAX ) {
			RenderNode( model, i, transform, vec4_white, MatrixPalettes(), UniformBlock(), AddViewWeaponDepthHack );
		}
	}
}

void DrawOutlinedModel( const Model * model, const Mat4 & transform, const Vec4 & color, float outline_height, MatrixPalettes palettes ) {
	UniformBlock outline_uniforms = UploadUniformBlock( color, outline_height );

	auto MakeOutlinePipeline = [ &outline_uniforms ]( PipelineState * pipeline, bool skinned ) {
		pipeline->shader = skinned ? &shaders.outline_skinned : &shaders.outline;
		pipeline->pass = frame_static.nonworld_opaque_pass;
		pipeline->cull_face = CullFace_Front;
		pipeline->set_uniform( "u_Outline", outline_uniforms );
	};

	UniformBlock pose_uniforms;
	if( palettes.skinning_matrices.ptr != NULL ) {
		pose_uniforms = UploadUniforms( palettes.skinning_matrices.ptr, palettes.skinning_matrices.num_bytes() );
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		if( model->nodes[ i ].parent == U8_MAX ) {
			RenderNode( model, i, transform, color, palettes, pose_uniforms, MakeOutlinePipeline );
		}
	}
}

void DrawModelSilhouette( const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes ) {
	UniformBlock material_uniforms = UploadMaterialUniforms( color, Vec2( 0 ), 0.0f );

	auto MakeSilhouettePipeline = [ &material_uniforms ]( PipelineState * pipeline, bool skinned ) {
		pipeline->shader = skinned ? &shaders.write_silhouette_gbuffer_skinned : &shaders.write_silhouette_gbuffer;
		pipeline->pass = frame_static.write_silhouette_gbuffer_pass;
		pipeline->write_depth = false;
		pipeline->set_uniform( "u_Material", material_uniforms );
	};

	UniformBlock pose_uniforms;
	if( palettes.skinning_matrices.ptr != NULL ) {
		pose_uniforms = UploadUniforms( palettes.skinning_matrices.ptr, palettes.skinning_matrices.num_bytes() );
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		if( model->nodes[ i ].parent == U8_MAX ) {
			RenderNode( model, i, transform, color, palettes, pose_uniforms, MakeSilhouettePipeline );
		}
	}
}

template< typename T, typename F >
static T SampleAnimationChannel( const Model::AnimationChannel< T > & channel, float t, T def, F lerp ) {
	if( channel.samples == NULL )
		return def;
	if( channel.num_samples == 1 )
		return channel.samples[ 0 ];

	t = Clamp( channel.times[ 0 ], t, channel.times[ channel.num_samples - 1 ] );

	u32 sample = 0;
	for( u32 i = 1; i < channel.num_samples; i++ ) {
		if( channel.times[ i ] >= t ) {
			sample = i - 1;
			break;
		}
	}

	float lerp_frac = ( t - channel.times[ sample ] ) / ( channel.times[ sample + 1 ] - channel.times[ sample ] );

	return lerp( channel.samples[ sample ], lerp_frac, channel.samples[ sample + 1 ] );
}

// can't use overloaded function as a template parameter
static Vec3 LerpVec3( Vec3 a, float t, Vec3 b ) { return Lerp( a, t, b ); }
static float LerpFloat( float a, float t, float b ) { return Lerp( a, t, b ); }

Span< TRS > SampleAnimation( Allocator * a, const Model * model, float t ) {
	ZoneScoped;

	Span< TRS > local_poses = ALLOC_SPAN( a, TRS, model->num_nodes );

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		const Model::Node * node = &model->nodes[ i ];
		local_poses[ i ].rotation = SampleAnimationChannel( node->rotations, t, node->local_transform.rotation, NLerp );
		local_poses[ i ].translation = SampleAnimationChannel( node->translations, t, node->local_transform.translation, LerpVec3 );
		local_poses[ i ].scale = SampleAnimationChannel( node->scales, t, node->local_transform.scale, LerpFloat );
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

MatrixPalettes ComputeMatrixPalettes( Allocator * a, const Model * model, Span< const TRS > local_poses ) {
	ZoneScoped;

	assert( local_poses.n == model->num_nodes );

	MatrixPalettes palettes = { };
	palettes.node_transforms = ALLOC_SPAN( a, Mat4, model->num_nodes );
	if( model->num_joints != 0 ) {
		palettes.skinning_matrices = ALLOC_SPAN( a, Mat4, model->num_joints );
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		u8 parent = model->nodes[ i ].parent;
		if( parent == U8_MAX ) {
			palettes.node_transforms[ i ] = TRSToMat4( local_poses[ i ] );
		}
		else {
			palettes.node_transforms[ i ] = palettes.node_transforms[ parent ] * TRSToMat4( local_poses[ i ] );
		}
	}

	for( u8 i = 0; i < model->num_joints; i++ ) {
		u8 node_idx = model->skin[ i ].node_idx;
		palettes.skinning_matrices[ i ] = palettes.node_transforms[ node_idx ] * model->skin[ i ].joint_to_bind;
	}

	return palettes;
}

bool FindNodeByName( const Model * model, u32 name, u8 * idx ) {
	for( u8 i = 0; i < model->num_nodes; i++ ) {
		if( model->nodes[ i ].name == name ) {
			*idx = i;
			return true;
		}
	}

	return false;
}

static void MergePosesRecursive( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 i ) {
	lower[ i ] = upper[ i ];

	const Model::Node * node = &model->nodes[ i ];
	if( node->sibling != U8_MAX )
		MergePosesRecursive( lower, upper, model, node->sibling );
	if( node->first_child != U8_MAX )
		MergePosesRecursive( lower, upper, model, node->first_child );
}

void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 upper_root_node ) {
	lower[ upper_root_node ] = upper[ upper_root_node ];

	const Model::Node * node = &model->nodes[ upper_root_node ];
	if( node->first_child != U8_MAX )
		MergePosesRecursive( lower, upper, model, node->first_child );
}
