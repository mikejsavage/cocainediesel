#include "qcommon/base.h"
#include "qcommon/time.h"
#include "client/renderer/renderer.h"
#include "client/renderer/material.h"

static Mesh sky_mesh;

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
}

void DrawSkybox( Time time ) {
	TracyZoneScoped;

	PipelineState pipeline;
	pipeline.shader = &shaders.skybox;
	pipeline.pass = frame_static.sky_pass;
	pipeline.cull_face = CullFace_Front;
	pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
	pipeline.bind_uniform( "u_Time", UploadUniformBlock( ToSeconds( time ) ) );
	pipeline.bind_texture_and_sampler( "u_Noise", FindMaterial( "textures/noise" )->texture, Sampler_Standard );
	pipeline.bind_texture_and_sampler( "u_BlueNoiseTexture", BlueNoiseTexture(), Sampler_Standard );
	DrawMesh( sky_mesh, pipeline );
}
