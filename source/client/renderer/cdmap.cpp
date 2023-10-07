#include "qcommon/base.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "gameshared/cdmap.h"

MapSharedRenderData NewMapRenderData( const MapData & map, const char * name ) {
	TempAllocator temp = cls.frame_arena.temp();

	MeshConfig mesh_config = { };
	mesh_config.name = name;
	mesh_config.set_attribute( VertexAttribute_Position, NewGPUBuffer( map.vertex_positions, temp( "{} positions", name ) ) );
	mesh_config.set_attribute( VertexAttribute_Normal, NewGPUBuffer( map.vertex_normals, temp( "{} normals", name ) ) );
	mesh_config.index_buffer = NewGPUBuffer( map.vertex_indices, temp( "{} indices", name ) );
	mesh_config.index_format = IndexFormat_U32;
	mesh_config.num_vertices = map.vertex_indices.n;

	MapSharedRenderData shared = { };
	shared.mesh = NewMesh( mesh_config );

	return shared;
}

void DeleteMapRenderData( const MapSharedRenderData & render_data ) {
	DeleteMesh( render_data.mesh );
	// DeleteGPUBuffer( render_data.nodes );
	// DeleteGPUBuffer( render_data.leaves );
	// DeleteGPUBuffer( render_data.brushes );
	// DeleteGPUBuffer( render_data.planes );
}

void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat4 & transform, const Vec4 & color ) {
	if( render_data == NULL )
		return;

	const Map * map = FindMap( render_data->base_hash );

	u32 first_mesh = map->data.models[ render_data->sub_model ].first_mesh;
	for( u32 i = 0; i < map->data.models[ render_data->sub_model ].num_meshes; i++ ) {
		const MapMesh & mesh = map->data.meshes[ i + first_mesh ];

		for( u32 j = 0; j < frame_static.shadow_parameters.num_cascades; j++ ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.shadowmap_pass[ j ];
			pipeline.shader = &shaders.depth_only;
			pipeline.clamp_depth = true;
			pipeline.bind_uniform( "u_View", frame_static.shadowmap_view_uniforms[ j ] );
			pipeline.bind_uniform( "u_Model", frame_static.identity_model_uniforms );

			DrawMesh( map->render_data.mesh, pipeline, mesh.num_vertices, mesh.first_vertex_index );
		}

		const Material * material = FindMaterial( StringHash( mesh.material ), &world_material );
		GPUMaterial gpu_material = GetGPUMaterial( material );

		{
			PipelineState pipeline = MaterialToPipelineState( material, gpu_material );
			pipeline.pass = frame_static.world_opaque_prepass_pass;
			pipeline.shader = &shaders.depth_only;
			pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
			pipeline.bind_uniform( "u_Model", UploadModelUniforms( transform ) );

			DrawMesh( map->render_data.mesh, pipeline, mesh.num_vertices, mesh.first_vertex_index );
		}

		{
			PipelineState pipeline = MaterialToPipelineState( material, gpu_material );
			pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
			pipeline.bind_uniform( "u_Model", frame_static.identity_model_uniforms );
			pipeline.write_depth = false;
			pipeline.depth_func = DepthFunc_Equal;

			DrawMesh( map->render_data.mesh, pipeline, mesh.num_vertices, mesh.first_vertex_index );
		}
	}
}
