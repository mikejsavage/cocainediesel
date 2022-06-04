#include "qcommon/base.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/maps.h"
#include "gameshared/cdmap.h"

MapRenderData NewMapRenderData( const MapData & map, const char * name ) {
	TempAllocator temp = cls.frame_arena.temp();

	MapRenderData render_data = { };

	MeshConfig mesh_config;
	mesh_config.unified_buffer = NewGPUBuffer( map.vertices, temp( "{} vertices", name ) );
	mesh_config.positions_offset = offsetof( MapVertex, position );
	mesh_config.normals_offset = offsetof( MapVertex, normal );
	mesh_config.indices = NewGPUBuffer( map.vertex_indices, temp( "{} indices", name ) );
	mesh_config.indices_format = IndexFormat_U32;
	mesh_config.num_vertices = map.vertex_indices.n;

	render_data.mesh = NewMesh( mesh_config );

	render_data.fog_strength = 0.0007f;

	return render_data;
}

void DeleteMapRenderData( const MapRenderData & render_data ) {
	DeleteMesh( render_data.mesh );
	DeleteGPUBuffer( render_data.nodes );
	DeleteGPUBuffer( render_data.leaves );
	DeleteGPUBuffer( render_data.brushes );
	DeleteGPUBuffer( render_data.planes );
}
