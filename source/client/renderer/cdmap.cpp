#include "qcommon/base.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "gameshared/cdmap.h"

MapSharedRenderData NewMapRenderData( const MapData & map, const char * name, u64 base_hash ) {
	TempAllocator temp = cls.frame_arena.temp();

	MeshConfig mesh_config;
	mesh_config.name = name;
	mesh_config.unified_buffer = NewGPUBuffer( map.vertices, temp( "{} vertices", name ) );
	mesh_config.stride = sizeof( MapVertex );
	mesh_config.positions_offset = offsetof( MapVertex, position );
	mesh_config.normals_offset = offsetof( MapVertex, normal );
	mesh_config.indices = NewGPUBuffer( map.vertex_indices, temp( "{} indices", name ) );
	mesh_config.indices_format = IndexFormat_U32;
	mesh_config.num_vertices = map.vertex_indices.n;

	MapSharedRenderData shared = { };
	shared.mesh = NewMesh( mesh_config );
	shared.fog_strength = 0.0007f;

	return shared;
}

void DeleteMapRenderData( const MapSharedRenderData & render_data ) {
	DeleteMesh( render_data.mesh );
	DeleteGPUBuffer( render_data.nodes );
	DeleteGPUBuffer( render_data.leaves );
	DeleteGPUBuffer( render_data.brushes );
	DeleteGPUBuffer( render_data.planes );
}


void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat4 & transform, const Vec4 & color ) {
	if( render_data == NULL )
		return;

	const Map * map = FindMap( render_data->base_hash );

	u32 first_mesh = map->data.models[ render_data->sub_model ].first_mesh;
	for( u32 i = 0; i < map->data.models[ render_data->sub_model ].num_meshes; i++ ) {
		const MapMesh & mesh = map->data.meshes[ i + first_mesh ];

		PipelineState pipeline = MaterialToPipelineState( FindMaterial( StringHash( mesh.material ) ), color );
		pipeline.pass = frame_static.world_opaque_prepass_pass;
		pipeline.shader = &shaders.depth_only;
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Model", UploadModelUniforms( transform ) );

		DrawMesh( map->render_data.mesh, pipeline, mesh.num_vertices, mesh.first_vertex_index );
	}
}
