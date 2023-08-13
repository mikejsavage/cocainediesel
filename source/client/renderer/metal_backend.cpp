#include "qcommon/platform.h"

#if PLATFORM_MACOS

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "client/renderer/renderer.h"

namespace MTL {
	class VertexDescriptor;
}
#include "metal-cpp/SingleHeader/Metal.hpp"
#include <dispatch/dispatch.h>

static dispatch_semaphore_t frame_semaphore;
static bool capturing_this_frame;
static u64 num_vertices_this_frame;

static NonRAIIDynamicArray< int > render_passes;
static NonRAIIDynamicArray< int > draw_calls;

static NonRAIIDynamicArray< Mesh > deferred_mesh_deletes;
static NonRAIIDynamicArray< GPUBuffer > deferred_buffer_deletes;
static NonRAIIDynamicArray< StreamingBuffer > deferred_streaming_buffer_deletes;

struct {
	MTL::Device * device;
	CA::MetalLayer * swapchain;
	MTL::CommandQueue * command_queue;
} metal;

void PipelineState::bind_uniform( StringHash name, UniformBlock block ) {
	bind_buffer( name, block.buffer, block.offset, block.size );
}

void PipelineState::bind_texture_and_sampler( StringHash name, const Texture * texture, SamplerType sampler ) {
	for( size_t i = 0; i < ARRAY_COUNT( textures ); i++ ) {
		if( shader->textures[ i ] == name.hash ) {
			textures[ num_textures ] = { name.hash, texture, sampler };
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

// TODO dedupe
static void RunDeferredDeletes() {
	TracyZoneScoped;

	for( const Mesh & mesh : deferred_mesh_deletes ) {
		DeleteMesh( mesh );
	}
	deferred_mesh_deletes.clear();

	for( const GPUBuffer & buffer : deferred_buffer_deletes ) {
		DeleteGPUBuffer( buffer );
	}
	deferred_buffer_deletes.clear();

	for( const StreamingBuffer & stream : deferred_streaming_buffer_deletes ) {
		DeleteStreamingBuffer( stream );
	}
	deferred_streaming_buffer_deletes.clear();
}


void InitRenderBackend() {
	TracyZoneScoped;

	frame_semaphore = dispatch_semaphore_create( MAX_FRAMES_IN_FLIGHT );
	capturing_this_frame = false;

	metal.device = MTL::CreateSystemDefaultDevice();

	metal.swapchain = CA::MetalLayer::layer();
	metal.swapchain->setDevice( metal.device );
	metal.swapchain->setPixelFormat( MTL::PixelFormatBGRA8Unorm );

	metal.command_queue = metal.device->newCommandQueue();

	GLFWShouldDoThisForUs( metal.swapchain );

	render_passes.init( sys_allocator );
	draw_calls.init( sys_allocator );
	deferred_mesh_deletes.init( sys_allocator );
	deferred_buffer_deletes.init( sys_allocator );
	deferred_streaming_buffer_deletes.init( sys_allocator );
}

void ShutdownRenderBackend() {
	RunDeferredDeletes();

	render_passes.shutdown();
	draw_calls.shutdown();
	deferred_mesh_deletes.shutdown();
	deferred_buffer_deletes.shutdown();
	deferred_streaming_buffer_deletes.shutdown();

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

	RunDeferredDeletes();

	TracyPlotSample( "Draw calls", s64( draw_calls.size() ) );
	TracyPlotSample( "Vertices", s64( num_vertices_this_frame ) );
}

GPUBuffer NewGPUBuffer( const void * data, u32 size, const char * name ) {
	MTL::Buffer * buf = metal.device->newBuffer( data, size, MTL::ResourceStorageModeManaged );
	NS::String * label = NS::String::string( name, NS::UTF8StringEncoding );
	defer { label->release(); };
	buf->setLabel( label );
	return { .buffer = buf };
}

GPUBuffer NewGPUBuffer( u32 size, const char * name ) {
	MTL::Buffer * buf = metal.device->newBuffer( size, MTL::ResourceStorageModeManaged );
	NS::String * label = NS::String::string( name, NS::UTF8StringEncoding );
	defer { label->release(); };
	buf->setLabel( label );
	return { .buffer = buf };
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
	return ( ( u8 * ) stream.ptr ) + stream.size * FrameSlot();
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

static MTL::SamplerAddressMode SamplerWrapToMetal( SamplerWrap wrap ) {
	switch( wrap ) {
		case SamplerWrap_Repeat:
			return MTL::SamplerAddressModeRepeat;
		case SamplerWrap_Clamp:
			return MTL::SamplerAddressModeClampToEdge;
	}

	Assert( false );
	return { };
}

Sampler NewSampler( const SamplerConfig & config ) {
	MTL::SamplerDescriptor * descriptor = MTL::SamplerDescriptor::alloc()->init();
	defer { descriptor->release(); };

	descriptor->setSAddressMode( SamplerWrapToMetal( config.wrap ) );
	descriptor->setTAddressMode( SamplerWrapToMetal( config.wrap ) );

	MTL::SamplerMinMagFilter minmag_filter = config.filter ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest;
	descriptor->setMinFilter( minmag_filter );
	descriptor->setMagFilter( minmag_filter );
	descriptor->setMipFilter( config.filter ? MTL::SamplerMipFilterLinear : MTL::SamplerMipFilterNearest );

	// TODO: lodbias

	if( config.shadowmap_sampler ) {
		descriptor->setCompareFunction( MTL::CompareFunctionLessEqual );
	}

	return Sampler {
		.handle = metal.device->newSamplerState( descriptor ),
	};
}

void DeleteSampler( Sampler sampler ) {
	if( sampler.handle != NULL ) {
		sampler.handle->release();
	}
}

Texture NewTexture( const TextureConfig & config ) {
	Texture texture = { };
	texture.width = config.width;
	texture.height = config.height;
	texture.num_layers = config.num_layers;
	texture.num_mipmaps = config.num_mipmaps;
	texture.msaa_samples = config.msaa_samples;
	texture.format = config.format;

	Optional< MTL::TextureSwizzleChannels > swizzle = NONE;
	switch( config.format ) {
		case TextureFormat_R_U8:
		case TextureFormat_R_S8:
		case TextureFormat_BC4:
			swizzle = {
				.red = MTL::TextureSwizzleRed,
				.green = MTL::TextureSwizzleRed,
				.blue = MTL::TextureSwizzleRed,
				.alpha = MTL::TextureSwizzleOne,
			};
			break;

		case TextureFormat_A_U8:
			swizzle = {
				.red = MTL::TextureSwizzleOne,
				.green = MTL::TextureSwizzleOne,
				.blue = MTL::TextureSwizzleOne,
				.alpha = MTL::TextureSwizzleRed,
			};

			break;

		case TextureFormat_RA_U8:
			swizzle = {
				.red = MTL::TextureSwizzleOne,
				.green = MTL::TextureSwizzleOne,
				.blue = MTL::TextureSwizzleOne,
				.alpha = MTL::TextureSwizzleRed,
			};
			break;

		default:
			break;
	}

	MTL::TextureDescriptor * descriptor = MTL::TextureDescriptor::alloc()->init();
	defer { descriptor->release(); };
	descriptor->setStorageMode( MTL::StorageModeManaged );
	descriptor->setUsage( MTL::TextureUsageShaderRead ); // TODO
	descriptor->setTextureType( config.msaa_samples > 0 ? MTL::TextureType2DMultisample : MTL::TextureType2D );
	descriptor->setPixelFormat( TextureFormatToMetal( config.format ) );
	descriptor->setWidth( config.width );
	descriptor->setHeight( config.height );
	descriptor->setDepth( config.num_layers );
	descriptor->setMipmapLevelCount( config.num_mipmaps );
	descriptor->setSampleCount( config.msaa_samples );
	if( swizzle.exists ) {
		descriptor->setSwizzle( swizzle.value );
	}

	texture.handle = metal.device->newTexture( descriptor );

	// TODO: need to use a staging buffer here
	// TODO: usage hints
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

void DeleteTexture( Texture texture ) {
	if( texture.handle != NULL ) {
		texture.handle->release();
	}
}

static void AddRenderTargetAttachment( const RenderTargetConfig::Attachment & attachment, Optional< u32 > * width, Optional< u32 > * height ) {
	Assert( !width->exists || attachment.texture.width == *width );
	Assert( !height->exists || attachment.texture.height == *height );
	*width = attachment.texture.width;
	*height = attachment.texture.height;
}

RenderTarget NewRenderTarget( const RenderTargetConfig & config ) {
	RenderTarget rt = { };

	Optional< u32 > width = NONE;
	Optional< u32 > height = NONE;

	for( size_t i = 0; i < ARRAY_COUNT( config.color_attachments ); i++ ) {
		const Optional< RenderTargetConfig::Attachment > & attachment = config.color_attachments[ i ];
		if( !attachment.exists )
			continue;
		AddRenderTargetAttachment( attachment.value, &width, &height );
		rt.color_attachments[ i ] = attachment.value.texture;
	}

	if( config.depth_attachment.exists ) {
		AddRenderTargetAttachment( config.depth_attachment.value, &width, &height );
		rt.depth_attachment = config.depth_attachment.value.texture;
	}

	Assert( width.exists && height.exists );
	rt.width = width.value;
	rt.height = height.value;

	return rt;
}

void DeleteRenderTarget( RenderTarget rt ) {
}

void DeleteRenderTargetAndTextures( RenderTarget rt ) {
	DeleteRenderTarget( rt );
	for( Texture texture : rt.color_attachments ) {
		DeleteTexture( texture );
	}
	DeleteTexture( rt.depth_attachment );
}

// TODO: dedupe this
Mesh NewMesh( const MeshConfig & config ) {
	Mesh mesh = { };
	mesh.vertex_descriptor = config.vertex_descriptor;
	mesh.index_format = config.index_format;
	mesh.num_vertices = config.num_vertices;
	mesh.cw_winding = config.cw_winding;

	for( size_t i = 0; i < ARRAY_COUNT( mesh.vertex_buffers ); i++ ) {
		mesh.vertex_buffers[ i ] = config.vertex_buffers[ i ];
	}
	mesh.index_buffer = config.index_buffer;

	return mesh;
}

void DeleteMesh( const Mesh & mesh ) {
	for( GPUBuffer buffer : mesh.vertex_buffers ) {
		DeleteGPUBuffer( buffer );
	}
	DeleteGPUBuffer( mesh.index_buffer );
}

void DeferDeleteMesh( const Mesh & mesh ) {
	deferred_mesh_deletes.add( mesh );
}

void DownloadFramebuffer( void * buf ) {
	// TODO https://stackoverflow.com/questions/33844130/take-a-snapshot-of-current-screen-with-metal-in-swift
	Com_Printf( S_COLOR_YELLOW "Screenshots aren't implemented on macOS yet\n" );
}

#endif // #if PLATFORM_MACOS
