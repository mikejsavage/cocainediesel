#pragma once

#include "client/renderer/api.h"

struct Shaders {
	PoolHandle< RenderPipeline > standard;
	PoolHandle< RenderPipeline > standard_shaded;
	PoolHandle< RenderPipeline > standard_vertexcolors;

	PoolHandle< RenderPipeline > standard_skinned;
	PoolHandle< RenderPipeline > standard_skinned_shaded;

	PoolHandle< RenderPipeline > standard_instanced;
	PoolHandle< RenderPipeline > standard_shaded_instanced;

	PoolHandle< RenderPipeline > depth_only;
	PoolHandle< RenderPipeline > depth_only_instanced;
	PoolHandle< RenderPipeline > depth_only_skinned;

	PoolHandle< RenderPipeline > world;
	PoolHandle< RenderPipeline > world_instanced;
	PoolHandle< RenderPipeline > postprocess_world_gbuffer;
	PoolHandle< RenderPipeline > postprocess_world_gbuffer_msaa;

	PoolHandle< RenderPipeline > write_silhouette_gbuffer;
	PoolHandle< RenderPipeline > write_silhouette_gbuffer_instanced;
	PoolHandle< RenderPipeline > write_silhouette_gbuffer_skinned;
	PoolHandle< RenderPipeline > postprocess_silhouette_gbuffer;

	PoolHandle< RenderPipeline > outline;
	PoolHandle< RenderPipeline > outline_instanced;
	PoolHandle< RenderPipeline > outline_skinned;

	PoolHandle< RenderPipeline > scope;

	PoolHandle< ComputePipeline > particle_compute;
	PoolHandle< ComputePipeline > particle_setup_indirect;

	PoolHandle< RenderPipeline > particle;

	PoolHandle< ComputePipeline > tile_culling;

	PoolHandle< RenderPipeline > skybox;

	PoolHandle< RenderPipeline > text;
	PoolHandle< RenderPipeline > text_alphatest;

	PoolHandle< RenderPipeline > postprocess;
};

extern Shaders shaders;

void InitShaders();
void HotloadShaders();
