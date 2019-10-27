#pragma once

#include "client/renderer/backend.h"

struct Shaders {
	Shader standard;
	Shader standard_vertexcolors;

	Shader standard_skinned;
	Shader standard_skinned_vertexcolors;

	Shader standard_alphatest;

	Shader world;
	Shader write_world_gbuffer;
	Shader postprocess_world_gbuffer;
	Shader postprocess_world_gbuffer_msaa;

	Shader write_silhouette_gbuffer;
	Shader write_silhouette_gbuffer_skinned;
	Shader postprocess_silhouette_gbuffer;

	Shader blur;

	Shader outline;
	Shader outline_skinned;

	Shader particle;

	Shader skybox;

	Shader text;
};

extern Shaders shaders;

void InitShaders();
void HotloadShaders();
void ShutdownShaders();
