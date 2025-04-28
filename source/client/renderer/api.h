#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "qcommon/opaque.h"

#include "client/renderer/shader_shared.h"

constexpr size_t MaxFramesInFlight = 2;

void InitRenderer();
void ShutdownRenderer();

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

CoherentBuffer NewTempBuffer( size_t size, size_t alignment );
GPUBuffer NewTempBuffer( const void * data, size_t size, size_t alignment );

template< typename T >
GPUBuffer NewTempBuffer( const T & x, size_t alignment = alignof( T ) ) {
	return NewTempBuffer( &x, sizeof( T ), alignment );
}

GPUBuffer NewDeviceTempBuffer( const char * label, size_t size, size_t alignment );

// template< typename T >
// GPUBuffer NewTempBuffer( size_t alignment = alignof( T ) ) {
// 	return NewTempBuffer( sizeof( T ), alignment, NULL );
// }

GPUBuffer NewBuffer( const char * label, size_t size, size_t alignment, bool texture, const void * data = NULL );

template< typename T >
GPUBuffer NewBuffer( const char * label, Span< const T > xs, size_t alignment = alignof( T ) ) {
	return NewBuffer( label, sizeof( T ) * xs.n, alignment, false, xs.ptr );
}

template< typename T >
GPUBuffer NewBuffer( const char * label, const T & x, size_t alignment = alignof( T ) ) {
	return NewBuffer( label, sizeof( T ), alignment, false, &x );
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
	const char * name;
	TextureFormat format;
	u32 width;
	u32 height;
	u32 num_layers = 1;
	u32 num_mipmaps = 1;
	u32 msaa_samples = 1;
	const void * data = NULL;
	bool dedicated_allocation = false;
};

enum TextureLayout {
	TextureLayout_ReadOnly,
	TextureLayout_ReadWrite,
	TextureLayout_Present,
};

PoolHandle< Texture > NewTexture( const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture = NONE );

TextureFormat GetTextureFormat( PoolHandle< Texture > texture );
u32 TextureWidth( PoolHandle< Texture > texture );
u32 TextureHeight( PoolHandle< Texture > texture );
u32 TextureLayers( PoolHandle< Texture > texture );
u32 TextureMipLevels( PoolHandle< Texture > texture );

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
	size_t num_vertices;

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
};

struct RenderPipelineOutputFormat {
	Optional< TextureFormat > colors[ FragmentShaderOutput_Count ];
	bool has_depth = true;
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

PoolHandle< RenderPipeline > NewRenderPipeline( const RenderPipelineConfig & config );

/*
 * ComputePipeline
 */

struct ComputePipeline;
template<> struct PoolHandleType< ComputePipeline > { using T = u8; };

PoolHandle< ComputePipeline > NewComputePipeline( Span< const char > path );

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

STATIC_ASSERT( sizeof( RenderPipelineDynamicState ) == 1 );

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
template<> inline constexpr size_t OpaqueSize< CommandBuffer > = 64;

enum CommandBufferSubmitType {
	SubmitCommandBuffer_Normal,
	SubmitCommandBuffer_Wait,
	SubmitCommandBuffer_Present,
};

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

struct RenderPassConfig {
	struct ColorTarget {
		PoolHandle< Texture > texture;
		u32 layer = 0;
		bool preserve_contents = true;
		Optional< Vec4 > clear = NONE;
		Optional< PoolHandle< Texture > > resolve_target = NONE;
	};

	struct DepthTarget {
		PoolHandle< Texture > texture;
		u32 layer = 0;
		bool preserve_contents = true;
		Optional< float > clear = NONE;
		Optional< PoolHandle< Texture > > resolve_target = NONE;
	};

	const char * name;
	Span< const ColorTarget > color_targets;
	Optional< DepthTarget > depth_target;

	PoolHandle< RenderPipeline > representative_shader;
	GPUBindings bindings;
};

Opaque< CommandBuffer > NewRenderPass( const RenderPassConfig & render_pass );
void SubmitCommandBuffer( Opaque< CommandBuffer > buffer, CommandBufferSubmitType type = SubmitCommandBuffer_Normal );

/*
 * Draw calls
 */

struct IndirectexedDrawArgs {
	u32 num_vertices;
	u32 num_instances;
	u32 first_vertex;
	u32 base_instance;
};

struct IndirectIndexedDrawArgs {
	u32 num_indices;
	u32 num_instances;
	u32 first_index;
	s32 base_vertex;
	u32 base_instance;
};

struct PipelineState {
	struct Scissor {
		u32 x, y, w, h;
	};

	PoolHandle< RenderPipeline > shader;
	RenderPipelineDynamicState dynamic_state;
	PoolHandle< BindGroup > material_bind_group;
	Optional< Scissor > scissor; // TODO
};

struct DrawCallExtras {
	u32 first_index = 0;
	u32 base_vertex = 0;
	Optional< size_t > override_num_vertices = NONE;
};

void EncodeDrawCall( Opaque< CommandBuffer > cmd_buf, const PipelineState & pipeline_state, Mesh mesh, Span< const BufferBinding > buffers, DrawCallExtras extras );
// void EncodeBindMesh( Opaque< CommandBuffer > cmd_buf, const DrawCall & draw );
// void EncodeBindMaterial( Opaque< CommandBuffer > cmd_buf, const DrawCall & draw );
// void EncodeScissor( Opaque< CommandBuffer > cmd_buf, Optional< Scissor > scissor );

/*
 * Compute passes
 */

struct IndirectComputeArgs {
	u32 num_threadgroups_x, num_threadgroups_y, num_threadgroups_z;
};

struct ComputePassConfig {
	const char * name;
	Optional< u64 > wait;
	Optional< u64 > signal;
};

Opaque< CommandBuffer > NewComputePass( const ComputePassConfig & compute_pass );

void EncodeComputeCall( Opaque< CommandBuffer > cmd_buf, PoolHandle< ComputePipeline > shader, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers );
void EncodeIndirectComputeCall( Opaque< CommandBuffer > cmd_buf, PoolHandle< ComputePipeline > shader, GPUBuffer indirect_args, Span< const BufferBinding > buffers );

/*
 * High level stuff TODO
 */

struct Transform {
	Quaternion rotation;
	Vec3 translation;
	float scale;
};

struct MatrixPalettes {
	Span< Mat3x4 > node_transforms;
	Span< Mat3x4 > skinning_matrices;
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
	RenderPass_SilhouetteGBuffer,
	RenderPass_NonworldOpaqueOutlined,
	RenderPass_AddOutlines,
	RenderPass_NonworldOpaque,

	RenderPass_Transparent,

	RenderPass_AddSilhouettes,

	RenderPass_UIBeforePostprocessing,
	RenderPass_Postprocessing,
	RenderPass_UIAfterPostprocessing,

	RenderPass_Count
};

enum RenderPassDependency {
	RenderPassDependency_ParticleUpdate_To_ParticleSetupIndirect,
	RenderPassDependency_Shadowmap_To_WorldOpaque,
};

void Draw( RenderPass pass, const PipelineState & pipeline_state, Mesh mesh, Span< const BufferBinding > buffers = { }, DrawCallExtras extras = DrawCallExtras() );
void EncodeComputeCall( RenderPass pass, PoolHandle< ComputePipeline > shader, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers );
void EncodeIndirectComputeCall( RenderPass pass, PoolHandle< ComputePipeline > shader, GPUBuffer indirect_args, Span< const BufferBinding > buffers );

/*
 * Material
 */

#include "material.h"

struct MaterialDescriptor {
	BlendFunc blend_func = BlendFunc_Disabled;

	RenderPipelineDynamicState dynamic_state;
	ColorGen rgbgen = { };
	ColorGen alphagen = { };

	PoolHandle< Texture > texture;
	SamplerType sampler = Sampler_Standard;

	bool outlined = true;
	bool shaded = false;
	bool world = false;

	MaterialProperties properties = {
		.specular = 0.0f,
		.shininess = 64.0f,
		.lod_bias = 0.0f,
	};
};

struct Material2 {
	Span< char > name;

	RenderPass render_pass;
	PoolHandle< RenderPipeline > shader;
	PoolHandle< BindGroup > bind_group;
	PoolHandle< Texture > texture;
	RenderPipelineDynamicState dynamic_state;

	ColorGen rgbgen;
	ColorGen alphagen;
};

Material2 NewMaterial( const MaterialDescriptor & desc );

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

inline FrameStatic frame_static;

void RendererBeginFrame( u32 viewport_width, u32 viewport_height );
void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov );
void RendererEndFrame();

void RenderBackendWaitForNewFrame();
PoolHandle< Texture > RenderBackendBeginFrame( bool capture );
void RenderBackendEndFrame();
