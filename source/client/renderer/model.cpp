#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/hashtable.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"

constexpr u32 MAX_MODELS = 1024;

static Model gltf_models[ MAX_MODELS ];
static u32 num_gltf_models;
static Hashtable< MAX_MODELS * 2 > gltf_models_hashtable;

static void LoadGLTF( const char * path ) {
	Span< const char > ext = FileExtension( path );
	if( ext != ".glb" )
		return;

	Model model;
	if( !LoadGLTFModel( &model, path ) )
		return;

	u64 hash = Hash64( StripExtension( path ) );

	u64 idx = num_gltf_models;
	if( !gltf_models_hashtable.get( hash, &idx ) ) {
		assert( num_gltf_models < ARRAY_COUNT( gltf_models ) );
		gltf_models_hashtable.add( hash, num_gltf_models );
		num_gltf_models++;
	}
	else {
		DeleteModel( &gltf_models[ idx ] );
	}

	gltf_models[ idx ] = model;
}

void InitModels() {
	ZoneScoped;

	num_gltf_models = 0;

	for( const char * path : AssetPaths() ) {
		LoadGLTF( path );
	}
}

void DeleteModel( Model * model ) {
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
		LoadGLTF( path );
	}
}

void ShutdownModels() {
	for( u32 i = 0; i < num_gltf_models; i++ ) {
		DeleteModel( &gltf_models[ i ] );
	}
}

const Model * FindModel( StringHash name ) {
	u64 idx;
	if( !gltf_models_hashtable.get( name.hash, &idx ) ) {
		return FindMapModel( name );
	}
	return &gltf_models[ idx ];
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

static void DrawModelNode( DrawModelConfig::DrawModel config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline ) {
	if( !config.enabled )
		return;

	if( config.view_weapon ) {
		pipeline.view_weapon_depth_hack = true;
	}

	DrawModelPrimitive( model, primitive, pipeline );
}

static void DrawShadowsNode( DrawModelConfig::DrawShadows config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline ) {
	if( !config.enabled )
		return;

	pipeline.shader = skinned ? &shaders.depth_only_skinned : &shaders.depth_only;
	pipeline.clamp_depth = true;
	// pipeline.cull_face = CullFace_Disabled;
	pipeline.write_depth = true;

	for( u32 i = 0; i < frame_static.shadow_parameters.entity_cascades; i++ ) {
		pipeline.pass = frame_static.shadowmap_pass[ i ];
		pipeline.set_uniform( "u_View", frame_static.shadowmap_view_uniforms[ i ] );

		DrawModelPrimitive( model, primitive, pipeline );
	}
}

static void DrawOutlinesNode( DrawModelConfig::DrawOutlines config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline, UniformBlock outline_uniforms ) {
	if( !config.enabled )
		return;
	
	pipeline.shader = skinned ? &shaders.outline_skinned : &shaders.outline;
	pipeline.pass = frame_static.nonworld_opaque_pass;
	pipeline.cull_face = CullFace_Front;
	pipeline.set_uniform( "u_Outline", outline_uniforms );

	DrawModelPrimitive( model, primitive, pipeline );
}

static void DrawSilhouetteNode( DrawModelConfig::DrawSilhouette config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline, UniformBlock silhouette_uniforms ) {
	if( !config.enabled )
		return;
	
	pipeline.shader = skinned ? &shaders.write_silhouette_gbuffer_skinned : &shaders.write_silhouette_gbuffer;
	pipeline.pass = frame_static.write_silhouette_gbuffer_pass;
	pipeline.write_depth = false;
	pipeline.set_uniform( "u_Material", silhouette_uniforms );

	DrawModelPrimitive( model, primitive, pipeline );
}

void DrawModel( DrawModelConfig config, const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes ) {
	bool animated = palettes.node_transforms.ptr != NULL;
	UniformBlock pose_uniforms = { };
	if( animated ) {
		pose_uniforms = UploadUniforms( palettes.skinning_matrices.ptr, palettes.skinning_matrices.num_bytes() );
	}

	UniformBlock outline_uniforms = { };
	if( config.draw_outlines.enabled ) {
		outline_uniforms = UploadUniformBlock( config.draw_outlines.outline_color, config.draw_outlines.outline_height );
	}

	UniformBlock silhouette_uniforms = { };
	if( config.draw_silhouette.enabled ) {
		silhouette_uniforms = UploadMaterialUniforms( config.draw_silhouette.silhouette_color, Vec2( 0.0f ), 0.0f, 0.0f );
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		const Model::Node * node = &model->nodes[ i ];
		if( node->primitive == U8_MAX )
			continue;

		bool skinned = animated && node->skinned;

		Mat4 node_transform;
		if( skinned ) {
			node_transform = Mat4::Identity();
		}
		else if( animated ) {
			node_transform = palettes.node_transforms[ i ];
		}
		else {
			node_transform = node->global_transform;
		}
		node_transform = transform * model->transform * node_transform;

		const Model::Primitive * primitive = &model->primitives[ node->primitive ];
		PipelineState pipeline = MaterialToPipelineState( primitive->material, color, skinned );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", UploadModelUniforms( node_transform ) );
		if( skinned ) {
			pipeline.set_uniform( "u_Pose", pose_uniforms );
		}

		DrawModelNode( config.draw_model, model, primitive, skinned, pipeline );
		DrawShadowsNode( config.draw_shadows, model, primitive, skinned, pipeline );
		DrawOutlinesNode( config.draw_outlines, model, primitive, skinned, pipeline, outline_uniforms );
		DrawSilhouetteNode( config.draw_silhouette, model, primitive, skinned, pipeline, silhouette_uniforms );
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

	// TODO: cubic
	if( channel.interpolation == InterpolationMode_Step ) {
		return channel.samples[ sample ];
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
