#pragma once

#include "client/renderer/api.h"
#include "client/renderer/dds.h"

/*
 * Init
 */

void InitRenderBackend( SDL_Window * window );
void ShutdownRenderBackend();

void InitShaders();
void HotloadShaders();

/*
 * Memory allocation
 */

constexpr size_t ArenaAllocatorSize = Megabytes( 32 );
void InitGPUAllocators( size_t slab_size, size_t constant_buffer_alignment, size_t buffer_image_granularity );
void ShutdownGPUAllocators();
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

GPUBuffer NewBuffer( GPUSlabAllocator * a, const char * label, size_t size, size_t alignment, bool texture, const void * data = NULL );

PoolHandle< GPUAllocation > AllocateGPUMemory( size_t size );
CoherentMemory AllocateCoherentMemory( size_t size );

/*
 * Textures
 */

struct BackendTexture;
template<> inline constexpr size_t OpaqueSize< BackendTexture > = 64;

struct Texture {
	BoundedString< 64 > name;
	u64 hash;

	Opaque< BackendTexture > backend;
	TextureFormat format;
	u32 width, height;
	u32 num_layers;
	u32 num_mipmaps;
	u32 msaa_samples;

	bool dummy_slot_for_missing_texture;
	void * stb_data;
	Span< const BC4Block > bc4_data;
	bool atlased;
};

Opaque< BackendTexture > NewBackendTexture( const TextureConfig & config );
Opaque< BackendTexture > NewBackendTexture( GPUSlabAllocator * a, const TextureConfig & config );
void DeleteDedicatedAllocationTexture( Opaque< BackendTexture > texture );

u32 TextureWidth( PoolHandle< Texture > texture );
u32 TextureHeight( PoolHandle< Texture > texture );
u32 TextureMSAASamples( PoolHandle< Texture > texture );
TextureFormat GetTextureFormat( PoolHandle< Texture > texture );

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
	Opaque< BackendTexture > dest, TextureFormat format, u32 w, u32 h, u32 num_layers, u32 mip_level,
	PoolHandle< GPUAllocation > src, size_t src_offset );

Opaque< CommandBuffer > NewTransferCommandBuffer();
void DeleteTransferCommandBuffer( Opaque< CommandBuffer > cb );

void UploadBuffer( GPUBuffer dest, const void * data, size_t n );
GPUBuffer StageArgumentBuffer( GPUBuffer dest, size_t n, size_t alignment );
void UploadTexture( const TextureConfig & config, Opaque< BackendTexture > dest );

/*
 * Debug info
 */

void AddDebugMarker( PoolHandle< GPUAllocation > allocation, size_t offset, size_t size, const char * label );
void RemoveAllDebugMarkers( PoolHandle< GPUAllocation > allocation );

// NOMERGE: unsorted
PoolHandle< BindGroup > NewMaterialBindGroup( const char * name, Opaque< BackendTexture > texture, SamplerType sampler, GPUBuffer properties );

size_t FrameSlot();

constexpr size_t MaxBufferBindings = 8;
constexpr size_t MaxTextureBindings = 8;
constexpr size_t MaxBindings = MaxBufferBindings + 2 * MaxTextureBindings;

constexpr size_t MaxShaderVariants = 4;

constexpr size_t MaxMaterials = 4096;
