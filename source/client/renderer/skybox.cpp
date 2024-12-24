#include "qcommon/base.h"
#include "qcommon/time.h"
#include "client/renderer/renderer.h"
#include "client/renderer/material.h"

static Mesh sky_mesh;
static PoolHandle< BindGroup > sky_bind_group;

void InitSkybox() {
	// w = 0 projects to infinity
	constexpr Vec4 verts[] = {
		Vec4( -1.0f,  1.0f,  1.0f, 0.0f ),
		Vec4(  1.0f,  1.0f,  1.0f, 0.0f ),
		Vec4( -1.0f, -1.0f,  1.0f, 0.0f ),
		Vec4(  1.0f, -1.0f,  1.0f, 0.0f ),
		Vec4( -1.0f,  1.0f, -1.0f, 0.0f ),
		Vec4(  1.0f,  1.0f, -1.0f, 0.0f ),
		Vec4( -1.0f, -1.0f, -1.0f, 0.0f ),
		Vec4(  1.0f, -1.0f, -1.0f, 0.0f ),
	};

	constexpr u16 indices[] = {
		6, 7, 3, 3, 2, 6,
		2, 3, 0, 0, 6, 2,
		6, 0, 4, 4, 7, 6,
		7, 4, 5, 5, 3, 7,
		5, 1, 3, 1, 0, 3,
		0, 1, 5, 5, 4, 0,
	};

	sky_mesh = { };
	sky_mesh.vertex_descriptor.attributes[ VertexAttribute_Position ] = { VertexFormat_Floatx4, 0, 0 };
	sky_mesh.vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec4 );
	sky_mesh.index_format = IndexFormat_U16;
	sky_mesh.num_vertices = ARRAY_COUNT( indices );
	sky_mesh.vertex_buffers[ 0 ] = NewBuffer( GPULifetime_Persistent, "Skybox vertices", verts );
	sky_mesh.index_buffer = NewBuffer( GPULifetime_Persistent, "Skybox indices", indices );

	sky_bind_group = NewBindGroup( shaders.skybox, {
		{ "u_Noise", RGBNoiseTexture(), Sampler_Standard },
		{ "u_BlueNoiseTexture", BlueNoiseTexture(), Sampler_Standard },
	} );
}

void DrawSkybox( Time time ) {
	TracyZoneScoped;

	PipelineState pipeline = {
		.shader = shaders.skybox,
		.dynamic_state = { .cull_face = CullFace_Front },
		.material_bind_group = sky_bind_group,
	};

	EncodeDrawCall( RenderPass_Sky, pipeline, sky_mesh, { { "u_Time", NewTempBuffer( ToSeconds( time ) ) } } );
}
