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
	sky_mesh.vertex_buffers[ 0 ] = NewBuffer( "Skybox vertices", verts );
	sky_mesh.index_buffer = NewBuffer( "Skybox indices", indices );
}

void DrawSkybox( Time time ) {
	TracyZoneScoped;

	frame_static.render_passes[ RenderPass_Sky ] = NewRenderPass( RenderPassConfig {
		.name = "Sky",
		.color_targets = {
			RenderPassConfig::ColorTarget {
				.texture = Default( frame_static.render_targets.msaa_color, frame_static.render_targets.resolved_color ),
			},
		},
		.depth_target = RenderPassConfig::DepthTarget {
			.texture = Default( frame_static.render_targets.msaa_depth, frame_static.render_targets.resolved_depth ),
		},
		.representative_shader = shaders.skybox,
		.bindings = {
			.buffers = { { "u_View", frame_static.view_uniforms } },
			.textures = {
				{ "u_Noise", RGBNoiseTexture() },
				{ "u_BlueNoiseTexture", BlueNoiseTexture() },
			},
			.samplers = { { "u_Sampler", Sampler_Standard } },
		},
	} );

	PipelineState pipeline = {
		.shader = shaders.skybox,
		.dynamic_state = { .cull_face = CullFace_Front },
	};

	Draw( RenderPass_Sky, pipeline, sky_mesh, { { "u_Time", NewTempBuffer( ToSeconds( time ) ) } } );
}
