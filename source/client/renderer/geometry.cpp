#include "qcommon/base.h"
#include "qcommon/array.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_constants.h"
#include "cgame/cg_dynamics.h"

constexpr size_t MAX_MULTIDRAW = 512;

struct FragmentAttribute {
	U10_U2_Vec4 normal;
	U16_Vec2 texcoord;
};

struct StaticGeometry {
	struct VertexAttribute {
		U16_Vec3 position;
	};

	NonRAIIDynamicArray< VertexAttribute > vertex_attributes;
	GPUBuffer vertex_attributes_buffer;
	NonRAIIDynamicArray< FragmentAttribute > fragment_attributes;
	GPUBuffer fragment_attributes_buffer;
	u32 num_vertices;

	NonRAIIDynamicArray< u16 > indices;
	GPUBuffer indices_buffer;
	u32 num_indices;

	Mesh mesh;
};

struct SkinnedStaticGeometry {
	struct VertexAttribute {
		U16_Vec3 position;
		U8_Vec4 joint_indices;
		U16_Vec4 joint_weights;
	};

	NonRAIIDynamicArray< VertexAttribute > vertex_attributes;
	GPUBuffer vertex_attributes_buffer;
	NonRAIIDynamicArray< FragmentAttribute > fragment_attributes;
	GPUBuffer fragment_attributes_buffer;
	u32 num_vertices;

	NonRAIIDynamicArray< u16 > indices;
	GPUBuffer indices_buffer;
	u32 num_indices;

	Mesh mesh;
};

static StaticGeometry static_geometry;
static SkinnedStaticGeometry skinned_static_geometry;

template< typename T >
struct MultiDrawList {
	MultiDrawElement elements[ MAX_MULTIDRAW ];
	StreamingBuffer elements_buffer;
	T instances[ MAX_MULTIDRAW ];
	StreamingBuffer instances_buffer;

	u32 num_elements;
};

template< typename T >
struct SkinnedMultiDrawList {
	MultiDrawElement elements[ MAX_MULTIDRAW ];
	StreamingBuffer elements_buffer;
	T instances[ MAX_MULTIDRAW ];
	StreamingBuffer instances_buffer;
	GPUSkinningInstance skinning_instances[ MAX_MULTIDRAW ];
	StreamingBuffer skinning_instances_buffer;

	u32 num_elements;
};

template< typename T >
struct CombinedMultiDrawList {
	MultiDrawList< T > normal;
	SkinnedMultiDrawList< T > skinned;
};

static Mat4 skinning_matrices[ SKINNED_MODEL_MAX_JOINTS * MAX_MULTIDRAW ];
static StreamingBuffer skinning_matrices_buffer;
static size_t num_skinning_matrices;

static CombinedMultiDrawList< GPUModelInstance > model_list;
static CombinedMultiDrawList< GPUModelShadowsInstance > model_shadow_list;
static CombinedMultiDrawList< GPUModelOutlinesInstance > model_outline_list;
static CombinedMultiDrawList< GPUModelSilhouetteInstance > model_silhouette_list;

template< typename T >
static void InitCombinedMultiDrawList( CombinedMultiDrawList< T > & list ) {
	list.normal.elements_buffer = NewStreamingBuffer( sizeof( list.normal.elements ) );
	list.normal.instances_buffer = NewStreamingBuffer( sizeof( list.normal.instances ) );
	list.normal.num_elements = 0;

	list.skinned.elements_buffer = NewStreamingBuffer( sizeof( list.skinned.elements ) );
	list.skinned.instances_buffer = NewStreamingBuffer( sizeof( list.skinned.instances ) );
	list.skinned.skinning_instances_buffer = NewStreamingBuffer( sizeof( list.skinned.skinning_instances ) );
	list.skinned.num_elements = 0;
}

template< typename T >
static void ShutdownCombinedMultiDrawList( CombinedMultiDrawList< T > & list ) {
	DeleteStreamingBuffer( list.normal.elements_buffer );
	DeleteStreamingBuffer( list.normal.instances_buffer );
	list.normal.num_elements = 0;

	DeleteStreamingBuffer( list.skinned.elements_buffer );
	DeleteStreamingBuffer( list.skinned.instances_buffer );
	DeleteStreamingBuffer( list.skinned.skinning_instances_buffer );
	list.skinned.num_elements = 0;
}

void InitGeometry() {
	{
		static_geometry.vertex_attributes.init( sys_allocator );
		static_geometry.fragment_attributes.init( sys_allocator );
		static_geometry.indices.init( sys_allocator );
	}

	{
		skinned_static_geometry.vertex_attributes.init( sys_allocator );
		skinned_static_geometry.fragment_attributes.init( sys_allocator );
		skinned_static_geometry.indices.init( sys_allocator );
	}

	InitCombinedMultiDrawList( model_list );
	InitCombinedMultiDrawList( model_shadow_list );
	InitCombinedMultiDrawList( model_outline_list );
	InitCombinedMultiDrawList( model_silhouette_list );

	{
		skinning_matrices_buffer = NewStreamingBuffer( sizeof( skinning_matrices ), "skinning matrices" );
		num_skinning_matrices = 0;
	}
}

void ShutdownGeometry() {
	static_geometry.vertex_attributes.shutdown();
	static_geometry.fragment_attributes.shutdown();
	static_geometry.indices.shutdown();
	DeleteMesh( static_geometry.mesh );
	
	skinned_static_geometry.vertex_attributes.shutdown();
	skinned_static_geometry.fragment_attributes.shutdown();
	skinned_static_geometry.indices.shutdown();
	DeleteMesh( skinned_static_geometry.mesh );

	ShutdownCombinedMultiDrawList( model_list );
	ShutdownCombinedMultiDrawList( model_shadow_list );
	ShutdownCombinedMultiDrawList( model_outline_list );
	ShutdownCombinedMultiDrawList( model_silhouette_list );

	DeleteStreamingBuffer( skinning_matrices_buffer );
	num_skinning_matrices = 0;
}

void UploadGeometry() {
	{
		DeleteMesh( static_geometry.mesh );

		static_geometry.vertex_attributes_buffer = NewGPUBuffer( static_geometry.vertex_attributes.span(), "Static Geometry vertex attributes" );
		static_geometry.fragment_attributes_buffer = NewGPUBuffer( static_geometry.fragment_attributes.span(), "Static Geometry fragment attributes" );
		static_geometry.indices_buffer = NewGPUBuffer( static_geometry.indices.span(), "Static Geometry indices" );

		MeshConfig config = { };
		config.name = "Static Geometry";
		config.vertex_descriptor.buffer_strides[ VertexAttribute_Position ] = sizeof( StaticGeometry::VertexAttribute );
		config.vertex_descriptor.buffer_strides[ VertexAttribute_Normal ] = sizeof( FragmentAttribute );
		config.vertex_descriptor.buffer_strides[ VertexAttribute_TexCoord ] = sizeof( FragmentAttribute );
		config.set_attribute( VertexAttribute_Position, static_geometry.vertex_attributes_buffer, VertexFormat_U16x3_Norm, offsetof( StaticGeometry::VertexAttribute, position ) );
		config.set_attribute( VertexAttribute_Normal, static_geometry.fragment_attributes_buffer, VertexFormat_U10x3_U2x1_Norm, offsetof( FragmentAttribute, normal ) );
		config.set_attribute( VertexAttribute_TexCoord, static_geometry.fragment_attributes_buffer, VertexFormat_U16x2_Norm, offsetof( FragmentAttribute, texcoord ) );

		config.index_buffer = static_geometry.indices_buffer;
		config.index_format = IndexFormat_U16;

		static_geometry.mesh = NewMesh( config );
	}

	{
		DeleteMesh( skinned_static_geometry.mesh );

		skinned_static_geometry.vertex_attributes_buffer = NewGPUBuffer( skinned_static_geometry.vertex_attributes.span(), "Skinned Static Geometry vertex attributes" );
		skinned_static_geometry.fragment_attributes_buffer = NewGPUBuffer( skinned_static_geometry.fragment_attributes.span(), "Skinned Static Geometry fragment attributes" );
		skinned_static_geometry.indices_buffer = NewGPUBuffer( skinned_static_geometry.indices.span(), "Skinned Static Geometry indices" );

		MeshConfig config = { };
		config.name = "Skinned Static Geometry";
		config.vertex_descriptor.buffer_strides[ VertexAttribute_Position ] = sizeof( SkinnedStaticGeometry::VertexAttribute );
		config.vertex_descriptor.buffer_strides[ VertexAttribute_Normal ] = sizeof( FragmentAttribute );
		config.vertex_descriptor.buffer_strides[ VertexAttribute_TexCoord ] = sizeof( FragmentAttribute );
		config.vertex_descriptor.buffer_strides[ VertexAttribute_JointIndices ] = sizeof( SkinnedStaticGeometry::VertexAttribute );
		config.vertex_descriptor.buffer_strides[ VertexAttribute_JointWeights ] = sizeof( SkinnedStaticGeometry::VertexAttribute );
		config.set_attribute( VertexAttribute_Position, skinned_static_geometry.vertex_attributes_buffer, VertexFormat_U16x3_Norm, offsetof( SkinnedStaticGeometry::VertexAttribute, position ) );
		config.set_attribute( VertexAttribute_JointIndices, skinned_static_geometry.vertex_attributes_buffer, VertexFormat_U8x4, offsetof( SkinnedStaticGeometry::VertexAttribute, joint_indices ) );
		config.set_attribute( VertexAttribute_JointWeights, skinned_static_geometry.vertex_attributes_buffer, VertexFormat_U16x4_Norm, offsetof( SkinnedStaticGeometry::VertexAttribute, joint_weights ) );

		config.set_attribute( VertexAttribute_Normal, skinned_static_geometry.fragment_attributes_buffer, VertexFormat_U10x3_U2x1_Norm, offsetof( FragmentAttribute, normal ) );
		config.set_attribute( VertexAttribute_TexCoord, skinned_static_geometry.fragment_attributes_buffer, VertexFormat_U16x2_Norm, offsetof( FragmentAttribute, texcoord ) );

		config.index_buffer = skinned_static_geometry.indices_buffer;
		config.index_format = IndexFormat_U16;

		skinned_static_geometry.mesh = NewMesh( config );
	}
}

GPUSkinningInstance UploadSkinningMatrices( MatrixPalettes palettes ) {
	for( size_t i = 0; i < palettes.skinning_matrices.n; i++ ) {
		skinning_matrices[ num_skinning_matrices + i ] = palettes.skinning_matrices[ i ];
	}

	GPUSkinningInstance skinning_instance = { };
	skinning_instance.offset = num_skinning_matrices;

	num_skinning_matrices += palettes.skinning_matrices.n;

	return skinning_instance;
}

template< typename T >
static void SubmitMultiDraw( PipelineState pipeline, MultiDrawList< T > & list ) {
	WriteAndFlushStreamingBuffer( list.elements_buffer, list.elements, list.num_elements );
	WriteAndFlushStreamingBuffer( list.instances_buffer, list.instances, list.num_elements );

	pipeline.bind_streaming_buffer( "b_Instances", list.instances_buffer );
	DrawMultiIndirect( static_geometry.mesh, pipeline, list.elements_buffer, list.num_elements );
}

template< typename T >
static void SubmitSkinnedMultiDraw( PipelineState pipeline, SkinnedMultiDrawList< T > & list ) {
	WriteAndFlushStreamingBuffer( list.elements_buffer, list.elements, list.num_elements );
	WriteAndFlushStreamingBuffer( list.instances_buffer, list.instances, list.num_elements );
	WriteAndFlushStreamingBuffer( list.skinning_instances_buffer, list.skinning_instances, list.num_elements );

	pipeline.bind_streaming_buffer( "b_Instances", list.instances_buffer );
	pipeline.bind_streaming_buffer( "b_SkinningInstances", list.skinning_instances_buffer );
	pipeline.bind_streaming_buffer( "b_SkinningMatrices", skinning_matrices_buffer );
	DrawMultiIndirect( skinned_static_geometry.mesh, pipeline, list.elements_buffer, list.num_elements );
}

template< typename T >
static void ResetCombinedMultiDrawList( CombinedMultiDrawList< T > & list ) {
	list.normal.num_elements = 0;
	list.skinned.num_elements = 0;
}

template< typename T > static void SubmitMultiDraw( CombinedMultiDrawList< T > & list );

void SubmitMultiDraw( CombinedMultiDrawList< GPUModelInstance > & list) {
	if( list.normal.num_elements > 0 ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.nonworld_opaque_outlined_pass;
		pipeline.shader = &shaders.standard_multidraw;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
		pipeline.bind_texture_and_sampler( "u_BaseTexture", cls.white_material->texture, Sampler_Standard );
		AddDynamicsToPipeline( &pipeline );

		SubmitMultiDraw( pipeline, list.normal );
	}
	if( list.skinned.num_elements > 0 ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.nonworld_opaque_outlined_pass;
		pipeline.shader = &shaders.standard_skinned_multidraw;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
		pipeline.bind_texture_and_sampler( "u_BaseTexture", cls.white_material->texture, Sampler_Standard );
		AddDynamicsToPipeline( &pipeline );

		SubmitSkinnedMultiDraw( pipeline, list.skinned );
	}
	ResetCombinedMultiDrawList( list );
}

void SubmitMultiDraw( CombinedMultiDrawList< GPUModelOutlinesInstance > & list ) {
	if( list.normal.num_elements > 0 ) {
		Com_GGPrint( "{} outline instances", list.normal.num_elements );
		PipelineState pipeline;
		pipeline.pass = frame_static.nonworld_opaque_pass;
		pipeline.shader = &shaders.outline_multidraw;
		pipeline.cull_face = CullFace_Front;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );

		SubmitMultiDraw( pipeline, list.normal );
	}
	if( list.skinned.num_elements > 0 ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.nonworld_opaque_pass;
		pipeline.shader = &shaders.outline_skinned_multidraw;
		pipeline.cull_face = CullFace_Front;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );

		SubmitSkinnedMultiDraw( pipeline, list.skinned );
	}
	ResetCombinedMultiDrawList( list );
}

void SubmitMultiDraw( CombinedMultiDrawList< GPUModelShadowsInstance > & list ) {
	for( size_t i = 0; i < frame_static.shadow_parameters.entity_cascades; i++ ) {
		if( list.normal.num_elements > 0 ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.shadowmap_pass[ i ];
			pipeline.shader = &shaders.depth_only_multidraw;
			pipeline.clamp_depth = true;
			pipeline.write_depth = true;
			pipeline.bind_uniform( "u_View", frame_static.shadowmap_view_uniforms[ i ] );

			SubmitMultiDraw( pipeline, list.normal );
		}
		if( list.skinned.num_elements > 0 ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.shadowmap_pass[ i ];
			pipeline.shader = &shaders.depth_only_skinned_multidraw;
			pipeline.clamp_depth = true;
			pipeline.write_depth = true;
			pipeline.bind_uniform( "u_View", frame_static.shadowmap_view_uniforms[ i ] );

			SubmitSkinnedMultiDraw( pipeline, list.skinned );
		}
	}
	ResetCombinedMultiDrawList( list );
}

void SubmitMultiDraw( CombinedMultiDrawList< GPUModelSilhouetteInstance > & list ) {
	if( list.normal.num_elements > 0 ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.write_silhouette_gbuffer_pass;
		pipeline.shader = &shaders.write_silhouette_gbuffer_multidraw;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );

		SubmitMultiDraw( pipeline, list.normal );
	}
	if( list.skinned.num_elements > 0 ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.write_silhouette_gbuffer_pass;
		pipeline.shader = &shaders.write_silhouette_gbuffer_skinned_multidraw;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );

		SubmitSkinnedMultiDraw( pipeline, list.skinned );
	}
	ResetCombinedMultiDrawList( list );
}

void DrawGeometry() {
	if( num_skinning_matrices > 0 ) {
		WriteAndFlushStreamingBuffer( skinning_matrices_buffer, skinning_matrices, num_skinning_matrices );
		num_skinning_matrices = 0;
	}
	
	SubmitMultiDraw( model_list );
	SubmitMultiDraw( model_outline_list );
	SubmitMultiDraw( model_shadow_list );
	SubmitMultiDraw( model_silhouette_list );
}

StaticMesh NewStaticMesh( const StaticMeshConfig & config ) {
	TracyCMessage( config.name, strlen( config.name ) );
	StaticMesh mesh = { };

	mesh.skinned = config.vertex_data[ VertexAttribute_JointIndices ].n != 0;
	mesh.num_indices = config.num_indices;

	mesh.base_vertex = static_geometry.num_vertices;
	mesh.first_index = static_geometry.num_indices;
	static_geometry.num_vertices += config.num_vertices;
	static_geometry.num_indices += config.num_indices;

	if( mesh.skinned ) {
		mesh.skinned_base_vertex = skinned_static_geometry.num_vertices;
		mesh.skinned_first_index = skinned_static_geometry.num_indices;
		skinned_static_geometry.num_vertices += config.num_vertices;
		skinned_static_geometry.num_indices += config.num_indices;
	}

	for( size_t i = 0; i < config.num_vertices; i++ ) {
		FragmentAttribute fragment_attrib = { };
		fragment_attrib.normal = U10_U2_Vec4( config.vertex_data[ VertexAttribute_Normal ].cast< const Vec3 >()[ i ] );
		switch( config.vertex_formats[ VertexAttribute_TexCoord ] ) {
			case VertexFormat_Floatx2: {
				fragment_attrib.texcoord = U16_Vec2( config.vertex_data[ VertexAttribute_TexCoord ].cast< const Vec2 >()[ i ] );
			} break;
			case VertexFormat_U16x2_Norm: {
				fragment_attrib.texcoord = config.vertex_data[ VertexAttribute_TexCoord ].cast< const U16_Vec2 >()[ i ];
			} break;
			case VertexFormat_U8x2_Norm: {
				fragment_attrib.texcoord = U16_Vec2( config.vertex_data[ VertexAttribute_TexCoord ].cast< const U8_Vec2 >()[ i ] );
			} break;
		}

		{
			StaticGeometry::VertexAttribute vertex_attrib = { };
			vertex_attrib.position = U16_Vec3( config.vertex_data[ VertexAttribute_Position ].cast< const Vec3 >()[ i ], config.bounds );

			static_geometry.vertex_attributes.add( vertex_attrib );
			static_geometry.fragment_attributes.add( fragment_attrib );
		}

		if( mesh.skinned ) {
			SkinnedStaticGeometry::VertexAttribute vertex_attrib = { };
			vertex_attrib.position = U16_Vec3( config.vertex_data[ VertexAttribute_Position ].cast< const Vec3 >()[ i ], config.bounds );
			vertex_attrib.joint_indices = U8_Vec4( config.vertex_data[ VertexAttribute_JointIndices ].cast< const U16_Vec4 >()[ i ] );
			switch( config.vertex_formats[ VertexAttribute_JointWeights ] ) {
				case VertexFormat_Floatx4: {
					vertex_attrib.joint_weights = U16_Vec4( config.vertex_data[ VertexAttribute_JointWeights ].cast< const Vec4 >()[ i ] );
				} break;
				case VertexFormat_U16x4_Norm: {
					vertex_attrib.joint_weights = config.vertex_data[ VertexAttribute_JointWeights ].cast< const U16_Vec4 >()[ i ];
				} break;
				case VertexFormat_U8x4_Norm: {
					vertex_attrib.joint_weights = U16_Vec4( config.vertex_data[ VertexAttribute_JointWeights ].cast< const U8_Vec4 >()[ i ] );
				} break;
			}

			skinned_static_geometry.vertex_attributes.add( vertex_attrib );
			skinned_static_geometry.fragment_attributes.add( fragment_attrib );
		}
	}

	for( size_t i = 0; i < config.num_indices; i++ ) {
    u16 index = config.index_format == IndexFormat_U16 ? config.index_data.cast< const u16 >()[ i ] : config.index_data.cast< const u32 >()[ i ];
		static_geometry.indices.add( index );
	  if( mesh.skinned ) {
		  skinned_static_geometry.indices.add( index );
    }
	}

	mesh.transform = Mat4Translation( config.bounds.mins ) * Mat4Scale( config.bounds.maxs - config.bounds.mins );

	return mesh;
}

template< typename T >
static void AddMeshToList( MultiDrawList< T > & list, StaticMesh mesh, T instance ) {
	MultiDrawElement element = { };
	element.num_vertices = mesh.num_indices;
	element.num_instances = 1;
	element.first_index = mesh.first_index;
	element.base_vertex = mesh.base_vertex;
	element.base_instance = 0;
	list.elements[ list.num_elements ] = element;
	list.instances[ list.num_elements ] = instance;
	list.num_elements++;
}

template< typename T >
static void AddMeshToList( SkinnedMultiDrawList< T > & list, StaticMesh mesh, T instance, GPUSkinningInstance skinning_instance ) {
	MultiDrawElement element = { };
	element.num_vertices = mesh.num_indices;
	element.num_instances = 1;
	element.first_index = mesh.skinned_first_index;
	element.base_vertex = mesh.skinned_base_vertex;
	element.base_instance = 0;
	list.elements[ list.num_elements ] = element;
	list.instances[ list.num_elements ] = instance;
	list.skinning_instances[ list.num_elements ] = skinning_instance;
	list.num_elements++;
}

template< typename T > static CombinedMultiDrawList< T > GetDrawList();
template<> CombinedMultiDrawList< GPUModelInstance > GetDrawList() { return model_list; }
template<> CombinedMultiDrawList< GPUModelShadowsInstance > GetDrawList() { return model_shadow_list; }
template<> CombinedMultiDrawList< GPUModelOutlinesInstance > GetDrawList() { return model_outline_list; }
template<> CombinedMultiDrawList< GPUModelSilhouetteInstance > GetDrawList() { return model_silhouette_list; }

template< typename T > void DrawStaticMesh( StaticMesh mesh, T instance );
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelInstance instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_list.normal, mesh, instance );
}
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelShadowsInstance instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_shadow_list.normal, mesh, instance );
}
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelOutlinesInstance instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_outline_list.normal, mesh, instance );
}
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelSilhouetteInstance instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_silhouette_list.normal, mesh, instance );
}

template< typename T > void DrawStaticMesh( StaticMesh mesh, T instance, GPUSkinningInstance skinning_instance );
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelInstance instance, GPUSkinningInstance skinning_instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_list.skinned, mesh, instance, skinning_instance );
}
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelShadowsInstance instance, GPUSkinningInstance skinning_instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_shadow_list.skinned, mesh, instance, skinning_instance );
}
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelOutlinesInstance instance, GPUSkinningInstance skinning_instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_outline_list.skinned, mesh, instance, skinning_instance );
}
template<> void DrawStaticMesh( StaticMesh mesh, GPUModelSilhouetteInstance instance, GPUSkinningInstance skinning_instance ) {
	instance.denormalize = Mat3x4( mesh.transform );
	AddMeshToList( model_silhouette_list.skinned, mesh, instance, skinning_instance );
}
