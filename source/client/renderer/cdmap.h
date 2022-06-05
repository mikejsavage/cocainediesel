#pragma once

struct MapRenderData {
	u64 base_hash;
	u32 sub_model;
};

struct MapSharedRenderData {
	Mesh mesh;

	float fog_strength;

	GPUBuffer nodes;
	GPUBuffer leaves;
	GPUBuffer brushes;
	GPUBuffer planes;
};

struct MapData;
MapSharedRenderData NewMapRenderData( const MapData & map, const char * name, u64 base_hash );
void DeleteMapRenderData( const MapRenderData & render_data );

// FindModelRenderData( ... ) -> MapRenderData
// DrawModel( MapRenderData )
// 	MapSharedRenderData = Find( base_hash )
// 	MapData = Find( base_hash )
// 	for( mesh : MapData.models[ sub_model ].meshes ) {
// 		DrawMesh( mesh )
// 	}
