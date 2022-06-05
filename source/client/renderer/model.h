#pragma once

#include "qcommon/types.h"
#include "client/renderer/types.h"
#include "client/renderer/cdmap.h"
#include "client/renderer/gltf.h"

struct DrawModelConfig {
	struct DrawModel {
		bool enabled;
		bool view_weapon;
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

struct ModelRenderData {
	ModelType type;
	union {
		GLTFRenderData gltf;
		MapRenderData map;
	};
};

const ModelRenderData * FindRenderModel( StringHash name );
const ModelRenderData * FindRenderModel( const char * name );

void InitModels();
void HotloadModels();
void ShutdownModels();

void DrawModelPrimitive( const GLTFRenderData * model, const GLTFRenderData::Primitive * primitive, const PipelineState & pipeline );
void DrawModel( DrawModelConfig config, const ModelRenderData * model, const Mat4 & transform, const Vec4 & color, MatrixPalettes palettes = MatrixPalettes() );
