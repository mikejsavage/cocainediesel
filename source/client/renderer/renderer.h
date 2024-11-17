#pragma once

#include "qcommon/types.h"
#include "qcommon/srgb.h"
#include "client/renderer/backend.h"
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

	UniformBlock view_uniforms;
	UniformBlock ortho_view_uniforms;
	UniformBlock shadowmap_view_uniforms[ 4 ];
	UniformBlock shadow_uniforms;
	UniformBlock identity_model_uniforms;
	UniformBlock identity_material_static_uniforms;
	UniformBlock identity_material_dynamic_uniforms;

	Mat3x4 V, inverse_V;
	Mat4 P, inverse_P;
	Vec3 light_direction;
	Vec3 position;
	float vertical_fov;
	float near_plane;

	struct {
		RenderTarget silhouette_mask;
		RenderTarget msaa;
		RenderTarget postprocess;
		RenderTarget msaa_masked;
		RenderTarget postprocess_masked;
		RenderTarget msaa_onlycolor;
		RenderTarget postprocess_onlycolor;
		RenderTarget shadowmaps[ 4 ];
	} render_targets;

	u8 particle_update_pass;
	u8 particle_setup_indirect_pass;
	u8 tile_culling_pass;

	u8 shadowmap_pass[ 4 ];

	u8 world_opaque_prepass_pass;
	u8 world_opaque_pass;
	u8 sky_pass;

	u8 write_silhouette_gbuffer_pass;

	u8 nonworld_opaque_outlined_pass;
	u8 add_outlines_pass;
	u8 nonworld_opaque_pass;

	u8 transparent_pass;

	u8 add_silhouettes_pass;

	u8 ui_pass;
	u8 postprocess_pass;
	u8 post_ui_pass;
};

extern FrameStatic frame_static;

void InitRenderer();
void ShutdownRenderer();

void RendererBeginFrame( u32 viewport_width, u32 viewport_height );
void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov );
void RendererSubmitFrame();

size_t FrameSlot();

const Texture * BlueNoiseTexture();

void DrawFullscreenMesh( const PipelineState & pipeline );

PipelineState MaterialToPipelineState( const Material * material, Vec4 color = white.vec4, bool skinned = false, GPUMaterial * gpu_material = NULL );

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

UniformBlock UploadModelUniforms( const Mat3x4 & M );
UniformBlock UploadMaterialStaticUniforms( float specular, float shininess, float lod_bias = 0.0f );
UniformBlock UploadMaterialDynamicUniforms( const Vec4 & color, Vec3 tcmod_row0 = Vec3( 1, 0, 0 ), Vec3 tcmod_row1 = Vec3( 0, 1, 0 ) );

const char * ShadowQualityToString( ShadowQuality mode );

void DrawModelInstances();
void ClearMaterialStaticUniforms();
