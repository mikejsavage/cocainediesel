#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"

struct DrawModelConfig {
	struct DrawModel {
		bool enabled;
		bool view_weapon;
		bool map_model;
	} draw_model;

	struct DrawShadows {
		bool enabled;
	} draw_shadows;

	struct DrawOutlines {
		bool enabled;
		float outline_height;
		Vec4 outline_color;
	} draw_outlines;

	struct DrawSilhouette {
		bool enabled;
		Vec4 silhouette_color;
	} draw_silhouette;
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
