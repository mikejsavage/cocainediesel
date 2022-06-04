#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

struct CollisionModel;

struct Map {
	const char * name;
	u64 base_hash;

	Model * models;
	u32 num_models;

	float fog_strength;

	CollisionModel * cms;

	GPUBuffer nodeBuffer;
	GPUBuffer leafBuffer;
	GPUBuffer brushBuffer;
	GPUBuffer planeBuffer;
};

struct MapRenderData {
	Mesh mesh;

	float fog_strength;

	GPUBuffer nodes;
	GPUBuffer leaves;
	GPUBuffer brushes;
	GPUBuffer planes;
};

void InitMaps();
void ShutdownMaps();

void HotloadMaps();

bool AddMap( Span< const u8 > data, const char * path );

const Map * FindMap( StringHash name );
const Map * FindMap( const char * name );

struct MapData;
MapRenderData NewMapRenderData( const MapData & map, const char * name );
void DeleteMapRenderData( const MapRenderData & render_data );
