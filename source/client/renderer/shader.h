#pragma once

#include "client/renderer/types.h"

struct Shaders {
	Shader standard;
	Shader standard_shaded;
	Shader standard_vertexcolors;

	Shader standard_skinned;
	Shader standard_skinned_shaded;

	Shader standard_instanced;
	Shader standard_shaded_instanced;

	Shader depth_only;
	Shader depth_only_instanced;
	Shader depth_only_skinned;

	Shader world;
	Shader world_instanced;
	Shader postprocess_world_gbuffer;
	Shader postprocess_world_gbuffer_msaa;

	Shader write_silhouette_gbuffer;
	Shader write_silhouette_gbuffer_instanced;
	Shader write_silhouette_gbuffer_skinned;
	Shader postprocess_silhouette_gbuffer;

	Shader outline;
	Shader outline_instanced;
	Shader outline_skinned;

	Shader scope;

	Shader particle_compute;
	Shader particle_setup_indirect;

	Shader particle;

	Shader tile_culling;

	Shader skybox;

	Shader text;

	Shader postprocess;
};

extern Shaders shaders;

void InitShaders();
void HotloadShaders();
void ShutdownShaders();
