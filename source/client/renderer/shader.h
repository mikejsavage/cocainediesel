#pragma once

#include "client/renderer/api.h"

struct Shaders {
	PoolHandle< RenderPipeline > standard;
	PoolHandle< RenderPipeline > standard_shaded;
	PoolHandle< RenderPipeline > standard_vertexcolors;
	PoolHandle< RenderPipeline > standard_vertexcolors_add;
	PoolHandle< RenderPipeline > standard_vertexcolors_blend;

	PoolHandle< RenderPipeline > standard_skinned;
	PoolHandle< RenderPipeline > standard_skinned_shaded;

	PoolHandle< RenderPipeline > world;
	PoolHandle< RenderPipeline > postprocess_world_gbuffer;
	PoolHandle< RenderPipeline > postprocess_world_gbuffer_msaa;

	PoolHandle< RenderPipeline > viewmodel;

	PoolHandle< RenderPipeline > depth_only;
	PoolHandle< RenderPipeline > depth_only_skinned;

	PoolHandle< RenderPipeline > write_silhouette_gbuffer;
	PoolHandle< RenderPipeline > write_silhouette_gbuffer_skinned;
	PoolHandle< RenderPipeline > postprocess_silhouette_gbuffer;

	PoolHandle< RenderPipeline > outline;
	PoolHandle< RenderPipeline > outline_skinned;

	PoolHandle< RenderPipeline > scope;

	PoolHandle< ComputePipeline > particle_compute;
	PoolHandle< ComputePipeline > particle_setup_indirect;

	PoolHandle< RenderPipeline > particle_add;
	PoolHandle< RenderPipeline > particle_blend;

	PoolHandle< ComputePipeline > tile_culling;

	PoolHandle< RenderPipeline > skybox;

	PoolHandle< RenderPipeline > text;
	PoolHandle< RenderPipeline > text_depth_only;

	PoolHandle< RenderPipeline > postprocess;
};

extern Shaders shaders;
