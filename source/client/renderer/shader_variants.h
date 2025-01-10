#pragma once

#include "qcommon/types.h"
#include "client/renderer/shader.h"

struct GraphicsShaderDescriptor {
	PoolHandle< RenderPipeline > Shaders:: * field;
	Span< const char > src;
	Span< Span< const char > > features;
	Span< const VertexDescriptor > mesh_variants;

	BlendFunc blend_func = BlendFunc_Disabled;
	bool clamp_depth = false;
	bool alpha_to_coverage = false;
};

struct ComputeShaderDescriptor {
	PoolHandle< ComputePipeline > Shaders:: * field;
	Span< const char > src;
};

struct ShaderDescriptors {
	Span< const GraphicsShaderDescriptor > graphics_shaders;
	Span< const ComputeShaderDescriptor > compute_shaders;
};

// this has to be a visitor to keep the initializer_lists in scope
template< typename R, typename F, typename... Rest >
R VisitShaderDescriptors( F f, Rest... rest ) {
	VertexDescriptor pos_normal = { };
	pos_normal.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_01, 0, 0 };
	pos_normal.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_01, 1, 0 };
	pos_normal.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	pos_normal.buffer_strides[ 1 ] = sizeof( u16 ) * 2;

	VertexDescriptor pos_normal_uv = { };
	pos_normal_uv.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_01, 0, 0 };
	pos_normal_uv.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_01, 1, 0 };
	pos_normal_uv.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_01, 1, sizeof( u16 ) * 2 };
	pos_normal_uv.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	pos_normal_uv.buffer_strides[ 1 ] = sizeof( u16 ) * 4;

	VertexDescriptor pos_uv = { };
	pos_uv.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_01, 0, 0 };
	pos_uv.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_01, 0, sizeof( u16 ) * 2 };
	pos_uv.buffer_strides[ 0 ] = sizeof( u16 ) * 5;

	VertexDescriptor pos_normal_uv_skinned = { };
	pos_normal_uv_skinned.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_01, 0, 0 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_01, 1, 0 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_01, 1, sizeof( u16 ) * 2 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_JointIndices ] = VertexAttribute { VertexFormat_U8x4, 1, sizeof( u8 ) * 4 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_JointWeights ] = VertexAttribute { VertexFormat_U16x4_01, 1, sizeof( u16 ) * 4 };
	pos_normal_uv_skinned.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	pos_normal_uv_skinned.buffer_strides[ 1 ] = sizeof( u16 ) * 2 + sizeof( u16 ) * 2 + sizeof( u8 ) * 4 + sizeof( u16 ) * 4;

	VertexDescriptor fullscreen_vertex_descriptor = { };
	fullscreen_vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 },
	fullscreen_vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec3 );

	VertexDescriptor skybox_vertex_descriptor = { };
	skybox_vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx4, 0, 0 },
	skybox_vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec4 );

	VertexDescriptor text_vertex_descriptor = { };
	text_vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 };
	text_vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, sizeof( Vec3 ) };
	text_vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec3 ) + sizeof( Vec2 );

	return f( ShaderDescriptors {
		.graphics_shaders = {
			GraphicsShaderDescriptor {
				.field = &Shaders::standard,
				.src = "standard.hlsl",
				.mesh_variants = { pos_normal, pos_normal_uv },
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::standard_vertexcolors,
				.src = "standard.hlsl",
				.features = { "VERTEX_COLORS" },
				.mesh_variants = { pos_uv },
			},
			// GraphicsShaderDescriptor {
			// 	.field = &Shaders::standard_skinned,
			// 	.src = "standard.hlsl",
			// 	.features = { "SKINNED" },
			// 	.mesh_variants = { pos_normal_uv_skinned },
			// },
			GraphicsShaderDescriptor {
				.field = &Shaders::world,
				.src = "standard.hlsl",
				.features = {
					"APPLY_DRAWFLAT",
					"APPLY_FOG",
					"APPLY_DYNAMICS",
					"APPLY_SHADOWS",
					"SHADED",
				},
				.mesh_variants = { pos_normal },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::depth_only,
				.src = "depth_only.hlsl",
				.mesh_variants = { pos_normal, pos_normal_uv },
				.clamp_depth = true,
			},
			// GraphicsShaderDescriptor {
			// 	.field = &Shaders::depth_only_skinned,
			// 	.src = "depth_only.hlsl",
			// 	.features = { "SKINNED" },
			// 	.mesh_variants = { pos_normal_uv_skinned },
			// 	.clamp_depth = true,
			// },

			GraphicsShaderDescriptor {
				.field = &Shaders::postprocess_world_gbuffer,
				.src = "postprocess_world_gbuffer.hlsl",
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::postprocess_world_gbuffer_msaa,
				.src = "postprocess_world_gbuffer.hlsl",
				.features = { "MSAA" },
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::write_silhouette_gbuffer,
				.src = "write_silhouette_gbuffer.hlsl",
				.mesh_variants = { pos_normal, pos_normal_uv },
			},
			// GraphicsShaderDescriptor {
			// 	.field = &Shaders::write_silhouette_gbuffer_skinned,
			// 	.src = "write_silhouette_gbuffer.hlsl",
			// 	.features = { "SKINNED" },
			// 	.mesh_variants = { pos_normal_uv_skinned },
			// },
			GraphicsShaderDescriptor {
				.field = &Shaders::postprocess_silhouette_gbuffer,
				.src = "postprocess_silhouette_gbuffer.hlsl",
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::outline,
				.src = "outline.hlsl",
				.mesh_variants = {
					{ pos_normal },
					{ pos_normal_uv },
				},
			},
			// GraphicsShaderDescriptor {
			// 	.field = &Shaders::outline_skinned,
			// 	.src = "outline.hlsl",
			// 	.features = { "SKINNED" },
			// 	.mesh_variants = { pos_normal_uv_skinned },
			// },

			GraphicsShaderDescriptor {
				.field = &Shaders::scope,
				.src = "scope.hlsl",
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::skybox,
				.src = "skybox.hlsl",
				.mesh_variants = { { skybox_vertex_descriptor } },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::text,
				.src = "text.hlsl",
				.mesh_variants = { { text_vertex_descriptor } },
				.blend_func = BlendFunc_Blend,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::text_depth_only,
				.src = "text.hlsl",
				.features = { "DEPTH_ONLY" },
				.mesh_variants = { { text_vertex_descriptor } },
				.clamp_depth = true,
				.alpha_to_coverage = true,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::particle_add,
				.src = "particle.hlsl",
				.blend_func = BlendFunc_Add,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::particle_blend,
				.src = "particle.hlsl",
				.blend_func = BlendFunc_Blend,
			},
		},

		.compute_shaders = {
			ComputeShaderDescriptor { &Shaders::particle_compute, "particle_compute.hlsl" },
			ComputeShaderDescriptor { &Shaders::particle_setup_indirect, "particle_setup_indirect.hlsl" },
			ComputeShaderDescriptor { &Shaders::tile_culling, "tile_culling.hlsl" },
		},
	}, rest... );
}
