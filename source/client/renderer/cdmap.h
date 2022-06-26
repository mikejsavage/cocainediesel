#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

struct MapSubModelRenderData {
	StringHash base_hash;
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
MapSharedRenderData NewMapRenderData( const MapData & map, const char * name );
void DeleteMapRenderData( const MapSharedRenderData & render_data );

void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat4 & transform, const Vec4 & color );
