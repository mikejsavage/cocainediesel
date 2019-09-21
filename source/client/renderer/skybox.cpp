#include "qcommon/base.h"
#include "client/renderer/renderer.h"
#include "client/renderer/material.h"

static Mesh sky_mesh;
static const Material * sky_material = NULL;

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

	sky_mesh = NewMesh( mesh_config );
	sky_material = FindMaterial( "sky" );
}

void ShutdownSkybox() {
	DeleteMesh( sky_mesh );
}

void DrawSkybox() {
	PipelineState pipeline = MaterialToPipelineState( sky_material );
	pipeline.pass = frame_static.sky_pass;
	pipeline.set_uniform( "u_View", UploadViewUniforms( Mat4::Identity(), frame_static.P, Vec3( 0 ), 0.0f ) );
	pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );
	DrawMesh( sky_mesh, pipeline );
}
