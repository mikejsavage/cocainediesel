#pragma once

#include "qcommon/types.h"
#include "client/renderer/api.h"

struct MapSubModelRenderData {
	StringHash base_hash;
	u32 sub_model;
};

struct MapData;
Mesh NewMapRenderData( const MapData & map, Span< const char > name );

struct DrawModelConfig;
void DrawMapModel( const DrawModelConfig & config, const MapSubModelRenderData * render_data, const Mat3x4 & transform, const Vec4 & color );
