#include "qcommon/base.h"
#include "client/client.h"
#include "client/renderer/api.h"
#include "client/renderer/shader.h"
#include "gameshared/cdmap.h"

Mesh NewMapRenderData( const MapData & map, Span< const char > name ) {
	TempAllocator temp = cls.frame_arena.temp();

	GPUBuffer positions_buffer = NewBuffer( GPULifetime_Persistent, temp( "{} positions", name ), map.vertex_positions );
	GPUBuffer normals_buffer = NewBuffer( GPULifetime_Persistent, temp( "{} normals", name ), map.vertex_normals );

	Mesh mesh = { };
	mesh.vertex_descriptor.attributes[ VertexAttribute_Position ] = { VertexFormat_Floatx3, 0, 0 };
	mesh.vertex_descriptor.attributes[ VertexAttribute_Normal ] = { VertexFormat_Floatx3, 1, 0 };
	mesh.vertex_descriptor.buffer_strides[ VertexAttribute_Position ] = sizeof( Vec3 );
	mesh.vertex_descriptor.buffer_strides[ VertexAttribute_Normal ] = sizeof( Vec3 );
	mesh.index_format = IndexFormat_U32,
	mesh.num_vertices = map.vertex_positions.n;
	mesh.vertex_buffers[ VertexAttribute_Position ] = positions_buffer;
	mesh.vertex_buffers[ VertexAttribute_Position ] = normals_buffer;
	mesh.index_buffer = NewBuffer( GPULifetime_Persistent, temp( "{} indices", name ), map.vertex_indices );

	return mesh;
}

void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat3x4 & transform, const Vec4 & color ) {
	if( render_data == NULL )
		return;

	const Map * map = FindMap( render_data->base_hash );

	BufferBinding model_binding = { "u_Model", NewTempBuffer( transform ) };

	u32 first_mesh = map->data.models[ render_data->sub_model ].first_mesh;
	for( u32 i = 0; i < map->data.models[ render_data->sub_model ].num_meshes; i++ ) {
		const MapMesh & mesh = map->data.meshes[ i + first_mesh ];
		DrawCallExtras mesh_extras = {
			.override_num_vertices = mesh.num_vertices,
			.first_index = mesh.first_vertex_index,
		};


		for( u32 j = 0; j < frame_static.shadow_parameters.num_cascades; j++ ) {
			// pipeline.clamp_depth = true;
			PipelineState pipeline = { .shader = shaders.depth_only };
			EncodeDrawCall( RenderPass_ShadowmapCascade0 + j, pipeline, map->render_data, { model_binding }, mesh_extras );
		}

		{
			// PipelineState pipeline = MaterialToPipelineState( FindMaterial( StringHash( mesh.material ) ) ); TODO why?
			PipelineState pipeline = { .shader = shaders.depth_only };
			EncodeDrawCall( RenderPass_WorldOpaqueZPrepass, pipeline, map->render_data, { model_binding }, mesh_extras );
		}

		{
			const Material * material;
			// TODO: remove this fallback at some point
			if( !TryFindMaterial( StringHash( mesh.material ), &material ) ) {
				material = FindMaterial( "world" );
			}

			RenderPass pass;
			PipelineState pipeline = MaterialToPipelineState( material, &pass );
			pipeline.dynamic_state.depth_func = DepthFunc_EqualNoWrite;
			EncodeDrawCall( pass, pipeline, map->render_data, { model_binding }, mesh_extras );
		}
	}
}
