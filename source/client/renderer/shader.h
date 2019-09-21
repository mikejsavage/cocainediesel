#pragma once

#include "client/renderer/backend.h"

struct Shaders {
	Shader standard;
	Shader standard_vertexcolors;

	Shader standard_skinned;
	Shader standard_skinned_vertexcolors;

	Shader standard_alphatest;

	Shader world;
	Shader world_write_gbuffer;
	Shader world_postprocess_gbuffer;

	Shader blur;

	Shader tonemap;

	Shader outline;
	Shader outline_skinned;

	Shader text;
};

extern Shaders shaders;

void InitShaders();
void ShutdownShaders();
