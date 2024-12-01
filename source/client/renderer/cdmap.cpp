#include "qcommon/base.h"
#include "client/client.h"
#include "client/renderer/api.h"
#include "gameshared/cdmap.h"

Mesh NewMapRenderData( const MapData & map, Span< const char > name ) {
	TempAllocator temp = cls.frame_arena.temp();

	GPUBuffer positions_buffer = NewBuffer( GPULifetime_Persistent, temp( "{} positions", name ), map.vertex_positions );
	GPUBuffer normals_buffer = NewBuffer( GPULifetime_Persistent, temp( "{} normals", name ), map.vertex_normals );

	Mesh mesh = { };
	mesh.vertex_descriptor.attributes[ VertexAttribute_Position ] = { VertexFormat_Floatx3, 0, 0 };
	mesh.vertex_descriptor.attributes[ VertexAttribute_Normal ] = { VertexFormat_Floatx3, 1, 0 };
	mesh.vertex_descriptor.buffer_strides = { sizeof( Vec3 ), sizeof( Vec3 ) };
	mesh.index_format = IndexFormat_U32,
	mesh.num_vertices = map_mesh.num_vertices,
	mesh.vertex_buffers = { positions_buffer, normals_buffer },
	mesh.index_buffer = NewBuffer( GPULifetime_Persistent, temp( "{} indices", name ), map.vertex_indices );

	return mesh;
}

void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat3x4 & transform, const Vec4 & color ) {
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

		{
			PipelineState pipeline = MaterialToPipelineState( FindMaterial( StringHash( mesh.material ) ) );
			pipeline.pass = frame_static.world_opaque_prepass_pass;
			pipeline.shader = &shaders.depth_only;
			pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
			pipeline.bind_uniform( "u_Model", UploadModelUniforms( transform ) );

			DrawMesh( map->render_data.mesh, pipeline, mesh.num_vertices, mesh.first_vertex_index );
		}

		{
			const Material * material;
			if( !TryFindMaterial( StringHash( mesh.material ), &material ) ) {
				material = FindMaterial( "world" );
			}

			PipelineState pipeline = MaterialToPipelineState( material );
			pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
			pipeline.bind_uniform( "u_Model", frame_static.identity_model_uniforms );
			pipeline.write_depth = false;
			pipeline.depth_func = DepthFunc_Equal;

			DrawMesh( map->render_data.mesh, pipeline, mesh.num_vertices, mesh.first_vertex_index );
		}
	}
}
