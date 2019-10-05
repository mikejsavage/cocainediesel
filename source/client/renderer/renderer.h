#pragma once

#include "qcommon/types.h"
#include "client/renderer/backend.h"
#include "client/renderer/material.h"
#include "client/renderer/model.h"
#include "client/renderer/shader.h"
#include "gameshared/q_math.h"
#include "cgame/ref.h"

/*
 * stuff that gets set once at the beginning of the frame but is needed all over the place
 */
struct FrameStatic {
	u32 viewport_width, viewport_height;
	Vec2 viewport;
	float aspect_ratio;

	UniformBlock view_uniforms;
	UniformBlock ortho_view_uniforms;
	UniformBlock identity_model_uniforms;
	UniformBlock identity_material_uniforms;
	UniformBlock blue_noise_uniforms;

	Mat4 V, P;
	Vec3 position;

	Framebuffer world_gbuffer;
	Framebuffer world_outlines_fb;
	Framebuffer teammate_gbuffer;
	Framebuffer teammate_outlines_fb;
	Framebuffer msaa_fb;

	u8 world_write_gbuffer_pass;
	u8 world_postprocess_gbuffer_pass;
	u8 world_opaque_pass;
	u8 world_add_outlines_pass;

	u8 teammate_write_gbuffer_pass;
	u8 teammate_postprocess_gbuffer_pass;

	u8 nonworld_opaque_pass;
	u8 sky_pass;
	u8 transparent_pass;

	u8 teammate_add_outlines_pass;

	u8 blur_pass;
	u8 ui_pass;
};

struct DynamicMesh {
	const Vec3 * positions;
	const Vec2 * uvs;
	const RGBA8 * colors;
	const u16 * indices;

	u32 num_vertices;
	u32 num_indices;
};

extern FrameStatic frame_static;

void InitRenderer();
void ShutdownRenderer();

void RendererBeginFrame( u32 viewport_width, u32 viewport_height );
void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov );
void RendererSubmitFrame();

Texture BlueNoiseTexture();
void DrawFullscreenMesh( const PipelineState & pipeline );

bool HasAlpha( TextureFormat format );
PipelineState MaterialToPipelineState( const Material * material, Vec4 color = vec4_white, bool skinned = false );

void Draw2DBox( u8 render_pass, float x, float y, float w, float h, Texture texture, Vec4 color = vec4_white );
void Draw2DBox( u8 render_pass, float x, float y, float w, float h, const Material * material, Vec4 color = vec4_white );
// void DrawRotatedBox( float x, float y, float w, float h, float angle, const Material * material, RGBA8 color );

u16 DynamicMeshBaseIndex();
void DrawDynamicMesh( const PipelineState & pipeline, const DynamicMesh & mesh );

UniformBlock UploadModelUniforms( const Mat4 & M );
UniformBlock UploadMaterialUniforms( const Vec4 & color, const Vec2 & texture_size, float alpha_cutoff );
