#pragma once

#include "qcommon/types.h"
#include "shader.h"

struct GraphicsShaderDescriptor {
	struct Variant {
		VertexDescriptor mesh_format;
		Span< const char * > features;
	};

	Shader Shaders:: * field;
	const char * src;
	Span< const char * > features;
	Span< const Variant > variants;
};

struct ComputeShaderDescriptor {
	Shader Shaders:: * field;
	const char * src;
};

struct ShaderDescriptors {
	Span< const GraphicsShaderDescriptor > graphics_shaders;
	Span< const ComputeShaderDescriptor > compute_shaders;
};

// this has to be a visitor to keep the initializer_lists in scope
template< typename R, typename F, typename... Rest >
R VisitShaderDescriptors( F f, Rest... rest ) {
	VertexDescriptor pos_normal = { };
	pos_normal.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 };
	pos_normal.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_Norm, 1, 0 };
	pos_normal.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	pos_normal.buffer_strides[ 1 ] = sizeof( u16 ) * 2;

	VertexDescriptor pos_normal_uv = { };
	pos_normal_uv.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 };
	pos_normal_uv.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_Norm, 1, 0 };
	pos_normal_uv.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_Norm, 1, sizeof( u16 ) * 2 };
	pos_normal_uv.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	pos_normal_uv.buffer_strides[ 1 ] = sizeof( u16 ) * 4;

	VertexDescriptor pos_uv = { };
	pos_uv.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 };
	pos_uv.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_Norm, 0, sizeof( u16 ) * 2 };
	pos_uv.buffer_strides[ 0 ] = sizeof( u16 ) * 5;

	VertexDescriptor pos_normal_uv_skinned = { };
	pos_normal_uv_skinned.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_Norm, 1, 0 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_Norm, 1, sizeof( u16 ) * 2 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_JointIndices ] = VertexAttribute { VertexFormat_U8x4, 1, sizeof( u8 ) * 4 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_JointWeights ] = VertexAttribute { VertexFormat_U16x4_Norm, 1, sizeof( u16 ) * 4 };
	pos_normal_uv_skinned.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	pos_normal_uv_skinned.buffer_strides[ 1 ] = sizeof( u16 ) * 2 + sizeof( u16 ) * 2 + sizeof( u8 ) * 4 + sizeof( u16 ) * 4;

	VertexDescriptor skybox_vertex_descriptor = { };
	skybox_vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 },
	skybox_vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec3 );

	VertexDescriptor text_vertex_descriptor = { };
	text_vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 };
	text_vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, sizeof( Vec3 ) };
	text_vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec3 ) + sizeof( Vec2 );

	return f( ShaderDescriptors {
		.graphics_shaders = {
			GraphicsShaderDescriptor {
				.field = &Shaders::standard,
				.src = "standard.glsl",
				.variants = {
					{ pos_normal },
					{ pos_normal_uv },
					{ pos_normal_uv_skinned, { "SKINNED" } },
				},
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::standard_vertexcolors,
				.src = "standard.glsl",
				.features = { "VERTEX_COLORS" },
				.variants = {
					{ pos_uv },
				},
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::world,
				.src = "standard.glsl",
				.features = {
					"APPLY_DRAWFLAT",
					"APPLY_FOG",
					"APPLY_DECALS",
					"APPLY_DLIGHTS",
					"APPLY_SHADOWS",
					"SHADED",
				},
				.variants = { { pos_normal } },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::depth_only,
				.src = "depth_only.glsl",
				.variants = {
					{ pos_normal },
					{ pos_normal_uv },
					{ pos_normal_uv_skinned, { "SKINNED" } },
				},
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::outline,
				.src = "outline.glsl",
				.variants = {
					{ pos_normal },
					{ pos_normal_uv },
					{ pos_normal_uv_skinned, { "SKINNED" } },
				},
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::skybox,
				.src = "skybox.glsl",
				.variants = { { skybox_vertex_descriptor } },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::text,
				.src = "text.glsl",
				.variants = { { text_vertex_descriptor } },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::skybox,
				.src = "particle.glsl",
				.variants = { { } },
			},
		},

		.compute_shaders = {
			ComputeShaderDescriptor { &Shaders::particle_compute, "particle_compute.glsl" },
			ComputeShaderDescriptor { &Shaders::particle_setup_indirect, "particle_setup_indirect.glsl" },
			ComputeShaderDescriptor { &Shaders::tile_culling, "tile_culling.glsl" },
		},
	}, rest... );
}
