#include "qcommon/base.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "gameshared/cdmap.h"

MapSharedRenderData NewMapRenderData( const MapData & map, const char * name, u64 base_hash ) {
	TempAllocator temp = cls.frame_arena.temp();

	MapSharedRenderData shared = { };

	MeshConfig mesh_config;
	mesh_config.unified_buffer = NewGPUBuffer( map.vertices, temp( "{} vertices", name ) );
	mesh_config.positions_offset = offsetof( MapVertex, position );
	mesh_config.normals_offset = offsetof( MapVertex, normal );
	mesh_config.indices = NewGPUBuffer( map.vertex_indices, temp( "{} indices", name ) );
	mesh_config.indices_format = IndexFormat_U32;
	mesh_config.num_vertices = map.vertex_indices.n;

	shared.mesh = NewMesh( mesh_config );

	shared.fog_strength = 0.0007f;

	// carfentanil*0
	for( size_t i = 0; i < map.models.n; i++ ) {
		const char * suffix = temp( "*{}", i );
		u64 hash = Hash64( suffix, strlen( suffix ), base_hash );

		ModelRenderData render_data = { };
		render_data.type = ModelType_Map;
		render_data.map.base_hash = base_hash;
		render_data.map.sub_model = checked_cast< u32 >( i );

		// TODO: AddModel( hash, render_data );
	}

	return shared;
}

void DeleteMapRenderData( const MapRenderData & render_data ) {
	DeleteMesh( render_data.mesh );
	// DeleteGPUBuffer( render_data.nodes );
	// DeleteGPUBuffer( render_data.leaves );
	// DeleteGPUBuffer( render_data.brushes );
	// DeleteGPUBuffer( render_data.planes );
}
