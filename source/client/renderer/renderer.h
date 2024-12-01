#pragma once

#include "qcommon/types.h"
#include "qcommon/srgb.h"
#include "client/renderer/api.h"
#include "client/renderer/material.h"
#include "client/renderer/model.h"
#include "client/renderer/gltf.h"
#include "client/renderer/cdmap.h"
#include "client/renderer/shader.h"
#include "cgame/ref.h"

enum ShadowQuality {
	ShadowQuality_Low,
	ShadowQuality_Medium,
	ShadowQuality_High,
	ShadowQuality_Ultra,

	ShadowQuality_Count
};

struct ShadowParameters {
	u32 num_cascades;
	float cascade_dists[ 4 ];
	u32 resolution;
	u32 entity_cascades;
};

/*
 * stuff that gets set once at the beginning of the frame but is needed all over the place
 */
struct FrameStatic {
	u32 viewport_width, viewport_height;
	Vec2 viewport;
	bool viewport_resized;
	float aspect_ratio;
	int msaa_samples;
	ShadowQuality shadow_quality;
	ShadowParameters shadow_parameters;

	GPUBuffer view_uniforms;
	GPUBuffer ortho_view_uniforms;
	GPUBuffer shadowmap_view_uniforms[ 4 ];
	GPUBuffer shadow_uniforms;
	GPUBuffer identity_model_uniforms;
	GPUBuffer identity_material_static_uniforms;
	GPUBuffer identity_material_dynamic_uniforms;

	Mat3x4 V, inverse_V;
	Mat4 P, inverse_P;
	Vec3 light_direction;
	Vec3 position;
	float vertical_fov;
	float near_plane;

	struct {
		PoolHandle< Texture > silhouette_mask;
		PoolHandle< Texture > msaa;
		PoolHandle< Texture > postprocess;
		PoolHandle< Texture > msaa_masked;
		PoolHandle< Texture > postprocess_masked;
		PoolHandle< Texture > msaa_onlycolor;
		PoolHandle< Texture > postprocess_onlycolor;
		PoolHandle< Texture > shadowmaps[ 4 ];
	} render_targets;

	Opaque< CommandBuffer > particle_update_pass;
	Opaque< CommandBuffer > particle_setup_indirect_pass;
	Opaque< CommandBuffer > tile_culling_pass;

	Opaque< CommandBuffer > shadowmap_pass[ 4 ];

	Opaque< CommandBuffer > world_opaque_prepass_pass;
	Opaque< CommandBuffer > world_opaque_pass;
	Opaque< CommandBuffer > sky_pass;

	Opaque< CommandBuffer > write_silhouette_gbuffer_pass;

	Opaque< CommandBuffer > nonworld_opaque_outlined_pass;
	Opaque< CommandBuffer > add_outlines_pass;
	Opaque< CommandBuffer > nonworld_opaque_pass;

	Opaque< CommandBuffer > transparent_pass;

	Opaque< CommandBuffer > add_silhouettes_pass;

	Opaque< CommandBuffer > ui_pass;
	Opaque< CommandBuffer > postprocess_pass;
	Opaque< CommandBuffer > post_ui_pass;
};

extern FrameStatic frame_static;

void InitRenderer();
void ShutdownRenderer();

void RendererBeginFrame( u32 viewport_width, u32 viewport_height );
void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov );
void RendererSubmitFrame();

size_t FrameSlot();

const Texture * BlueNoiseTexture();

struct PipelineState { };
void DrawFullscreenMesh( const PipelineState & pipeline );

// PipelineState MaterialToPipelineState( const Material * material, Vec4 color = white.vec4, bool skinned = false, GPUMaterial * gpu_material = NULL );

void Draw2DBox( float x, float y, float w, float h, const Material * material, Vec4 color = white.vec4 );
void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, const Material * material, Vec4 color );
// void DrawRotatedBox( float x, float y, float w, float h, float angle, const Material * material, RGBA8 color );

struct DynamicDrawData {
	VertexDescriptor vertex_descriptor;
	size_t base_vertex;
	size_t first_index;
	size_t num_vertices;
};

DynamicDrawData UploadDynamicGeometry( Span< const u8 > vertices, Span< const u16 > indices, const VertexDescriptor & vertex_descriptor );
void DrawDynamicGeometry( const PipelineState & pipeline, const DynamicDrawData & data, Optional< size_t > override_num_vertices = NONE, size_t extra_first_index = 0 );

template< typename T >
void DrawDynamicGeometry( const PipelineState & pipeline, Span< T > vertices, Span< const u16 > indices, const VertexDescriptor & vertex_descriptor ) {
	DynamicDrawData data = UploadDynamicGeometry( vertices.template cast< const u8 >(), indices, vertex_descriptor );
	DrawDynamicGeometry( pipeline, data );
}

GPUBuffer UploadModelUniforms( const Mat3x4 & M );
GPUBuffer UploadMaterialStaticUniforms( float specular, float shininess, float lod_bias = 0.0f );
GPUBuffer UploadMaterialDynamicUniforms( const Vec4 & color );

const char * ShadowQualityToString( ShadowQuality mode );

void DrawModelInstances();
void ClearMaterialStaticUniforms();
