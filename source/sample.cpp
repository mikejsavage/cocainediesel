#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/time.h"
#include "qcommon/hashmap.h"
#include "qcommon/threadpool.h"
#include "qcommon/version.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/sdl_window.h"
#include "client/vid.h"
#include "client/renderer/api.h"
#include "client/renderer/private.h"

#define SDL_MAIN_USE_CALLBACKS
#include "sdl/SDL3/SDL_audio.h"
#include "sdl/SDL3/SDL_events.h"
#include "sdl/SDL3/SDL_init.h"
#include "sdl/SDL3/SDL_main.h"
#include "sdl/SDL3/SDL_mouse.h"

static int capture_frames;

static Mat4 PerspectiveProjection( float vertical_fov_degrees, float aspect_ratio, float near_plane ) {
	float tan_half_vertical_fov = tanf( Radians( vertical_fov_degrees ) / 2.0f );
	constexpr float epsilon = 4.8e-7f; // http://www.terathon.com/gdc07_lengyel.pdf

	return Mat4(
		1.0f / ( tan_half_vertical_fov * aspect_ratio ),
		0.0f,
		0.0f,
		0.0f,

		0.0f,
		1.0f / tan_half_vertical_fov,
		0.0f,
		0.0f,

		0.0f,
		0.0f,
		epsilon - 1.0f,
		( epsilon - 2.0f ) * near_plane,

		0.0f,
		0.0f,
		-1.0f,
		0.0f
	);
}

constexpr Mat4 Mat4TranslationLmao( float x, float y, float z ) {
	return Mat4(
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

constexpr Mat4 Mat4TranslationLmao( Vec3 v ) {
	return Mat4TranslationLmao( v.x, v.y, v.z );
}

static Mat4 ViewMatrix( Vec3 position, Vec3 forward ) {
	Vec3 right, up;
	ViewVectors( forward, &right, &up );
	Mat4 rotation(
		right.x, right.y, right.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		-forward.x, -forward.y, -forward.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	return rotation * Mat4TranslationLmao( -position );
}

PoolHandle< Texture > UploadBC4( const char * path ) {
	Span< u8 > dds = ReadFileBinary( sys_allocator, path );
	defer { Free( sys_allocator, dds.ptr ); };

	Assert( dds.num_bytes() >= sizeof( DDSHeader ) );

	const DDSHeader * header = align_cast< const DDSHeader >( dds.ptr );
	return NewTexture( TextureConfig {
		.name = MakeSpan( path ),
		.format = TextureFormat_BC4,
		.width = header->width,
		.height = header->height,
		.num_mipmaps = header->mipmap_count,
		.data = ( dds + sizeof( DDSHeader ) ).ptr,
	} );
}

struct Vertex3 {
	Vec3 position;
	Vec3 normal;
	Vec3 color;
	Vec2 uv;
};

inline HashPool< Texture, MaxMaterials > textures; // NOTE(mike): this is inline so the backends can read it

static int framebuffer_width;
static int framebuffer_height;
static BoundedDynamicArray< Vertex3, 6 * 4 > vertices;
static PoolHandle< Texture > depth_framebuffer;
static PoolHandle< Texture > offscreen_framebuffer;
static PoolHandle< ComputePipeline > compute_shader;
static PoolHandle< RenderPipeline > shader;
static PoolHandle< RenderPipeline > always_white;
static PoolHandle< BindGroup > material_bind_group;
static Mesh mesh;

template< typename T >
GPUBuffer NewBuffer( const char * label, size_t alignment = alignof( T ) ) {
	return NewBuffer( label, sizeof( T ), alignment, false, NULL );
}

SDL_AppResult SDL_AppInit( void ** appstate, int argc, char ** argv ) {
	CreateAutoreleasePoolOnMacOS();
	InitTime();

	constexpr size_t frame_arena_size = Megabytes( 1 );
	void * frame_arena_memory = sys_allocator->allocate( frame_arena_size, 16 );
	cls.frame_arena = ArenaAllocator( frame_arena_memory, frame_arena_size );

	TempAllocator temp = cls.frame_arena.temp();
	InitFS();
	InitThreadPool();
	InitAssets( &temp );

	SDL_SetAppMetadata( APPLICATION, APP_VERSION, "fun.cocainediesel" );
	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD );

	CreateWindow( WindowMode {
		.video_mode = { 640, 480 },
		.fullscreen = FullscreenMode_Windowed,
	} );

	InitRenderBackend( sdl_window );

	constexpr float s = 0.5f;
	constexpr Vec3 normal_x = Vec3( 1.0f, 0.0f, 0.0f );
	constexpr Vec3 normal_y = Vec3( 0.0f, 1.0f, 0.0f );
	constexpr Vec3 normal_z = Vec3( 0.0f, 0.0f, 1.0f );

	vertices.clear();

	vertices.must_add( { Vec3( -s, -s, +s ), normal_z } );
	vertices.must_add( { Vec3( +s, -s, +s ), normal_z } );
	vertices.must_add( { Vec3( +s, +s, +s ), normal_z } );
	vertices.must_add( { Vec3( -s, +s, +s ), normal_z } );

	vertices.must_add( { Vec3( +s, -s, +s ), normal_x } );
	vertices.must_add( { Vec3( +s, -s, -s ), normal_x } );
	vertices.must_add( { Vec3( +s, +s, -s ), normal_x } );
	vertices.must_add( { Vec3( +s, +s, +s ), normal_x } );

	vertices.must_add( { Vec3( +s, -s, -s ), -normal_z } );
	vertices.must_add( { Vec3( -s, -s, -s ), -normal_z } );
	vertices.must_add( { Vec3( -s, +s, -s ), -normal_z } );
	vertices.must_add( { Vec3( +s, +s, -s ), -normal_z } );

	vertices.must_add( { Vec3( -s, -s, -s ), -normal_x } );
	vertices.must_add( { Vec3( -s, -s, +s ), -normal_x } );
	vertices.must_add( { Vec3( -s, +s, +s ), -normal_x } );
	vertices.must_add( { Vec3( -s, +s, -s ), -normal_x } );

	vertices.must_add( { Vec3( -s, +s, +s ), normal_y } );
	vertices.must_add( { Vec3( +s, +s, +s ), normal_y } );
	vertices.must_add( { Vec3( +s, +s, -s ), normal_y } );
	vertices.must_add( { Vec3( -s, +s, -s ), normal_y } );

	vertices.must_add( { Vec3( -s, -s, -s ), -normal_y } );
	vertices.must_add( { Vec3( +s, -s, -s ), -normal_y } );
	vertices.must_add( { Vec3( +s, -s, +s ), -normal_y } );
	vertices.must_add( { Vec3( -s, -s, +s ), -normal_y } );

	for( Vertex3 & v : vertices ) {
		v.color = v.position + 0.5f;
	}

	constexpr Vec2 uvs[] = {
		Vec2( 0.0f, 0.0f ),
		Vec2( 1.0f, 0.0f ),
		Vec2( 1.0f, 1.0f ),
		Vec2( 0.0f, 1.0f ),
	};

	for( size_t i = 0; i < ARRAY_COUNT( vertices ); i++ ) {
		vertices[ i ].uv = uvs[ i % ARRAY_COUNT( uvs ) ];
	}

	constexpr u16 indices[] = {
		0,  1,  2,  2,  3,  0,  /* front */
		4,  5,  6,  6,  7,  4,  /* right */
		8,  9,  10, 10, 11, 8,  /* back */
		12, 13, 14, 14, 15, 12, /* left */
		16, 17, 18, 18, 19, 16, /* top */
		20, 21, 22, 22, 23, 20, /* bottom */
	};

	GPUBuffer vertex_buffer = NewBuffer( "cube vertices", vertices );
	GPUBuffer index_buffer = NewBuffer( "cube indices", indices );

	VertexDescriptor vertex_descriptor = { .buffer_strides = { sizeof( Vertex3 ) } };
	vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( Vertex3, position ) };
	vertex_descriptor.attributes[ VertexAttribute_Normal ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( Vertex3, normal ) };
	vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( Vertex3, uv ) };
	vertex_descriptor.attributes[ VertexAttribute_Color ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( Vertex3, color ) };

	mesh = {
		.vertex_descriptor = vertex_descriptor,
		.index_format = IndexFormat_U16,
		.num_vertices = ARRAY_COUNT( indices ),
		.vertex_buffers = { vertex_buffer },
		.index_buffer = index_buffer,
	};

	compute_shader = NewComputePipeline( "shaders/compute_shader" );
	shader = NewRenderPipeline( RenderPipelineConfig {
		.path = "shaders/shader",
		.output_format = { .colors = { TextureFormat_Swapchain }, .has_depth = true },
		.blend_func = BlendFunc_Blend,
		.mesh_variants = { mesh.vertex_descriptor /*, carf_mesh.vertex_descriptor*/ },
	} );
	always_white = NewRenderPipeline( RenderPipelineConfig {
		.path = "shaders/always_white",
		.output_format = { .colors = { TextureFormat_RGBA_U8 } },
		.mesh_variants = { { } },
	} );

	GetFramebufferSize( &framebuffer_width, &framebuffer_height );

	depth_framebuffer = NewTexture( TextureConfig {
		.name = "Depth framebuffer",
		.format = TextureFormat_Depth,
		.width = u32( framebuffer_width ),
		.height = u32( framebuffer_height ),
		.dedicated_allocation = true,
	} );
	offscreen_framebuffer = NewTexture( TextureConfig {
		.name = "Offscreen 1x1",
		.format = TextureFormat_RGBA_U8,
		.width = 1,
		.height = 1,
	} );

	PoolHandle< Texture > texture = UploadBC4( "respectthenest.dds" );
	GPUBuffer properties = NewBuffer( "respectthenest.dds properties", MaterialProperties { .shininess = 1.0f } );
	material_bind_group = NewMaterialBindGroup( "respectthenest.dds", textures[ texture ].backend, Sampler_Standard, properties );

	FlushStagingBuffer();

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent( void * appstate, SDL_Event * event ) {
	switch( event->type ) {
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;

		case SDL_EVENT_KEY_DOWN:
			if( event->key.key >= SDLK_F1 && event->key.key <= SDLK_F9 )
				capture_frames = event->key.key - SDLK_F1 + 1;
			if( event->key.key == SDLK_Q || event->key.key == SDLK_ESCAPE )
				return SDL_APP_SUCCESS;
			break;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate( void * appstate ) {
	RenderBackendWaitForNewFrame();

	Time t = Now();

	RenderBackendBeginFrame( capture_frames );
	capture_frames = 0;

	GPUBuffer render_pass_data = NewDeviceTempBuffer< Vec4 >( "render pass uniforms" );

	{
		Opaque< CommandBuffer > compute_pass = NewComputePass( ComputePassConfig {
			.name = "GG",
			.pass = RenderPass( 0 ),
		} );
		EncodeComputeCall( compute_pass, compute_shader, 1, 1, 1, {
			{ "u_Time", NewTempBuffer( Sawtooth01( t, Hours( 1 ) ) ) },
			{ "u_RenderPass", render_pass_data },
		} );
		SubmitCommandBuffer( compute_pass, SubmitCommandBuffer_Normal, RenderPass( 1 ) );
	}

	{
		Opaque< CommandBuffer > offscreen_pass = NewRenderPass( RenderPassConfig {
			.name = "Offscreen",
			.pass = RenderPass( 1 ),
			.color_targets = {
				RenderPassConfig::ColorTarget { .texture = offscreen_framebuffer, .store = StoreOp_Store },
			},
			.attachment_transitions = { offscreen_framebuffer },
			.representative_shader = always_white,
		} );

		EncodeDrawCall( offscreen_pass, { .shader = always_white }, Mesh { .num_vertices = 3 }, { }, DrawCallExtras { } );
		SubmitCommandBuffer( offscreen_pass, SubmitCommandBuffer_Normal, RenderPass( 2 ) );
	}

	{
		Opaque< CommandBuffer > cb = NewRenderPass( RenderPassConfig {
			.name = "main",
			.pass = RenderPass( 2 ),
			.color_targets = {
				RenderPassConfig::ColorTarget {
					.load = LoadOp_Clear,
					.clear = Vec4( 1.0f, 0.0f, 0.0f, 0.0f ),
					.store = StoreOp_Store,
				},
			},
			.depth_target = RenderPassConfig::DepthTarget {
				.texture = depth_framebuffer,
				.load = LoadOp_Clear,
				.clear = 1.0f,
				.store = StoreOp_Store,
			},
			.barrier = GPUBarrier_FragmentToFragmentSample, // TODO: also needs frag2fragwrite
			.attachment_transitions = { depth_framebuffer },
			.readonly_transitions = { offscreen_framebuffer },
			.swapchain_attachment_transition = true,
			.representative_shader = shader,
			.bindings = {
				.buffers = { { "u_RenderPass", render_pass_data } },
				.textures = { { "u_White", offscreen_framebuffer } },
			},
		} );

		Vec3 camera_pos = Vec3( 1.5f * Vec2( Cos( t, Seconds( 4 ) ), Sin( t, Seconds( 4 ) ) ), 1.0f /*500.0f*/ );

		struct ViewData {
			Mat4 camera_from_world;
			Mat4 clip_from_camera;
		};

		GPUBuffer view_data = NewTempBuffer( ViewData {
			.camera_from_world = ViewMatrix( camera_pos, Normalize( -camera_pos ) ),
			// .camera_from_world = ViewMatrix( camera_pos, Normalize( Vec3( -camera_pos.xy(), 0.0f ) ) ),
			.clip_from_camera = PerspectiveProjection( 100.0f, float( framebuffer_width ) / float( framebuffer_height ), 0.001f ),
		} );

		PipelineState pipeline = {
			.shader = shader,
			.dynamic_state = {
				.depth_func = DepthFunc_Less,
			},
			.material_bind_group = material_bind_group,
		};
		EncodeDrawCall( cb, pipeline, mesh, { { "u_ViewData", view_data } }, DrawCallExtras { } );
		SubmitCommandBuffer( cb, SubmitCommandBuffer_Present, RenderPass_Count );
	}

	RenderBackendEndFrame();

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit( void * appstate, SDL_AppResult result ) {
	ShutdownRenderBackend();
	DestroyWindow();
	ShutdownAssets();
	ShutdownThreadPool();
	ShutdownFS();
	ReleaseAutoreleasePoolOnMacOS();
	Free( sys_allocator, cls.frame_arena.get_memory() );
	SDL_Quit();
}
