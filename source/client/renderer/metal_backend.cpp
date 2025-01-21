#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/arraymap.h"
#include "qcommon/hash.h"
#include "qcommon/pool.h"
#include "client/client.h"
#include "client/renderer/api.h"
#include "client/renderer/private.h"
#include <math.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include "glfw3/GLFW/glfw3.h"
#include "glfw3/GLFW/glfw3native.h"

namespace MTL {
	class VertexDescriptor;
}
#include "SingleHeader/Metal.hpp"

#include <dispatch/dispatch.h>

// #include "tracy/tracy/Tracy.hpp"

extern "C" void PleaseGLFWDoThisForMe( GLFWwindow * window, CA::MetalLayer * swapchain );

struct AutoReleaseString {
	NS::AutoreleasePool * pool;
	NS::String * nsstr;

	explicit AutoReleaseString( const char * str ) {
		pool = NS::AutoreleasePool::alloc()->init();
		nsstr = NS::String::string( str, NS::UTF8StringEncoding );
	}

	~AutoReleaseString() {
		pool->release();
	}

	operator NS::String *() { return nsstr; }
};

struct ArgumentBufferEncoder {
	MTL::ArgumentEncoder * encoder;
	ArrayMap< StringHash, u32, MaxBufferBindings > buffers;
	ArrayMap< StringHash, u32, MaxTextureBindings > textures;
	ArrayMap< StringHash, u32, MaxTextureBindings > samplers;
};

struct RenderPipeline {
	Span< const char > name;

	struct Variant {
		BoundedDynamicArray< MTL::RenderPipelineState *, Log2_CT( MaxMSAA + 1 ) > msaa_variants;
	};

	ArrayMap< VertexDescriptor, Variant, MaxShaderVariants > mesh_variants;

	ArgumentBufferEncoder render_pass_args;
	ArgumentBufferEncoder draw_call_args;
	bool clamp_depth;

	static constexpr size_t FirstVertexAttributeIndex = DescriptorSet_Count;
};

struct ComputePipeline {
	Span< const char > name;
	MTL::ComputePipelineState * pso;
	ArgumentBufferEncoder args;
};

/*
 * Staging buffer shit
 */

struct MetalDevice {
	MTL::Device * device;
	MTL::CommandQueue * command_queue;
	MTL::Event * event;
	u64 pass_counter;
	MTL::ArgumentEncoder * material_argument_encoder;
	MTL::ArgumentBuffersTier argument_buffers_tier;
	u32 max_msaa;

	GPUAllocator persistent_allocator;
	GPUAllocator framebuffer_allocator;
	GPUTempAllocator temp_allocator;
};

struct GPUAllocation {
	MTL::Heap * heap;
	MTL::Buffer * buffer;
};

struct Texture {
	MTL::Texture * texture;
	bool is_swapchain = false;
};

struct CommandBuffer {
	MTL::CommandBuffer * command_buffer;
	MTL::RenderCommandEncoder * rce;
	MTL::ComputeCommandEncoder * cce;
	MTL::BlitCommandEncoder * bce;
	Optional< u64 > signal;
};

struct BindGroup {
	GPUBuffer argument_buffer;
};

static MetalDevice global_device;
static GLFWwindow * global_window;
static CA::MetalLayer * global_swapchain;
static PoolHandle< ::Texture > swapchain_texture;
static dispatch_semaphore_t frame_semaphore;
static u64 frame_counter;
static u64 pass_counter;
static int old_framebuffer_width, old_framebuffer_height;

static struct {
	NS::AutoreleasePool * pool;
	CA::MetalDrawable * swapchain_surface;
	bool capture;
} frame;

static void StartCapture( MTL::Device * gpu ) {
	MTL::CaptureManager * capture_manager = MTL::CaptureManager::sharedCaptureManager();
	if( !capture_manager->supportsDestination( MTL::CaptureDestinationGPUTraceDocument ) ) {
		printf( "Capture support is not enabled\n" );
		abort();
	}

	char filename[ NAME_MAX ];
	time_t now;
	time( &now );
	strftime( filename, NAME_MAX, "trace-%Y%m%d%H%M%S.gputrace", localtime( &now ) );

	NS::String * capture_filename = NS::String::string( filename, NS::UTF8StringEncoding );
	NS::URL * url = NS::URL::alloc()->initFileURLWithPath( capture_filename );

	MTL::CaptureDescriptor * capture = MTL::CaptureDescriptor::alloc()->init();
	capture->setDestination( MTL::CaptureDestinationGPUTraceDocument );
	capture->setOutputURL( url );
	capture->setCaptureObject( gpu );

	printf( "Captured into %s\n", capture_filename->utf8String() );

	NS::Error * pError = NULL;
	if( !capture_manager->startCapture( capture, &pError ) ) {
		printf( "Failed to start capture: %s\n", pError->localizedDescription()->utf8String() );
		abort();
	}

	url->release();
	capture->release();

	frame.capture = true;
}

static void EndCapture() {
	MTL::CaptureManager::sharedCaptureManager()->stopCapture();
	frame.capture = false;
}

static Pool< GPUAllocation, 1024 > allocations;
static Pool< Texture, 1024 > textures;
static Pool< BindGroup, 1024 > bind_groups;
static Pool< RenderPipeline, 128 > render_pipelines;
static Pool< ComputePipeline, 128 > compute_pipelines;

PoolHandle< GPUAllocation > AllocateGPUMemory( size_t size ) {
	MTL::ResourceOptions options = MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeUntracked;

	NS::UInteger allocation_size = global_device.device->heapBufferSizeAndAlign( size, options ).size;

	MTL::HeapDescriptor * descriptor = MTL::HeapDescriptor::alloc()->init();
	descriptor->setSize( allocation_size );
	descriptor->setStorageMode( MTL::StorageModePrivate );
	descriptor->setType( MTL::HeapTypePlacement );
	descriptor->setHazardTrackingMode( MTL::HazardTrackingModeUntracked );
	defer { descriptor->release(); };

	MTL::Heap * heap = global_device.device->newHeap( descriptor );

	return allocations.allocate( GPUAllocation {
		.heap = heap,
		.buffer = heap->newBuffer( size, options, 0 ),
	} );
}

CoherentMemory AllocateCoherentMemory( size_t size ) {
	MTL::Buffer * buffer = global_device.device->newBuffer( size, MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeUntracked );
	return CoherentMemory {
		.allocation = allocations.allocate( { .buffer = buffer } ),
		.ptr = buffer->contents(),
	};
}

GPUAllocator * AllocatorForLifetime( GPULifetime lifetime ) {
	return lifetime == GPULifetime_Persistent ? &global_device.persistent_allocator : &global_device.framebuffer_allocator;
}

GPUTempBuffer NewTempBuffer( size_t size, size_t alignment ) {
	return NewTempBuffer( &global_device.temp_allocator, size, alignment );
}

GPUBuffer NewTempBuffer( const void * data, size_t size, size_t alignment ) {
	return NewTempBuffer( &global_device.temp_allocator, data, size, alignment );
}

void AddDebugMarker( const char * label, PoolHandle< GPUAllocation > allocation, size_t offset, size_t size ) {
	NS::Range range( checked_cast< NS::UInteger >( offset ), checked_cast< NS::UInteger >( size ) );
	allocations[ allocation ].buffer->addDebugMarker( AutoReleaseString( label ), range );
}

void RemoveAllDebugMarkers( PoolHandle< GPUAllocation > allocation ) {
	allocations[ allocation ].buffer->removeAllDebugMarkers();
}

Opaque< CommandBuffer > NewTransferCommandBuffer() {
	MTL::CommandBuffer * command_buffer = global_device.command_queue->commandBufferWithUnretainedReferences();
	return CommandBuffer {
		.command_buffer = command_buffer,
		.bce = command_buffer->blitCommandEncoder(),
	};
}

void DeleteTransferCommandBuffer( Opaque< CommandBuffer > cb ) {
	cb.unwrap()->bce->endEncoding();
}

void SubmitCommandBuffer( Opaque< CommandBuffer > buffer, CommandBufferSubmitType type ) {
	CommandBuffer * cmd_buf = buffer.unwrap();

	if( type == SubmitCommandBuffer_Present ) {
		cmd_buf->command_buffer->addCompletedHandler( [&]( MTL::CommandBuffer * ) {
			dispatch_semaphore_signal( frame_semaphore );
		} );
		cmd_buf->command_buffer->presentDrawable( frame.swapchain_surface );
	}

	if( cmd_buf->rce != NULL ) {
		cmd_buf->rce->endEncoding();
	}
	if( cmd_buf->cce != NULL ) {
		cmd_buf->cce->endEncoding();
	}
	if( cmd_buf->bce != NULL ) {
		cmd_buf->bce->endEncoding();
	}

	if( cmd_buf->signal.exists ) {
		cmd_buf->command_buffer->encodeSignalEvent( global_device.event, cmd_buf->signal.value );
	}

	cmd_buf->command_buffer->commit();

	if( type == SubmitCommandBuffer_Wait ) {
		cmd_buf->command_buffer->waitUntilCompleted();
	}
}

void CopyGPUBufferToBuffer(
	Opaque< CommandBuffer > cmd_buf,
	PoolHandle< GPUAllocation > dest, size_t dest_offset,
	PoolHandle< GPUAllocation > src, size_t src_offset,
	size_t n
) {
	cmd_buf.unwrap()->bce->copyFromBuffer( allocations[ src ].buffer, src_offset, allocations[ dest ].buffer, dest_offset, n );
}

void CopyGPUBufferToTexture(
	Opaque< CommandBuffer > cmd_buf,
	PoolHandle< Texture > dest, u32 w, u32 h, u32 num_layers, u32 mip_level,
	PoolHandle< GPUAllocation > src, size_t src_offset
) {
	size_t cursor = src_offset;
	u32 bytes_per_row_of_blocks = w * 4 * BitsPerPixel( TextureFormat_BC4 ) / 8;
	u32 bytes_per_slice = bytes_per_row_of_blocks * ( h / 4 );
	for( u32 i = 0; i < num_layers; i++ ) {
		cmd_buf.unwrap()->bce->copyFromBuffer( allocations[ src ].buffer, cursor,
			bytes_per_row_of_blocks, bytes_per_slice,
			MTL::Size::Make( w, h, 1 ),
			textures[ dest ].texture,
			i, mip_level, MTL::Origin::Make( 0, 0, 0 ) );
		cursor += bytes_per_slice;
	}
}

u32 TextureWidth( PoolHandle< Texture > texture ) { return textures[ texture ].texture->width(); }
u32 TextureHeight( PoolHandle< Texture > texture ) { return textures[ texture ].texture->height(); }
u32 TextureLayers( PoolHandle< Texture > texture ) { return textures[ texture ].texture->depth(); }
u32 TextureMipLevels( PoolHandle< Texture > texture ) { return textures[ texture ].texture->mipmapLevelCount(); }

/*
 * Back to normal shit
 */

enum SamplerWrap : u8 {
	SamplerWrap_Repeat,
	SamplerWrap_Clamp,
};

struct SamplerConfig {
	SamplerWrap wrap = SamplerWrap_Repeat;
	bool filter = true;
	bool shadowmap_sampler = false;
};

static MTL::SamplerAddressMode SamplerWrapToMetal( SamplerWrap wrap ) {
	switch( wrap ) {
		case SamplerWrap_Repeat: return MTL::SamplerAddressModeRepeat;
		case SamplerWrap_Clamp: return MTL::SamplerAddressModeClampToEdge;
	}
}

static MTL::SamplerState * NewSampler( const SamplerConfig & config ) {
	MTL::SamplerDescriptor * desc = MTL::SamplerDescriptor::alloc()->init();
	defer { desc->release(); };

	desc->setSAddressMode( SamplerWrapToMetal( config.wrap ) );
	desc->setTAddressMode( SamplerWrapToMetal( config.wrap ) );

	MTL::SamplerMinMagFilter min_mag_filter = config.filter ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest;
	desc->setMinFilter( min_mag_filter );
	desc->setMagFilter( min_mag_filter );
	desc->setMaxAnisotropy( 16 );

	desc->setMipFilter( config.filter ? MTL::SamplerMipFilterLinear : MTL::SamplerMipFilterNearest );

	if( config.shadowmap_sampler ) {
		desc->setCompareFunction( MTL::CompareFunctionLess );
	}

	desc->setSupportArgumentBuffers( true );

	return global_device.device->newSamplerState( desc );
}

static void DeleteSampler( MTL::SamplerState * sampler ) {
	sampler->release();
}

static MTL::SamplerState * samplers[ Sampler_Count ];

static void CreateSamplers() {
	samplers[ Sampler_Standard ] = NewSampler( SamplerConfig { } );
	samplers[ Sampler_Clamp ] = NewSampler( SamplerConfig { .wrap = SamplerWrap_Clamp } );
	samplers[ Sampler_Unfiltered ] = NewSampler( SamplerConfig { .filter = false } );
	samplers[ Sampler_Shadowmap ] = NewSampler( SamplerConfig { .shadowmap_sampler = true } );
}

static void DeleteSamplers() {
	for( MTL::SamplerState * sampler : samplers ) {
		DeleteSampler( sampler );
	}
}

struct DepthFuncConfig {
	MTL::CompareFunction compare_op;
	bool write_depth = true;
};

static MTL::DepthStencilState * NewDepthFunc( const DepthFuncConfig & config ) {
	MTL::DepthStencilDescriptor * desc = MTL::DepthStencilDescriptor::alloc()->init();
	defer { desc->release(); };

	desc->setDepthCompareFunction( config.compare_op );
	desc->setDepthWriteEnabled( config.write_depth );

	return global_device.device->newDepthStencilState( desc );
}

static void DeleteDepthFunc( MTL::DepthStencilState * depth_func ) {
	depth_func->release();
}

static MTL::DepthStencilState * depth_funcs[ DepthFunc_Count ];

static void CreateDepthFuncs() {
	depth_funcs[ DepthFunc_Less ] = NewDepthFunc( DepthFuncConfig { .compare_op = MTL::CompareFunctionLess } );
	depth_funcs[ DepthFunc_LessNoWrite ] = NewDepthFunc( DepthFuncConfig {
		.compare_op = MTL::CompareFunctionLess,
		.write_depth = false,
	} );
	depth_funcs[ DepthFunc_Equal ] = NewDepthFunc( DepthFuncConfig { .compare_op = MTL::CompareFunctionEqual } );
	depth_funcs[ DepthFunc_EqualNoWrite ] = NewDepthFunc( DepthFuncConfig {
		.compare_op = MTL::CompareFunctionEqual,
		.write_depth = false,
	} );
	depth_funcs[ DepthFunc_Always ] = NewDepthFunc( DepthFuncConfig { .compare_op = MTL::CompareFunctionAlways } );
	depth_funcs[ DepthFunc_AlwaysNoWrite ] = NewDepthFunc( DepthFuncConfig {
		.compare_op = MTL::CompareFunctionAlways,
		.write_depth = false,
	} );
}

static void DeleteDepthFuncs() {
	for( MTL::DepthStencilState * depth_func : depth_funcs ) {
		DeleteDepthFunc( depth_func );
	}
}

static MTL::PixelFormat TextureFormatToMetal( TextureFormat format ) {
	switch( format ) {
		case TextureFormat_R_U8: return MTL::PixelFormatR8Unorm;
		case TextureFormat_R_U8_sRGB: return MTL::PixelFormatR8Unorm_sRGB;
		case TextureFormat_R_S8: return MTL::PixelFormatR8Snorm;
		case TextureFormat_R_UI8: return MTL::PixelFormatR8Uint;

		case TextureFormat_A_U8: return MTL::PixelFormatA8Unorm;

		case TextureFormat_RA_U8: return MTL::PixelFormatRG8Unorm;

		case TextureFormat_RGBA_U8: return MTL::PixelFormatRGBA8Unorm;
		case TextureFormat_RGBA_U8_sRGB: return MTL::PixelFormatRGBA8Unorm_sRGB;

		case TextureFormat_BC1_sRGB: return MTL::PixelFormatBC1_RGBA_sRGB;
		case TextureFormat_BC3_sRGB: return MTL::PixelFormatBC3_RGBA_sRGB;
		case TextureFormat_BC4: return MTL::PixelFormatBC4_RUnorm;
		case TextureFormat_BC5: return MTL::PixelFormatBC5_RGUnorm;

		case TextureFormat_Depth: return MTL::PixelFormatDepth32Float;

		case TextureFormat_Swapchain: return global_swapchain->pixelFormat();
	}
}

PoolHandle< Texture > NewTexture( GPUAllocator * a, const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture ) {
	MTL::TextureType type;
	if( config.num_layers == 1 ) {
		type = config.msaa_samples > 1 ? MTL::TextureType2DMultisample : MTL::TextureType2D;
	}
	else {
		type = config.msaa_samples > 1 ? MTL::TextureType2DMultisampleArray : MTL::TextureType2DArray;
	}

	MTL::TextureSwizzleChannels swizzle = { MTL::TextureSwizzleRed, MTL::TextureSwizzleGreen, MTL::TextureSwizzleBlue, MTL::TextureSwizzleAlpha };
	switch( config.format ) {
		default: break;

		case TextureFormat_R_U8:
		case TextureFormat_R_U8_sRGB:
		case TextureFormat_R_S8:
		case TextureFormat_R_UI8:
			 swizzle = { MTL::TextureSwizzleRed, MTL::TextureSwizzleRed, MTL::TextureSwizzleRed, MTL::TextureSwizzleOne };
			 break;

		case TextureFormat_A_U8:
		case TextureFormat_BC4:
			 swizzle = { MTL::TextureSwizzleOne, MTL::TextureSwizzleOne, MTL::TextureSwizzleOne, MTL::TextureSwizzleRed };
			 break;

		case TextureFormat_RA_U8:
			 swizzle = { MTL::TextureSwizzleRed, MTL::TextureSwizzleRed, MTL::TextureSwizzleRed, MTL::TextureSwizzleGreen };
			 break;
	}

	MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
	if( config.data == NULL ) {
		usage |= MTL::TextureUsageRenderTarget;
	}

	MTL::TextureDescriptor * descriptor = MTL::TextureDescriptor::alloc()->init();
	descriptor->setTextureType( type );
	descriptor->setPixelFormat( TextureFormatToMetal( config.format ) );
	descriptor->setWidth( config.width );
	descriptor->setHeight( config.height );
	descriptor->setArrayLength( config.num_layers );
	descriptor->setMipmapLevelCount( config.num_mipmaps );
	descriptor->setSwizzle( swizzle );
	descriptor->setStorageMode( MTL::StorageModePrivate );
	descriptor->setUsage( usage );
	defer { descriptor->release(); };

	if( old_texture.exists ) {
		textures[ old_texture.value ].texture->release();
	}

	MTL::Texture * texture;
	if( a != NULL ) {
		MTL::SizeAndAlign memory_requirements = global_device.device->heapTextureSizeAndAlign( descriptor );
		GPUBuffer alloc = NewBuffer( a, "texture", memory_requirements.size, memory_requirements.align, true );

		texture = allocations[ alloc.allocation ].heap->newTexture( descriptor, alloc.offset );
	}
	else {
		texture = global_device.device->newTexture( descriptor );
	}

	texture->setLabel( AutoReleaseString( config.name ) );

	PoolHandle< Texture > handle = textures.upsert( old_texture, Texture {
		.texture = texture,
	} );

	if( config.data != NULL ) {
		UploadTexture( handle, config.data );
	}

	return handle;
}

PoolHandle< BindGroup > NewMaterialBindGroup( Span< const char > name, PoolHandle< Texture > texture, SamplerType sampler, GPUBuffer properties ) {
	MTL::ArgumentEncoder * encoder = global_device.material_argument_encoder;

	// ArgumentEncoders can't write into private memory so we have to encode
	// into a coherent buffer and upload that, i.e. run it through the staging
	// buffer
	GPUBuffer args = NewBuffer( GPULifetime_Persistent, "material argument buffer", encoder->encodedLength(), encoder->alignment(), false );
	GPUBuffer staging = StageArgumentBuffer( args, encoder->encodedLength(), encoder->alignment() );

	encoder->setArgumentBuffer( allocations[ staging.allocation ].buffer, staging.offset );
	encoder->setBuffer( allocations[ properties.allocation ].buffer, properties.offset, 0 );
	encoder->setTexture( textures[ texture ].texture, 1 );
	encoder->setSamplerState( samplers[ sampler ], 2 );

	return bind_groups.allocate( { args } );
}

static MTL::VertexFormat VertexFormatToMetal( VertexFormat format ) {
	switch( format ) {
		case VertexFormat_U8x2: return MTL::VertexFormatUChar2;
		case VertexFormat_U8x2_01: return MTL::VertexFormatUChar2Normalized;
		case VertexFormat_U8x3: return MTL::VertexFormatUChar3;
		case VertexFormat_U8x3_01: return MTL::VertexFormatUChar3Normalized;
		case VertexFormat_U8x4: return MTL::VertexFormatUChar4;
		case VertexFormat_U8x4_01: return MTL::VertexFormatUChar4Normalized;

		case VertexFormat_U16x2: return MTL::VertexFormatUChar2;
		case VertexFormat_U16x2_01: return MTL::VertexFormatUChar2Normalized;
		case VertexFormat_U16x3: return MTL::VertexFormatUChar3;
		case VertexFormat_U16x3_01: return MTL::VertexFormatUChar3Normalized;
		case VertexFormat_U16x4: return MTL::VertexFormatUChar4;
		case VertexFormat_U16x4_01: return MTL::VertexFormatUChar4Normalized;

		case VertexFormat_U10x3_U2x1_01: return MTL::VertexFormatUInt1010102Normalized;

		case VertexFormat_Floatx2: return MTL::VertexFormatFloat2;
		case VertexFormat_Floatx3: return MTL::VertexFormatFloat3;
		case VertexFormat_Floatx4: return MTL::VertexFormatFloat4;
	}
}

static Optional< ArgumentBufferEncoder > ParseArgumentBuffer( MTL::Function * func, const NS::Array * args, DescriptorSetType descriptor_set ) {
	constexpr const char * descriptor_set_prefix = "spvDescriptorSet";

	for( size_t i = 0; i < args->count(); i++ ) {
		const MTL::Argument * arg = args->object< const MTL::Argument >( i );
		const char * name = arg->name()->utf8String();

		if( arg->type() != MTL::ArgumentTypeBuffer || !StartsWith( name, descriptor_set_prefix ) )
			continue;

		if( DescriptorSetType( arg->index() ) != descriptor_set )
			continue;

		ArgumentBufferEncoder buffer = {
			.encoder = func->newArgumentEncoder( arg->index() ),
		};

		const NS::Array * members = arg->bufferStructType()->members();
		for( size_t i = 0; i < members->count(); i++ ) {
			const MTL::StructMember * member = members->object< const MTL::StructMember >( i );
			switch( member->dataType() ) {
				case MTL::DataTypePointer:
					buffer.buffers.must_add( StringHash( member->name()->utf8String() ), member->argumentIndex() );
					break;
				case MTL::DataTypeTexture:
					buffer.textures.must_add( StringHash( member->name()->utf8String() ), member->argumentIndex() );
					break;
				case MTL::DataTypeSampler:
					buffer.samplers.must_add( StringHash( member->name()->utf8String() ), member->argumentIndex() );
					break;
				default: break;
			}
		}

		return buffer;
	}

	return NONE;
}

PoolHandle< RenderPipeline > NewRenderPipeline( const RenderPipelineConfig & config ) {
	TempAllocator temp = cls.frame_arena.temp();

	const char * fullpath = temp( "{}.metallib", config.path );

	MTL::Library * library;
	{
		NS::Error * error = NULL;
		library = global_device.device->newLibrary( AutoReleaseString( fullpath ), &error );
		if( library == NULL ) {
			printf( "NewRenderPipeline: %s\n", error->localizedDescription()->utf8String() );
			abort();
		}
	}

	MTL::Function * vertex_main = library->newFunction( AutoReleaseString( "vertex_main" ) );
	MTL::Function * fragment_main = library->newFunction( AutoReleaseString( "fragment_main" ) );

	if( vertex_main == NULL || fragment_main == NULL ) {
		printf( "missing mains\n" );
		abort();
	}

	RenderPipeline shader = { };
	shader.name = config.path;
	shader.clamp_depth = config.clamp_depth;

	for( size_t i = 0; i < config.mesh_variants.n; i++ ) {
		const VertexDescriptor & mesh_variant = config.mesh_variants[ i ];

		RenderPipeline::Variant * variant = shader.mesh_variants.add( mesh_variant );
		Assert( variant != NULL );

		for( u32 msaa = 1; msaa <= global_device.max_msaa; msaa *= 2 ) {
			MTL::RenderPipelineDescriptor * pipeline = MTL::RenderPipelineDescriptor::alloc()->init();
			defer { pipeline->release(); };
			pipeline->setLabel( AutoReleaseString( temp( "{}", shader.name ) ) );
			pipeline->setVertexFunction( vertex_main );
			pipeline->setFragmentFunction( fragment_main );

			// Metal needs this for rendering to multi-layer textures, i.e. shadow
			// cascades, but we only draw triangles so just hardcode it
			// https://developer.apple.com/documentation/metal/render_passes/rendering_to_multiple_texture_slices_in_a_draw_command#3362353
			pipeline->setInputPrimitiveTopology( MTL::PrimitiveTopologyClassTriangle );

			for( size_t color_output_index = 0; color_output_index < ARRAY_COUNT( config.output_format.colors ); color_output_index++ ) {
				if( !config.output_format.colors[ color_output_index ].exists )
					continue;
				MTL::RenderPipelineColorAttachmentDescriptor * attachment = pipeline->colorAttachments()->object( color_output_index );
				attachment->setPixelFormat( TextureFormatToMetal( config.output_format.colors[ color_output_index ].value ) );

				if( config.blend_func == BlendFunc_Blend ) {
					attachment->setBlendingEnabled( true );
					attachment->setSourceRGBBlendFactor( MTL::BlendFactorSourceAlpha );
					attachment->setSourceAlphaBlendFactor( MTL::BlendFactorSourceAlpha );
					attachment->setDestinationRGBBlendFactor( MTL::BlendFactorOneMinusSourceAlpha );
					attachment->setDestinationAlphaBlendFactor( MTL::BlendFactorOneMinusSourceAlpha );
					attachment->setRgbBlendOperation( MTL::BlendOperationAdd );
					attachment->setAlphaBlendOperation( MTL::BlendOperationAdd );
				}
				else if( config.blend_func == BlendFunc_Add ) {
					attachment->setBlendingEnabled( true );
					attachment->setSourceRGBBlendFactor( MTL::BlendFactorSourceAlpha );
					attachment->setSourceAlphaBlendFactor( MTL::BlendFactorOne );
					attachment->setDestinationRGBBlendFactor( MTL::BlendFactorOne );
					attachment->setDestinationAlphaBlendFactor( MTL::BlendFactorOne );
					attachment->setRgbBlendOperation( MTL::BlendOperationAdd );
					attachment->setAlphaBlendOperation( MTL::BlendOperationAdd );
				}
			}

			if( config.output_format.has_depth ) {
				pipeline->setDepthAttachmentPixelFormat( MTL::PixelFormat::PixelFormatDepth32Float );
			}

			pipeline->setAlphaToCoverageEnabled( config.alpha_to_coverage );

			MTL::VertexDescriptor * vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
			defer { vertex_descriptor->release(); };

			for( size_t j = 0; j < ARRAY_COUNT( mesh_variant.attributes ); j++ ) {
				if( !mesh_variant.attributes[ j ].exists )
					continue;

				const VertexAttribute & attr = mesh_variant.attributes[ j ].value;

				MTL::VertexAttributeDescriptor * metal = vertex_descriptor->attributes()->object( j );
				metal->setFormat( VertexFormatToMetal( attr.format ) );
				metal->setOffset( attr.offset );
				metal->setBufferIndex( attr.buffer + RenderPipeline::FirstVertexAttributeIndex );
			}

			for( size_t j = 0; j < ARRAY_COUNT( mesh_variant.buffer_strides ); j++ ) {
				if( mesh_variant.buffer_strides[ j ].exists ) {
					vertex_descriptor->layouts()->object( j + RenderPipeline::FirstVertexAttributeIndex )->setStride( mesh_variant.buffer_strides[ j ].value );
				}
			}

			pipeline->setVertexDescriptor( vertex_descriptor );

			pipeline->setRasterSampleCount( msaa );

			NS::Error * error = NULL;
			MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
			MTL::RenderPipelineState * pso = global_device.device->newRenderPipelineState( pipeline, MTL::PipelineOptionBufferTypeInfo, &reflection, &error );
			if( pso == NULL ) {
				printf( "NewRenderPipeline: %s\n", error->localizedDescription()->utf8String() );
				abort();
			}

			if( shader.mesh_variants.size() == 1 && variant->msaa_variants.size() == 0 ) {
				const NS::Array * vertex_args = reflection->vertexArguments();
				const NS::Array * fragment_args = reflection->fragmentArguments();

				Optional< ArgumentBufferEncoder > render_pass_args = ParseArgumentBuffer( vertex_main, vertex_args, DescriptorSet_RenderPass );
				if( !render_pass_args.exists ) {
					render_pass_args = ParseArgumentBuffer( fragment_main, fragment_args, DescriptorSet_RenderPass );
				}
				if( render_pass_args.exists ) {
					shader.render_pass_args = render_pass_args.value;
				}

				Optional< ArgumentBufferEncoder > draw_call_args = ParseArgumentBuffer( vertex_main, vertex_args, DescriptorSet_DrawCall );
				if( !draw_call_args.exists ) {
					draw_call_args = ParseArgumentBuffer( fragment_main, fragment_args, DescriptorSet_DrawCall );
				}
				if( draw_call_args.exists ) {
					shader.draw_call_args = draw_call_args.value;
				}
			}

			variant->msaa_variants.must_add( pso );
		}
	}

	vertex_main->release();
	fragment_main->release();
	library->release();

	return render_pipelines.allocate( shader );
}

static const MTL::RenderPipelineState * SelectRenderPipelineVariant( const RenderPipeline & shader, const VertexDescriptor & mesh_format, u32 msaa ) {
	const RenderPipeline::Variant * mesh_variant = shader.mesh_variants.get( mesh_format );
	return mesh_variant == NULL ? NULL : mesh_variant->msaa_variants[ Log2( msaa ) ];
}

static void DeleteRenderPipeline( const RenderPipeline & shader ) {
	for( const auto & [ _, mesh_variant ] : shader.mesh_variants ) {
		for( MTL::RenderPipelineState * pso : mesh_variant.msaa_variants ) {
			pso->release();
		}
	}
}

static void BindMesh( MTL::RenderCommandEncoder * encoder, const Mesh & mesh ) {
	for( size_t i = 0; i < ARRAY_COUNT( mesh.vertex_buffers ); i++ ) {
		size_t idx = i + RenderPipeline::FirstVertexAttributeIndex;
		if( mesh.vertex_buffers[ i ].exists ) {
			encoder->setVertexBuffer( allocations[ mesh.vertex_buffers[ i ].value.allocation ].buffer, mesh.vertex_buffers[ i ].value.offset, idx );
		}
		else {
			encoder->setVertexBuffer( NULL, 0, idx );
		}
	}
}

static MTL::CullMode CullFaceToMetal( CullFace cull ) {
	switch( cull ) {
		case CullFace_Back: return MTL::CullModeBack;
		case CullFace_Front: return MTL::CullModeFront;
		case CullFace_Disabled: return MTL::CullModeNone;
	}
}

static void BindBindGroup( MTL::RenderCommandEncoder * encoder, PoolHandle< BindGroup > handle, DescriptorSetType type ) {
	const BindGroup & bind_group = bind_groups[ handle ];
	encoder->setVertexBuffer( allocations[ bind_group.argument_buffer.allocation ].buffer, bind_group.argument_buffer.offset, type );
	encoder->setFragmentBuffer( allocations[ bind_group.argument_buffer.allocation ].buffer, bind_group.argument_buffer.offset, type );
}

static GPUBuffer EncodeArgumentBuffer( ArgumentBufferEncoder * encoder, Span< const BufferBinding > buffers ) {
	GPUBuffer args = NewTempBuffer( encoder->encoder->encodedLength(), encoder->encoder->alignment() ).buffer;

	encoder->encoder->setArgumentBuffer( allocations[ args.allocation ].buffer, args.offset );

	for( size_t i = 0; i < buffers.n; i++ ) {
		const BufferBinding & b = buffers[ i ];
		Optional< u32 > idx = encoder->buffers.get2( b.name );
		if( !idx.exists ) {
			printf( "can't bind\n" );
			continue;
		}
		encoder->encoder->setBuffer( allocations[ b.buffer.allocation ].buffer, b.buffer.offset, idx.value );
	}

	return args;
}

static void BindArgumentBuffer( MTL::RenderCommandEncoder * rce, GPUBuffer args, DescriptorSetType descriptor_set ) {
	rce->setVertexBuffer( allocations[ args.allocation ].buffer, args.offset, descriptor_set );
	rce->setFragmentBuffer( allocations[ args.allocation ].buffer, args.offset, descriptor_set );
}

static void BindArgumentBuffer( MTL::ComputeCommandEncoder * cce, GPUBuffer args, DescriptorSetType descriptor_set ) {
	cce->setBuffer( allocations[ args.allocation ].buffer, args.offset, descriptor_set );
}

template< typename Encoder >
void EncodeAndBindArgumentBuffer( Encoder * ce, ArgumentBufferEncoder * encoder, const GPUBindings & bindings, DescriptorSetType descriptor_set ) {
	GPUBuffer args = EncodeArgumentBuffer( encoder, bindings.buffers );

	for( size_t i = 0; i < bindings.textures.n; i++ ) {
		const GPUBindings::TextureBinding & b = bindings.textures[ i ];

		Optional< u32 > idx = encoder->textures.get2( b.name );
		if( !idx.exists ) {
			printf( "can't bind\n" );
			continue;
		}
		encoder->encoder->setTexture( textures[ b.texture ].texture, idx.value );
	}

	for( size_t i = 0; i < bindings.samplers.n; i++ ) {
		const GPUBindings::SamplerBinding & b = bindings.samplers[ i ];
		Optional< u32 > idx = encoder->samplers.get2( b.name );
		if( !idx.exists ) {
			printf( "can't bind\n" );
			continue;
		}
		encoder->encoder->setSamplerState( samplers[ b.sampler ], idx.value );
	}

	BindArgumentBuffer( ce, args, descriptor_set );
}

template< typename Encoder >
void EncodeAndBindArgumentBuffer( Encoder * ce, ArgumentBufferEncoder * encoder, Span< const BufferBinding > buffers, DescriptorSetType descriptor_set ) {
	GPUBuffer args = EncodeArgumentBuffer( encoder, buffers );
	BindArgumentBuffer( ce, args, descriptor_set );
}

void EncodeDrawCall( Opaque< CommandBuffer > ocb, const PipelineState & pipeline, Mesh mesh, Span< const BufferBinding > buffers, DrawCallExtras extras ) {
	const MTL::RenderPipelineState * pso = SelectRenderPipelineVariant( render_pipelines[ pipeline.shader ], mesh.vertex_descriptor, 1 );
	if( pso == NULL ) {
		printf( "no shader variant!\n" );
		return;
	}

	CommandBuffer * cb = ocb.unwrap();

	cb->rce->setRenderPipelineState( pso );
	cb->rce->setDepthClipMode( render_pipelines[ pipeline.shader ].clamp_depth ? MTL::DepthClipModeClamp : MTL::DepthClipModeClip );
	cb->rce->setFrontFacingWinding( MTL::WindingCounterClockwise );
	cb->rce->setCullMode( CullFaceToMetal( pipeline.dynamic_state.cull_face ) );
	cb->rce->setDepthStencilState( depth_funcs[ pipeline.dynamic_state.depth_func ] );

	if( pipeline.scissor.exists ) {
		PipelineState::Scissor s = pipeline.scissor.value;
		cb->rce->setScissorRect( MTL::ScissorRect { s.x, s.y, s.w, s.y } );
	}
	else {
		cb->rce->setScissorRect( { 0, 0, NS::UIntegerMax, NS::UIntegerMax } );
	}

	BindMesh( cb->rce, mesh );
	BindBindGroup( cb->rce, pipeline.material_bind_group, DescriptorSet_Material );

	EncodeAndBindArgumentBuffer( cb->rce, &render_pipelines[ pipeline.shader ].draw_call_args, buffers, DescriptorSet_DrawCall );

	MTL::IndexType index_type = mesh.index_format == IndexFormat_U16 ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
	size_t index_size = mesh.index_format == IndexFormat_U16 ? sizeof( u16 ) : sizeof( u32 );
	// if( !draw_call.indirect_args.exists ) {
		if( mesh.index_buffer.exists ) {
			Assert( mesh.index_buffer.value.offset % 4 == 0 );
			cb->rce->drawIndexedPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, Default( extras.override_num_vertices, mesh.num_vertices ), index_type, allocations[ mesh.index_buffer.value.allocation ].buffer, mesh.index_buffer.value.offset + extras.first_index * index_size, 1, extras.base_vertex, 0 );
		}
		else {
			cb->rce->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, mesh.num_vertices, NS::UInteger( 0 ) );
		}
	// }
	// else {
	// 	const MTL::Buffer * buffer = allocations[ draw_call.indirect_args.value.allocation ].buffer;
	// 	if( draw_call.mesh.index_buffer.exists ) {
	// 		Assert( draw_call.mesh.index_buffer.value.offset % 4 == 0 );
	// 		cb->rce->drawIndexedPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, index_type, allocations[ draw_call.mesh.index_buffer.value.allocation ].buffer, draw_call.mesh.index_buffer.value.offset + draw_call.first_index * index_size, buffer, draw_call.indirect_args.value.offset );
	// 	}
	// 	else {
	// 		cb->rce->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, buffer, draw_call.indirect_args.value.offset );
	// 	}
	// }
}

PoolHandle< ComputePipeline > NewComputePipeline( Span< const char > path ) {
	TempAllocator temp = cls.frame_arena.temp();

	const char * fullpath = temp( "{}.metallib", path );

	MTL::Library * library;
	{
		NS::Error * error = NULL;
		library = global_device.device->newLibrary( AutoReleaseString( fullpath ), &error );
		if( library == NULL ) {
			printf( "NewComputePipeline: %s\n", error->localizedDescription()->utf8String() );
			abort();
		}
	}
	defer { library->release(); };

	MTL::Function * compute_main = library->newFunction( AutoReleaseString( "main0" ) );
	if( compute_main == NULL ) {
		printf( "missing main\n" );
		abort();
	}
	defer { compute_main->release(); };

	MTL::ComputePipelineDescriptor * pipeline = MTL::ComputePipelineDescriptor::alloc()->init();
	pipeline->setLabel( AutoReleaseString( temp( "{}", path ) ) );
	pipeline->setComputeFunction( compute_main );

	NS::Error * error = NULL;
	MTL::AutoreleasedComputePipelineReflection reflection = NULL;
	MTL::ComputePipelineState * pso = global_device.device->newComputePipelineState( pipeline, MTL::PipelineOptionBufferTypeInfo, &reflection, &error );
	if( pso == NULL ) {
		printf( "NewComputePipeline: %s\n", error->localizedDescription()->utf8String() );
		abort();
	}

	Optional< ArgumentBufferEncoder > args = ParseArgumentBuffer( compute_main, reflection->arguments(), DescriptorSetType( 0 ) );
	Assert( args.exists );

	return compute_pipelines.allocate( ComputePipeline {
		.name = path,
		.pso = pso,
		.args = args.value,
	} );
}

static CommandBuffer * PrepareCompute( Opaque< CommandBuffer > ocb, PoolHandle< ComputePipeline > shader, Span< const BufferBinding > buffers ) {
	CommandBuffer * cb = ocb.unwrap();
	cb->cce->setComputePipelineState( compute_pipelines[ shader ].pso );
	GPUBindings bindings = { .buffers = buffers };
	EncodeAndBindArgumentBuffer( cb->cce, &compute_pipelines[ shader ].args, bindings, DescriptorSetType( 0 ) );
	return cb;
}

static MTL::Size SubgroupSize( PoolHandle< ComputePipeline > shader ) {
	// https://developer.apple.com/documentation/metal/compute_passes/calculating_threadgroup_and_grid_sizes
	const MTL::ComputePipelineState * pso = compute_pipelines[ shader ].pso;
	MTL::Size size;
	size.width = pso->threadExecutionWidth();
	size.height = pso->maxTotalThreadsPerThreadgroup() / size.width;
	size.depth = 1;
	return size;
}

void EncodeComputeCall( Opaque< CommandBuffer > ocb, PoolHandle< ComputePipeline > shader, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers ) {
	CommandBuffer * cb = PrepareCompute( ocb, shader, buffers );
	SubgroupSize( shader );
	cb->cce->dispatchThreadgroups( MTL::Size::Make( x, y, z ), SubgroupSize( shader ) );
}

void EncodeIndirectComputeCall( Opaque< CommandBuffer > ocb, PoolHandle< ComputePipeline > shader, GPUBuffer indirect_args, Span< const BufferBinding > buffers ) {
	CommandBuffer * cb = PrepareCompute( ocb, shader, buffers );
	cb->cce->dispatchThreadgroups( allocations[ indirect_args.allocation ].buffer, indirect_args.offset, SubgroupSize( shader ) );
}

void InitRenderBackend( GLFWwindow * window ) {
	frame_semaphore = dispatch_semaphore_create( MaxFramesInFlight );
	frame_counter = 0;
	pass_counter = 0;

	MTL::Device * device = MTL::CreateSystemDefaultDevice();
	MTL::CommandQueue * command_queue = device->newCommandQueue();
	MTL::Event * event = device->newEvent();

	// see some combination of
	// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
	// https://developer.apple.com/documentation/metal/mtlcomputecommandencoder/2928169-setbuffers
	constexpr size_t constant_buffer_alignment = 4;
	constexpr size_t metal_doesnt_have_buffer_image_granularity = 0; // or rather it is baked into heap texture alignment requirements

	global_device = MetalDevice {
		.device = device,
		.command_queue = command_queue,
		.event = event,
		.pass_counter = 1,
		.argument_buffers_tier = device->argumentBuffersSupport(),
	};

	for( u32 i = 1; i <= MaxMSAA; i *= 2 ) {
		if( device->supportsTextureSampleCount( i ) ) {
			global_device.max_msaa = i;
		}
	}

	// these rely on global_device.device so have to be down here
	global_device.persistent_allocator = NewGPUAllocator( Megabytes( 128 ), constant_buffer_alignment, metal_doesnt_have_buffer_image_granularity ),
	global_device.framebuffer_allocator = NewGPUAllocator( Megabytes( 32 ), constant_buffer_alignment, metal_doesnt_have_buffer_image_granularity ),
	global_device.temp_allocator = NewGPUTempAllocator( Megabytes( 32 ), constant_buffer_alignment ),

	InitStagingBuffer();

	global_swapchain = CA::MetalLayer::layer();
	global_swapchain->setDevice( device );
	global_swapchain->setPixelFormat( MTL::PixelFormatBGRA8Unorm );
	global_swapchain->setFramebufferOnly( true );

	swapchain_texture = textures.allocate();

	shim( window, global_swapchain );

	global_window = window;
	glfwGetFramebufferSize( global_window, &old_framebuffer_width, &old_framebuffer_height );

	CreateSamplers();
	CreateDepthFuncs();

	{
		MTL::ArgumentDescriptor * texture_arg = MTL::ArgumentDescriptor::alloc()->init();
		texture_arg->setDataType( MTL::DataTypeTexture );
		texture_arg->setIndex( 0 );
		texture_arg->setTextureType( MTL::TextureType2D );
		defer { texture_arg->release(); };

		MTL::ArgumentDescriptor * sampler_arg = MTL::ArgumentDescriptor::alloc()->init();
		sampler_arg->setDataType( MTL::DataTypeSampler );
		sampler_arg->setIndex( 1 );
		defer { sampler_arg->release(); };

		MTL::ArgumentDescriptor * buffer_arg = MTL::ArgumentDescriptor::alloc()->init();
		buffer_arg->setDataType( MTL::DataTypePointer );
		buffer_arg->setIndex( 2 );
		defer { buffer_arg->release(); };

		NS::Object * args_c[] = { texture_arg, sampler_arg, buffer_arg };
		NS::Array * args = NS::Array::alloc()->init( args_c, ARRAY_COUNT( args_c ) );
		defer { args->release(); };

		global_device.material_argument_encoder = global_device.device->newArgumentEncoder( args );
	}
}

void ShutdownRenderBackend() {
	for( u64 i = 0; i < Min2( frame_counter, u64( MaxFramesInFlight ) ); i++ ) {
		dispatch_semaphore_wait( frame_semaphore, DISPATCH_TIME_FOREVER );
	}

	global_device.material_argument_encoder->release();

	for( GPUAllocation allocation : allocations ) {
		if( allocation.heap != NULL ) {
			allocation.heap->release();
		}
		allocation.buffer->release();
	}

	for( Texture texture : textures ) {
		if( !texture.is_swapchain ) {
			texture.texture->release();
		}
	}

	for( RenderPipeline shader : render_pipelines ) {
		DeleteRenderPipeline( shader );
	}

	for( ComputePipeline shader : compute_pipelines ) {
		shader.pso->release();
	}

	DeleteSamplers();
	DeleteDepthFuncs();
	ShutdownStagingBuffer();
	DeleteGPUAllocator( &global_device.persistent_allocator );
	DeleteGPUAllocator( &global_device.framebuffer_allocator );
	global_device.command_queue->release();
	global_device.device->release();
}

void RenderBackendWaitForNewFrame() {
	dispatch_semaphore_wait( frame_semaphore, DISPATCH_TIME_FOREVER );
}

PoolHandle< Texture > RenderBackendBeginFrame( bool capture ) {
	NS::AutoreleasePool * pool = NS::AutoreleasePool::alloc()->init();

	ClearGPUTempAllocator( &global_device.temp_allocator );

	int framebuffer_width, framebuffer_height;
	glfwGetFramebufferSize( global_window, &framebuffer_width, &framebuffer_height );
	bool resized = framebuffer_width != old_framebuffer_width || framebuffer_height != old_framebuffer_height;
	bool nonzero = framebuffer_width > 0 && framebuffer_height > 0;

	if( resized && nonzero ) {
		global_swapchain->setDrawableSize( CGSize { CGFloat( framebuffer_width ), CGFloat( framebuffer_height ) } );
		old_framebuffer_width = framebuffer_width;
		old_framebuffer_height = framebuffer_height;
	}

	CA::MetalDrawable * surface = global_swapchain->nextDrawable();
	textures.upsert( swapchain_texture, {
		.texture = surface->texture(),
		.is_swapchain = true,
	} );

	frame = {
		.pool = pool,
		.swapchain_surface = surface,
	};

	if( capture ) {
		StartCapture( global_device.device );
	}

	return swapchain_texture;
}

void RenderBackendEndFrame() {
	if( frame.capture ) {
		EndCapture();
	}
	frame.pool->release();
	frame_counter++;
}

size_t FrameSlot() {
	return frame_counter % MaxFramesInFlight;
}

template< typename T >
static void UseGPUAllocator( T * encoder, GPUAllocator * a ) {
	const GPUAllocator::Slab * slab = a->slabs;
	while( slab != NULL ) {
		if( slab->capacity > 0 ) {
			encoder->useHeap( allocations[ slab->buffer ].heap );
		}
		slab = slab->next;
	}
}

template< typename T >
static void UseAllocators( T * encoder ) {
	UseGPUAllocator( encoder, &global_device.persistent_allocator );
	UseGPUAllocator( encoder, &global_device.framebuffer_allocator );
	encoder->useResource( allocations[ global_device.temp_allocator.memory.allocation ].buffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite );
}

Opaque< CommandBuffer > NewRenderPass( const RenderPassConfig & config ) {
	MTL::RenderPassDescriptor * render_pass = MTL::RenderPassDescriptor::alloc()->init();

	MTL::CommandBuffer * command_buffer = global_device.command_queue->commandBufferWithUnretainedReferences();
	command_buffer->setLabel( AutoReleaseString( config.name ) );

	if( global_device.pass_counter > 1 ) {
		command_buffer->encodeWait( global_device.event, global_device.pass_counter );
	}

	for( size_t i = 0; i < config.color_targets.n; i++ ) {
		const RenderPassConfig::ColorTarget & attachment_config = config.color_targets[ i ];
		MTL::RenderPassColorAttachmentDescriptor * attachment = render_pass->colorAttachments()->object( i );

		if( attachment_config.clear.exists ) {
			Assert( !attachment_config.preserve_contents );
			attachment->setLoadAction( MTL::LoadActionClear );
			Vec4 color = attachment_config.clear.value;
			attachment->setClearColor( MTL::ClearColor( color.x, color.y, color.z, color.w ) );
		}
		else {
			attachment->setLoadAction( attachment_config.preserve_contents ? MTL::LoadActionLoad : MTL::LoadActionDontCare );
		}

		attachment->setStoreAction( MTL::StoreAction::StoreActionStore );

		attachment->setTexture( textures[ attachment_config.texture ].texture );
		attachment->setSlice( attachment_config.layer );

		if( attachment_config.resolve_target.exists ) {
			attachment->setStoreAction( MTL::StoreActionDontCare );
			attachment->setResolveTexture( textures[ attachment_config.resolve_target.value ].texture );
		}
	}

	if( config.depth_target.exists ) {
		const RenderPassConfig::DepthTarget & attachment_config = config.depth_target.value;
		MTL::RenderPassDepthAttachmentDescriptor * attachment = render_pass->depthAttachment();

		if( attachment_config.clear.exists ) {
			Assert( !attachment_config.preserve_contents );
			attachment->setLoadAction( MTL::LoadActionClear );
			attachment->setClearDepth( attachment_config.clear.value );
		}
		else {
			attachment->setLoadAction( attachment_config.preserve_contents ? MTL::LoadActionLoad : MTL::LoadActionDontCare );
		}

		attachment->setTexture( textures[ attachment_config.texture ].texture );
		attachment->setSlice( attachment_config.layer );
	}

	MTL::RenderCommandEncoder * encoder = command_buffer->renderCommandEncoder( render_pass );
	UseAllocators( encoder );
	EncodeAndBindArgumentBuffer( encoder, &render_pipelines[ config.representative_shader ].render_pass_args, config.bindings, DescriptorSet_RenderPass );

	render_pass->release();

	return CommandBuffer {
		.command_buffer = command_buffer,
		.rce = encoder,
		.signal = ++global_device.pass_counter,
	};
}

Opaque< CommandBuffer > NewComputePass( const ComputePassConfig & config ) {
	MTL::ComputePassDescriptor * compute_pass = MTL::ComputePassDescriptor::alloc()->init();
	compute_pass->setDispatchType( MTL::DispatchTypeSerial );

	MTL::CommandBuffer * command_buffer = global_device.command_queue->commandBufferWithUnretainedReferences();
	command_buffer->setLabel( AutoReleaseString( config.name ) );

	if( global_device.pass_counter > 1 ) {
		command_buffer->encodeWait( global_device.event, global_device.pass_counter );
	}

	MTL::ComputeCommandEncoder * encoder = command_buffer->computeCommandEncoder( compute_pass );
	UseAllocators( encoder );

	compute_pass->release();

	return CommandBuffer {
		.command_buffer = command_buffer,
		.cce = encoder,
		.signal = ++global_device.pass_counter,
	};
}
