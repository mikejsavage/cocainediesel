#pragma once

#include "client/renderer/api.h"
#include "client/renderer/dds.h"

/*
 * Init
 */

void InitRenderBackend();
void ShutdownRenderBackend();

void InitShaders();
void HotloadShaders();

/*
 * Memory allocation
 */

void InitRenderBackendAllocators( size_t slab_size, size_t constant_buffer_alignment, size_t buffer_image_granularity );
void ShutdownRenderBackendAllocators();
void ClearGPUArenaAllocators();

struct CoherentMemory {
	PoolHandle< GPUAllocation > allocation;
	void * ptr;
};

struct GPUSlabAllocator {
	struct Slab {
		PoolHandle< GPUAllocation > buffer;
		size_t cursor;
		size_t capacity;
		Slab * next;
	};

	size_t slab_size;
	Slab * slabs;

	size_t used;
	size_t wasted;

	size_t min_alignment;
	// to deal with bufferImageGranularity. we don't need to special case the
	// first allocation in a slab because align( 0, x ) == 0
	size_t buffer_image_granularity;
	bool last_allocation_was_buffer;
};

struct GPUArenaAllocator {
	PoolHandle< GPUAllocation > allocation;
	size_t min_alignment;
	size_t capacity;
	size_t cursor;
};

struct CoherentGPUArenaAllocator {
	GPUArenaAllocator a;
	void * ptr;
};

GPUSlabAllocator NewGPUSlabAllocator( size_t slab_size, size_t min_alignment, size_t buffer_image_granularity );
void DeleteGPUSlabAllocator( GPUSlabAllocator * a );
GPUBuffer NewBuffer( GPUSlabAllocator * a, const char * label, size_t size, size_t alignment, bool texture, const void * data = NULL );

GPUArenaAllocator NewDeviceGPUArenaAllocator( size_t size, size_t min_alignment );
CoherentGPUArenaAllocator NewCoherentGPUArenaAllocator( size_t size, size_t min_alignment );

PoolHandle< GPUAllocation > AllocateGPUMemory( size_t size );
CoherentMemory AllocateCoherentMemory( size_t size );

/*
 * Textures
 */

#if PLATFORM_MACOS
namespace MTL { class Texture; }
using BackendTexture = MTL::Texture *;
#else
using BackendTexture = VkTexture;
#endif

struct Texture {
	BoundedString< 64 > name;
	u64 hash;

	BackendTexture handle;
	TextureFormat format;
	u32 width, height;
	u32 depth;
	u32 num_mipmaps;
	void * stb_data;
	Span< const BC4Block > bc4_data;
	bool atlased;

	bool is_swapchain = false;
};

// pass a = NULL for a dedicated allocation
BackendTexture NewBackendTexture( const TextureConfig & config, Optional< BackendTexture > old_texture = NONE );
BackendTexture NewFramebufferBackendTexture( const TextureConfig & config, Optional< BackendTexture > old_texture );
BackendTexture NewBackendTexture( GPUSlabAllocator * a, const TextureConfig & config, Optional< BackendTexture > = NONE );
PoolHandle< Texture > UploadBC4( GPUSlabAllocator * a, const char * path );

u32 TextureWidth( PoolHandle< Texture > texture );
u32 TextureHeight( PoolHandle< Texture > texture );

/*
 * Resource transfers
 */

void CopyGPUBufferToBuffer(
	Opaque< CommandBuffer > cmd_buf,
	PoolHandle< GPUAllocation > dest, size_t dest_offset,
	PoolHandle< GPUAllocation > src, size_t src_offset,
	size_t n );
void CopyGPUBufferToTexture(
	Opaque< CommandBuffer > cmd_buf,
	BackendTexture dest, u32 w, u32 h, u32 num_layers, u32 mip_level,
	PoolHandle< GPUAllocation > src, size_t src_offset );

Opaque< CommandBuffer > NewTransferCommandBuffer();
void DeleteTransferCommandBuffer( Opaque< CommandBuffer > cb );

void UploadBuffer( GPUBuffer dest, const void * data, size_t n );
GPUBuffer StageArgumentBuffer( GPUBuffer dest, size_t n, size_t alignment );
void UploadTexture( const TextureConfig & config, BackendTexture dest );

/*
 * Debug info
 */

void AddDebugMarker( const char * label, PoolHandle< GPUAllocation > allocation, size_t offset, size_t size );
void RemoveAllDebugMarkers( PoolHandle< GPUAllocation > allocation );

PoolHandle< BindGroup > NewMaterialBindGroup( Span< const char > name, BackendTexture texture, SamplerType sampler, GPUBuffer properties );

size_t FrameSlot();

constexpr u32 Log2_CT( u64 x ) {
	u32 log = 0;
	x >>= 1;

	while( x > 0 ) {
		x >>= 1;
		log++;
	}

	return log;
}

constexpr u32 MaxMSAA = 8;

constexpr size_t MaxBufferBindings = 8;
constexpr size_t MaxTextureBindings = 8;

constexpr size_t MaxShaderVariants = 8;

constexpr size_t MaxMaterials = 4096;
