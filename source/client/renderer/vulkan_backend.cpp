#include "qcommon/platform.h"

#if !PLATFORM_MACOS

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/arraymap.h"
#include "qcommon/hash.h"
#include "qcommon/hashmap.h"
#include "qcommon/pool.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/api.h"
#include "client/renderer/private.h"
#include "client/platform/renderdoc.h"
#include "gameshared/q_math.h"

#include "SDL3/SDL_vulkan.h"

#define VK_ENABLE_BETA_EXTENSIONS
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/spirv.h"
#include "volk/volk.h"

#if !PUBLIC_BUILD
#define ENABLE_VALIDATION_LAYERS 1
#define ENABLE_SYNC_VALIDATION 1
#endif

static constexpr bool macOS = IFDEF( PLATFORM_MACOS );

static void VkAbort( VkResult res, SourceLocation srcloc = CurrentSourceLocation() ) {
	Fatal( "VK_CHECK failed (%d): %s:%d (%s)\n", res, srcloc.file, srcloc.line, srcloc.function );
}

static void VK_CHECK( VkResult res, SourceLocation srcloc = CurrentSourceLocation() ) {
	if( res != VK_SUCCESS ) {
		VkAbort( res, srcloc );
	}
}

template< typename T, typename F, typename... Rest >
Span< T > VulkanSpan( Allocator * a, SourceLocation srcloc, F f, Rest... rest ) {
	u32 n;
	VK_CHECK( f( rest..., &n, NULL ), srcloc );
	Span< T > span = AllocSpan< T >( a, n );
	VK_CHECK( f( rest..., &n, span.ptr ), srcloc );
	return span;
}

template< typename T, typename F, typename... Rest >
Span< T > VulkanSpanUnchecked( Allocator * a, F f, Rest... rest ) {
	u32 n;
	f( rest..., &n, NULL );
	Span< T > span = AllocSpan< T >( a, n );
	f( rest..., &n, span.ptr );
	return span;
}

struct GPUAllocation {
	VkDeviceMemory memory;
	VkBuffer buffer;
	u64 addr;
};

struct BackendTexture {
	VkImage image;
	VkImageView image_view;
	Span< VkImageView > per_layer_image_views;
	Optional< GPUAllocation > allocation;
};

struct Vertex2 {
	Vec2 position;
	Vec2 uv;
	Vec3 color;
};

struct VulkanDevice {
	VkPhysicalDevice physical_device;
	u32 device_local_memory_type;
	u32 coherent_memory_type;
	VkDevice device;
	VkQueue queue;
	u32 queue_family_index;

	VkSemaphore frame_timeline_semaphore;

	VkDescriptorPool descriptor_pool;

	struct CommandPool {
		VkCommandPool pool;
		NonRAIIDynamicArray< VkCommandBuffer > free_list;
		size_t num_used;
	};
	CommandPool command_pools[ MaxFramesInFlight ];

	VkCommandPool transfer_command_pool;
	VkCommandBuffer transfer_command_buffer;

	VkDescriptorSetLayout material_descriptor_set_layout;

	struct {
		size_t vram;
		size_t max_allocs;
		size_t max_alloc_size;
		size_t buffer_image_granularity;
		size_t ssbo_range;
		size_t ssbo_alignment;
		u32 msaa;
	} limits;
};

struct Swapchain {
	struct Image {
		VkImage image;
		VkImageView image_view;
	};

	struct Semaphores {
		VkSemaphore acquire;
		VkSemaphore present;
	};

	VkSwapchainKHR swapchain;
	u32 width, height;
	VkFormat format;

	Span< Image > images;
	Span< Semaphores > semaphores;
};

struct BindGroup {
	VkDescriptorSet descriptor_set;
};

struct PushDescriptorTemplate {
	VkDescriptorUpdateTemplate update_template;
	ArrayMap< StringHash, u32, MaxBufferBindings > buffers;
	ArrayMap< StringHash, u32, MaxTextureBindings > textures;
	ArrayMap< StringHash, u32, MaxTextureBindings > samplers;
};

struct ReflectedDescriptorSet {
	struct Binding {
		VkDescriptorType type;
		u32 index;
		u32 stride;
	};

	ArrayMap< StringHash, Binding, MaxBindings > bindings;
};

struct RenderPipeline {
	struct Variant {
		VkPipeline msaa_variants[ Log2( MaxMSAA ) + 1 ];
	};

	Span< char > name;
	ArrayMap< VertexDescriptor, Variant, MaxShaderVariants > mesh_variants;
	VkPipelineLayout layout;
	VkDescriptorSetLayout descriptor_set_layouts[ DescriptorSet_Count ];
	ReflectedDescriptorSet reflection[ DescriptorSet_Count ];
	PushDescriptorTemplate render_pass_push_descriptors;
};

struct ComputePipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorSetLayout descriptor_set_layout;
	PushDescriptorTemplate push_descriptors;
};

struct CommandBuffer {
	VkCommandBuffer buffer;
	bool is_render_command_buffer;
	bool wait_on_swapchain_acquire;
	u32 msaa_samples;
};

static VkInstance global_instance;
static VkDebugReportCallbackEXT global_debug_callback;
static VulkanDevice global_device;
static VkSurfaceKHR global_surface;
static Swapchain global_swapchain;
static u64 frame_counter;

static struct {
	u32 image_index;
} frame;

static Pool< GPUAllocation, 1024 > allocations;
inline HashPool< Texture, MaxMaterials > textures;
static Pool< BindGroup, MaxMaterials > bind_groups;
static Pool< RenderPipeline, 128 > render_pipelines;
static Pool< ComputePipeline, 128 > compute_pipelines;
static VkSampler samplers[ Sampler_Count ];

static constexpr VkRect2D no_scissor = { .extent = { S32_MAX, S32_MAX } };
static constexpr size_t MaxPushConstants = 3;

template< typename T >
void DebugLabel( VkDevice device, T handle, VkObjectType type, const char * name ) {
	const VkDebugUtilsObjectNameInfoEXT name_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType = type,
		.objectHandle = checked_cast< u64 >( handle ),
		.pObjectName = name,
	};

	VK_CHECK( vkSetDebugUtilsObjectNameEXT( device, &name_info ) );
}

template< typename T >
void DebugLabel( T handle, VkObjectType type, const char * name ) {
	DebugLabel( global_device.device, handle, type, name );
}

static GPUAllocation GPUMalloc( size_t size, bool device_local ) {
	constexpr VkBufferUsageFlags common_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	constexpr VkBufferUsageFlags device_local_flags = common_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	constexpr VkBufferUsageFlags coherent_flags = common_flags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = device_local ? device_local_flags : coherent_flags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkBuffer buffer;
	VK_CHECK( vkCreateBuffer( global_device.device, &buffer_info, NULL, &buffer ) );

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements( global_device.device, buffer, &memory_requirements );

	const VkMemoryAllocateFlagsInfo alloc_flags = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
	};

	const VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &alloc_flags,
		.allocationSize = memory_requirements.size,
		.memoryTypeIndex = device_local ? global_device.device_local_memory_type : global_device.coherent_memory_type,
	};

	VkDeviceMemory memory;
	VK_CHECK( vkAllocateMemory( global_device.device, &alloc_info, NULL, &memory ) );
	VK_CHECK( vkBindBufferMemory( global_device.device, buffer, memory, 0 ) );

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = buffer,
	};
	VkDeviceAddress addr = vkGetBufferDeviceAddress( global_device.device, &address_info );

	return GPUAllocation {
		.memory = memory,
		.buffer = buffer,
		.addr = addr,
	};
}

static void GPUFree( GPUAllocation allocation ) {
	vkDestroyBuffer( global_device.device, allocation.buffer, NULL );
	vkFreeMemory( global_device.device, allocation.memory, NULL );
}

static void PrintExtensions() {
	TempAllocator temp = cls.frame_arena.temp();
	Span< VkExtensionProperties > extensions = VulkanSpan< VkExtensionProperties >( &temp, CurrentSourceLocation(), vkEnumerateInstanceExtensionProperties, nullptr );
	Span< VkLayerProperties > layers = VulkanSpan< VkLayerProperties >( &temp, CurrentSourceLocation(), vkEnumerateInstanceLayerProperties );

	printf( "Extensions:\n" );
	for( const VkExtensionProperties & extension : extensions ) {
		printf( "    %s\n", extension.extensionName );
	}

	printf( "Layers:\n" );
	for( const VkLayerProperties & layer : layers ) {
		printf( "    %s\n", layer.layerName );
	}
}

static VkInstance createInstance() {
	const VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_3,
	};

	Assert( volkGetInstanceVersion() >= appInfo.apiVersion );

	BoundedDynamicArray< const char *, 2 > layers = { };
	BoundedDynamicArray< VkValidationFeatureEnableEXT, 3 > enabledValidationFeatures = { };

	if( IFDEF( ENABLE_VALIDATION_LAYERS ) && !RunningInRenderDoc() ) {
		layers.must_add( "VK_LAYER_KHRONOS_validation" );
		enabledValidationFeatures.must_add( VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT );
	}
	if( IFDEF( ENABLE_SYNC_VALIDATION ) ) {
		enabledValidationFeatures.must_add( VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT );
		// VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
	}

	const VkValidationFeaturesEXT validationFeatures = {
		.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		.enabledValidationFeatureCount = checked_cast< u32 >( enabledValidationFeatures.size() ),
		.pEnabledValidationFeatures = enabledValidationFeatures.ptr(),
	};

	u32 num_sdl_device_extensions;
	const char * const * sdl_device_extensions = SDL_Vulkan_GetInstanceExtensions( &num_sdl_device_extensions );

	constexpr const char * my_device_extensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#if PLATFORM_WINDOWS
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
#endif
#if PLATFORM_MACOS
		VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
#if ENABLE_VALIDATION_LAYERS
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
	};

	TempAllocator temp = cls.frame_arena.temp();
	Span< const char * > device_extensions = AllocSpan< const char * >( &temp, num_sdl_device_extensions + ARRAY_COUNT( my_device_extensions ) );
	for( u32 i = 0; i < num_sdl_device_extensions; i++ ) {
		device_extensions[ i ] = sdl_device_extensions[ i ];
	}
	for( size_t i = 0; i < ARRAY_COUNT( my_device_extensions ); i++ ) {
		device_extensions[ i + num_sdl_device_extensions ] = my_device_extensions[ i ];
	}

	const VkInstanceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = !RunningInRenderDoc() ? &validationFeatures : NULL,
		.flags = macOS ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0,

		.pApplicationInfo = &appInfo,
		.enabledLayerCount = checked_cast< u32 >( layers.size() ),
		.ppEnabledLayerNames = layers.ptr(),
		.enabledExtensionCount = u32( device_extensions.n ),
		.ppEnabledExtensionNames = device_extensions.ptr,
	};

	VkInstance instance = VK_NULL_HANDLE;
	VK_CHECK( vkCreateInstance( &createInfo, NULL, &instance ) );

	return instance;
}

static VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
	if( location == 0x609a13b ) return VK_FALSE; // UNASSIGNED-CoreValidation-Shader-OutputNotConsumed

	const char * type =
		(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "\033[1;31mERROR" // red
		: (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ? "\033[1;33mWARNING" // yellow
		: (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) ? "\033[1;34mPERF" // blue
		: "\033[1;32mINFO"; // green

	printf( "%s:\033[0m %s\n", type, pMessage );

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		Fatal( pMessage );
	}

	return VK_FALSE;
}

static VkDebugReportCallbackEXT RegisterDebugCallback( VkInstance instance ) {
	if( vkCreateDebugReportCallbackEXT == NULL ) {
		return VK_NULL_HANDLE;
	}

	const VkDebugReportCallbackCreateInfoEXT createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
		.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT,
		.pfnCallback = debugReportCallback,
	};

	VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
	VK_CHECK( vkCreateDebugReportCallbackEXT( instance, &createInfo, NULL, &callback ) );

	return callback;
}

static VkPhysicalDevice PickPhysicalDevice( VkInstance instance ) {
	VkPhysicalDevice discrete_gpu = VK_NULL_HANDLE;
	VkPhysicalDevice integrated_gpu = VK_NULL_HANDLE;
	VkPhysicalDevice very_bad_gpu = VK_NULL_HANDLE;

	printf( "Devices:\n" );

	TempAllocator temp = cls.frame_arena.temp();
	Span< VkPhysicalDevice > devices = VulkanSpan< VkPhysicalDevice >( &temp, CurrentSourceLocation(), vkEnumeratePhysicalDevices, instance );

	for( const VkPhysicalDevice & device : devices ) {
		// TODO: need to do more filtering here

		VkPhysicalDeviceMaintenance3Properties props3 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES };
		VkPhysicalDeviceProperties2 props2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &props3,
		};
		vkGetPhysicalDeviceProperties2( device, &props2 );
		printf( "    %s v%u:\n", props2.properties.deviceName, props2.properties.driverVersion );

		Span< VkExtensionProperties > extensions = VulkanSpan< VkExtensionProperties >( &temp, CurrentSourceLocation(), vkEnumerateDeviceExtensionProperties, device, nullptr );

		for( const VkExtensionProperties & extension : extensions ) {
			printf( "        %s\n", extension.extensionName );
		}

		printf( "        Max allocs: %u\n", props2.properties.limits.maxMemoryAllocationCount ); // 2^30 on MoltenVK, 4096 on nv
		printf( "        Max alloc size: %llu\n", props3.maxMemoryAllocationSize ); // 2^33 on MoltenVK, 2^32 on nv
		printf( "        Max textures: %u\n", props2.properties.limits.maxPerStageDescriptorSampledImages ); // 128 on MoltenVK, 2^20 on nv
		printf( "        Max descriptor set images: %u\n", props2.properties.limits.maxDescriptorSetSampledImages ); // 640 on MoltenVK, 2^20 on nv
		printf( "        UBO alignment: %llu\n", props2.properties.limits.minUniformBufferOffsetAlignment ); // 16 on MoltenVK, 64 on nv
		printf( "        SSBO alignment: %llu\n", props2.properties.limits.minStorageBufferOffsetAlignment ); // 16 on MoltenVK/nv
		printf( "        Optimal buffer copy alignment: %llu\n", props2.properties.limits.optimalBufferCopyOffsetAlignment ); // 16 on MoltenVK, 1 on nv
		printf( "        Optimal buffer row pitch copy alignment: %llu\n", props2.properties.limits.optimalBufferCopyRowPitchAlignment ); // 1 on MoltenVK/nv

		if( discrete_gpu == VK_NULL_HANDLE && props2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			discrete_gpu = device;
		}
		else if( integrated_gpu == VK_NULL_HANDLE && props2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			integrated_gpu = device;
		}
		else if( very_bad_gpu == VK_NULL_HANDLE ) {
			very_bad_gpu = device;
		}
	}

	if( discrete_gpu != VK_NULL_HANDLE ) {
		return discrete_gpu;
	}

	return integrated_gpu != VK_NULL_HANDLE ? integrated_gpu : very_bad_gpu;
}

static VulkanDevice CreateDevice( VkInstance instance ) {
	VulkanDevice device = { };

	{
		device.physical_device = PickPhysicalDevice( instance );
		Assert( device.physical_device != VK_NULL_HANDLE );

		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties( device.physical_device, &memory_properties );

		Optional< u32 > device_local_memory_type = NONE;
		Optional< u32 > coherent_memory_type = NONE;
		for( u32 i = 0; i < memory_properties.memoryTypeCount; i++ ) {
			VkMemoryPropertyFlags flags = memory_properties.memoryTypes[ i ].propertyFlags;
			printf( "memory type %x on heap %u\n", flags, memory_properties.memoryTypes[ i ].heapIndex );

			if( ( flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) != 0 && !device_local_memory_type.exists ) {
				printf( "  device local\n" );
				device_local_memory_type = i;
			}
			if( ( flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) != 0 && ( flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) != 0 && !coherent_memory_type.exists ) {
				printf( "  coherent\n" );
				coherent_memory_type = i;
			}
		}

		Assert( device_local_memory_type.exists && coherent_memory_type.exists );

		device.device_local_memory_type = device_local_memory_type.value;
		device.coherent_memory_type = coherent_memory_type.value;

		for( u32 i = 0; i < memory_properties.memoryHeapCount; i++ ) {
			VkMemoryHeap heap = memory_properties.memoryHeaps[ i ];
			printf( "heap %u size %.2fGB flags %u\n", i, heap.size / 1024.0 / 1024.0 / 1024.0, heap.flags );
			if( heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				printf( "  device local\n" );
			}
		}

		{
			VkPhysicalDevicePortabilitySubsetPropertiesKHR props_portability_subset = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR };
			VkPhysicalDeviceMaintenance3Properties props_maintenance3 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,
				.pNext = &props_portability_subset,
			};
			VkPhysicalDeviceProperties2 props2 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
				.pNext = &props_maintenance3,
			};
			vkGetPhysicalDeviceProperties2( device.physical_device, &props2 );

			device.limits.vram = memory_properties.memoryHeaps[ memory_properties.memoryTypes[ device.device_local_memory_type ].heapIndex ].size;
			device.limits.max_allocs = props2.properties.limits.maxMemoryAllocationCount;
			device.limits.max_alloc_size = props_maintenance3.maxMemoryAllocationSize;
			device.limits.buffer_image_granularity = props2.properties.limits.bufferImageGranularity;
			device.limits.ssbo_range = props2.properties.limits.maxStorageBufferRange;
			device.limits.ssbo_alignment = props2.properties.limits.minStorageBufferOffsetAlignment;

			u32 available_msaa = props2.properties.limits.framebufferColorSampleCounts & props2.properties.limits.framebufferDepthSampleCounts;
			for( u32 i = 1; i <= MaxMSAA; i++ ) {
				if( available_msaa & i ) {
					device.limits.msaa |= i;
				}
			}

			Assert( IsPowerOf2( device.limits.buffer_image_granularity ) );
			Assert( IsPowerOf2( device.limits.ssbo_alignment ) );

			printf( "minVertexInputBindingStrideAlignment = %u\n", props_portability_subset.minVertexInputBindingStrideAlignment );
		}

		device.queue_family_index = VK_QUEUE_FAMILY_IGNORED;

		TempAllocator temp = cls.frame_arena.temp();
		Span< VkQueueFamilyProperties > queues = VulkanSpanUnchecked< VkQueueFamilyProperties >( &temp, vkGetPhysicalDeviceQueueFamilyProperties, device.physical_device );

		for( size_t i = 0; i < queues.n; i++ ) {
			// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkQueueFlagBits.html
			// "All commands that are allowed on a queue that supports transfer operations are also
			// allowed on a queue that supports either graphics or compute operations. Thus, if the
			// capabilities of a queue family include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT,
			// then reporting the VK_QUEUE_TRANSFER_BIT capability separately for that queue family
			// is optional."

			bool graphics = ( queues[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0;
			bool compute = ( queues[ i ].queueFlags & VK_QUEUE_COMPUTE_BIT ) != 0;
			bool presentation = SDL_Vulkan_GetPresentationSupport( instance, device.physical_device, i );
			if( graphics && compute && presentation ) {
				device.queue_family_index = u32( i );
				break;
			}
		}

		const float queue_priorities[] = { 1.0f };

		const VkDeviceQueueCreateInfo queueInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = device.queue_family_index,
			.queueCount = 1,
			.pQueuePriorities = queue_priorities,
		};

		constexpr const char * extensions[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
			// VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#if PLATFORM_WINDOWS
			VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
#endif
#if PLATFORM_MACOS
			VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
		};

		VkPhysicalDeviceVulkan13Features features13 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			// .robustImageAccess = VK_TRUE,
			.shaderDemoteToHelperInvocation = VK_TRUE,
			.synchronization2 = VK_TRUE,
			.dynamicRendering = VK_TRUE,
			// .maintenance4 = VK_TRUE,
		};

		VkPhysicalDevicePortabilitySubsetFeaturesKHR features_portability_subset = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
			.pNext = &features13,
			.imageViewFormatSwizzle = VK_TRUE,
		};

		VkPhysicalDeviceVulkan12Features features12 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = macOS ? ( void * ) &features_portability_subset : ( void * ) &features13,
			// .drawIndirectCount = VK_TRUE, XXX metal doesn't have this
			// .storageBuffer8BitAccess = VK_TRUE,
			// .uniformAndStorageBuffer8BitAccess = VK_TRUE,
			// .shaderFloat16 = VK_TRUE,
			// .shaderInt8 = VK_TRUE,
			// .samplerFilterMinmax = VK_TRUE,
			.scalarBlockLayout = VK_TRUE,
			.timelineSemaphore = VK_TRUE,
			.bufferDeviceAddress = VK_TRUE,
		};

		VkPhysicalDeviceVulkan11Features features11 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &features12,
			// .storageBuffer16BitAccess = VK_TRUE,
			.shaderDrawParameters = VK_TRUE,
		};

		const VkPhysicalDeviceFeatures2 features2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &features11,
			.features = {
				.sampleRateShading = VK_TRUE,
				.multiDrawIndirect = VK_TRUE,
				.depthClamp = VK_TRUE,
				.samplerAnisotropy = VK_TRUE,
				.textureCompressionBC = VK_TRUE,
				// .pipelineStatisticsQuery = VK_TRUE,
				.shaderInt64 = VK_TRUE,
				// .shaderInt16 = VK_TRUE,
			},
		};

		const VkDeviceCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features2,

			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queueInfo,

			.enabledExtensionCount = ARRAY_COUNT( extensions ),
			.ppEnabledExtensionNames = extensions,
		};

		VK_CHECK( vkCreateDevice( device.physical_device, &createInfo, NULL, &device.device ) );
		volkLoadDevice( device.device );
		vkGetDeviceQueue( device.device, device.queue_family_index, 0, &device.queue );
	}

	// allocate frame timeline semaphore
	{
		VkSemaphoreTypeCreateInfo semaphore_type = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};

		const VkSemaphoreCreateInfo semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &semaphore_type,
		};

		VK_CHECK( vkCreateSemaphore( device.device, &semaphore_info, NULL, &device.frame_timeline_semaphore ) );
		DebugLabel( device.device, device.frame_timeline_semaphore, VK_OBJECT_TYPE_SEMAPHORE, "frame sync timeline semaphore" );
	}

	// allocate descriptor pool
	// TODO: size these according MaxMaterials etc
	{
		const VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 10000 },
		};

		const VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = 10000,
			.poolSizeCount = ARRAY_COUNT( pool_sizes ),
			.pPoolSizes = pool_sizes,
		};

		VK_CHECK( vkCreateDescriptorPool( device.device, &pool_info, NULL, &device.descriptor_pool ) );
	}

	// allocate command pools
	{
		const VkCommandPoolCreateInfo pool_create_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = device.queue_family_index,
		};

		for( VulkanDevice::CommandPool & pool : device.command_pools ) {
			pool.free_list.init( sys_allocator );
			VK_CHECK( vkCreateCommandPool( device.device, &pool_create_info, NULL, &pool.pool ) );
		}

		VK_CHECK( vkCreateCommandPool( device.device, &pool_create_info, NULL, &device.transfer_command_pool ) );
	}

	// allocate transfer command buffer
	{
		const VkCommandBufferAllocateInfo buffer_create_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = device.transfer_command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VK_CHECK( vkAllocateCommandBuffers( device.device, &buffer_create_info, &device.transfer_command_buffer ) );
		DebugLabel( device.device, device.transfer_command_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, "Transfer command buffer" );
	}

	// create standard material descriptor set layout
	{
		const VkDescriptorSetLayoutBinding bindings[] = {
			VkDescriptorSetLayoutBinding {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_ALL,
			},
			VkDescriptorSetLayoutBinding {
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_ALL,
			},
			VkDescriptorSetLayoutBinding {
				.binding = 2,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_ALL,
			},
		};

		const VkDescriptorSetLayoutCreateInfo layout_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = ARRAY_COUNT( bindings ),
			.pBindings = bindings,
		};

		VK_CHECK( vkCreateDescriptorSetLayout( device.device, &layout_info, NULL, &device.material_descriptor_set_layout ) );
		DebugLabel( device.device, device.material_descriptor_set_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Standard material descriptor set layout" );
	}

	return device;
}

static VkSurfaceFormatKHR GetSwapchainFormat( VulkanDevice device, VkSurfaceKHR surface ) {
	TempAllocator temp = cls.frame_arena.temp();
	Span< VkSurfaceFormatKHR > formats = VulkanSpan< VkSurfaceFormatKHR >( &temp, CurrentSourceLocation(), vkGetPhysicalDeviceSurfaceFormatsKHR, device.physical_device, surface );

	for( const VkSurfaceFormatKHR & format : formats ) {
		if( format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			return format;
		}
	}

	for( const VkSurfaceFormatKHR & format : formats ) {
		if( format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			return format;
		}
	}

	Assert( formats.n > 0 );

	return formats[ 0 ];
}

static void DeleteTexture( const BackendTexture & texture ) {
	vkDestroyImageView( global_device.device, texture.image_view, NULL );
	for( VkImageView layer_image_view : texture.per_layer_image_views ) {
		vkDestroyImageView( global_device.device, layer_image_view, NULL );
	}
	vkDestroyImage( global_device.device, texture.image, NULL );
	Free( sys_allocator, texture.per_layer_image_views.ptr );

	if( texture.allocation.exists ) {
		GPUFree( texture.allocation.value );
	}
}

static Swapchain CreateSwapchain( VulkanDevice device, VkSurfaceKHR surface, Optional< Swapchain > old_swapchain ) {
	VkSurfaceFormatKHR format = GetSwapchainFormat( device, surface );

	VkSurfaceCapabilitiesKHR capabilities;
	VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device.physical_device, surface, &capabilities ) );

	if( capabilities.minImageCount > MaxFramesInFlight ) {
		Assert( capabilities.minImageCount <= MaxFramesInFlight );
		abort();
	}

	TempAllocator temp = cls.frame_arena.temp();
	Span< VkPresentModeKHR > present_modes = VulkanSpan< VkPresentModeKHR >( &temp, CurrentSourceLocation(), vkGetPhysicalDeviceSurfacePresentModesKHR, device.physical_device, surface );

	constexpr bool prefer_vsync = false;
	bool can_vsync = false;
	bool can_no_vsync = false;
	bool can_mailbox = false;
	for( VkPresentModeKHR m : present_modes ) {
		if( m == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
			can_no_vsync = true;
		}
		if( m == VK_PRESENT_MODE_FIFO_KHR ) {
			can_vsync = true;
		}
		if( m == VK_PRESENT_MODE_MAILBOX_KHR ) {
			can_mailbox = true;
		}
	}
	Assert( can_no_vsync || can_vsync );

	VkPresentModeKHR present_mode = prefer_vsync && can_vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
	if( can_mailbox ) {
		present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	}

	u32 image_count = capabilities.minImageCount + 1;
	if( capabilities.maxImageCount != 0 ) {
		image_count = Min2( image_count, capabilities.maxImageCount );
	}

	const VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = image_count,
		.imageFormat = format.format,
		.imageColorSpace = format.colorSpace,
		.imageExtent = {
			.width = capabilities.currentExtent.width,
			.height = capabilities.currentExtent.height,
		},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &device.queue_family_index,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		.oldSwapchain = old_swapchain.exists ? old_swapchain->swapchain : VK_NULL_HANDLE,
	};

	VkSwapchainKHR swapchain = 0;
	VK_CHECK( vkCreateSwapchainKHR( device.device, &createInfo, NULL, &swapchain ) );

	Span< VkImage > vk_images = VulkanSpan< VkImage >( &temp, CurrentSourceLocation(), vkGetSwapchainImagesKHR, device.device, swapchain );

	Span< Swapchain::Image > images = AllocSpan< Swapchain::Image >( sys_allocator, vk_images.n );

	for( size_t i = 0; i < images.n; i++ ) {
		images[ i ].image = vk_images[ i ];

		const VkImageViewCreateInfo image_view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vk_images[ i ],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format.format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			},
		};

		VK_CHECK( vkCreateImageView( device.device, &image_view_info, NULL, &images[ i ].image_view ) );

		DebugLabel( device.device, images[ i ].image, VK_OBJECT_TYPE_IMAGE, temp( "Swapchain image {}", i ) );
	}

	Span< Swapchain::Semaphores > semaphore_pairs;
	if( !old_swapchain.exists ) {
		semaphore_pairs = AllocSpan< Swapchain::Semaphores >( sys_allocator, images.n );
		for( Swapchain::Semaphores & semaphores : semaphore_pairs ) {
			VkSemaphoreTypeCreateInfo semaphore_type = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
				.semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
			};

			const VkSemaphoreCreateInfo semaphore_info = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = &semaphore_type,
			};

			VK_CHECK( vkCreateSemaphore( device.device, &semaphore_info, NULL, &semaphores.acquire ) );
			VK_CHECK( vkCreateSemaphore( device.device, &semaphore_info, NULL, &semaphores.present ) );
		}
	}
	else {
		semaphore_pairs = old_swapchain->semaphores;
		for( Swapchain::Image image : old_swapchain->images ) {
			vkDestroyImageView( global_device.device, image.image_view, NULL );
		}
		Free( sys_allocator, old_swapchain->images.ptr );
	}

	return Swapchain {
		.swapchain = swapchain,
		.width = capabilities.currentExtent.width,
		.height = capabilities.currentExtent.height,
		.format = format.format,
		.images = images,
		.semaphores = semaphore_pairs,
	};
}

static void DeleteSwapchain( Swapchain swapchain ) {
	for( Swapchain::Semaphores semaphores : swapchain.semaphores ) {
		vkDestroySemaphore( global_device.device, semaphores.acquire, NULL );
		vkDestroySemaphore( global_device.device, semaphores.present, NULL );
	}
	vkDestroySwapchainKHR( global_device.device, swapchain.swapchain, NULL );
	for( Swapchain::Image image : swapchain.images ) {
		vkDestroyImageView( global_device.device, image.image_view, NULL );
	}
	Free( sys_allocator, swapchain.images.ptr );
	Free( sys_allocator, swapchain.semaphores.ptr );
}

static bool MaybeRecreateSwapchain( const VkSurfaceCapabilitiesKHR & surface_capabilities ) {
	if( surface_capabilities.currentExtent.width == global_swapchain.width && surface_capabilities.currentExtent.height == global_swapchain.height ) {
		return false;
	}

	Swapchain new_swapchain = CreateSwapchain( global_device, global_surface, global_swapchain );
	VK_CHECK( vkDeviceWaitIdle( global_device.device ) );
	DeleteSwapchain( global_swapchain );
	global_swapchain = new_swapchain;

	return true;
}

static VkFormat VertexFormatToVulkan( VertexFormat format ) {
	switch( format ) {
		case VertexFormat_U8x2: return VK_FORMAT_R8G8_UINT;
		case VertexFormat_U8x2_01: return VK_FORMAT_R8G8_UNORM;
		case VertexFormat_U8x3: return VK_FORMAT_R8G8B8_UINT;
		case VertexFormat_U8x3_01: return VK_FORMAT_R8G8B8_UNORM;
		case VertexFormat_U8x4: return VK_FORMAT_R8G8B8A8_UINT;
		case VertexFormat_U8x4_01: return VK_FORMAT_R8G8B8A8_UNORM;

		case VertexFormat_U16x2: return VK_FORMAT_R16G16_UINT;
		case VertexFormat_U16x2_01: return VK_FORMAT_R16G16_UNORM;
		case VertexFormat_U16x3: return VK_FORMAT_R16G16B16_UINT;
		case VertexFormat_U16x3_01: return VK_FORMAT_R16G16B16_UNORM;
		case VertexFormat_U16x4: return VK_FORMAT_R16G16B16A16_UINT;
		case VertexFormat_U16x4_01: return VK_FORMAT_R16G16B16A16_UNORM;

		case VertexFormat_U10x3_U2x1_01: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

		case VertexFormat_Floatx2: return VK_FORMAT_R32G32_SFLOAT;
		case VertexFormat_Floatx3: return VK_FORMAT_R32G32B32_SFLOAT;
		case VertexFormat_Floatx4: return VK_FORMAT_R32G32B32A32_SFLOAT;
	}
}

struct SpirvID {
	Optional< SpvOp > op;
	Optional< const char * > name;
	Optional< u32 > next;
	Optional< SpvStorageClass > storage_class;
	Optional< u32 > num_members;
	Optional< u32 > stride;
	Optional< u32 > descriptor_set;
	Optional< u32 > binding;
};

static const char * StripPrefix( const char * str, const char * prefix ) {
	return StartsWith( str, prefix ) ? str + strlen( prefix ) : str;
}

static void ParseSPIRV( Span< const u32 > spirv, ReflectedDescriptorSet ( &sets )[ DescriptorSet_Count ] ) {
	Assert( spirv[ 0 ] == SpvMagicNumber );

	TempAllocator temp = cls.frame_arena.temp();
	Span< SpirvID > ids = AllocSpan< SpirvID >( &temp, spirv[ 3 ] );
	memset( ids.ptr, 0, ids.num_bytes() );

	constexpr size_t skip_spirv_headers = 5;
	size_t cursor = skip_spirv_headers;
	while( cursor < spirv.n ) {
		SpvOp op = SpvOp( u16( spirv[ cursor ] ) );
		u16 op_size = u16( spirv[ cursor ] >> 16 );

		switch( op ) {
			case SpvOpName: {
				u32 id = spirv[ cursor + 1 ];
				ids[ id ].name = ( const char * ) &spirv[ cursor + 2 ];
			} break;

			case SpvOpDecorate: {
				u32 id = spirv[ cursor + 1 ];
				u32 field = spirv[ cursor + 2 ];
				u32 value = spirv[ cursor + 3 ];
				if( field == SpvDecorationArrayStride ) {
					ids[ id ].op = op;
					ids[ id ].stride = value;
				}
				else if( field == SpvDecorationDescriptorSet ) {
					ids[ id ].descriptor_set = value;
				}
				else if( field == SpvDecorationBinding ) {
					ids[ id ].binding = value;
				}
			} break;

			case SpvOpTypeStruct:
			case SpvOpTypeImage:
			case SpvOpTypeSampler:
			case SpvOpTypeSampledImage: {
				u32 id = spirv[ cursor + 1 ];
				ids[ id ].op = op;
				if( op != SpvOpTypeSampler ) {
					u32 type = spirv[ cursor + 2 ];
					ids[ id ].next = type;
					if( op == SpvOpTypeStruct ) {
						ids[ id ].num_members = op_size - 2;
					}
				}
			} break;

			case SpvOpVariable: {
				u32 id = spirv[ cursor + 1 ];
				u32 var = spirv[ cursor + 2 ];
				Assert( !ids[ var ].op.exists );
				ids[ var ].op = op;
				ids[ var ].next = id;
				ids[ var ].storage_class = SpvStorageClass( spirv[ cursor + 3 ] );
			} break;

			case SpvOpTypePointer: {
				u32 id = spirv[ cursor + 1 ];
				Assert( !ids[ id ].op.exists );
				ids[ id ].op = op;
				ids[ id ].storage_class = SpvStorageClass( spirv[ cursor + 2 ] );
				ids[ id ].next = spirv[ cursor + 3 ];
			} break;

			default: break;
		}

		cursor += op_size;
	}

	for( SpirvID id : ids ) {
		if( !id.op.exists )
			continue;

		if( id.op.value == SpvOpTypePointer && id.storage_class == SpvStorageClassPushConstant ) {
			SpirvID resource = ids[ id.next.value ];
			if( resource.op.exists ) {
				if( resource.op.value == SpvOpTypeStruct && resource.num_members.exists ) {
					Assert( ids[ id.next.value ].num_members.value <= MaxPushConstants );
				}
			}
			continue;
		}

		if( id.op.value == SpvOpVariable ) {
			Assert( id.storage_class.exists );
			if( id.storage_class.value != SpvStorageClassUniform && id.storage_class.value != SpvStorageClassUniformConstant && id.storage_class.value != SpvStorageClassStorageBuffer )
				continue;

			Assert( id.name.exists && id.descriptor_set.exists && id.binding.exists && id.next.exists );
			SpirvID ptr = ids[ id.next.value ];
			Assert( ptr.op.exists && ptr.op.value == SpvOpTypePointer && ptr.next.exists );
			SpirvID resource = ids[ ptr.next.value ];
			Assert( resource.op.exists );
			SpirvID type = ids[ resource.next.value ];

			VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
			switch( resource.op.value ) {
				case SpvOpTypeStruct:
					Assert( type.op.exists && type.stride.exists && type.stride.value > 0 );
					descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					break;
				case SpvOpTypeImage:
					descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					break;
				case SpvOpTypeSampler:
					descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
					break;
				default: Assert( false ); break;
			}

			sets[ id.descriptor_set.value ].bindings.must_add_or_exists( StringHash( StripPrefix( id.name.value, "args." ) ), {
				.type = descriptor_type,
				.index = id.binding.value,
				.stride = Default( type.stride, 0_u32 ),
			} );
		}
	}
}

static VkPipelineShaderStageCreateInfo CompileShader( Span< const char > path, VkShaderStageFlagBits stage, ReflectedDescriptorSet ( &reflected_descriptor_sets )[ DescriptorSet_Count ] ) {
	TempAllocator temp = cls.frame_arena.temp();

	Span< const u32 > spirv = AssetBinary( path ).cast< const u32 >();

	ParseSPIRV( spirv, reflected_descriptor_sets );

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = spirv.num_bytes(),
		.pCode = spirv.ptr,
	};

	VkShaderModule module;
	VK_CHECK( vkCreateShaderModule( global_device.device, &create_info, NULL, &module ) );
	DebugLabel( module, VK_OBJECT_TYPE_SHADER_MODULE, temp( "{}", path ) );

	const char * main_name = "";
	switch( stage ) {
		case VK_SHADER_STAGE_VERTEX_BIT: main_name = "VertexMain"; break;
		case VK_SHADER_STAGE_FRAGMENT_BIT: main_name = "FragmentMain"; break;
		case VK_SHADER_STAGE_COMPUTE_BIT: main_name = "ComputeMain"; break;
		default: Assert( false ); break;
	}

	return VkPipelineShaderStageCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = stage,
		.module = module,
		.pName = main_name,
	};
}

struct PushDescriptor {
	union {
		VkDescriptorBufferInfo buffer;
		VkDescriptorImageInfo image;
		VkDescriptorImageInfo sampler;
	};
};

static PushDescriptorTemplate NewPushDescriptorTemplate( const ReflectedDescriptorSet & reflection, VkPipelineBindPoint bind_point, VkPipelineLayout layout, DescriptorSetType set ) {
	PushDescriptorTemplate descriptor_template = { };

	BoundedDynamicArray< VkDescriptorUpdateTemplateEntry, MaxBufferBindings + MaxTextureBindings + MaxTextureBindings > entries = { };
	for( const auto & [ name, binding ] : reflection.bindings ) {
		entries.must_add( VkDescriptorUpdateTemplateEntry {
			.dstBinding = binding.index,
			.descriptorCount = 1,
			.descriptorType = binding.type,
			.offset = entries.size() * sizeof( PushDescriptor ),
			.stride = sizeof( PushDescriptor ),
		} );

		switch( binding.type ) {
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				descriptor_template.buffers.must_add( name, binding.index );
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				descriptor_template.textures.must_add( name, binding.index );
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				descriptor_template.samplers.must_add( name, binding.index );
				break;
			default: Assert( false ); break;
		}
	}

	if( entries.size() > 0 ) {
		VkDescriptorUpdateTemplateCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,
			.descriptorUpdateEntryCount = checked_cast< u32 >( entries.size() ),
			.pDescriptorUpdateEntries = entries.ptr(),
			.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR,
			.pipelineBindPoint = bind_point,
			.pipelineLayout = layout,
			.set = u32( set ),
		};

		VK_CHECK( vkCreateDescriptorUpdateTemplate( global_device.device, &create_info, NULL, &descriptor_template.update_template ) );
		DebugLabel( descriptor_template.update_template, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, "descriptor set template" );
	}

	return descriptor_template;
}

static VkFormat TextureFormatToVulkan( TextureFormat format ) {
	switch( format ) {
		case TextureFormat_R_U8: return VK_FORMAT_R8_UNORM;
		case TextureFormat_R_U8_sRGB: return VK_FORMAT_R8_SRGB;
		case TextureFormat_R_S8: return VK_FORMAT_R8_SNORM;
		case TextureFormat_R_UI8: return VK_FORMAT_R8_UINT;

		case TextureFormat_A_U8: return VK_FORMAT_R8_UNORM;

		case TextureFormat_RA_U8: return VK_FORMAT_R8G8_UNORM;

		case TextureFormat_RGBA_U8: return VK_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat_RGBA_U8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;

		case TextureFormat_BC1_sRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case TextureFormat_BC3_sRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
		case TextureFormat_BC4: return VK_FORMAT_BC4_UNORM_BLOCK;
		case TextureFormat_BC5: return VK_FORMAT_BC5_UNORM_BLOCK;

		case TextureFormat_Depth: return VK_FORMAT_D32_SFLOAT;

		case TextureFormat_Swapchain: return global_swapchain.format;
	}
}

PoolHandle< GPUAllocation > ShitGuh();

static constexpr VkPipelineColorBlendAttachmentState blend_states[ BlendFunc_Count ] = {
	{
		.blendEnable = false,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	},
	{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	},
	{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	},
};

PoolHandle< RenderPipeline > NewRenderPipeline( const RenderPipelineConfig & config ) {
	ReflectedDescriptorSet descriptor_sets[ DescriptorSet_Count ] = { };
	u32 push_constant_size = 0;

	// compile and parse shaders
	TempAllocator temp = cls.frame_arena.temp();
	VkPipelineShaderStageCreateInfo stages[] = {
		CompileShader( temp.sv( "{}.vert.spv", config.path ), VK_SHADER_STAGE_VERTEX_BIT, descriptor_sets ),
		CompileShader( temp.sv( "{}.frag.spv", config.path ), VK_SHADER_STAGE_FRAGMENT_BIT, descriptor_sets ),
	};

	// create descriptor set layouts
	VkDescriptorSetLayout descriptor_set_layouts[ DescriptorSet_Count ];
	for( size_t i = 0; i < DescriptorSet_Count; i++ ) {
		VkDescriptorSetLayoutBinding bindings[ 16 ];
		size_t n = 0;

		for( const auto & [ name, binding ] : descriptor_sets[ i ].bindings ) {
			bindings[ n++ ] = VkDescriptorSetLayoutBinding {
				.binding = binding.index,
				.descriptorType = binding.type,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_ALL,
			};
		}

		VkDescriptorSetLayoutCreateFlags flags = i == DescriptorSet_RenderPass ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
		const VkDescriptorSetLayoutCreateInfo layout_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = flags,
			.bindingCount = u32( n ),
			.pBindings = bindings,
		};

		VK_CHECK( vkCreateDescriptorSetLayout( global_device.device, &layout_info, NULL, &descriptor_set_layouts[ i ] ) );

		DebugLabel( descriptor_set_layouts[ i ], VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, temp( "{} descriptor layout {}", config.path, i ) );
	}

	const VkPushConstantRange push_constant = {
		.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
		.size = MaxPushConstants * sizeof( u64 ),
	};

	const VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = u32( ARRAY_COUNT( descriptor_set_layouts ) ),
		.pSetLayouts = descriptor_set_layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant,
	};

	VkPipelineLayout layout;
	VK_CHECK( vkCreatePipelineLayout( global_device.device, &pipeline_layout_info, NULL, &layout ) );
	DebugLabel( layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, temp( "{} pipeline layout", config.path ) );

	// create pipelines
	BoundedDynamicArray< VkFormat, FragmentShaderOutput_Count > output_formats = { };
	BoundedDynamicArray< VkPipelineColorBlendAttachmentState, FragmentShaderOutput_Count > color_blends = { };
	for( TextureFormat format : config.output_format.colors ) {
		output_formats.must_add( TextureFormatToVulkan( format ) );
		color_blends.must_add( blend_states[ config.blend_func ] );
	}

	const VkPipelineRenderingCreateInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = u32( output_formats.size() ),
		.pColorAttachmentFormats = output_formats.ptr(),
		.depthAttachmentFormat = config.output_format.has_depth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED,
	};

	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkPipelineRasterizationStateCreateInfo rasterization_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = config.clamp_depth,
		// .cullMode is dynamic state
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	const VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		// .depthWriteEnable is dynamic state
		// .depthCompareOp is dynamic state
	};

	const VkPipelineColorBlendStateCreateInfo color_blend_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = u32( config.output_format.colors.n ),
		.pAttachments = color_blends.ptr(),
	};

	// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDynamicState.html
	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_CULL_MODE,
		VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
		VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
	};

	const VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = ARRAY_COUNT( dynamic_states ),
		.pDynamicStates = dynamic_states,
	};

	ArrayMap< VertexDescriptor, RenderPipeline::Variant, MaxShaderVariants > mesh_variants = { };
	for( VertexDescriptor mesh_variant : config.mesh_variants ) {
		RenderPipeline::Variant * variant = mesh_variants.add( mesh_variant );

		BoundedDynamicArray< VkVertexInputAttributeDescription, VertexAttribute_Count > vertex_attributes = { };
		for( size_t i = 0; i < ARRAY_COUNT( mesh_variant.attributes ); i++ ) {
			if( !mesh_variant.attributes[ i ].exists )
				continue;
			VertexAttribute attribute = mesh_variant.attributes[ i ].value;
			vertex_attributes.must_add( VkVertexInputAttributeDescription {
				.location = u32( i ),
				.binding = u32( attribute.buffer ),
				.format = VertexFormatToVulkan( attribute.format ),
				.offset = u32( attribute.offset ),
			} );
		}

		BoundedDynamicArray< VkVertexInputBindingDescription, VertexAttribute_Count > vertex_bindings = { };
		for( size_t i = 0; i < ARRAY_COUNT( mesh_variant.buffer_strides ); i++ ) {
			if( !mesh_variant.buffer_strides[ i ].exists )
				continue;
			vertex_bindings.must_add( VkVertexInputBindingDescription {
				.binding = u32( i ),
				.stride = u32( mesh_variant.buffer_strides[ i ].value ),
			} );
		}

		const VkPipelineVertexInputStateCreateInfo vertex_input = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = u32( vertex_bindings.size() ),
			.pVertexBindingDescriptions = vertex_bindings.ptr(),
			.vertexAttributeDescriptionCount = u32( vertex_attributes.size() ),
			.pVertexAttributeDescriptions = vertex_attributes.ptr(),
		};

		for( u32 msaa = 1; msaa <= MaxMSAA; msaa *= 2 ) {
			if( !HasAnyBit( global_device.limits.msaa, msaa ) )
				continue;

			const VkPipelineMultisampleStateCreateInfo multisample_state = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = VkSampleCountFlagBits( msaa ),
				.alphaToCoverageEnable = config.alpha_to_coverage,
			};

			const VkGraphicsPipelineCreateInfo create_info = {
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &rendering_info,

				.stageCount = 2,
				.pStages = stages,
				.pVertexInputState = &vertex_input,
				.pInputAssemblyState = &input_assembly,
				.pViewportState = &viewport_state,
				.pRasterizationState = &rasterization_state,
				.pMultisampleState = &multisample_state,
				.pDepthStencilState = &depth_stencil_state,
				.pColorBlendState = &color_blend_state,
				.pDynamicState = &dynamic_state,
				.layout = layout,
			};

			VkPipeline pipeline;
			VK_CHECK( vkCreateGraphicsPipelines( global_device.device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline ) );
			DebugLabel( pipeline, VK_OBJECT_TYPE_PIPELINE, temp( "{}", config.path ) );

			variant->msaa_variants[ Log2( msaa ) ] = pipeline;
		}
	}

	for( const VkPipelineShaderStageCreateInfo & stage : stages ) {
		vkDestroyShaderModule( global_device.device, stage.module, NULL );
	}

	RenderPipeline pso = {
		.name = CloneSpan( sys_allocator, config.path ),
		.mesh_variants = mesh_variants,
		.layout = layout,
		.render_pass_push_descriptors = NewPushDescriptorTemplate( descriptor_sets[ DescriptorSet_RenderPass ], VK_PIPELINE_BIND_POINT_GRAPHICS, layout, DescriptorSet_RenderPass ),
	};
	memcpy( pso.descriptor_set_layouts, descriptor_set_layouts, sizeof( descriptor_set_layouts ) );
	memcpy( pso.reflection, descriptor_sets, sizeof( descriptor_sets ) );
	return render_pipelines.allocate( pso );
}

static VkPipeline SelectRenderPipelineVariant( const RenderPipeline & shader, const VertexDescriptor & mesh_format, u32 msaa ) {
	const RenderPipeline::Variant * mesh_variant = shader.mesh_variants.get( mesh_format );
	return mesh_variant == NULL ? VkPipeline( VK_NULL_HANDLE ) : mesh_variant->msaa_variants[ Log2( msaa ) ];
}

static void DeleteRenderPipeline( RenderPipeline shader ) {
	Free( sys_allocator, shader.name.ptr );
	vkDestroyDescriptorUpdateTemplate( global_device.device, shader.render_pass_push_descriptors.update_template, NULL );
	for( VkDescriptorSetLayout & set : shader.descriptor_set_layouts ) {
		vkDestroyDescriptorSetLayout( global_device.device, set, NULL );
	}
	vkDestroyPipelineLayout( global_device.device, shader.layout, NULL );
	for( auto [ k, v ] : shader.mesh_variants ) {
		for( VkPipeline pipeline : v.msaa_variants ) {
			if( pipeline != VK_NULL_HANDLE ) {
				vkDestroyPipeline( global_device.device, pipeline, NULL );
			}
		}
	}
}

static constexpr struct {
	VkCompareOp op;
	bool write_depth;
} depth_funcs[ DepthFunc_Count ] = {
	{ VK_COMPARE_OP_LESS, true },
	{ VK_COMPARE_OP_LESS, false },
	{ VK_COMPARE_OP_EQUAL, true },
	{ VK_COMPARE_OP_EQUAL, false },
	{ VK_COMPARE_OP_ALWAYS, true },
	{ VK_COMPARE_OP_ALWAYS, false },
};

static void BindMesh( VkCommandBuffer cb, const Mesh & mesh ) {
	if( mesh.index_buffer.exists ) {
		Assert( mesh.index_buffer->offset % 4 == 0 );
		VkIndexType type = mesh.index_format == IndexFormat_U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		vkCmdBindIndexBuffer( cb, allocations[ mesh.index_buffer->allocation ].buffer, mesh.index_buffer->offset, type );
	}

	for( size_t i = 0; i < ARRAY_COUNT( mesh.vertex_buffers ); i++ ) {
		if( mesh.vertex_buffers[ i ].exists ) {
			GPUBuffer buffer = mesh.vertex_buffers[ i ].value;
			VkDeviceSize offset = buffer.offset;
			vkCmdBindVertexBuffers( cb, i, 1, &allocations[ buffer.allocation ].buffer, &offset );
		}
	}
}

static VkCommandBuffer PrepareDraw( Opaque< CommandBuffer > ocb, const PipelineState & pipeline_state, Mesh mesh, Span< const GPUBuffer > buffers ) {
	VkCommandBuffer cb = ocb.unwrap()->buffer;

	const RenderPipeline & shader = render_pipelines[ pipeline_state.shader ];
	VkPipeline pso = SelectRenderPipelineVariant( shader, mesh.vertex_descriptor, ocb.unwrap()->msaa_samples );
	if( pso == VK_NULL_HANDLE ) {
		Com_GGPrint( S_COLOR_YELLOW "No shader variant: {} {}", shader.name, mesh.vertex_descriptor );
		return cb;
	}

	vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pso );
	vkCmdSetCullMode( cb, pipeline_state.dynamic_state.cull_face == CullFace_Back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT );
	vkCmdSetDepthCompareOp( cb, depth_funcs[ pipeline_state.dynamic_state.depth_func ].op );
	vkCmdSetDepthWriteEnable( cb, depth_funcs[ pipeline_state.dynamic_state.depth_func ].write_depth );

	BindMesh( cb, mesh );

	if( pipeline_state.material_bind_group.exists ) {
		vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.layout, DescriptorSet_Material, 1, &bind_groups[ pipeline_state.material_bind_group.value ].descriptor_set, 0, NULL );
	}

	if( buffers.n > 0 ) {
		u64 addrs[ MaxPushConstants ] = { };
		for( size_t i = 0; i < buffers.n; i++ ) {
			addrs[ i ] = allocations[ buffers[ i ].allocation ].addr + buffers[ i ].offset;
		}
		vkCmdPushConstants( cb, shader.layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof( addrs ), addrs );
	}

	return cb;
}

void EncodeDrawCall( Opaque< CommandBuffer > ocb, const PipelineState & pipeline_state, Mesh mesh, Span< const GPUBuffer > buffers, DrawCallExtras extras ) {
	VkCommandBuffer cb = PrepareDraw( ocb, pipeline_state, mesh, buffers );

	u32 num_vertices = Default( extras.override_num_vertices, mesh.num_vertices );
	if( mesh.index_buffer.exists ) {
		vkCmdDrawIndexed( cb, num_vertices, 1, extras.first_index, extras.base_vertex, 0 );
	}
	else {
		vkCmdDraw( cb, num_vertices, 1, extras.first_index, 0 );
	}
}

void EncodeIndirectDrawCall( Opaque< CommandBuffer > ocb, const PipelineState & pipeline_state, Mesh mesh, GPUBuffer indirect_args, Span< const GPUBuffer > buffers ) {
	VkCommandBuffer cb = PrepareDraw( ocb, pipeline_state, mesh, buffers );

	if( mesh.index_buffer.exists ) {
		vkCmdDrawIndexedIndirect( cb, allocations[ indirect_args.allocation ].buffer, indirect_args.offset, 1, 0 );
	}
	else {
		vkCmdDrawIndirect( cb, allocations[ indirect_args.allocation ].buffer, indirect_args.offset, 1, 0 );
	}
}

void EncodeScissor( Opaque< CommandBuffer > ocb, Optional< Scissor > scissor ) {
	VkRect2D vk = no_scissor;
	if( scissor.exists ) {
		vk = {
			.offset = { s32( scissor->x ), s32( scissor->y ) },
			.extent = { scissor->w, scissor->h },
		};
	}
	vkCmdSetScissor( ocb.unwrap()->buffer, 0, 1, &vk );
}

PoolHandle< ComputePipeline > NewComputePipeline( Span< const char > path ) {
	TempAllocator temp = cls.frame_arena.temp();

	// compile and parse shaders
	ReflectedDescriptorSet descriptor_sets[ DescriptorSet_Count ] = { };
	VkPipelineShaderStageCreateInfo stage = CompileShader( temp.sv( "{}.comp.spv", path ), VK_SHADER_STAGE_COMPUTE_BIT, descriptor_sets );

	// create descriptor set layout
	VkDescriptorSetLayout descriptor_set_layout;
	{
		VkDescriptorSetLayoutBinding bindings[ 16 ];
		size_t n = 0;

		for( const auto & [ name, binding ] : descriptor_sets[ 0 ].bindings ) {
			bindings[ n++ ] = VkDescriptorSetLayoutBinding {
				.binding = binding.index,
				.descriptorType = binding.type,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_ALL,
			};
		}

		VkDescriptorSetLayoutCreateFlags flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		const VkDescriptorSetLayoutCreateInfo layout_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags = flags,
			.bindingCount = u32( n ),
			.pBindings = bindings,
		};

		VK_CHECK( vkCreateDescriptorSetLayout( global_device.device, &layout_info, NULL, &descriptor_set_layout ) );

		DebugLabel( descriptor_set_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "descriptor set layout" );
	}

	const VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set_layout,
	};

	VkPipelineLayout layout;
	VK_CHECK( vkCreatePipelineLayout( global_device.device, &pipeline_layout_info, NULL, &layout ) );
	DebugLabel( layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, temp( "{} pipeline layout", path ) );

	// create pipeline
	VkComputePipelineCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = stage,
		.layout = layout,
	};

	VkPipeline pipeline;
	VK_CHECK( vkCreateComputePipelines( global_device.device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline ) );

	PushDescriptorTemplate push_descriptors = NewPushDescriptorTemplate( descriptor_sets[ DescriptorSetType( 0 ) ], VK_PIPELINE_BIND_POINT_COMPUTE, layout, DescriptorSetType( 0 ) );

	vkDestroyShaderModule( global_device.device, stage.module, NULL );

	return compute_pipelines.allocate( ComputePipeline {
		.pipeline = pipeline,
		.layout = layout,
		.descriptor_set_layout = descriptor_set_layout,
		.push_descriptors = push_descriptors,
	} );
}

static void DeleteComputePipeline( ComputePipeline pipeline ) {
	vkDestroyDescriptorUpdateTemplate( global_device.device, pipeline.push_descriptors.update_template, NULL );
	vkDestroyDescriptorSetLayout( global_device.device, pipeline.descriptor_set_layout, NULL );
	vkDestroyPipelineLayout( global_device.device, pipeline.layout, NULL );
	vkDestroyPipeline( global_device.device, pipeline.pipeline, NULL );
}

static void Barriers( VkCommandBuffer command_buffer, Optional< VkMemoryBarrier2 > global_barrier, Span< VkImageMemoryBarrier2 > image_barriers ) {
	for( VkImageMemoryBarrier2 & barrier : image_barriers ) {
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	const VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = global_barrier.exists ? 1_u32 : 0_u32,
		.pMemoryBarriers = &global_barrier.value,
		.imageMemoryBarrierCount = u32( image_barriers.n ),
		.pImageMemoryBarriers = image_barriers.ptr,
	};

	vkCmdPipelineBarrier2( command_buffer, &dependency_info );
}

static void Barrier( VkCommandBuffer command_buffer, VkImageMemoryBarrier2 barrier ) {
	Barriers( command_buffer, NONE, Span< VkImageMemoryBarrier2 >( &barrier, 1 ) );
}

/*
 * Staging buffer shit
 */

PoolHandle< GPUAllocation > AllocateGPUMemory( size_t size ) {
	return allocations.allocate( GPUMalloc( size, true ) );
}

void ReallocateGPUMemory( Opaque< CommandBuffer > cmd_buf, PoolHandle< GPUAllocation > allocation, size_t old_size, size_t new_size ) {
	GPUAllocation old_alloc = allocations[ allocation ];
	GPUAllocation new_alloc = GPUMalloc( new_size, true );

	VkBufferCopy copy = { .size = old_size };
	vkCmdCopyBuffer( cmd_buf.unwrap()->buffer, old_alloc.buffer, new_alloc.buffer, 1, &copy );
	FlushStagingBuffer();

	allocations[ allocation ] = new_alloc;
	GPUFree( old_alloc );
}

CoherentMemory AllocateCoherentMemory( size_t size ) {
	PoolHandle< GPUAllocation > allocation = allocations.allocate( GPUMalloc( size, false ) );

	void * mapped;
	VK_CHECK( vkMapMemory( global_device.device, allocations[ allocation ].memory, 0, size, 0, &mapped ) );

	return CoherentMemory {
		.allocation = allocation,
		.ptr = mapped,
	};
}

void AddDebugMarker( PoolHandle< GPUAllocation > allocation, size_t offset, size_t size, const char * label ) {
	// Vulkan doesn't have this functionality
}

void RemoveAllDebugMarkers( PoolHandle< GPUAllocation > allocation ) {
	// Vulkan doesn't have this functionality
}

Opaque< CommandBuffer > NewTransferCommandBuffer() {
	// VkCommandBufferAllocateInfo buffer_create_info = {
	// 	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	// 	.commandPool = global_device.transfer_command_pool,
	// 	.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	// 	.commandBufferCount = 1,
	// };
    //
	// VkCommandBuffer command_buffer;
	// VK_CHECK( vkAllocateCommandBuffers( global_device.device, &buffer_create_info, &command_buffer ) );
	// return command_buffer;

	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK( vkResetCommandPool( global_device.device, global_device.transfer_command_pool, 0 ) );
	VK_CHECK( vkBeginCommandBuffer( global_device.transfer_command_buffer, &begin_info ) );

	VkDebugUtilsLabelEXT label = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pLabelName = "Staging buffer copies",
	};

	vkCmdBeginDebugUtilsLabelEXT( global_device.transfer_command_buffer, &label );

	return CommandBuffer { global_device.transfer_command_buffer };
}

void DeleteTransferCommandBuffer( Opaque< CommandBuffer > cb ) {
}

static VkCommandBuffer NewCommandBuffer() {
	VulkanDevice::CommandPool * pool = &global_device.command_pools[ FrameSlot() ];
	if( pool->num_used == pool->free_list.size() ) {
		const VkCommandBufferAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = pool->pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkCommandBuffer buffer;
		VK_CHECK( vkAllocateCommandBuffers( global_device.device, &alloc_info, &buffer ) );

		pool->free_list.add( buffer );
		pool->num_used++;

		return buffer;
	}

	return pool->free_list[ pool->num_used++ ];
}

static Optional< VkMemoryBarrier2 > MakeGlobalBarrier( Span< const GPUBarrier > barriers ) {
	if( barriers.n == 0 )
		return NONE;

	VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };

	for( GPUBarrier b : barriers ) {
		switch( b ) {
			case GPUBarrier_ComputeToIndirect:
				barrier.srcStageMask |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
				barrier.srcAccessMask |= VK_ACCESS_2_SHADER_WRITE_BIT;
				barrier.dstStageMask |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
				barrier.dstAccessMask |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
				break;

			case GPUBarrier_ComputeToCompute:
				barrier.srcStageMask |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
				barrier.srcAccessMask |= VK_ACCESS_2_SHADER_WRITE_BIT;
				barrier.dstStageMask |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
				barrier.dstAccessMask |= VK_ACCESS_2_SHADER_READ_BIT;
				break;

			case GPUBarrier_ComputeToFragment:
				barrier.srcStageMask |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
				barrier.srcAccessMask |= VK_ACCESS_2_SHADER_WRITE_BIT;
				barrier.dstStageMask |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
				barrier.dstAccessMask |= VK_ACCESS_2_SHADER_READ_BIT;
				break;

			case GPUBarrier_FragmentToFragmentSample:
				barrier.srcStageMask |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
				barrier.srcAccessMask |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				barrier.dstStageMask |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
				barrier.dstAccessMask |= VK_ACCESS_2_SHADER_READ_BIT;
				break;

			case GPUBarrier_FragmentToFragmentOutput:
				barrier.srcStageMask |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
				barrier.srcAccessMask |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				barrier.dstStageMask |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
				barrier.dstAccessMask |= VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
				break;
		}
	}

	return barrier;
}

static VkImageMemoryBarrier2 MakeImageTransition( VkImage image, u32 num_layers, u32 num_mipmaps, bool is_depth, bool to_attachment ) {
	constexpr VkImageMemoryBarrier2 color_attachment = {
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
	};

	constexpr VkImageMemoryBarrier2 color_readonly = {
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
	};

	constexpr VkImageMemoryBarrier2 depth_attachment = {
		.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
	};

	constexpr VkImageMemoryBarrier2 depth_readonly = {
		.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
	};

	VkImageMemoryBarrier2 barrier;
	if( !is_depth &&  to_attachment ) barrier = color_attachment;
	if( !is_depth && !to_attachment ) barrier = color_readonly;
	if(  is_depth &&  to_attachment ) barrier = depth_attachment;
	if(  is_depth && !to_attachment ) barrier = depth_readonly;

	barrier.image = image;
	barrier.subresourceRange = {
		.aspectMask = VkImageAspectFlags( is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ),
		.levelCount = num_mipmaps,
		.layerCount = num_layers,
	};
	return barrier;
}

static VkImageMemoryBarrier2 MakeImageTransition( PoolHandle< Texture > handle, bool to_attachment ) {
	const Texture & texture = textures[ handle ];
	return MakeImageTransition( texture.backend.unwrap()->image, texture.num_layers, texture.num_mipmaps, texture.format == TextureFormat_Depth, to_attachment );
}

Opaque< CommandBuffer > NewRenderPass( const RenderPassConfig & config ) {
	TracyZoneScoped;
	TracyZoneSpan( config.name );

	Assert( config.color_targets.n <= FragmentShaderOutput_Count );

	TempAllocator temp = cls.frame_arena.temp();
	const char * name = temp( "{}", config.name );

	VkCommandBuffer command_buffer = NewCommandBuffer();
	DebugLabel( command_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, name );


	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK( vkBeginCommandBuffer( command_buffer, &begin_info ) );

	{
		VkDebugUtilsLabelEXT label = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = name,
		};
		vkCmdBeginDebugUtilsLabelEXT( command_buffer, &label );
	}

	Optional< u32 > width = NONE;
	Optional< u32 > height = NONE;
	Optional< u32 > msaa = NONE;

	auto set_rt_params = [&]( u32 w, u32 h, u32 m ) {
		Assert( !width.exists || ( *width == w && *height == h ) );
		Assert( !msaa.exists || *msaa == m );
		width = w;
		height = h;
		msaa = m;
	};

	auto set_rt_params_optional = [&]( Optional< PoolHandle< Texture > > texture ) {
		if( texture.exists ) {
			set_rt_params( TextureWidth( texture.value ), TextureHeight( texture.value ), TextureMSAASamples( texture.value ) );
		}
		else {
			set_rt_params( global_swapchain.width, global_swapchain.height, 1 );
		}
	};

	Swapchain::Image swapchain = global_swapchain.images[ frame.image_index ];

	VkRenderingAttachmentInfo color_attachments[ FragmentShaderOutput_Count ];
	for( size_t i = 0; i < config.color_targets.n; i++ ) {
		VkRenderingAttachmentInfo * attachment = &color_attachments[ i ];
		const RenderPassConfig::ColorTarget & target = config.color_targets[ i ];

		VkImageView image_view = swapchain.image_view;
		if( target.texture.exists ) {
			image_view = textures[ *target.texture ].backend.unwrap()->per_layer_image_views[ target.layer ];
		}

		set_rt_params_optional( target.texture );

		*attachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = image_view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.storeOp = target.store == StoreOp_Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
		};

		switch( target.load ) {
			case LoadOp_DontCare: attachment->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; break;
			case LoadOp_Load: attachment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; break;
			case LoadOp_Clear:
				attachment->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				memcpy( attachment->clearValue.color.float32, target.clear.ptr(), sizeof( target.clear ) );
				break;
		}

		if( target.resolve_target.exists ) {
			attachment->storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment->resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
			attachment->resolveImageView = textures[ target.resolve_target.value ].backend.unwrap()->image_view;
			attachment->resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	VkRenderingAttachmentInfo depth_attachment;
	if( config.depth_target.exists ) {
		const RenderPassConfig::DepthTarget & target = config.depth_target.value;

		set_rt_params( TextureWidth( target.texture ), TextureHeight( target.texture ), TextureMSAASamples( target.texture ) );

		depth_attachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = textures[ target.texture ].backend.unwrap()->per_layer_image_views[ target.layer ],
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.storeOp = target.store == StoreOp_Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
		};

		switch( target.load ) {
			case LoadOp_DontCare: depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; break;
			case LoadOp_Load: depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; break;
			case LoadOp_Clear:
				depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depth_attachment.clearValue.depthStencil.depth = target.clear;
				break;
		}
	}

	// barriers
	// we can pretend n = FragmentShaderOutput_Count * 4 + 1 (in/out X
	// color/depth + swapchain) or we can just hardcode something big enough
	BoundedDynamicArray< VkImageMemoryBarrier2, 8 > image_barriers = { };
	for( PoolHandle< Texture > texture : config.attachment_transitions ) {
		image_barriers.must_add( MakeImageTransition( texture, true ) );
	}

	for( PoolHandle< Texture > texture : config.readonly_transitions ) {
		image_barriers.must_add( MakeImageTransition( texture, false ) );
	}

	if( config.swapchain_attachment_transition ) {
		image_barriers.must_add( MakeImageTransition( swapchain.image, 1, 1, false, true ) );
	}

	Barriers( command_buffer, MakeGlobalBarrier( config.barriers ), image_barriers.span() );

	// BeginRendering
	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = { .extent = { *width, *height } },
		.layerCount = 1, // TODO: probably need to fix this for shadowmaps
		.colorAttachmentCount = u32( config.color_targets.n ),
		.pColorAttachments = color_attachments,
		.pDepthAttachment = config.depth_target.exists ? &depth_attachment : NULL,
	};

	vkCmdBeginRendering( command_buffer, &rendering_info );

	// bindings
	RenderPipeline representative_shader = render_pipelines[ config.representative_shader ];
	if( representative_shader.render_pass_push_descriptors.update_template != VK_NULL_HANDLE ) {
		PushDescriptor push_descriptors[ MaxBindings ] = { };
		const ReflectedDescriptorSet & reflection = representative_shader.reflection[ DescriptorSet_RenderPass ];

		for( const BufferBinding buffer : config.bindings.buffers ) {
			Optional< ReflectedDescriptorSet::Binding > b = reflection.bindings.get2( buffer.name );
			if( !b.exists || b->type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
				continue;
			push_descriptors[ b->index ] = PushDescriptor {
				.buffer = {
					.buffer = allocations[ buffer.buffer.allocation ].buffer,
					.offset = buffer.buffer.offset,
					.range = Max2( buffer.buffer.size, size_t( 1 ) ),
				},
			};
		}

		for( const GPUBindings::TextureBinding texture : config.bindings.textures ) {
			Optional< ReflectedDescriptorSet::Binding > b = reflection.bindings.get2( texture.name );
			if( !b.exists || b->type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE )
				continue;
			push_descriptors[ b->index ] = PushDescriptor {
				.image = {
					.imageView = textures[ texture.texture ].backend.unwrap()->image_view,
					.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
				},
			};
		}

		for( const GPUBindings::SamplerBinding sampler : config.bindings.samplers ) {
			Optional< ReflectedDescriptorSet::Binding > b = reflection.bindings.get2( sampler.name );
			if( !b.exists || b->type != VK_DESCRIPTOR_TYPE_SAMPLER )
				continue;
			push_descriptors[ b->index ] = PushDescriptor {
				.sampler = {
					.sampler = samplers[ sampler.sampler ],
				},
			};
		}

		vkCmdPushDescriptorSetWithTemplateKHR( command_buffer, representative_shader.render_pass_push_descriptors.update_template, representative_shader.layout, DescriptorSet_RenderPass, push_descriptors );
	}

	// initial dynamic state
	VkViewport viewport = {
		.y = float( *height ),
		.width = float( *width ),
		// NOTE(mike 20260119): negative viewport height makes NDC Y point downwards to match Metal, see VK_KHR_maintenance1
		.height = -float( *height ),
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport( command_buffer, 0, 1, &viewport );

	vkCmdSetScissor( command_buffer, 0, 1, &no_scissor );

	return CommandBuffer {
		.buffer = command_buffer,
		.is_render_command_buffer = true,
		.wait_on_swapchain_acquire = config.swapchain_attachment_transition,
		.msaa_samples = *msaa,
	};
}

Opaque< CommandBuffer > NewComputePass( const ComputePassConfig & config ) {
	TracyZoneScoped;
	TracyZoneSpan( config.name );

	TempAllocator temp = cls.frame_arena.temp();
	const char * name = temp( "{}", config.name );

	VkCommandBuffer command_buffer = NewCommandBuffer();
	DebugLabel( command_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, name );

	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK( vkBeginCommandBuffer( command_buffer, &begin_info ) );

	{
		VkDebugUtilsLabelEXT label = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = name,
		};

		vkCmdBeginDebugUtilsLabelEXT( command_buffer, &label );
	}

	Barriers( command_buffer, MakeGlobalBarrier( config.barriers ), { } );

	return CommandBuffer {
		.buffer = command_buffer,
	};
}

static VkCommandBuffer PrepareCompute( Opaque< CommandBuffer > ocb, PoolHandle< ComputePipeline > shader, Span< const BufferBinding > buffers ) {
	VkCommandBuffer cb = ocb.unwrap()->buffer;
	const ComputePipeline & pipeline = compute_pipelines[ shader ];

	vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline );

	PushDescriptor push_descriptors[ MaxBufferBindings ] = { };
	for( BufferBinding b : buffers ) {
		Optional< u32 > idx = pipeline.push_descriptors.buffers.get2( b.name );
		if( !idx.exists ) {
			Com_Printf( "can't bind\n" );
			continue;
		}

		push_descriptors[ idx.value ] = {
			.buffer = {
				.buffer = allocations[ b.buffer.allocation ].buffer,
				.offset = b.buffer.offset,
				.range = Max2( b.buffer.size, size_t( 1 ) ),
			},
		};
	}

	vkCmdPushDescriptorSetWithTemplateKHR( cb, pipeline.push_descriptors.update_template, pipeline.layout, DescriptorSetType( 0 ), push_descriptors );

	return cb;
}

void EncodeComputeCall( Opaque< CommandBuffer > ocb, PoolHandle< ComputePipeline > shader, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers ) {
	VkCommandBuffer cb = PrepareCompute( ocb, shader, buffers );
	vkCmdDispatch( cb, x, y, z );
}

void EncodeIndirectComputeCall( Opaque< CommandBuffer > ocb, PoolHandle< ComputePipeline > shader, GPUBuffer indirect_args, Span< const BufferBinding > buffers ) {
	VkCommandBuffer cb = PrepareCompute( ocb, shader, buffers );
	vkCmdDispatchIndirect( cb, allocations[ indirect_args.allocation ].buffer, indirect_args.offset );
}

void SignalFirstRenderPass( RenderPass metal_only_dont_care ) {
}

void SubmitCommandBuffer( Opaque< CommandBuffer > buffer, CommandBufferSubmitType type, Optional< RenderPass > metal_only_dont_care ) {
	TracyZoneScoped;

	CommandBuffer * command_buffer = buffer.unwrap();

	if( command_buffer->is_render_command_buffer ) {
		vkCmdEndRendering( command_buffer->buffer );
	}

	if( type == SubmitCommandBuffer_Present ) {
		Barrier( command_buffer->buffer, VkImageMemoryBarrier2 {
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = global_swapchain.images[ frame.image_index ].image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			},
		} );
	}

	vkCmdEndDebugUtilsLabelEXT( command_buffer->buffer );
	VK_CHECK( vkEndCommandBuffer( command_buffer->buffer ) );

	Swapchain::Semaphores semaphores = global_swapchain.semaphores[ frame_counter % global_swapchain.semaphores.n ];

	VkSemaphoreSubmitInfo present_wait_semaphore = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = semaphores.acquire,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	VkSemaphoreSubmitInfo present_signal_semaphores[] = {
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = semaphores.present,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		},
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = global_device.frame_timeline_semaphore,
			.value = frame_counter + 1,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		},
	};

	const VkCommandBufferSubmitInfo command_buffer_submit_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = command_buffer->buffer,
	};

	VkSubmitInfo2 submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pWaitSemaphoreInfos = &present_wait_semaphore,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &command_buffer_submit_info,
		.pSignalSemaphoreInfos = present_signal_semaphores,
	};

	if( command_buffer->wait_on_swapchain_acquire ) {
		submit_info.waitSemaphoreInfoCount = 1;
	}

	if( type == SubmitCommandBuffer_Present ) {
		submit_info.signalSemaphoreInfoCount = ARRAY_COUNT( present_signal_semaphores );
	}

	VK_CHECK( vkQueueSubmit2( global_device.queue, 1, &submit_info, VK_NULL_HANDLE ) );

	if( type == SubmitCommandBuffer_Wait ) {
		TracyZoneScopedNC( "SubmitCommandBuffer_Wait", 0xff0000 );
		VK_CHECK( vkDeviceWaitIdle( global_device.device ) );
	}
	else if( type == SubmitCommandBuffer_Present ) {
		TracyZoneScopedN( "vkQueuePresentKHR" );
		const VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &semaphores.present,
			.swapchainCount = 1,
			.pSwapchains = &global_swapchain.swapchain,
			.pImageIndices = &frame.image_index,
		};

		VkResult res = vkQueuePresentKHR( global_device.queue, &present_info );
		if( res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR && res != VK_ERROR_OUT_OF_DATE_KHR ) {
			VkAbort( res );
		}
	}
}

void CopyGPUBufferToBuffer(
	Opaque< CommandBuffer > cmd_buf,
	PoolHandle< GPUAllocation > dest, size_t dest_offset,
	PoolHandle< GPUAllocation > src, size_t src_offset,
	size_t n
) {
	VkBufferCopy copy = {
		.srcOffset = src_offset,
		.dstOffset = dest_offset,
		.size = n,
	};

	vkCmdCopyBuffer( cmd_buf.unwrap()->buffer, allocations[ src ].buffer, allocations[ dest ].buffer, 1, &copy );
}

void CopyGPUBufferToTexture(
	Opaque< CommandBuffer > cmd_buf,
	Opaque< BackendTexture > dest, TextureFormat format, u32 w, u32 h, u32 layer, u32 mip_level,
	PoolHandle< GPUAllocation > src, size_t src_offset
) {
	VkBufferImageCopy copy = {
		.bufferOffset = src_offset,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = mip_level,
			.baseArrayLayer = layer,
			.layerCount = 1,
		},
		.imageExtent = {
			.width = w,
			.height = h,
			.depth = 1,
		},
	};

	vkCmdCopyBufferToImage( cmd_buf.unwrap()->buffer, allocations[ src ].buffer, dest.unwrap()->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );
}

static u64 NextPow2( u64 x ) {
	x--;
	x |= x >> 1_u64;
	x |= x >> 2_u64;
	x |= x >> 4_u64;
	x |= x >> 8_u64;
	x |= x >> 16_u64;
	x |= x >> 32_u64;
	return x + 1;
}

static size_t GetGoodGPUAllocationSize() {
	constexpr size_t preferred_size = Megabytes( 128 );
	size_t min_size = global_device.limits.vram / Min2( global_device.limits.max_allocs, size_t( allocations.capacity ) );
	size_t max_size = global_device.limits.max_alloc_size;
	return Clamp( size_t( NextPow2( min_size ) ), preferred_size, max_size );
}

/*
 * Back to normal shit
 */

static void GPUUploadTexture( const TextureConfig & config, Opaque< BackendTexture > texture ) {
	Barrier( global_device.transfer_command_buffer, VkImageMemoryBarrier2 {
		.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = texture.unwrap()->image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = config.num_mipmaps,
			.layerCount = Default( config.num_layers, 1_u32 ),
		},
	} );

	UploadTexture( config, texture );

	Barrier( global_device.transfer_command_buffer, VkImageMemoryBarrier2 {
		.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		.image = texture.unwrap()->image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = config.num_mipmaps,
			.layerCount = Default( config.num_layers, 1_u32 ),
		},
	} );
}

Opaque< BackendTexture > NewBackendTexture( GPUSlabAllocator * a, const TextureConfig & config ) {
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	if( config.data == NULL ) {
		usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		usage |= config.format == TextureFormat_Depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	u32 num_layers = Default( config.num_layers, 1_u32 );

	const VkImageCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = TextureFormatToVulkan( config.format ),
		.extent = {
			.width = config.width,
			.height = config.height,
			.depth = 1,
		},
		.mipLevels = config.num_mipmaps,
		.arrayLayers = num_layers,
		.samples = VkSampleCountFlagBits( config.msaa_samples ),
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VkImage image;
	VK_CHECK( vkCreateImage( global_device.device, &image_info, NULL, &image ) );

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements( global_device.device, image, &memory_requirements );

	Optional< GPUAllocation > dedicated_allocation = NONE;
	if( config.dedicated_allocation ) {
		dedicated_allocation = GPUMalloc( memory_requirements.size, true );
		VK_CHECK( vkBindImageMemory( global_device.device, image, dedicated_allocation->memory, 0 ) );
	}
	else {
		GPUBuffer alloc = NewBuffer( a, "texture", memory_requirements.size, memory_requirements.alignment, true );
		VK_CHECK( vkBindImageMemory( global_device.device, image, allocations[ alloc.allocation ].memory, alloc.offset ) );
	}

	VkComponentMapping swizzle = { };
	if( !HasAnyBit( usage, VkImageUsageFlags( VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) ) ) {
		switch( config.format ) {
			default: break;

			case TextureFormat_R_U8:
			case TextureFormat_R_U8_sRGB:
			case TextureFormat_R_S8:
			case TextureFormat_R_UI8:
				 swizzle = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE };
				 break;

			case TextureFormat_A_U8:
			case TextureFormat_BC4:
				 swizzle = { VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_R };
				 break;

			case TextureFormat_RA_U8:
				 swizzle = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G };
				 break;
		}
	}

	VkImageAspectFlags aspect_mask = config.format == TextureFormat_Depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageViewCreateInfo image_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = config.num_layers.exists ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D ,
		.format = TextureFormatToVulkan( config.format ),
		.components = swizzle,
		.subresourceRange = {
			.aspectMask = aspect_mask,
			.levelCount = config.num_mipmaps,
			.layerCount = num_layers,
		},
	};

	VkImageView image_view;
	VK_CHECK( vkCreateImageView( global_device.device, &image_view_info, NULL, &image_view ) );

	Span< VkImageView > per_layer_image_views = AllocSpan< VkImageView >( sys_allocator, num_layers );
	for( u32 i = 0; i < num_layers; i++ ) {
		VkImageViewCreateInfo layer_image_view_info = image_view_info;
		layer_image_view_info.subresourceRange.baseArrayLayer = i;
		layer_image_view_info.subresourceRange.layerCount = 1;
		VK_CHECK( vkCreateImageView( global_device.device, &layer_image_view_info, NULL, &per_layer_image_views[ i ] ) );
	}

	TempAllocator temp = cls.frame_arena.temp();
	DebugLabel( image, VK_OBJECT_TYPE_IMAGE, temp( "{}", config.name ) );
	DebugLabel( image_view, VK_OBJECT_TYPE_IMAGE_VIEW, temp( "{}", config.name ) );

	BackendTexture texture = {
		.image = image,
		.image_view = image_view,
		.per_layer_image_views = per_layer_image_views,
		.allocation = dedicated_allocation,
	};

	if( config.data != NULL ) {
		GPUUploadTexture( config, texture );
	}

	return texture;
}

void DeleteDedicatedAllocationTexture( Opaque< BackendTexture > texture ) {
	Assert( texture.unwrap()->allocation.exists );
	DeleteTexture( *texture.unwrap() );
}

enum SamplerWrap : u8 {
	SamplerWrap_Repeat,
	SamplerWrap_Clamp,
};

struct SamplerConfig {
	const char * name;
	SamplerWrap wrap = SamplerWrap_Repeat;
	bool filter = true;
	bool shadowmap_sampler = false;
};

static VkSamplerAddressMode SamplerWrapToVulkan( SamplerWrap wrap ) {
	switch( wrap ) {
		case SamplerWrap_Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case SamplerWrap_Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}
}

static VkSampler NewSampler( const SamplerConfig & config ) {
	VkFilter filter = config.filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	const VkSamplerCreateInfo sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = filter,
		.minFilter = filter,
		.mipmapMode = config.filter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = SamplerWrapToVulkan( config.wrap ),
		.addressModeV = SamplerWrapToVulkan( config.wrap ),
		.addressModeW = SamplerWrapToVulkan( config.wrap ),
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 16.0f,
		.compareEnable = config.shadowmap_sampler,
		.compareOp = VK_COMPARE_OP_LESS,
	};

	VkSampler sampler;
	VK_CHECK( vkCreateSampler( global_device.device, &sampler_info, NULL, &sampler ) );
	DebugLabel( sampler, VK_OBJECT_TYPE_SAMPLER, config.name );

	return sampler;
}

static void DeleteSampler( VkSampler sampler ) {
	vkDestroySampler( global_device.device, sampler, NULL );
}

static void CreateSamplers() { // NOMERGE: move this into generic I guess
	samplers[ Sampler_Standard ] = NewSampler( SamplerConfig { .name = "Standard" } );
	samplers[ Sampler_Clamp ] = NewSampler( SamplerConfig { .name = "Clamp", .wrap = SamplerWrap_Clamp } );
	samplers[ Sampler_Unfiltered ] = NewSampler( SamplerConfig { .name = "Unfiltered", .filter = false } );
	samplers[ Sampler_Shadowmap ] = NewSampler( SamplerConfig { .name = "Shadowmap", .shadowmap_sampler = true } );
}

PoolHandle< BindGroup > NewMaterialBindGroup( const char * name, Opaque< BackendTexture > texture, SamplerType sampler, GPUBuffer properties ) {
	TempAllocator temp = cls.frame_arena.temp();

	const VkDescriptorSetAllocateInfo descriptor_set_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = global_device.descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &global_device.material_descriptor_set_layout,
	};

	VkDescriptorSet descriptor_set;
	VK_CHECK( vkAllocateDescriptorSets( global_device.device, &descriptor_set_info, &descriptor_set ) );
	DebugLabel( descriptor_set, VK_OBJECT_TYPE_DESCRIPTOR_SET, temp( "{} bind group", name ) );

	const VkDescriptorBufferInfo descriptor_buffer_info = {
		.buffer = allocations[ properties.allocation ].buffer,
		.offset = properties.offset,
		.range = properties.size,
	};

	const VkDescriptorImageInfo descriptor_image_info = {
		.imageView = texture.unwrap()->image_view,
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
	};

	const VkDescriptorImageInfo descriptor_sampler_info = {
		.sampler = samplers[ sampler ],
	};

	const VkWriteDescriptorSet descriptor_writes[] = {
		VkWriteDescriptorSet {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &descriptor_buffer_info,
		},

		VkWriteDescriptorSet {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = &descriptor_image_info,
		},

		VkWriteDescriptorSet {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.pImageInfo = &descriptor_sampler_info,
		},
	};

	vkUpdateDescriptorSets( global_device.device, ARRAY_COUNT( descriptor_writes ), descriptor_writes, 0, NULL );

	return bind_groups.allocate( { descriptor_set } );
}

void InitRenderBackend( SDL_Window * window ) {
	VK_CHECK( volkInitialize() );
	PrintExtensions();
	global_instance = createInstance();
	Assert( global_instance );
	volkLoadInstanceOnly( global_instance );

	global_debug_callback = VK_NULL_HANDLE;
	if( IFDEF( ENABLE_VALIDATION_LAYERS ) ) {
		global_debug_callback = RegisterDebugCallback( global_instance );
	}

	VulkanDevice device = CreateDevice( global_instance );
	global_device = device;

	if( !SDL_Vulkan_CreateSurface( window, global_instance, NULL, &global_surface ) ) {
		Fatal( "SDL_Vulkan_CreateSurface: %s", SDL_GetError() );
	}

	global_swapchain = CreateSwapchain( device, global_surface, NONE );
	frame_counter = 0;

	Assert( device.limits.ssbo_range >= ArenaAllocatorSize );
	InitGPUAllocators( GetGoodGPUAllocationSize(), global_device.limits.ssbo_alignment, global_device.limits.buffer_image_granularity );

	CreateSamplers();
}

void ShutdownRenderBackend() {
	VK_CHECK( vkDeviceWaitIdle( global_device.device ) );

	for( VkSampler sampler : samplers ) {
		DeleteSampler( sampler );
	}
	for( Texture texture : textures.span() ) {
		if( !texture.dummy_slot_for_missing_texture ) {
			DeleteTexture( *texture.backend.unwrap() );
		}
	}
	for( GPUAllocation allocation : allocations ) {
		GPUFree( allocation );
	}

	for( RenderPipeline shader : render_pipelines ) {
		DeleteRenderPipeline( shader );
	}

	for( ComputePipeline shader : compute_pipelines ) {
		DeleteComputePipeline( shader );
	}

	vkDestroyDescriptorPool( global_device.device, global_device.descriptor_pool, NULL );

	ShutdownGPUAllocators();

	vkDestroySemaphore( global_device.device, global_device.frame_timeline_semaphore, NULL );
	DeleteSwapchain( global_swapchain );
	vkDestroySurfaceKHR( global_instance, global_surface, NULL );

	vkDestroyCommandPool( global_device.device, global_device.transfer_command_pool, NULL );

	vkDestroyDescriptorSetLayout( global_device.device, global_device.material_descriptor_set_layout, NULL );

	for( VulkanDevice::CommandPool & pool : global_device.command_pools ) {
		vkDestroyCommandPool( global_device.device, pool.pool, NULL );
		pool.free_list.shutdown();
	}

	vkDestroyDevice( global_device.device, NULL );
	if( global_debug_callback != VK_NULL_HANDLE ) {
		vkDestroyDebugReportCallbackEXT( global_instance, global_debug_callback, NULL );
	}
	vkDestroyInstance( global_instance, NULL );
	volkFinalize();
}

void RenderBackendWaitForNewFrame() {
	TracyZoneScopedNC( "RenderBackendWaitForNewFrame", 0xff0000 );

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &global_device.frame_timeline_semaphore,
		.pValues = &frame_counter,
	};

	VK_CHECK( vkWaitSemaphores( global_device.device, &wait_info, U64_MAX ) );
}

void RenderBackendBeginFrame( int frames_to_capture ) {
	TracyZoneScoped;

	VkSurfaceCapabilitiesKHR surface_capabilities;
	VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( global_device.physical_device, global_surface, &surface_capabilities ) );
	MaybeRecreateSwapchain( surface_capabilities );

	{
		TracyZoneScopedN( "vkResetCommandPool" );
		VK_CHECK( vkResetCommandPool( global_device.device, global_device.command_pools[ FrameSlot() ].pool, 0 ) );
	}

	{
		TracyZoneScopedNC( "vkAcquireNextImageKHR", 0xff0000 );
		Swapchain::Semaphores semaphores = global_swapchain.semaphores[ frame_counter % global_swapchain.semaphores.n ];

		VkResult res = vkAcquireNextImageKHR( global_device.device, global_swapchain.swapchain, U64_MAX, semaphores.acquire, VK_NULL_HANDLE, &frame.image_index );
		if( res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR ) {
			VkAbort( res );
		}
	}

	ClearGPUArenaAllocators();
}

void RenderBackendEndFrame() {
	frame_counter++;
}

u32 RenderBackendSupportedMSAA() {
	return global_device.limits.msaa;
}

size_t FrameSlot() {
	return frame_counter % MaxFramesInFlight;
}

#endif // #if PLATFORM_MACOS
