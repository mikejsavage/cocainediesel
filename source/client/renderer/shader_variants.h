#pragma once

#include "qcommon/types.h"
#include "client/renderer/shader.h"
#include "imgui/imgui.h"

struct GraphicsShaderDescriptor {
	PoolHandle< RenderPipeline > Shaders:: * field;
	Span< const char > src;
	Span< Span< const char > > features;
	Span< const VertexDescriptor > mesh_variants;

	BlendFunc blend_func = BlendFunc_Disabled;
	bool clamp_depth = false;
	bool alpha_to_coverage = false;
	bool viewmodel_depth = false;
};

struct ComputeShaderDescriptor {
	PoolHandle< ComputePipeline > Shaders:: * field;
	Span< const char > src;
};

struct ShaderDescriptors {
	Span< const GraphicsShaderDescriptor > graphics_shaders;
	Span< const ComputeShaderDescriptor > compute_shaders;
};

constexpr VertexDescriptor ImGuiVertexDescriptor() {
	VertexDescriptor imgui = { };
	imgui.attributes[ VertexAttribute_Position ] = MakeOptional( VertexAttribute { VertexFormat_Floatx2, 0, offsetof( ImDrawVert, pos ) } );
	imgui.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( ImDrawVert, uv ) };
	imgui.attributes[ VertexAttribute_Color ] = VertexAttribute { VertexFormat_U8x4_01, 0, offsetof( ImDrawVert, col ) };
	imgui.buffer_strides[ 0 ] = sizeof( ImDrawVert );
	return imgui;
}

// this has to be a visitor to keep the initializer_lists in scope
template< typename R, typename F, typename... Rest >
R VisitShaderDescriptors( F f, Rest... rest ) {
	// VertexDescriptor standard_vertex = { };
	// standard_vertex.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_U16x3_01, 0, 0 };
	// standard_vertex.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_U10x3_U2x1_01, 1, 0 };
	// standard_vertex.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_U16x2_01, 1, sizeof( u16 ) * 2 };
	// standard_vertex.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	// standard_vertex.buffer_strides[ 1 ] = sizeof( u16 ) * 4;

	VertexDescriptor standard_vertex = { };
	standard_vertex.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, 0 };
	standard_vertex.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_Floatx3, 1, 0 };
	standard_vertex.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 1, sizeof( Vec3 ) };
	standard_vertex.buffer_strides[ 0 ] = sizeof( u16 ) * 3;
	standard_vertex.buffer_strides[ 1 ] = sizeof( u16 ) * 4;

	VertexDescriptor vfx = { };
	vfx.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( VFXVertex, position ) };
	vfx.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( VFXVertex, uv ) };
	vfx.attributes[ VertexAttribute_Color ] = VertexAttribute { VertexFormat_U8x4_01, 0, offsetof( VFXVertex, color ) };
	vfx.buffer_strides[ 0 ] = sizeof( VFXVertex );

	VertexDescriptor pos_normal_uv_skinned = standard_vertex;
	pos_normal_uv_skinned.attributes[ VertexAttribute_JointIndices ] = VertexAttribute { VertexFormat_U8x4, 1, sizeof( u8 ) * 4 };
	pos_normal_uv_skinned.attributes[ VertexAttribute_JointWeights ] = VertexAttribute { VertexFormat_U16x4_01, 1, sizeof( u16 ) * 4 };
	pos_normal_uv_skinned.buffer_strides[ 1 ] = sizeof( u8 ) * 4 + sizeof( u16 ) * 4;

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
				.src = "standard.slang",
				.mesh_variants = { standard_vertex },
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::standard_skinned,
				.src = "standard.slang",
				.features = { "SKINNED" },
				.mesh_variants = { pos_normal_uv_skinned },
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::world,
				.src = "standard.slang",
				.features = {
					"APPLY_DRAWFLAT",
					"APPLY_FOG",
					"APPLY_DYNAMICS",
					"APPLY_SHADOWS",
					"SHADED",
				},
				.mesh_variants = { standard_vertex },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::imgui,
				.src = "simple.slang",
				.features = { "IMGUI" },
				.mesh_variants = { ImGuiVertexDescriptor() },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::vfx_add,
				.src = "simple.slang",
				.mesh_variants = { vfx },
				.blend_func = BlendFunc_Add,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::vfx_blend,
				.src = "simple.slang",
				.mesh_variants = { vfx },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::viewmodel,
				.src = "standard.slang",
				.mesh_variants = { standard_vertex },
				.viewmodel_depth = true,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::depth_only,
				.src = "depth_only.slang",
				.mesh_variants = { standard_vertex },
				.clamp_depth = true,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::depth_only_skinned,
				.src = "depth_only.slang",
				.features = { "SKINNED" },
				.mesh_variants = { pos_normal_uv_skinned },
				.clamp_depth = true,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::postprocess_world_gbuffer,
				.src = "postprocess_world_gbuffer.slang",
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::postprocess_world_gbuffer_msaa,
				.src = "postprocess_world_gbuffer.slang",
				.features = { "MSAA" },
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::write_silhouette_gbuffer,
				.src = "write_silhouette_gbuffer.slang",
				.mesh_variants = { standard_vertex },
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::write_silhouette_gbuffer_skinned,
				.src = "write_silhouette_gbuffer.slang",
				.features = { "SKINNED" },
				.mesh_variants = { pos_normal_uv_skinned },
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::postprocess_silhouette_gbuffer,
				.src = "postprocess_silhouette_gbuffer.slang",
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::outline,
				.src = "outline.slang",
				.mesh_variants = { standard_vertex },
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::outline_skinned,
				.src = "outline.slang",
				.features = { "SKINNED" },
				.mesh_variants = { pos_normal_uv_skinned },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::scope,
				.src = "scope.slang",
				.mesh_variants = { fullscreen_vertex_descriptor },
				.blend_func = BlendFunc_Blend,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::skybox,
				.src = "skybox.slang",
				.mesh_variants = { { skybox_vertex_descriptor } },
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::text,
				.src = "text.slang",
				.mesh_variants = { { text_vertex_descriptor } },
				.blend_func = BlendFunc_Blend,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::text_depth_only,
				.src = "text.slang",
				.features = { "DEPTH_ONLY" },
				.mesh_variants = { { text_vertex_descriptor } },
				.clamp_depth = true,
				.alpha_to_coverage = true,
			},

			GraphicsShaderDescriptor {
				.field = &Shaders::particle_add,
				.src = "particle.slang",
				.blend_func = BlendFunc_Add,
			},
			GraphicsShaderDescriptor {
				.field = &Shaders::particle_blend,
				.src = "particle.slang",
				.blend_func = BlendFunc_Blend,
			},
		},

		.compute_shaders = {
			ComputeShaderDescriptor { &Shaders::particle_compute, "particle_compute.slang" },
			ComputeShaderDescriptor { &Shaders::particle_setup_indirect, "particle_setup_indirect.slang" },
			ComputeShaderDescriptor { &Shaders::tile_culling, "tile_culling.slang" },
		},
	}, rest... );
}
