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
	constexpr VertexDescriptor pos_normal = {
		.attributes = {
			[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 },
			[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_Norm, 1, 0 },
		},
		.buffer_strides = {
			sizeof( u16 ) * 3,
			sizeof( u16 ) * 2,
		},
	};

	constexpr VertexDescriptor pos_normal_uv = {
		.attributes = {
			[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 },
			[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_Norm, 1, 0 },
			[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_Norm, 1, sizeof( u16 ) * 2 },
		},
		.buffer_strides = {
			sizeof( u16 ) * 3,
			sizeof( u16 ) * 4,
		},
	};

	constexpr VertexDescriptor pos_uv = {
		.attributes = {
			[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 },
			[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_Norm, 0, sizeof( u16 ) * 2 },
		},
		.buffer_strides = { sizeof( u16 ) * 5 },
	};

	constexpr VertexDescriptor pos_normal_uv_skinned = {
		.attributes = {
			[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_Norm, 0, 0 },
			[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_Norm, 1, 0 },
			[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_Norm, 1, sizeof( u16 ) * 2 },
			[ VertexAttribute_JointIndices ] = VertexAttribute { VertexFormat_U8x4, 1, sizeof( u8 ) * 4 },
			[ VertexAttribute_JointWeights ] = VertexAttribute { VertexFormat_U16x4_Norm, 1, sizeof( u16 ) * 4 },
		},
		.buffer_strides = {
			sizeof( u16 ) * 3,
			sizeof( u16 ) * 4,
		},
	};

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
				.variants = {
					{
						VertexDescriptor {
							.attributes = {
								[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 },
							},
							.buffer_strides = { sizeof( Vec3 ) },
						},
					}
				},
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::text,
				.src = "text.glsl",
				.variants = {
					{
						VertexDescriptor {
							.attributes = {
								[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 },
								[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, 0 },
							},
							.buffer_strides = { sizeof( Vec3 ) + sizeof( Vec2 ) },
						},
					}
				},
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
