#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "qcommon/opaque.h"

#include "client/renderer/shader_shared.h"

constexpr size_t MaxFramesInFlight = 2;

struct SDL_Window;
struct WindowMode;
void InitRenderer( SDL_Window * window, const WindowMode & window_mode );
void ShutdownRenderer();

constexpr u32 MaxMSAA = 8;
u32 RenderBackendSupportedMSAA();

/*
 * Memory allocation
 */

struct GPUAllocation;
template<> struct PoolHandleType< GPUAllocation > { using T = u16; };

struct GPUBuffer {
	PoolHandle< GPUAllocation > allocation;
	size_t offset;
	size_t size;
};

struct CoherentBuffer {
	GPUBuffer buffer;
	void * ptr;
};

template< typename T >
struct CoherentSpan {
	GPUBuffer buffer;
	Span< T > data;
};

GPUBuffer NewBuffer( Span< const char > label, size_t size, size_t alignment, bool texture, const void * data = NULL );

template< typename T >
GPUBuffer NewBuffer( Span< const char > label, const T & x, size_t alignment = alignof( T ) ) {
	return NewBuffer( label, sizeof( T ), alignment, false, &x );
}

template< typename T >
GPUBuffer NewBuffer( Span< const char > label, Span< const T > xs, size_t alignment = alignof( T ) ) {
	return NewBuffer( label, xs.num_bytes(), alignment, false, xs.ptr );
}

GPUBuffer NewTempBuffer( const void * data, size_t size, size_t alignment );

template< typename T >
GPUBuffer NewTempBuffer( Span< const T > xs, size_t alignment = alignof( T ) ) {
	return NewTempBuffer( xs.ptr, xs.num_bytes(), alignment );
}

template< typename T >
GPUBuffer NewTempBuffer( const T & x, size_t alignment = alignof( T ) ) {
	return NewTempBuffer( &x, sizeof( T ), alignment );
}

CoherentBuffer NewCoherentTempBuffer( size_t size, size_t alignment );

template< typename T >
CoherentSpan< T > NewCoherentSpan( size_t n, size_t alignment = alignof( T ) ) {
	CoherentBuffer buf = NewCoherentTempBuffer( n * sizeof( T ), alignment );
	return CoherentSpan< T > {
		.buffer = buf.buffer,
		.data = Span< T >( ( T * ) buf.ptr, n ),
	};
}

GPUBuffer NewDeviceTempBuffer( Span< const char > label, size_t size, size_t alignment );

template< typename T >
GPUBuffer NewDeviceTempBuffer( Span< const char > label, size_t alignment = alignof( T ) ) {
	return NewDeviceTempBuffer( label, sizeof( T ), alignment );
}

void FlushStagingBuffer();

/*
 * Textures
 */

struct Texture;
template<> struct PoolHandleType< Texture > { using T = u16; };

enum TextureFormat : u8 {
	TextureFormat_R_U8,
	TextureFormat_R_U8_sRGB,
	TextureFormat_R_S8,
	TextureFormat_R_UI8,

	TextureFormat_A_U8,

	TextureFormat_RA_U8,

	TextureFormat_RGBA_U8,
	TextureFormat_RGBA_U8_sRGB,

	TextureFormat_BC1_sRGB,
	TextureFormat_BC3_sRGB,
	TextureFormat_BC4,
	TextureFormat_BC5,

	TextureFormat_Depth,

	TextureFormat_Swapchain,
};

struct TextureConfig {
	Span< const char > name;
	TextureFormat format;
	u32 width;
	u32 height;
	Optional< u32 > num_layers;
	u32 num_mipmaps = 1;
	u32 msaa_samples = 1;
	const void * data;
	bool dedicated_allocation;
};

PoolHandle< Texture > NewTexture( const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture = NONE );
PoolHandle< Texture > NewRenderTargetTexture( const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture );

PoolHandle< Texture > RGBNoiseTexture();
PoolHandle< Texture > BlueNoiseTexture();

/*
 * Mesh
 */

enum VertexFormat : u8 {
	VertexFormat_U8x2,
	VertexFormat_U8x2_01,
	VertexFormat_U8x3,
	VertexFormat_U8x3_01,
	VertexFormat_U8x4,
	VertexFormat_U8x4_01,

	VertexFormat_U16x2,
	VertexFormat_U16x2_01,
	VertexFormat_U16x3,
	VertexFormat_U16x3_01,
	VertexFormat_U16x4,
	VertexFormat_U16x4_01,

	VertexFormat_U10x3_U2x1_01,

    VertexFormat_Floatx2,
    VertexFormat_Floatx3,
    VertexFormat_Floatx4,
};

struct VertexAttribute {
	VertexFormat format;
	size_t buffer;
	size_t offset;
};

struct VertexDescriptor {
	Optional< VertexAttribute > attributes[ VertexAttribute_Count ];
	Optional< size_t > buffer_strides[ VertexAttribute_Count ];
};

bool operator==( const VertexAttribute & lhs, const VertexAttribute & rhs );
bool operator==( const VertexDescriptor & lhs, const VertexDescriptor & rhs );

enum IndexFormat : u8 {
	IndexFormat_U16,
	IndexFormat_U32,
};

struct Mesh {
	VertexDescriptor vertex_descriptor;
	IndexFormat index_format;
	u32 num_vertices;

	Optional< GPUBuffer > vertex_buffers[ VertexAttribute_Count ];
	Optional< GPUBuffer > index_buffer;
};

Mesh FullscreenMesh();

/*
 * RenderPipeline
 */

enum BlendFunc : u8 {
	BlendFunc_Disabled,
	BlendFunc_Blend,
	BlendFunc_Add,

	BlendFunc_Count
};

struct RenderPipelineOutputFormat {
	Span< const TextureFormat > colors;
	bool has_depth;
};

struct RenderPipelineConfig {
	Span< const char > path;

	RenderPipelineOutputFormat output_format;
	BlendFunc blend_func = BlendFunc_Disabled;
	bool clamp_depth = false;
	bool alpha_to_coverage = false;

	Span< const VertexDescriptor > mesh_variants;
};

struct RenderPipeline;
template<> struct PoolHandleType< RenderPipeline > { using T = u8; };

PoolHandle< RenderPipeline > NewRenderPipeline( const RenderPipelineConfig & config, Optional< PoolHandle< RenderPipeline > > old_pipeline );

/*
 * ComputePipeline
 */

struct ComputePipeline;
template<> struct PoolHandleType< ComputePipeline > { using T = u8; };

PoolHandle< ComputePipeline > NewComputePipeline( Span< const char > path, Optional< PoolHandle< ComputePipeline > > old_pipeline );

/*
 * RenderPipelineDynamicState
 */

enum DepthFunc : u8 {
	DepthFunc_Less,
	DepthFunc_LessNoWrite,
	DepthFunc_Equal,
	DepthFunc_EqualNoWrite,
	DepthFunc_Always,
	DepthFunc_AlwaysNoWrite,

	DepthFunc_Count
};

enum CullFace : u8 {
	CullFace_Back,
	CullFace_Front,
	CullFace_Disabled,
};

struct RenderPipelineDynamicState {
    DepthFunc depth_func : 3 = DepthFunc_Less;
    CullFace cull_face : 2 = CullFace_Back;
};

static_assert( sizeof( RenderPipelineDynamicState ) == 1 );

/*
 * BindGroup
 */

enum SamplerType : u8 {
	Sampler_Standard,
	Sampler_Clamp,
	Sampler_Unfiltered,
	Sampler_Shadowmap,

	Sampler_Count
};

struct BindGroup;
template<> struct PoolHandleType< BindGroup > { using T = u16; };

/*
 * Render passes
 */

struct CommandBuffer;
template<> inline constexpr size_t OpaqueSize< CommandBuffer > = 96;

struct BufferBinding {
	StringHash name;
	GPUBuffer buffer;
};

struct GPUBindings {
	struct TextureBinding {
		StringHash name;
		PoolHandle< Texture > texture;
	};

	struct SamplerBinding {
		StringHash name;
		SamplerType sampler;
	};

	Span< const BufferBinding > buffers;
	Span< const TextureBinding > textures;
	Span< const SamplerBinding > samplers;
};

enum RenderPass {
	RenderPass_ParticleUpdate,
	RenderPass_ParticleSetupIndirect, // could be merged into above?
	RenderPass_TileCulling,

	RenderPass_ShadowmapCascade0,
	RenderPass_ShadowmapCascade1,
	RenderPass_ShadowmapCascade2,
	RenderPass_ShadowmapCascade3,

	// the "world" is everything that receives decals, we do a z-prepass
	// because decal rendering is expensive and we don't want to do it on
	// invisible surfaces
	RenderPass_WorldOpaqueZPrepass,
	RenderPass_WorldOpaque,
	RenderPass_Sky,

	// silhouettes are player etc outlines visible through walls, outlines are world outlines
	RenderPass_WriteSilhouetteMask,
	RenderPass_NonworldOpaqueOutlined,
	RenderPass_AddOutlines,
	RenderPass_NonworldOpaque, // NOTE(mike 20260327): also MSAA resolve

	RenderPass_Transparent, // -> color0

	RenderPass_AddSilhouettes, // -> color0

	RenderPass_PreUIPostprocessing, // color0 -> color1
	RenderPass_UIBeforePostprocessing, // -> color1
	RenderPass_Postprocessing, // color1 -> (hdr ? color0 : swapchain)
	RenderPass_UIAfterPostprocessing, // -> hdr ? color0 : swapchain

	RenderPass_10BitTosRGB, // hdr only, -> swapchain

	RenderPass_Count
};

enum LoadOp {
	LoadOp_DontCare,
	LoadOp_Load,
	LoadOp_Clear,
};

enum StoreOp {
	StoreOp_DontCare,
	StoreOp_Store,
	// StoreOp_Resolve,
	// StoreOp_ResolveAndStore,
};

enum GPUBarrier {
	GPUBarrier_ComputeToIndirect,
	GPUBarrier_ComputeToCompute,
	GPUBarrier_ComputeToFragment,
	GPUBarrier_FragmentToFragmentSample,
	GPUBarrier_FragmentToFragmentOutput,
	GPUBarrier_MSAAResolveToFragmentOutput,
};

struct RenderPassConfig {
	struct ColorTarget {
		Optional< PoolHandle< Texture > > texture; // NONE = swapchain
		u32 layer = 0;
		LoadOp load = LoadOp_DontCare;
		Vec4 clear;
		StoreOp store = StoreOp_Store;
		Optional< PoolHandle< Texture > > resolve_target = NONE;
	};

	struct DepthTarget {
		PoolHandle< Texture > texture;
		u32 layer = 0;
		LoadOp load = LoadOp_DontCare;
		StoreOp store = StoreOp_Store;
		Optional< PoolHandle< Texture > > resolve_target = NONE;
	};

	Span< const char > name;
	RenderPass pass; // Metal synchronization
	Span< const ColorTarget > color_targets;
	Optional< DepthTarget > depth_target;

	// Vulkan synchronization
	Span< const GPUBarrier > barriers;
	Span< const PoolHandle< Texture > > attachment_transitions;
	Span< const PoolHandle< Texture > > reattachment_transitions;
	Span< const PoolHandle< Texture > > readonly_transitions;
	bool swapchain_attachment_transition;

	PoolHandle< RenderPipeline > representative_shader;
	GPUBindings bindings;
};

Optional< Opaque< CommandBuffer > > NewRenderPass( const RenderPassConfig & render_pass );

struct RenderPassSubmit {
	Opaque< CommandBuffer > buffer;
	RenderPass pass;
	RenderPass next_pass;
};

void SubmitRenderPasses( Span< const RenderPassSubmit > passes, RenderPass first_pass );

/*
 * Draw calls
 */

struct PipelineState {
	PoolHandle< RenderPipeline > shader;
	RenderPipelineDynamicState dynamic_state;
	Optional< PoolHandle< BindGroup > > material_bind_group;
};

struct DrawCallExtras {
	u32 first_index = 0;
	u32 base_vertex = 0;
	Optional< u32 > override_num_vertices = NONE;
};

void EncodeDrawCall( Opaque< CommandBuffer > cmd_buf, const PipelineState & pipeline_state, Mesh mesh, Span< const GPUBuffer > buffers, DrawCallExtras extras );
void EncodeIndirectDrawCall( Opaque< CommandBuffer > cmd_buf, const PipelineState & pipeline_state, Mesh mesh, GPUBuffer indirect_args, Span< const GPUBuffer > buffers );
// void EncodeBindMesh( Opaque< CommandBuffer > cmd_buf, const DrawCall & draw );
// void EncodeBindMaterial( Opaque< CommandBuffer > cmd_buf, const DrawCall & draw );

struct Scissor {
	u32 x, y, w, h;
};

void EncodeScissor( Opaque< CommandBuffer > cmd_buf, Optional< Scissor > scissor );

/*
 * Compute passes
 */

struct ComputePassConfig {
	Span< const char > name;
	RenderPass pass; // Metal synchronization
	Span< const GPUBarrier > barriers; // Vulkan synchronization
};

Opaque< CommandBuffer > NewComputePass( const ComputePassConfig & config );

void EncodeComputeCall( Opaque< CommandBuffer > cmd_buf, PoolHandle< ComputePipeline > shader, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers );
void EncodeIndirectComputeCall( Opaque< CommandBuffer > cmd_buf, PoolHandle< ComputePipeline > shader, GPUBuffer indirect_args, Span< const BufferBinding > buffers );

/*
 * High level stuff NOMERGE
 */

struct MatrixPalettes {
	Span< const Mat3x4 > node_transforms;
	Span< const Mat3x4 > skinning_matrices;
};

void Draw( RenderPass pass, const PipelineState & pipeline_state, Mesh mesh, Span< const GPUBuffer > buffers = { }, DrawCallExtras extras = DrawCallExtras() );
void DrawIndirect( RenderPass pass, const PipelineState & pipeline_state, Mesh mesh, GPUBuffer indirect, Span< const GPUBuffer > buffers );
void EncodeComputeCall( RenderPass pass, PoolHandle< ComputePipeline > shader, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers );
void EncodeIndirectComputeCall( RenderPass pass, PoolHandle< ComputePipeline > shader, GPUBuffer indirect_args, Span< const BufferBinding > buffers );
void EncodeScissor( RenderPass pass, Optional< Scissor > scissor );

/*
 * Material
 */

#include "client/renderer/material.h"

struct Material {
	BoundedString< 64 > name;

	RenderPass render_pass;
	PoolHandle< RenderPipeline > shader;
	PoolHandle< BindGroup > bind_group;
	RenderPipelineDynamicState dynamic_state;

	ColorGen rgbgen;
	ColorGen alphagen;

	struct {
		// used to recreate material bindgroups after texture hotloading
		// TODO(mike 20260406): we should only need texture_name here, check
		// whether we can do partial descriptor set updates in vulkan
		StringHash texture;
		SamplerType sampler;
		MaterialProperties properties;
	} stuff_to_recreate_bind_group;

	// TODO(mike 20260406): we should get rid of this and expose textures in
	// the public API since this is only used by things that don't care about
	// the material anyway, e.g. menu icons and various VFX
	PoolHandle< Texture > texture;
};

template<> struct PoolHandleType< Material > { using T = u16; };
PoolHandle< BindGroup > NewMaterialBindGroup( Span< const char > name,
	PoolHandle< Texture > texture, SamplerType sampler, const MaterialProperties & properties,
	Optional< PoolHandle< BindGroup > > old_bind_group = NONE );

PoolHandle< Material > FindMaterial( StringHash name );
Optional< PoolHandle< Material > > TryFindMaterial( StringHash name );

u32 TextureWidth( PoolHandle< Material > material );
u32 TextureHeight( PoolHandle< Material > material );

RenderPass MaterialRenderPass( PoolHandle< Material > material );
PoolHandle< BindGroup > MaterialBindGroup( PoolHandle< Material > material );
PipelineState MaterialPipelineState( PoolHandle< Material > material );

/*
 * Frame
 */

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

struct FrameStatic {
	u32 viewport_width, viewport_height;
	Vec2 viewport;
	bool minimized;
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
		PoolHandle< Texture > resolved_color0;
		PoolHandle< Texture > resolved_color1;
		Optional< PoolHandle< Texture > > msaa_depth;
		PoolHandle< Texture > resolved_depth;
		PoolHandle< Texture > shadowmap;
	} render_targets;

	Optional< Opaque< CommandBuffer > > render_passes[ RenderPass_Count ];
};

inline FrameStatic frame_static;

void RendererBeginFrame( SDL_Window * window, u32 viewport_width, u32 viewport_height, bool minimized, bool fullscreen_exclusive );
void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov );
void RendererEndFrame();

bool SwapchainIsNotsRGB();
