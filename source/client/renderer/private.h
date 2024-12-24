#pragma once

#include "api.h"

struct CoherentMemory {
	PoolHandle< GPUAllocation > allocation;
	void * ptr;
};

struct GPUAllocator {
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

struct GPUTempAllocator {
	CoherentMemory memory;
	size_t min_alignment;
	size_t cursor;
	size_t capacity;
};

GPUAllocator NewGPUAllocator( size_t slab_size, size_t min_alignment, size_t buffer_image_granularity );
void DeleteGPUAllocator( GPUAllocator * a );
GPUBuffer NewBuffer( GPUAllocator * a, const char * label, size_t size, size_t alignment, bool texture, const void * data = NULL );
void ResetGPUAllocator( GPUAllocator * a );

GPUTempAllocator NewGPUTempAllocator( size_t size, size_t min_alignment );
void ClearGPUTempAllocator( GPUTempAllocator * a );

PoolHandle< GPUAllocation > AllocateGPUMemory( size_t size );
CoherentMemory AllocateCoherentMemory( size_t size );

GPUTempBuffer NewTempBuffer( GPUTempAllocator * a, size_t size, size_t alignment );
GPUBuffer NewTempBuffer( GPUTempAllocator * a, const void * data, size_t size, size_t alignment );

// pass a = NULL for a dedicated allocation
PoolHandle< Texture > NewTexture( GPUAllocator * a, const TextureConfig & config, Optional< PoolHandle< Texture > > = NONE );
PoolHandle< Texture > UploadBC4( GPUAllocator * a, const char * path );

GPUAllocator * AllocatorForLifetime( GPULifetime lifetime );

void CopyGPUBufferToBuffer(
	Opaque< CommandBuffer > cmd_buf,
	PoolHandle< GPUAllocation > dest, size_t dest_offset,
	PoolHandle< GPUAllocation > src, size_t src_offset,
	size_t n );
void CopyGPUBufferToTexture(
	Opaque< CommandBuffer > cmd_buf,
	PoolHandle< Texture > dest, u32 w, u32 h, u32 num_layers, u32 mip_level,
	PoolHandle< GPUAllocation > src, size_t src_offset );

Opaque< CommandBuffer > NewTransferCommandBuffer();
void DeleteTransferCommandBuffer( Opaque< CommandBuffer > cb );

void InitStagingBuffer();
void ShutdownStagingBuffer();

void UploadBuffer( GPUBuffer dest, const void * data, size_t n );
GPUBuffer StageArgumentBuffer( GPUBuffer dest, size_t n, size_t alignment );
void UploadTexture( PoolHandle< Texture > dest, const void * data );

void AddDebugMarker( const char * label, PoolHandle< GPUAllocation > allocation, size_t offset, size_t size );
void RemoveAllDebugMarkers( PoolHandle< GPUAllocation > allocation );

PoolHandle< BindGroup > NewMaterialBindGroup( Span< const char > name, PoolHandle< Texture > texture, SamplerType sampler, GPUBuffer properties );

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

constexpr size_t Megabytes( size_t mb ) {
	return mb * 1024 * 1024;
}
