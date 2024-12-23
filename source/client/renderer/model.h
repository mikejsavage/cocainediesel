#pragma once

#include "qcommon/types.h"
#include "client/renderer/api.h"

struct DrawModelConfig {
	struct DrawModel {
		bool enabled;
		bool view_weapon;
	} draw_model;

	bool cast_shadows;
	Optional< OutlineUniforms > outline;
	Optional< Vec4 > silhouette_color;
};

enum ModelType {
	ModelType_GLTF,
	ModelType_Map,
};

struct GLTFRenderData;
struct MapSubModelRenderData;
struct ModelRenderData {
	ModelType type;
	union {
		const GLTFRenderData * gltf;
		const MapSubModelRenderData * map;
	};
};

Optional< ModelRenderData > FindModelRenderData( StringHash name );
Optional< ModelRenderData > FindModelRenderData( const char * name );

void InitModels();
void HotloadModels();
void ShutdownModels();

void DrawModel( DrawModelConfig config, ModelRenderData render_data, const Mat3x4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );
