#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
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

	constexpr u16 indices[] = { 7, 6, 3, 2, 0, 6, 4, 7, 5, 3, 1, 0, 5, 4 };

	MeshConfig mesh_config;
	mesh_config.positions = NewVertexBuffer( verts, sizeof( verts ) );
	mesh_config.positions_format = VertexFormat_Floatx4;
	mesh_config.indices = NewIndexBuffer( indices, sizeof( indices ) );
	mesh_config.num_vertices = ARRAY_COUNT( indices );
	mesh_config.primitive_type = PrimitiveType_TriangleStrip;
	mesh_config.ccw_winding = false;

	sky_mesh = NewMesh( mesh_config );
}

void ShutdownSkybox() {
	DeleteMesh( sky_mesh );
}

void DrawSkybox() {
	ZoneScoped;

	PipelineState pipeline;
	pipeline.shader = &shaders.skybox;
	pipeline.pass = frame_static.sky_pass;
	pipeline.cull_face = CullFace_Front;
	pipeline.set_uniform( "u_View", frame_static.view_uniforms );
	pipeline.set_uniform( "u_Sky", UploadUniformBlock( Vec3( 0.625, 0.25, 0.625 ), Vec3( 0.75, 0.375, 0.125 ) ) );
	pipeline.set_texture( "u_BlueNoiseTexture", BlueNoiseTexture() );
	pipeline.set_uniform( "u_BlueNoiseTextureParams", frame_static.blue_noise_uniforms );
	DrawMesh( sky_mesh, pipeline );
}
