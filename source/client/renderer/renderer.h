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
	u32 msaa_samples;
	ShadowQuality shadow_quality;
	ShadowParameters shadow_parameters;

	GPUBuffer view_uniforms;
	GPUBuffer ortho_view_uniforms;
	GPUBuffer shadowmap_view_uniforms[ 4 ];
	GPUBuffer shadow_uniforms;
	GPUBuffer identity_model_transform_uniforms;
	GPUBuffer identity_material_properties_uniforms;
	GPUBuffer identity_material_color_uniforms;

	Mat3x4 V, inverse_V;
	Mat4 P, inverse_P;
	Vec3 sun_direction;
	Vec3 position;
	float vertical_fov;
	float near_plane;

	struct {
		PoolHandle< Texture > silhouette_mask;
		PoolHandle< Texture > curved_surface_mask;
		Optional< PoolHandle< Texture > > msaa_color;
		Optional< PoolHandle< Texture > > msaa_depth;
		PoolHandle< Texture > resolved_color;
		PoolHandle< Texture > resolved_depth;
		PoolHandle< Texture > shadowmap;
		PoolHandle< Texture > swapchain;
	} render_targets;

	Opaque< CommandBuffer > render_passes[ RenderPass_Count ];
};

extern FrameStatic frame_static;

void InitRenderer();
void ShutdownRenderer();

void RendererBeginFrame( u32 viewport_width, u32 viewport_height );
void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov );
void RendererEndFrame();

size_t FrameSlot();

PoolHandle< Texture > RGBNoiseTexture();
PoolHandle< Texture > BlueNoiseTexture();

Mesh FullscreenMesh();

// PipelineState MaterialToPipelineState( const Material * material, Vec4 color = white.vec4, bool skinned = false, GPUMaterial * gpu_material = NULL );

void Draw2DBox( float x, float y, float w, float h, const Material * material, Vec4 color = white.vec4 );
void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, const Material * material, Vec4 color = white.vec4 );
// void DrawRotatedBox( float x, float y, float w, float h, float angle, const Material * material, RGBA8 color );

const char * ShadowQualityToString( ShadowQuality mode );
