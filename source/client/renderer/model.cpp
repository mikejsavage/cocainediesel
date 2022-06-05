#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/hashtable.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"

#include "cgame/cg_particles.h"
#include "cgame/cg_dynamics.h"

constexpr u32 MAX_MODELS = 1024;

static ModelRenderData models[ MAX_MODELS ];
static u32 num_models;
static Hashtable< MAX_MODELS * 2 > models_hashtable;


static Model gltf_models[ MAX_MODELS ];
static u32 num_gltf_models;
static Hashtable< MAX_MODELS * 2 > gltf_models_hashtable;

constexpr u32 MAX_INSTANCES = 1024;
constexpr u32 MAX_INSTANCE_GROUPS = 128;

template< typename T >
struct ModelInstanceGroup {
	T instances[ MAX_INSTANCES ];
	u32 num_instances;
	PipelineState pipeline;
	GPUBuffer instance_data;
	const Model * model;
	const Model::Primitive * primitive;
};

template< typename T >
struct ModelInstanceCollection {
	ModelInstanceGroup< T > groups[ MAX_INSTANCE_GROUPS ];
	Hashtable< MAX_INSTANCE_GROUPS * 2 > groups_hashtable;
	u32 num_groups;
};

static ModelInstanceCollection< GPUModelInstance > model_instance_collection;
static ModelInstanceCollection< GPUModelShadowsInstance > model_shadows_instance_collection;
static ModelInstanceCollection< GPUModelOutlinesInstance > model_outlines_instance_collection;
static ModelInstanceCollection< GPUModelSilhouetteInstance > model_silhouette_instance_collection;

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

void InitModelInstances();
void ShutdownModelInstances();

void InitModels() {
	TracyZoneScoped;

	num_gltf_models = 0;

	for( const char * path : AssetPaths() ) {
		LoadGLTF( path );
	}

	InitModelInstances();
}

void DeleteModel( Model * model ) {
}

void HotloadModels() {
	TracyZoneScoped;

	for( const char * path : ModifiedAssetPaths() ) {
		LoadGLTF( path );
	}
}

void ShutdownModels() {
	for( u32 i = 0; i < num_gltf_models; i++ ) {
		DeleteModel( &gltf_models[ i ] );
	}

	ShutdownModelInstances();
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

static void DrawModelPrimitiveInstanced( const Model * model, const Model::Primitive * primitive, const PipelineState & pipeline, GPUBuffer instance_data, u32 num_instances, InstanceType instance_type ) {
	if( primitive->num_vertices != 0 ) {
		u32 index_size = model->mesh.indices_format == IndexFormat_U16 ? sizeof( u16 ) : sizeof( u32 );
		DrawInstancedMesh( model->mesh, pipeline, instance_data, num_instances, instance_type, primitive->num_vertices, primitive->first_index * index_size );
	}
	else {
		DrawInstancedMesh( primitive->mesh, pipeline, instance_data, num_instances, instance_type );
	}
}

template< typename T >
static void AddInstanceToCollection( ModelInstanceCollection< T > & collection, const Model * model, const Model::Primitive * primitive, PipelineState pipeline, T & instance, u64 hash ) {
	u64 idx = collection.num_groups;
	if( !collection.groups_hashtable.get( hash, &idx ) ) {
		assert( collection.num_groups < ARRAY_COUNT( collection.groups ) );
		collection.groups_hashtable.add( hash, collection.num_groups );
		collection.groups[ idx ].pipeline = pipeline;
		collection.groups[ idx ].model = model;
		collection.groups[ idx ].primitive = primitive;
		collection.num_groups++;
	}

	assert( collection.groups[ idx ].num_instances < ARRAY_COUNT( collection.groups[ idx ].instances ) );
	collection.groups[ idx ].instances[ collection.groups[ idx ].num_instances++ ] = instance;
}

static void DrawModelNode( DrawModelConfig::DrawModel config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline, u64 hash, Mat4 & transform, GPUMaterial gpu_material ) {
	if( !config.enabled )
		return;

	if( config.view_weapon ) {
		pipeline.view_weapon_depth_hack = true;
	}

	if( skinned ) {
		DrawModelPrimitive( model, primitive, pipeline );
		return;
	}

	hash = Hash64( &config.view_weapon, sizeof( config.view_weapon ), hash );

	GPUModelInstance instance = { };
	instance.material = gpu_material;
	instance.transform[ 0 ] = transform.row0();
	instance.transform[ 1 ] = transform.row1();
	instance.transform[ 2 ] = transform.row2();

	AddInstanceToCollection( model_instance_collection, model, primitive, pipeline, instance, hash );
}

static void DrawShadowsNode( DrawModelConfig::DrawShadows config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline, u64 hash, Mat4 & transform ) {
	if( !config.enabled )
		return;

	pipeline.shader = skinned ? &shaders.depth_only_skinned : &shaders.depth_only_instanced;
	pipeline.clamp_depth = true;
	// pipeline.cull_face = CullFace_Disabled;
	pipeline.write_depth = true;

	for( u32 i = 0; i < frame_static.shadow_parameters.entity_cascades; i++ ) {
		pipeline.pass = frame_static.shadowmap_pass[ i ];
		pipeline.set_uniform( "u_View", frame_static.shadowmap_view_uniforms[ i ] );

		if( skinned ) {
			DrawModelPrimitive( model, primitive, pipeline );
			continue;
		}

		hash = Hash64( &i, sizeof( i ), hash );

		GPUModelShadowsInstance instance = { };
		instance.transform[ 0 ] = transform.row0();
		instance.transform[ 1 ] = transform.row1();
		instance.transform[ 2 ] = transform.row2();

		AddInstanceToCollection( model_shadows_instance_collection, model, primitive, pipeline, instance, hash );
	}
}

static void DrawOutlinesNode( DrawModelConfig::DrawOutlines config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline, UniformBlock outline_uniforms, u64 hash, Mat4 & transform ) {
	if( !config.enabled )
		return;

	pipeline.shader = skinned ? &shaders.outline_skinned : &shaders.outline_instanced;
	pipeline.pass = frame_static.nonworld_opaque_pass;
	pipeline.cull_face = CullFace_Front;

	if( skinned ) {
		pipeline.set_uniform( "u_Outline", outline_uniforms );
		DrawModelPrimitive( model, primitive, pipeline );
		return;
	}

	GPUModelOutlinesInstance instance = { };
	instance.color = config.outline_color;
	instance.height = config.outline_height;
	instance.transform[ 0 ] = transform.row0();
	instance.transform[ 1 ] = transform.row1();
	instance.transform[ 2 ] = transform.row2();

	AddInstanceToCollection( model_outlines_instance_collection, model, primitive, pipeline, instance, hash );
}

static void DrawSilhouetteNode( DrawModelConfig::DrawSilhouette config, const Model * model, const Model::Primitive * primitive, bool skinned, PipelineState pipeline, UniformBlock silhouette_uniforms, u64 hash, Mat4 & transform ) {
	if( !config.enabled )
		return;

	pipeline.shader = skinned ? &shaders.write_silhouette_gbuffer_skinned : &shaders.write_silhouette_gbuffer_instanced;
	pipeline.pass = frame_static.write_silhouette_gbuffer_pass;
	pipeline.write_depth = false;

	if( skinned ) {
		pipeline.set_uniform( "u_Silhouette", silhouette_uniforms );
		DrawModelPrimitive( model, primitive, pipeline );
		return;
	}

	GPUModelSilhouetteInstance instance = { };
	instance.color = config.silhouette_color;
	instance.transform[ 0 ] = transform.row0();
	instance.transform[ 1 ] = transform.row1();
	instance.transform[ 2 ] = transform.row2();

	AddInstanceToCollection( model_silhouette_instance_collection, model, primitive, pipeline, instance, hash );
}

static void DrawVfxNode( DrawModelConfig::DrawModel config, const Model::Node * node, Mat4 & transform, const Vec4 & color ) {
	if( !config.enabled || node->vfx_type == ModelVfxType_Generic )
		return;

	// TODO: idk about this cheers
	Vec3 scale = Vec3( Length( transform.col0.xyz() ), Length( transform.col1.xyz() ), Length( transform.col2.xyz() ) );
	float size = Min2( Min2( Abs( scale.x ), Abs( scale.y ) ), Abs( scale.z ) );

	if( size <= 0.01f )
		return;

	Vec3 origin = transform.col3.xyz();
	Vec3 normal = SafeNormalize( transform.col1.xyz() );
	switch( node->vfx_type ) {
		case ModelVfxType_Vfx:
			DoVisualEffect( node->vfx_node.name, origin, normal, size, node->vfx_node.color * color );
			break;
		case ModelVfxType_DynamicLight:
			DrawDynamicLight( origin, node->dlight_node.color, node->dlight_node.intensity * size );
			break;
		case ModelVfxType_Decal:
			DrawDecal( origin, normal, node->decal_node.radius * size, node->decal_node.angle, node->decal_node.name, node->decal_node.color );
			break;
	}
}

void DrawModel( DrawModelConfig config, const Model * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes ) {
	if( model == NULL )
		return;

	bool animated = palettes.node_transforms.ptr != NULL;
	bool any_skinned = model->skin != NULL;

	UniformBlock pose_uniforms = { };
	if( any_skinned && animated ) {
		pose_uniforms = UploadUniforms( palettes.skinning_matrices.ptr, palettes.skinning_matrices.num_bytes() );
	}

	UniformBlock outline_uniforms = { };
	if( any_skinned && config.draw_outlines.enabled ) {
		outline_uniforms = UploadUniformBlock( config.draw_outlines.outline_color, config.draw_outlines.outline_height );
	}

	UniformBlock silhouette_uniforms = { };
	if( any_skinned && config.draw_silhouette.enabled ) {
		silhouette_uniforms = UploadUniformBlock( config.draw_silhouette.silhouette_color );
	}

	for( u8 i = 0; i < model->num_nodes; i++ ) {
		const Model::Node * node = &model->nodes[ i ];
		if( node->primitive == U8_MAX && node->vfx_type == ModelVfxType_Generic )
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

		DrawVfxNode( config.draw_model, node, node_transform, color );

		if( node->primitive == U8_MAX )
			continue;

		const Model::Primitive * primitive = &model->primitives[ node->primitive ];
		GPUMaterial gpu_material;
		PipelineState pipeline = MaterialToPipelineState( primitive->material, color, skinned, &gpu_material );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );

		// skinned models can't be instanced
		if( skinned ) {
			pipeline.set_uniform( "u_Model", UploadModelUniforms( node_transform ) );
			pipeline.set_uniform( "u_Pose", pose_uniforms );
		}

		u64 hash = Hash64( u64( model ) );
		hash = Hash64( &primitive, sizeof( primitive ), hash );

		DrawModelNode( config.draw_model, model, primitive, skinned, pipeline, hash, node_transform, gpu_material );
		DrawShadowsNode( config.draw_shadows, model, primitive, skinned, pipeline, hash, node_transform );
		DrawOutlinesNode( config.draw_outlines, model, primitive, skinned, pipeline, outline_uniforms, hash, node_transform );
		DrawSilhouetteNode( config.draw_silhouette, model, primitive, skinned, pipeline, silhouette_uniforms, hash, node_transform );
	}
}

void InitModelInstances() {
	TracyZoneScoped;
	for( u32 i = 0; i < MAX_INSTANCE_GROUPS; i++ ) {
		model_instance_collection.groups[ i ].instance_data = NewGPUBuffer( sizeof( GPUModelInstance ) * MAX_INSTANCES );
		model_instance_collection.num_groups = 0;

		model_shadows_instance_collection.groups[ i ].instance_data = NewGPUBuffer( sizeof( GPUModelShadowsInstance ) * MAX_INSTANCES );
		model_shadows_instance_collection.num_groups = 0;

		model_outlines_instance_collection.groups[ i ].instance_data = NewGPUBuffer( sizeof( GPUModelOutlinesInstance ) * MAX_INSTANCES );
		model_outlines_instance_collection.num_groups = 0;

		model_silhouette_instance_collection.groups[ i ].instance_data = NewGPUBuffer( sizeof( GPUModelSilhouetteInstance ) * MAX_INSTANCES );
		model_silhouette_instance_collection.num_groups = 0;
	}
}

void ShutdownModelInstances() {
	for( u32 i = 0; i < MAX_INSTANCE_GROUPS; i++ ) {
		DeleteGPUBuffer( model_instance_collection.groups[ i ].instance_data );
		DeleteGPUBuffer( model_shadows_instance_collection.groups[ i ].instance_data );
		DeleteGPUBuffer( model_outlines_instance_collection.groups[ i ].instance_data );
		DeleteGPUBuffer( model_silhouette_instance_collection.groups[ i ].instance_data );
	}
}

template< typename T >
static void DrawModelInstanceCollection( ModelInstanceCollection< T > & collection, InstanceType instance_type ) {
	for( u32 i = 0; i < collection.num_groups; i++ ) {
		ModelInstanceGroup< T > & group = collection.groups[ i ];
		WriteGPUBuffer( group.instance_data, group.instances, sizeof( T ) * group.num_instances );
		DrawModelPrimitiveInstanced( group.model, group.primitive, group.pipeline, group.instance_data, group.num_instances, instance_type );
		group.num_instances = 0;
	}
	collection.num_groups = 0;
	collection.groups_hashtable.clear();
}

void DrawModelInstances() {
	TracyZoneScoped;

	DrawModelInstanceCollection( model_instance_collection, InstanceType_Model );
	DrawModelInstanceCollection( model_shadows_instance_collection, InstanceType_ModelShadows );
	DrawModelInstanceCollection( model_outlines_instance_collection, InstanceType_ModelOutlines );
	DrawModelInstanceCollection( model_silhouette_instance_collection, InstanceType_ModelSilhouette );
}
