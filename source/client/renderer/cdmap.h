#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

struct MapSubModelRenderData {
	StringHash base_hash;
	u32 sub_model;
};

struct MapSharedRenderData {
	Mesh mesh;

	// GPUBuffer nodes;
	// GPUBuffer leaves;
	// GPUBuffer brushes;
	// GPUBuffer planes;
};

struct MapData;
MapSharedRenderData NewMapRenderData( const MapData & map, Span< const char > name );
void DeleteMapRenderData( const MapSharedRenderData & render_data );

void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat3x4 & transform, const Vec4 & color );
