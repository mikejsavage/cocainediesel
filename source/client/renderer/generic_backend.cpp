#include "qcommon/base.h"
#include "client/assets.h"
#include "client/renderer/api.h"
#include "client/renderer/private.h"
#include "client/renderer/dds.h"

static constexpr size_t STAGING_BUFFER_CAPACITY = 32 * 1024 * 1024;
static CoherentMemory staging_buffer;
static Opaque< CommandBuffer > staging_command_buffer;
static size_t staging_buffer_cursor;

static GPUSlabAllocator persistent_allocator;
static GPUArenaAllocator device_temp_allocator;
static CoherentGPUArenaAllocator coherent_temp_allocator;

void InitRenderBackendAllocators( size_t slab_size, size_t constant_buffer_alignment, size_t buffer_image_granularity ) {
	staging_buffer = AllocateCoherentMemory( STAGING_BUFFER_CAPACITY );
	staging_buffer_cursor = 0;
	staging_command_buffer = NewTransferCommandBuffer();

	persistent_allocator = NewGPUSlabAllocator( slab_size, constant_buffer_alignment, buffer_image_granularity );
	device_temp_allocator = NewDeviceGPUArenaAllocator( Megabytes( 32 ), constant_buffer_alignment );
	coherent_temp_allocator = NewCoherentGPUArenaAllocator( Megabytes( 32 ), constant_buffer_alignment );
}

void ShutdownRenderBackendAllocators() {
	DeleteTransferCommandBuffer( staging_command_buffer );
}

static size_t Reserve( size_t n, size_t alignment ) {
	Assert( n <= STAGING_BUFFER_CAPACITY );

	if( AlignPow2( staging_buffer_cursor, alignment ) + n > STAGING_BUFFER_CAPACITY ) {
		FlushStagingBuffer();
	}

	size_t cursor = AlignPow2( staging_buffer_cursor, alignment );
	staging_buffer_cursor = cursor + n;
	return cursor;
}

static size_t Stage( const void * data, size_t n, size_t alignment ) {
	size_t cursor = Reserve( n, alignment );
	memcpy( ( ( char * ) staging_buffer.ptr ) + cursor, data, n );
	return cursor;
}

GPUBuffer StageArgumentBuffer( GPUBuffer dest, size_t n, size_t alignment ) {
	size_t cursor = Reserve( n, alignment );
	CopyGPUBufferToBuffer( staging_command_buffer, dest.allocation, dest.offset, staging_buffer.allocation, cursor, n );
	return GPUBuffer {
		.allocation = staging_buffer.allocation,
		.offset = cursor,
		.size = n,
	};
}

void UploadBuffer( GPUBuffer dest, const void * data, size_t n ) {
	size_t cursor = Stage( data, n, 1 );
	CopyGPUBufferToBuffer( staging_command_buffer, dest.allocation, dest.offset, staging_buffer.allocation, cursor, n );
}

static void UploadMipLevel( PoolHandle< Texture > dest, u32 w, u32 h, u32 num_layers, u32 mip_level, const void * data, size_t n ) {
	size_t cursor = Stage( data, n, 16 );
	CopyGPUBufferToTexture( staging_command_buffer, dest, w, h, num_layers, mip_level, staging_buffer.allocation, cursor );
}

void UploadTexture( PoolHandle< Texture > dest, const void * data ) {
	u32 w = TextureWidth( dest );
	u32 h = TextureHeight( dest );
	u32 num_layers = TextureLayers( dest );

	constexpr size_t bc4_bpp = 4; // TODO

	const char * cursor = ( const char * ) data;
	for( u32 i = 0; i < TextureMipLevels( dest ); i++ ) {
		u32 mip_w = w >> i;
		u32 mip_h = h >> i;
		size_t mip_bytes = ( mip_w * mip_h * num_layers * bc4_bpp ) / 8;
		UploadMipLevel( dest, mip_w, mip_h, num_layers, i, cursor, mip_bytes );

		cursor += mip_bytes;
	}
}

void FlushStagingBuffer() {
	// TODO: should double buffer this and use semaphores so we can stage and transfer in parallel
	SubmitCommandBuffer( staging_command_buffer, SubmitCommandBuffer_Wait );
	staging_command_buffer = NewTransferCommandBuffer();

	staging_buffer_cursor = 0;
}

GPUSlabAllocator NewGPUAllocator( size_t slab_size, size_t min_alignment, size_t buffer_image_granularity ) {
	GPUSlabAllocator::Slab * dummy = Alloc< GPUSlabAllocator::Slab >( sys_allocator );
	*dummy = { };
	return GPUSlabAllocator {
		.slab_size = slab_size,
		.slabs = dummy,
		.min_alignment = min_alignment,
		.buffer_image_granularity = buffer_image_granularity,
	};
}

void DeleteGPUAllocator( GPUSlabAllocator * a ) {
	printf( "used %zu bytes, wasted %zu bytes, unused %zu bytes\n", a->used, a->wasted, a->slabs == NULL ? 0 : a->slabs->capacity - a->slabs->cursor );

	GPUSlabAllocator::Slab * slab = a->slabs;
	while( slab != NULL ) {
		GPUSlabAllocator::Slab * next = slab->next;
		free( slab );
		slab = next;
	}
}

GPUBuffer NewBuffer( GPUSlabAllocator * a, const char * label, size_t size, size_t alignment, bool texture, const void * data ) {
	Assert( IsPowerOf2( alignment ) );
	Assert( IsPowerOf2( a->buffer_image_granularity ) );

	// https://vulkan.gpuinfo.org/displaycoreproperty.php?core=1.1&name=maxMemoryAllocationSize&platform=all
	// lowest max alloc size is still 500MB, which is bigger than anything we
	// should need to allocate
	Assert( size < a->slab_size );

	alignment = Max2( alignment, a->min_alignment );

	if( a->last_allocation_was_buffer == texture ) {
		// bufferImageGranularity is specced to be pow2 so this is ok
		alignment = Max2( alignment, a->buffer_image_granularity );
	}

	size_t aligned_cursor = AlignPow2( a->slabs->cursor, alignment );
	if( aligned_cursor > a->slab_size || a->slabs->capacity - aligned_cursor < size ) {
		a->wasted += a->slabs->capacity - a->slabs->cursor;

		// check if the next slab has its cursor at 0, i.e. the allocator has
		// been reset, and swap that to the head instead of allocating
		if( a->slabs->next != NULL && a->slabs->next->cursor == 0 && a->slabs->next->capacity > 0 ) {
			GPUSlabAllocator::Slab * next_next = a->slabs->next->next;
			GPUSlabAllocator::Slab * next = a->slabs->next;
			next->next = a->slabs;
			a->slabs->next = next_next;
			a->slabs = next;
		}
		else {
			GPUSlabAllocator::Slab * new_slab = Alloc< GPUSlabAllocator::Slab >( sys_allocator );
			*new_slab = GPUSlabAllocator::Slab {
				.buffer = AllocateGPUMemory( a->slab_size ),
				.cursor = 0,
				.capacity = a->slab_size,
				.next = a->slabs,
			};
			a->slabs = new_slab;
		}
		aligned_cursor = 0;
	}

	a->used += size;
	a->wasted += aligned_cursor - a->slabs->cursor;
	a->slabs->cursor = aligned_cursor + size;
	a->last_allocation_was_buffer = !texture;

	AddDebugMarker( label, a->slabs->buffer, aligned_cursor, size );

	GPUBuffer buffer = {
		.allocation = a->slabs->buffer,
		.offset = aligned_cursor,
		.size = size,
	};

	if( data != NULL ) {
		UploadBuffer( buffer, data, size );
	}

	return buffer;
}

GPUBuffer NewBuffer( const char * label, size_t size, size_t alignment, bool texture, const void * data ) {
	return NewBuffer( &persistent_allocator, label, size, alignment, texture, data );
}

GPUArenaAllocator NewDeviceGPUArenaAllocator( size_t size, size_t min_alignment ) {
	Assert( IsPowerOf2( min_alignment ) );

	return GPUArenaAllocator {
		.allocation = AllocateGPUMemory( size * MaxFramesInFlight ),
		.min_alignment = min_alignment,
		.capacity = size,
	};
}

CoherentGPUArenaAllocator NewCoherentGPUArenaAllocator( size_t size, size_t min_alignment ) {
	Assert( IsPowerOf2( min_alignment ) );

	CoherentMemory memory = AllocateCoherentMemory( size * MaxFramesInFlight );
	return CoherentGPUArenaAllocator {
		.a = GPUArenaAllocator {
			.allocation = memory.allocation,
			.min_alignment = min_alignment,
			.capacity = size,
		},
		.ptr = memory.ptr,
	};
}

static void ClearGPUArenaAllocator( GPUArenaAllocator * a ) {
	a->cursor = 0;
}

void ClearGPUArenaAllocators() {
	ClearGPUArenaAllocator( &device_temp_allocator );
	ClearGPUArenaAllocator( &coherent_temp_allocator.a );
}

GPUBuffer NewDeviceTempBuffer( GPUArenaAllocator * a, size_t size, size_t alignment ) {
	// alignment and min_alignment are both pow2 so Max2( alignment, min_alignment ) == LeastCommonMultiple( alignment, min_alignment )
	Assert( IsPowerOf2( alignment ) );
	size_t aligned_cursor = AlignPow2( a->cursor, Max2( alignment, a->min_alignment ) );
	Assert( a->capacity - aligned_cursor >= size );

	a->cursor = aligned_cursor + size;

	return GPUBuffer {
		.allocation = a->allocation,
		.offset = aligned_cursor + a->capacity * FrameSlot(),
		.size = size,
	};
}

CoherentBuffer NewTempBuffer( size_t size, size_t alignment ) {
	GPUBuffer buffer = NewDeviceTempBuffer( &coherent_temp_allocator.a, size, alignment );
	return CoherentBuffer {
		.buffer = buffer,
		.ptr = ( ( char * ) coherent_temp_allocator.ptr ) + buffer.offset,
	};
}

GPUBuffer NewTempBuffer( const void * data, size_t size, size_t alignment ) {
	CoherentBuffer buffer = NewTempBuffer( size, alignment );
	memcpy( buffer.ptr, data, size );
	return buffer.buffer;
}

PoolHandle< Texture > NewTexture( const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture ) {
	return NewTexture( &persistent_allocator, config, old_texture );
}

PoolHandle< Texture > NewFramebufferTexture( const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture ) {
	return NewTexture( NULL, config, old_texture );
}

bool operator==( const VertexAttribute & lhs, const VertexAttribute & rhs ) {
	return lhs.format == rhs.format && lhs.buffer == rhs.buffer && lhs.offset == rhs.offset;
}

bool operator==( const VertexDescriptor & lhs, const VertexDescriptor & rhs ) {
	for( size_t i = 0; i < ARRAY_COUNT( lhs.attributes ); i++ ) {
		if( lhs.attributes[ i ] != rhs.attributes[ i ] ) {
			return false;
		}
	}

	for( size_t i = 0; i < ARRAY_COUNT( lhs.buffer_strides ); i++ ) {
		if( lhs.buffer_strides[ i ] != rhs.buffer_strides[ i ] ) {
			return false;
		}
	}

	return true;
}

struct PersistentBufferAllocator {
	PoolHandle< GPUAllocation > memory;
	size_t capacity;
	size_t cursor;
};
