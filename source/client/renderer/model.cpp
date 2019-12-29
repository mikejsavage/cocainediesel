#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/hashtable.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"

constexpr u32 MAX_MODEL_ASSETS = 1024;
constexpr u32 MAX_MAPS = 128;

static Model models[ MAX_MODEL_ASSETS ];
static u32 num_models;
static Hashtable< MAX_MODEL_ASSETS * 2 > models_hashtable;

static MapMetadata maps[ MAX_MAPS ];
static u32 num_maps;
static Hashtable< MAX_MAPS * 2 > maps_hashtable;

void InitModels() {
	ZoneScoped;

	num_models = 0;
	num_maps = 0;

	for( const char * path : AssetPaths() ) {
		Span< const char > ext = FileExtension( path );
		if( ext == ".bsp" ) {
			if( !LoadBSPMap( &maps[ num_maps ], path ) )
				continue;
			maps_hashtable.add( Hash64( path, strlen( path ) - ext.n ), num_maps );
			num_maps++;
		}
	}

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

Model * NewModel( u64 hash ) {
	Model * model = &models[ num_models ];
	models_hashtable.add( hash, num_models );
	num_models++;
	return model;
}

static void DeleteModel( Model * model ) {
	for( u32 i = 0; i < model->num_primitives; i++ ) {
		if( model->primitives[ i ].num_vertices == 0 ) {
			DeleteMesh( model->primitives[ i ].mesh );
		}
	}

	for( u8 i = 0; i < model->num_joints; i++ ) {
		FREE( sys_allocator, model->joints[ i ].rotations.times );
		FREE( sys_allocator, model->joints[ i ].translations.times );
		FREE( sys_allocator, model->joints[ i ].scales.times );
	}

	DeleteMesh( model->mesh );

	FREE( sys_allocator, model->primitives );
	FREE( sys_allocator, model->joints );
}

void ShutdownModels() {
	for( u32 i = 0; i < num_models; i++ ) {
		DeleteModel( &models[ i ] );
	}

	for( u32 i = 0; i < num_maps; i++ ) {
		FREE( sys_allocator, maps[ i ].pvs.ptr );
	}
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

const MapMetadata * FindMapMetadata( StringHash name ) {
	u64 idx;
	if( !maps_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &maps[ idx ];
}

const MapMetadata * FindMapMetadata( const char * name ) {
	return FindMapMetadata( StringHash( name ) );
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

void DrawModel( const Model * model, const Mat4 & transform, const Vec4 & color, Span< const Mat4 > skinning_matrices ) {
	bool skinned = skinning_matrices.ptr != NULL;

	UniformBlock model_uniforms = UploadModelUniforms( transform * model->transform );
	UniformBlock pose_uniforms;
	if( skinned ) {
		pose_uniforms = UploadUniforms( skinning_matrices.ptr, skinning_matrices.num_bytes() );
	}

	for( u32 i = 0; i < model->num_primitives; i++ ) {
		PipelineState pipeline = MaterialToPipelineState( model->primitives[ i ].material, color, skinned );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", model_uniforms );
		if( skinned ) {
			pipeline.set_uniform( "u_Pose", pose_uniforms );
		}

		DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
	}
}

void DrawOutlinedModel( const Model * model, const Mat4 & transform, const Vec4 & color, float outline_height, Span< const Mat4 > skinning_matrices ) {
	bool skinned = skinning_matrices.ptr != NULL;

	UniformBlock model_uniforms = UploadModelUniforms( transform * model->transform );
	UniformBlock outline_uniforms = UploadUniformBlock( color, outline_height );
	UniformBlock pose_uniforms;
	if( skinned ) {
		pose_uniforms = UploadUniforms( skinning_matrices.ptr, skinning_matrices.num_bytes() );
	}

	for( u32 i = 0; i < model->num_primitives; i++ ) {
		PipelineState pipeline;
		pipeline.shader = skinned ? &shaders.outline_skinned : &shaders.outline;
		pipeline.pass = frame_static.nonworld_opaque_pass;
		pipeline.cull_face = CullFace_Front;
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", model_uniforms );
		pipeline.set_uniform( "u_Outline", outline_uniforms );
		if( skinned ) {
			pipeline.set_uniform( "u_Pose", pose_uniforms );
		}

		DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
	}
}

void DrawModelSilhouette( const Model * model, const Mat4 & transform, const Vec4 & color, Span< const Mat4 > skinning_matrices ) {
	bool skinned = skinning_matrices.ptr != NULL;

	UniformBlock model_uniforms = UploadModelUniforms( transform * model->transform );
	UniformBlock material_uniforms = UploadMaterialUniforms( color, Vec2( 0 ), 0.0f );
	UniformBlock pose_uniforms;
	if( skinned ) {
		pose_uniforms = UploadUniforms( skinning_matrices.ptr, skinning_matrices.num_bytes() );
	}

	for( u32 i = 0; i < model->num_primitives; i++ ) {
		PipelineState pipeline;
		pipeline.shader = skinned ? &shaders.write_silhouette_gbuffer_skinned : &shaders.write_silhouette_gbuffer;
		pipeline.pass = frame_static.write_silhouette_gbuffer_pass;
		pipeline.write_depth = false;
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", model_uniforms );
		pipeline.set_uniform( "u_Material", material_uniforms );
		if( skinned ) {
			pipeline.set_uniform( "u_Pose", pose_uniforms );
		}

		DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
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

	Span< TRS > local_poses = ALLOC_SPAN( a, TRS, model->num_joints );

	for( u8 i = 0; i < model->num_joints; i++ ) {
		const Model::Joint & joint = model->joints[ i ];
		local_poses[ i ].rotation = SampleAnimationChannel( joint.rotations, t, Quaternion::Identity(), NLerp );
		local_poses[ i ].translation = SampleAnimationChannel( joint.translations, t, Vec3( 0 ), LerpVec3 );
		local_poses[ i ].scale = SampleAnimationChannel( joint.scales, t, 1.0f, LerpFloat );
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

MatrixPalettes ComputeMatrixPalettes( Allocator * a, const Model * model, Span< TRS > local_poses ) {
	ZoneScoped;

	assert( local_poses.n == model->num_joints );

	MatrixPalettes palettes;
	palettes.joint_poses = ALLOC_SPAN( a, Mat4, model->num_joints );
	palettes.skinning_matrices = ALLOC_SPAN( a, Mat4, model->num_joints );

	u8 joint_idx = model->root_joint;
	palettes.joint_poses[ joint_idx ] = TRSToMat4( local_poses[ joint_idx ] );
	for( u8 i = 0; i < model->num_joints - 1; i++ ) {
		joint_idx = model->joints[ joint_idx ].next;
		u8 parent = model->joints[ joint_idx ].parent;
		palettes.joint_poses[ joint_idx ] = palettes.joint_poses[ parent ] * TRSToMat4( local_poses[ joint_idx ] );
	}

	for( u8 i = 0; i < model->num_joints; i++ ) {
		palettes.skinning_matrices[ i ] = palettes.joint_poses[ i ] * model->joints[ i ].joint_to_bind;
	}

	return palettes;
}

bool FindJointByName( const Model * model, u32 name, u8 * joint_idx ) {
	for( u8 i = 0; i < model->num_joints; i++ ) {
		if( model->joints[ i ].name == name ) {
			*joint_idx = i;
			return true;
		}
	}

	return false;
}

static void MergePosesRecursive( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 i ) {
	lower[ i ] = upper[ i ];

	const Model::Joint & joint = model->joints[ i ];
	if( joint.sibling != U8_MAX )
		MergePosesRecursive( lower, upper, model, joint.sibling );
	if( joint.first_child != U8_MAX )
		MergePosesRecursive( lower, upper, model, joint.first_child );
}

void MergeLowerUpperPoses( Span< TRS > lower, Span< const TRS > upper, const Model * model, u8 upper_root_joint ) {
	lower[ upper_root_joint ] = upper[ upper_root_joint ];

	const Model::Joint & joint = model->joints[ upper_root_joint ];
	if( joint.first_child != U8_MAX )
		MergePosesRecursive( lower, upper, model, joint.first_child );
}
