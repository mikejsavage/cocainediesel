#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "client/renderer/renderer.h"

namespace MTL {
	class VertexDescriptor;
}
#include "metal-cpp/SingleHeader/Metal.hpp"
#include <dispatch/dispatch.h>

static dispatch_semaphore_t frame_semaphore;
static bool capturing_this_frame;
static u64 frame_counter;
static u64 num_vertices_this_frame;

static NonRAIIDynamicArray< int > render_passes;
static NonRAIIDynamicArray< int > draw_calls;

struct {
	MTL::Device * device;
	CA::MetalLayer * swapchain;
	MTL::CommandQueue * command_queue;
} metal;

void PipelineState::bind_uniform( StringHash name, UniformBlock block ) {
	bind_buffer( name, block.buffer, block.offset, block.size );
}

void PipelineState::bind_texture( StringHash name, const Texture * texture ) {
	for( size_t i = 0; i < ARRAY_COUNT( textures ); i++ ) {
		if( shader->textures[ i ] == name.hash ) {
			textures[ num_textures ] = { name.hash, texture };
			num_textures++;
			return;
		}
	}
}

void PipelineState::bind_buffer( StringHash name, GPUBuffer buffer, u32 offset, u32 size ) {
	for( size_t i = 0; i < ARRAY_COUNT( buffers ); i++ ) {
		if( shader->buffers[ i ] == name.hash ) {
			buffers[ num_buffers ] = { name.hash, buffer, offset, size };
			num_buffers++;
			return;
		}
	}

	abort();
}

void PipelineState::bind_streaming_buffer( StringHash name, StreamingBuffer stream ) {
	u32 offset = stream.size * FrameSlot();
	bind_buffer( name, stream.buffer, offset, stream.size );
}

extern "C" void GLFWShouldDoThisForUs( CA::MetalLayer * swapchain );

void InitRenderBackend() {
	TracyZoneScoped;

	frame_semaphore = dispatch_semaphore_create( MAX_FRAMES_IN_FLIGHT );
	capturing_this_frame = false;
	frame_counter = 0;

	metal.device = MTL::CreateSystemDefaultDevice();

	metal.swapchain = CA::MetalLayer::layer();
	metal.swapchain->setDevice( metal.device );
	metal.swapchain->setPixelFormat( MTL::PixelFormatBGRA8Unorm );

	metal.command_queue = metal.device->newCommandQueue();

	GLFWShouldDoThisForUs( metal.swapchain );

	render_passes.init( sys_allocator );
	draw_calls.init( sys_allocator );
}

void ShutdownRenderBackend() {
	render_passes.shutdown();
	draw_calls.shutdown();

	metal.command_queue->release();
	metal.swapchain->release();
	metal.device->release();
}

void RenderBackendBeginFrame() {
	TracyZoneScoped;

	dispatch_semaphore_wait( frame_semaphore, DISPATCH_TIME_FOREVER );

	render_passes.clear();
	draw_calls.clear();

	num_vertices_this_frame = 0;
}

void RenderBackendSubmitFrame() {
	TracyZoneScoped;

	frame_counter++;

	TracyPlotSample( "Draw calls", s64( draw_calls.size() ) );
	TracyPlotSample( "Vertices", s64( num_vertices_this_frame ) );
}

GPUBuffer NewGPUBuffer( const void * data, u32 size, const char * name ) {
	MTL::Buffer * buf = metal.device->newBuffer( data, size, MTL::ResourceStorageModeManaged );
	return { buf };
}

void DeleteGPUBuffer( GPUBuffer buf ) {
	if( buf.buffer != NULL ) {
		buf.buffer->release();
	}
}

StreamingBuffer NewStreamingBuffer( u32 size, const char * name ) {
	StreamingBuffer stream;
	stream.buffer.buffer = metal.device->newBuffer( size * MAX_FRAMES_IN_FLIGHT, MTL::ResourceStorageModeManaged );
	stream.ptr = stream.buffer.buffer->contents();
	stream.size = size;
	return stream;
}

void * GetStreamingBufferMemory( StreamingBuffer stream ) {
	return ( ( u8 * ) stream.ptr ) + stream.size * ( frame_counter % MAX_FRAMES_IN_FLIGHT );
}

void DeleteStreamingBuffer( StreamingBuffer stream ) {
	DeleteGPUBuffer( stream.buffer );
}

static MTL::PixelFormat TextureFormatToMetal( TextureFormat format ) {
	switch( format ) {
		case TextureFormat_R_U8: return MTL::PixelFormatR8Unorm;
		case TextureFormat_R_S8: return MTL::PixelFormatR8Snorm;
		case TextureFormat_R_UI8: return MTL::PixelFormatR8Uint;
		case TextureFormat_R_U16: return MTL::PixelFormatR16Uint;

		case TextureFormat_A_U8: return MTL::PixelFormatA8Unorm;

		case TextureFormat_RG_Half: return MTL::PixelFormatRG16Float;

		case TextureFormat_RA_U8: return MTL::PixelFormatRG8Unorm;

		case TextureFormat_RGBA_U8: return MTL::PixelFormatRGBA8Unorm;
		case TextureFormat_RGBA_U8_sRGB: return MTL::PixelFormatRGBA8Unorm_sRGB;

		case TextureFormat_BC1_sRGB: return MTL::PixelFormatBC1_RGBA_sRGB;
		case TextureFormat_BC3_sRGB: return MTL::PixelFormatBC3_RGBA_sRGB;
		case TextureFormat_BC4: return MTL::PixelFormatBC4_RUnorm;
		case TextureFormat_BC5: return MTL::PixelFormatBC5_RGUnorm;

		case TextureFormat_Depth: return MTL::PixelFormatDepth32Float;
		case TextureFormat_Shadow: return MTL::PixelFormatDepth32Float;

		default: Fatal( "lol" ); return { };
	}
}

static Texture NewTextureSamples( TextureConfig config, int msaa_samples ) {
	Texture texture = { };
	texture.width = config.width;
	texture.height = config.height;
	texture.num_mipmaps = config.num_mipmaps;
	texture.msaa = msaa_samples > 1;
	texture.format = config.format;

	MTL::TextureSwizzleChannels swizzle_rrr1 = {
		.red = MTL::TextureSwizzleRed,
		.green = MTL::TextureSwizzleRed,
		.blue = MTL::TextureSwizzleRed,
		.alpha = MTL::TextureSwizzleOne,
	};

	MTL::TextureSwizzleChannels swizzle_111r = {
		.red = MTL::TextureSwizzleOne,
		.green = MTL::TextureSwizzleOne,
		.blue = MTL::TextureSwizzleOne,
		.alpha = MTL::TextureSwizzleRed,
	};

	MTL::TextureSwizzleChannels swizzle_rrrg = {
		.red = MTL::TextureSwizzleOne,
		.green = MTL::TextureSwizzleOne,
		.blue = MTL::TextureSwizzleOne,
		.alpha = MTL::TextureSwizzleRed,
	};

	Optional< MTL::TextureSwizzleChannels > swizzle = NONE;
	switch( config.format ) {
		case TextureFormat_R_U8:
		case TextureFormat_R_S8:
		case TextureFormat_BC4:
			swizzle = swizzle_rrr1;
			break;

		case TextureFormat_A_U8:
			swizzle = swizzle_111r;
			break;

		case TextureFormat_RA_U8:
			swizzle = swizzle_rrrg;
			break;

		default:
			break;
	}

	MTL::TextureDescriptor * descriptor = MTL::TextureDescriptor::alloc()->init();
	defer { descriptor->release(); };
	descriptor->setStorageMode( MTL::StorageModeManaged );
	descriptor->setUsage( MTL::TextureUsageShaderRead ); // TODO
	descriptor->setTextureType( texture.msaa ? MTL::TextureType2DMultisample : MTL::TextureType2D );
	descriptor->setPixelFormat( TextureFormatToMetal( config.format ) );
	descriptor->setWidth( config.width );
	descriptor->setHeight( config.height );
	descriptor->setMipmapLevelCount( config.num_mipmaps );
	if( swizzle.exists ) {
		descriptor->setSwizzle( swizzle.value );
	}

	texture.handle = metal.device->newTexture( descriptor );

	// TODO: figure out how samplers should work...
	// TODO: TextureFormat_Shadow needs compare sampler
	if( !CompressedTextureFormat( config.format ) ) {
		Assert( config.num_mipmaps == 1 );
		texture.handle->replaceRegion( MTL::Region( 0, 0, config.width, config.height ), 0, config.data, config.width * BitsPerPixel( config.format ) / 8 );
	}
	else {
		const u8 * cursor = ( const u8 * ) config.data;
		for( u32 i = 0; i < config.num_mipmaps; i++ ) {
			u32 w = config.width >> i;
			u32 h = config.height >> i;
			u32 bytes_per_row_of_blocks = w * 4 * BitsPerPixel( config.format ) / 8;
			texture.handle->replaceRegion( MTL::Region( 0, 0, w, h ), i, cursor, bytes_per_row_of_blocks );
			cursor += ( BitsPerPixel( config.format ) * w * h ) / 8;
		}
	}

	return texture;
}

Texture NewTexture( TextureConfig config ) {
	return NewTextureSamples( config, 0 );
}

void DeleteTexture( Texture texture ) {
	if( texture.handle != NULL ) {
		texture.handle->release();
	}
}

#endif
