#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/time.h"
#include "client/client.h"
#include "client/gltf.h"
#include "client/renderer/renderer.h"
#include "client/renderer/blue_noise.h"
#include "client/renderer/cdmap.h"
#include "client/renderer/gltf.h"
#include "client/renderer/skybox.h"
#include "client/renderer/text.h"

#include "client/maps.h"
#include "cgame/cg_particles.h"

#include "imgui/imgui.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

FrameStatic frame_static;
static u64 frame_counter;

static PoolHandle< Texture > blue_noise;

static Mesh fullscreen_mesh;

struct DynamicGeometry {
	static constexpr size_t BufferSize = 1024 * 1024; // 1MB

	StreamingBuffer buffer;
	size_t cursor;
};

static DynamicGeometry dynamic_geometry;

static char last_screenshot_date[ 256 ];
static int same_date_count;

static u32 last_viewport_width, last_viewport_height;
static int last_msaa;
static ShadowQuality last_shadow_quality;

static Cvar * r_samples;
static Cvar * r_shadow_quality;

static void TakeScreenshot() {
	RGB8 * framebuffer = AllocMany< RGB8 >( sys_allocator, frame_static.viewport_width * frame_static.viewport_height );
	defer { Free( sys_allocator, framebuffer ); };
	DownloadFramebuffer( framebuffer );

	stbi_flip_vertically_on_write( 1 );

	int ok = stbi_write_png_to_func( []( void * context, void * png, int png_size ) {
		char date[ 256 ];
		FormatCurrentTime( date, sizeof( date ), "%y%m%d_%H%M%S" );

		TempAllocator temp = cls.frame_arena.temp();

		DynamicString path( &temp, "{}/screenshots/{}", HomeDirPath(), date );

		if( StrEqual( date, last_screenshot_date ) ) {
			same_date_count++;
			path.append( "_{}", same_date_count );
		}
		else {
			same_date_count = 0;
		}
		strcpy( last_screenshot_date, date );

		path.append( ".png" );

		if( WriteFile( &temp, path.c_str(), png, png_size ) ) {
			Com_Printf( "Wrote %s\n", path.c_str() );
		}
		else {
			Com_Printf( "Couldn't write %s\n", path.c_str() );
		}
	}, NULL, frame_static.viewport_width, frame_static.viewport_height, 3, framebuffer, 0 );

	if( ok == 0 ) {
		Com_Printf( "Couldn't convert screenshot to PNG\n" );
	}
}

const char * ShadowQualityToString( ShadowQuality mode ) {
	switch( mode ) {
		case ShadowQuality_Low: return "Rookie";
		case ShadowQuality_Medium: return "Regular";
		case ShadowQuality_High: return "Made Man";
		case ShadowQuality_Ultra: return "Murica";
		default: return "";
	};
}

static ShadowParameters GetShadowParameters( ShadowQuality mode ) {
	switch( mode ) {
		case ShadowQuality_Low:    return { 2, { 256.0f, 2048.0f, 2048.0f, 2048.0f }, 1024, 2 };
		case ShadowQuality_Medium: return { 4, { 256.0f, 768.0f, 2304.0f, 6912.0f }, 1024, 2 };
		case ShadowQuality_High:   return { 4, { 256.0f, 768.0f, 2304.0f, 6912.0f }, 2048, 4 };
		case ShadowQuality_Ultra:  return { 4, { 256.0f, 768.0f, 2304.0f, 6912.0f }, 4096, 4 };
	}

	Assert( false );
	return { };
}

void InitRenderer() {
	TracyZoneScoped;

	InitRenderBackend();

	TempAllocator temp = cls.frame_arena.temp();

	r_samples = NewCvar( "r_samples", "0", CvarFlag_Archive );
	r_shadow_quality = NewCvar( "r_shadow_quality", temp.sv( "{}", ShadowQuality_Ultra ), CvarFlag_Archive );

	frame_static = { };
	frame_counter = 0;
	last_viewport_width = 0;
	last_viewport_height = 0;
	last_msaa = 0;
	last_shadow_quality = ShadowQuality_Count;

	{
		int w, h;
		u8 * img = stbi_load_from_memory( blue_noise_png, blue_noise_png_len, &w, &h, NULL, 1 );
		Assert( img != NULL );

		blue_noise = NewTexture( TextureConfig {
			.format = TextureFormat_R_S8,
			.width = u32( w ),
			.height = u32( h ),
			.data = img,
		} );

		stbi_image_free( img );
	}

	{
		constexpr Vec3 positions[] = {
			Vec3( -1.0f, -1.0f, 0.0f ),
			Vec3( 3.0f, -1.0f, 0.0f ),
			Vec3( -1.0f, 3.0f, 0.0f ),
		};

		MeshConfig config = { };
		config.name = "Fullscreen triangle";
		config.set_attribute( VertexAttribute_Position, NewGPUBuffer( positions, sizeof( positions ), "Fullscreen triangle vertices" ) );
		config.num_vertices = 3;
		fullscreen_mesh = NewMesh( config );
	}

	dynamic_geometry.buffer = NewStreamingBuffer( DynamicGeometry::BufferSize, "Dynamic geometry buffer" );

	AddCommand( "screenshot", []( const Tokenized & args ) { TakeScreenshot(); } );
	strcpy( last_screenshot_date, "" );
	same_date_count = 0;

	InitShaders();
	InitMaterials();
	InitText();
	InitSkybox();
	InitVisualEffects();
}

void ShutdownRenderer() {
	TracyZoneScoped;

	FlushRenderBackend();

	ShutdownMaterials();

	RemoveCommand( "screenshot" );

	ShutdownRenderBackend();
}

static Mat4 OrthographicProjection( float left, float top, float right, float bottom, float near_plane, float far_plane ) {
	return Mat4(
		2.0f / ( right - left ),
		0.0f,
		0.0f,
		-( right + left ) / ( right - left ),

		0.0f,
		2.0f / ( top - bottom ),
		0.0f,
		-( top + bottom ) / ( top - bottom ),

		0.0f,
		0.0f,
		-2.0f / ( far_plane - near_plane ),
		-( far_plane + near_plane ) / ( far_plane - near_plane ),

		0.0f,
		0.0f,
		0.0f,
		1.0f
	);
}

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

static Mat4 InvertPerspectiveProjection( const Mat4 & P ) {
	float a = P.col0.x;
	float b = P.col1.y;
	float c = P.col2.z;
	float d = P.col3.z;
	float e = P.col2.w;

	return Mat4(
		1.0f / a, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / b, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f / e,
		0.0f, 0.0f, 1.0f / d, -c / ( d * e )
	);
}

static Mat3x4 ViewMatrix( Vec3 position, EulerDegrees3 angles ) {
	float pitch = Radians( angles.pitch );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	float yaw = Radians( angles.yaw );
	float sy = sinf( yaw );
	float cy = cosf( yaw );
	float roll = Radians( angles.roll );
	float sr = sinf( roll );
	float cr = cosf( roll );

	Vec3 forward = Vec3( cp * cy, cp * sy, -sp );
	Vec3 right = Vec3(
		sy * cr - sp * cy * sr,
		-cy * cr - sp * sy * sr,
		-cp * sr
	);
	Vec3 up = Vec3(
		sy * sr + sp * cy * cr,
		-cy * sr + sp * sy * cr,
		cp * cr
	);

	Mat3x4 rotation(
		right.x, right.y, right.z, 0,
		up.x, up.y, up.z, 0,
		-forward.x, -forward.y, -forward.z, 0
	);
	return rotation * Mat4Translation( -position );
}

static Mat3x4 ViewMatrix( Vec3 position, Vec3 forward ) {
	Vec3 right, up;
	ViewVectors( forward, &right, &up );
	Mat3x4 rotation(
		right.x, right.y, right.z, 0,
		up.x, up.y, up.z, 0,
		-forward.x, -forward.y, -forward.z, 0
	);
	return rotation * Mat4Translation( -position );
}

static Mat3x4 InvertViewMatrix( const Mat3x4 & V, Vec3 position ) {
	return Mat3x4(
		// transpose rotation part
		Vec3( V.row0().xyz() ),
		Vec3( V.row1().xyz() ),
		Vec3( V.row2().xyz() ),

		Vec3( position )
	);
}

static UniformBlock UploadViewUniforms( const Mat3x4 & V, const Mat3x4 & inverse_V, const Mat4 & P, const Mat4 & inverse_P, Vec3 camera_pos, Vec2 viewport_size, float near_plane, int samples, Vec3 light_dir ) {
	return UploadUniformBlock( V, inverse_V, P, inverse_P, camera_pos, viewport_size, near_plane, samples, light_dir );
}

static void CreateRenderTargets() {
	// TODO: save old so texture handles can be recycled
	frame_static.render_targets.silhouette_mask = NewTexture( GPULifetime_Framebuffer, TextureConfig {
		.format = TextureFormat_RGBA_U8_sRGB,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
	}, frame_static.render_targets.silhouette_mask );

	frame_static.render_targets.curved_surface_mask = NewTexture( GPULifetime_Framebuffer, TextureConfig {
		.format = TextureFormat_R_UI8,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
		.msaa_samples = frame_static.msaa_samples,
	}, frame_static.render_targets.curved_surface_mask );

	frame_static.render_targets.depth = NewTexture( GPULifetime_Framebuffer, TextureConfig {
		.format = TextureFormat_Depth,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
		.msaa_samples = frame_static.msaa_samples,
	}, frame_static.render_targets.depth );

	if( frame_static.msaa_samples > 1 ) {
		frame_static.render_targets.msaa_color = NewTexture( GPULifetime_Framebuffer, TextureConfig {
			.format = TextureFormat_RGBA_U8_sRGB,
			.width = frame_static.viewport_width,
			.height = frame_static.viewport_height,
			.msaa_samples = frame_static.msaa_samples,
		}, frame_static.render_targets.msaa_color );
	}
	else {
		frame_static.render_targets.msaa_color = NONE;
	}

	for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
		frame_static.render_targets.shadowmaps[ i ] = NewTexture( GPULifetime_Framebuffer, TextureConfig {
			.format = TextureFormat_Depth,
			.width = frame_static.shadow_parameters.resolution,
			.height = frame_static.shadow_parameters.resolution,
			.num_layers = frame_static.shadow_parameters.num_cascades,
		}, frame_static.render_targets.shadowmaps[ i ] );
	}
}

namespace tracy {
struct SourceLocationData {
	const char * name;
	const char * function;
	const char * file;
	uint32_t line;
	uint32_t color;
};
}

void RendererBeginFrame( u32 viewport_width, u32 viewport_height ) {
	HotloadShaders();
	HotloadMaterials();
	HotloadGLTFModels();
	HotloadMaps();
	HotloadVisualEffects();

	RenderBackendBeginFrame();

	dynamic_geometry.cursor = 0;

	if( !IsPowerOf2( r_samples->integer ) || r_samples->integer > 16 || r_samples->integer == 1 ) {
		Com_Printf( "Invalid r_samples value (%d), resetting\n", r_samples->integer );
		Cvar_Set( "r_samples", r_samples->default_value );
	}

	if( r_shadow_quality->integer < ShadowQuality_Low || r_shadow_quality->integer > ShadowQuality_Ultra ) {
		Com_Printf( "Invalid r_shadow_quality value (%d), resetting\n", r_shadow_quality->integer );
		Cvar_Set( "r_shadow_quality", r_shadow_quality->default_value );
	}

	frame_static.viewport_width = Max2( u32( 1 ), viewport_width );
	frame_static.viewport_height = Max2( u32( 1 ), viewport_height );
	frame_static.viewport = Vec2( frame_static.viewport_width, frame_static.viewport_height );
	frame_static.viewport_resized = frame_static.viewport_width != last_viewport_width || frame_static.viewport_height != last_viewport_height;
	frame_static.aspect_ratio = float( frame_static.viewport_width ) / float( frame_static.viewport_height );
	frame_static.msaa_samples = r_samples->integer;
	frame_static.shadow_quality = ShadowQuality( r_shadow_quality->integer );
	frame_static.shadow_parameters = GetShadowParameters( frame_static.shadow_quality );

	if( frame_static.viewport_resized || frame_static.msaa_samples != last_msaa || frame_static.shadow_quality != last_shadow_quality ) {
		CreateRenderTargets();
	}

	last_viewport_width = viewport_width;
	last_viewport_height = viewport_height;
	last_msaa = frame_static.msaa_samples;
	last_shadow_quality = frame_static.shadow_quality;

	frame_static.ortho_view_uniforms = UploadViewUniforms( Mat3x4::Identity(), Mat3x4::Identity(), OrthographicProjection( 0, 0, viewport_width, viewport_height, -1, 1 ), Mat4::Identity(), Vec3( 0 ), frame_static.viewport, -1, frame_static.msaa_samples, Vec3() );
	frame_static.identity_model_uniforms = UploadModelUniforms( Mat3x4::Identity() );
	frame_static.identity_material_static_uniforms = UploadMaterialStaticUniforms( 0.0f, 64.0f );
	frame_static.identity_material_dynamic_uniforms = UploadMaterialDynamicUniforms( white.vec4 );

#define TRACY_HACK( name ) { name, __FUNCTION__, __FILE__, uint32_t( __LINE__ ), 0 }
	static const tracy::SourceLocationData particle_update_tracy = TRACY_HACK( "Update particles" );
	static const tracy::SourceLocationData particle_setup_indirect_tracy = TRACY_HACK( "Write particle indirect draw buffer" );
	static const tracy::SourceLocationData tile_culling_tracy = TRACY_HACK( "Decal/dlight tile culling" );
	static const tracy::SourceLocationData write_shadowmap_tracy = TRACY_HACK( "Write shadowmap" );
	static const tracy::SourceLocationData world_opaque_prepass_tracy = TRACY_HACK( "World z-prepass" );
	static const tracy::SourceLocationData world_opaque_tracy = TRACY_HACK( "Render world opaque" );
	static const tracy::SourceLocationData sky_tracy = TRACY_HACK( "Render sky" );
	static const tracy::SourceLocationData write_silhouette_buffer_tracy = TRACY_HACK( "Write silhouette buffer" );
	static const tracy::SourceLocationData nonworld_opaque_outlined_tracy = TRACY_HACK( "Render nonworld opaque outlined" );
	static const tracy::SourceLocationData add_outlines_tracy = TRACY_HACK( "Render outlines" );
	static const tracy::SourceLocationData nonworld_opaque_tracy = TRACY_HACK( "Render nonworld opaque" );
	static const tracy::SourceLocationData msaa_tracy = TRACY_HACK( "Resolve MSAA" );
	static const tracy::SourceLocationData transparent_tracy = TRACY_HACK( "Render transparent" );
	static const tracy::SourceLocationData silhouettes_tracy = TRACY_HACK( "Render silhouettes" );
	static const tracy::SourceLocationData ui_tracy = TRACY_HACK( "Render game HUD" );
	static const tracy::SourceLocationData postprocess_tracy = TRACY_HACK( "Postprocess" );
	static const tracy::SourceLocationData post_ui_tracy = TRACY_HACK( "Render non-game UI" );
#undef TRACY_HACK

	frame_static.particle_update_pass = AddRenderPass( &particle_update_tracy );
	frame_static.particle_setup_indirect_pass = AddRenderPass( RenderPassConfig {
		.barrier = true,
		.tracy = &particle_setup_indirect_tracy,
	} );
	frame_static.tile_culling_pass = AddRenderPass( &tile_culling_tracy );

	Vec4 clear_color = Vec4( 0.0f );
	float clear_depth = 1.0f;

	for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
		frame_static.shadowmap_pass[ i ] = AddRenderPass( &write_shadowmap_tracy, frame_static.render_targets.shadowmaps[ i ], NONE, clear_depth );
	}

	bool msaa = frame_static.msaa_samples;
	if( msaa ) {
		frame_static.world_opaque_prepass_pass = AddRenderPass( &world_opaque_prepass_tracy, frame_static.render_targets.msaa, clear_color, clear_depth );
		frame_static.world_opaque_pass = AddRenderPass( RenderPassConfig {
			.target = frame_static.render_targets.msaa_masked,
			.barrier = true,
			.tracy = &world_opaque_tracy,
		} );
		frame_static.sky_pass = AddRenderPass( &sky_tracy, frame_static.render_targets.msaa );
	}
	else {
		frame_static.world_opaque_prepass_pass = AddRenderPass( &world_opaque_prepass_tracy, frame_static.render_targets.postprocess, clear_color, clear_depth );
		frame_static.world_opaque_pass = AddRenderPass( RenderPassConfig {
			.target = frame_static.render_targets.postprocess_masked,
			.barrier = true,
			.tracy = &world_opaque_tracy,
		} );
		frame_static.sky_pass = AddRenderPass( &sky_tracy, frame_static.render_targets.postprocess );
	}

	frame_static.write_silhouette_gbuffer_pass = AddRenderPass( &write_silhouette_buffer_tracy, frame_static.render_targets.silhouette_mask, clear_color, NONE );

	if( msaa ) {
		frame_static.nonworld_opaque_outlined_pass = AddRenderPass( &nonworld_opaque_outlined_tracy, frame_static.render_targets.msaa_masked );
		frame_static.add_outlines_pass = AddRenderPass( &add_outlines_tracy, frame_static.render_targets.msaa_onlycolor );
		frame_static.nonworld_opaque_pass = AddRenderPass( &nonworld_opaque_tracy, frame_static.render_targets.msaa );
		AddRenderPass( RenderPassConfig {
			.type = RenderPass_Blit,
			.target = frame_static.render_targets.postprocess,
			.blit_source = frame_static.render_targets.msaa,
			.tracy = &msaa_tracy,
		} );
	}
	else {
		frame_static.nonworld_opaque_outlined_pass = AddRenderPass( &nonworld_opaque_outlined_tracy, frame_static.render_targets.postprocess_masked );
		frame_static.add_outlines_pass = AddRenderPass( &add_outlines_tracy, frame_static.render_targets.postprocess_onlycolor );
		frame_static.nonworld_opaque_pass = AddRenderPass( &nonworld_opaque_tracy, frame_static.render_targets.postprocess );
	}

	frame_static.transparent_pass = AddRenderPass( RenderPassConfig {
		.target = frame_static.render_targets.postprocess,
		.barrier = true,
		.tracy = &transparent_tracy,
	} );

	frame_static.add_silhouettes_pass = AddRenderPass( &silhouettes_tracy, frame_static.render_targets.postprocess );

	frame_static.ui_pass = AddRenderPass( RenderPassConfig {
		.target = frame_static.render_targets.postprocess,
		.sorted = false,
		.tracy = &ui_tracy,
	} );

	frame_static.postprocess_pass = AddRenderPass( &postprocess_tracy, clear_color );

	frame_static.post_ui_pass = AddRenderPass( RenderPassConfig {
		.sorted = false,
		.tracy = &post_ui_tracy,
	} );
}

static Mat4 InverseScaleTranslation( Mat4 m ) {
	Mat4 inv = Mat4::Identity();
	inv.col0.x = 1.0f / m.col0.x;
	inv.col1.y = 1.0f / m.col1.y;
	inv.col2.z = 1.0f / m.col2.z;
	inv.col3.x = -m.col3.x * inv.col0.x;
	inv.col3.y = -m.col3.y * inv.col1.y;
	inv.col3.z = -m.col3.z * inv.col2.z;
	return inv;
}

void SetupShadowCascades() {
	TracyZoneScoped;
	constexpr float near_plane = 4.0f;
	float cascade_dist[ 5 ];
	constexpr u32 num_planes = ARRAY_COUNT( cascade_dist );
	constexpr u32 num_cascades = num_planes - 1;
	constexpr u32 num_corners = 4;

	cascade_dist[ 0 ] = near_plane;
	for( u32 i = 0; i < num_cascades; i++ ) {
		cascade_dist[ i + 1 ] = frame_static.shadow_parameters.cascade_dists[ i ];
	}

	const Vec4 frustum_direction_corners[] = {
		Vec4( -1.0f,  1.0f, 1.0f, 1.0f ),
		Vec4(  1.0f,  1.0f, 1.0f, 1.0f ),
		Vec4(  1.0f, -1.0f, 1.0f, 1.0f ),
		Vec4( -1.0f, -1.0f, 1.0f, 1.0f ),
	};

	Vec3 frustum_directions[ num_corners ];
	for( u32 i = 0; i < num_corners; i++ ) {
		Vec4 corner = Mat4( frame_static.inverse_V ) * frame_static.inverse_P * frustum_direction_corners[ i ];
		frustum_directions[ i ] = Normalize( frame_static.position - corner.xyz() / corner.w );
	}

	Vec3 frustum_corners[ num_planes ][ 4 ];
	Vec3 frustum_centers[ num_planes ] = { };
	u32 counts[ num_planes ] = { };
	for( u32 i = 0; i < num_planes; i++ ) {
		for( u32 j = 0; j < num_corners; j++ ) {
			Vec3 corner = frame_static.position + frustum_directions[ j ] * cascade_dist[ i ];
			frustum_corners[ i ][ j ] = corner;
			frustum_centers[ i ] += corner;
			counts[ i ]++;
			frustum_centers[ ( i + 1 ) % num_planes ] += corner;
			counts[ ( i + 1 ) % num_planes ]++;
		}
	}

	Vec3 shadow_camera_positions[ num_planes ];
	Mat3x4 shadow_views[ num_planes ];
	Mat4 shadow_projections[ num_planes ];
	for( u32 i = 0; i < num_planes; i++ ) {
		frustum_centers[ i ] /= num_corners * 2;
		float radius = 0.0f;
		for( u32 j = 0; j < num_corners; j++ ) {
			float dist = Length( frustum_corners[ i ][ j ] - frustum_centers[ i ] );
			radius = Max2( radius, dist );
			dist = Length( frustum_corners[ ( i - 1 ) % num_planes ][ j ] - frustum_centers[ i ] );
			radius = Max2( radius, dist );
		}
		radius = roundf( radius * 16.0f ) / 16.0f;
		shadow_camera_positions[ i ] = frustum_centers[ i ] - frame_static.light_direction * radius;

		shadow_views[ i ] = ViewMatrix( shadow_camera_positions[ i ], frame_static.light_direction );
		shadow_projections[ i ] = OrthographicProjection( -radius, radius, radius, -radius, 0.0f, radius * 2.0f );
	}

	Vec3 cascade_offsets[ num_cascades ];
	Vec3 cascade_scales[ num_cascades ];
	for( u32 i = 0; i < num_cascades; i++ ) {
		Mat4 & shadow_projection = shadow_projections[ i + 1 ];
		const Mat3x4 & shadow_view = shadow_views[ i + 1 ];
		const Vec3 & shadow_camera_position = shadow_camera_positions[ i + 1 ];
		Mat4 shadow_matrix = shadow_projection * Mat4( shadow_view );

		{
			u32 shadowmap_size = frame_static.shadow_parameters.resolution;
			Vec2 shadow_origin = ( shadow_matrix * Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xy();
			shadow_origin *= shadowmap_size / 2.0f;
			Vec2 rounded_origin = Vec2( roundf( shadow_origin.x ), roundf( shadow_origin.y ) );
			Vec2 rounded_offset = ( rounded_origin - shadow_origin ) * ( 2.0f / shadowmap_size );
			shadow_projection.col3.x += rounded_offset.x;
			shadow_projection.col3.y += rounded_offset.y;
		}

		Mat3x4 inv_shadow_view = InvertViewMatrix( shadow_view, shadow_camera_position );
		frame_static.shadowmap_view_uniforms[ i ] = UploadViewUniforms( shadow_view, Mat3x4::Identity(), shadow_projection, Mat4::Identity(), shadow_camera_position, Vec2(), cascade_dist[ i ], 0, frame_static.light_direction );

		Mat4 inv_shadow_projection = InverseScaleTranslation( shadow_projection );

		Mat4 tex_scale_bias = Mat4( Mat4Translation( 0.5f, 0.5f, 0.0f ) * Mat4Scale( 0.5f, 0.5f, 1.0f ) );
		Mat4 inv_tex_scale_bias = InverseScaleTranslation( tex_scale_bias );

		Mat4 inv_cascade = Mat4( shadow_views[ 0 ] * inv_shadow_view ) * inv_shadow_projection * inv_tex_scale_bias;
		Vec3 cascade_corner = ( inv_cascade * Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xyz();
		Vec3 other_corner = ( inv_cascade * Vec4( 1.0f, 1.0f, 1.0f, 1.0f ) ).xyz();

		cascade_offsets[ i ] = -cascade_corner;
		cascade_scales[ i ] = 1.0f / ( other_corner - cascade_corner );
	}

	frame_static.shadow_uniforms = UploadUniformBlock(
		s32( frame_static.shadow_parameters.num_cascades ), shadow_views[ 0 ],
		cascade_dist[ 1 ], cascade_dist[ 2 ], cascade_dist[ 3 ], cascade_dist[ 4 ],
		cascade_offsets[ 0 ], cascade_offsets[ 1 ], cascade_offsets[ 2 ], cascade_offsets[ 3 ],
		cascade_scales[ 0 ], cascade_scales[ 1 ], cascade_scales[ 2 ], cascade_scales[ 3 ]
	);
}

void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov ) {
	float near_plane = 4.0f;

	frame_static.V = ViewMatrix( position, angles );
	frame_static.inverse_V = InvertViewMatrix( frame_static.V, position );
	frame_static.P = PerspectiveProjection( vertical_fov, frame_static.aspect_ratio, near_plane );
	frame_static.inverse_P = InvertPerspectiveProjection( frame_static.P );
	frame_static.position = position;
	frame_static.vertical_fov = vertical_fov;
	frame_static.near_plane = near_plane;

	float t = 1.0f;
	if( client_gs.gameState.sun_moved_from != client_gs.gameState.sun_moved_to ) {
		t = Unlerp01( client_gs.gameState.sun_moved_from, cls.game_time, client_gs.gameState.sun_moved_to );
	}
	EulerDegrees3 sun_angles = LerpAngles( client_gs.gameState.sun_angles_from, t, client_gs.gameState.sun_angles_to );
	AngleVectors( sun_angles, &frame_static.light_direction, NULL, NULL );

	SetupShadowCascades();

	frame_static.view_uniforms = UploadViewUniforms( frame_static.V, frame_static.inverse_V, frame_static.P, frame_static.inverse_P, position, frame_static.viewport, near_plane, frame_static.msaa_samples, frame_static.light_direction );
}

void RendererSubmitFrame() {
	RenderBackendSubmitFrame();
	frame_counter++;
}

size_t FrameSlot() {
	return frame_counter % MAX_FRAMES_IN_FLIGHT;
}

const Texture * BlueNoiseTexture() {
	return &blue_noise;
}

void DrawFullscreenMesh( const PipelineState & pipeline ) {
	DrawMesh( fullscreen_mesh, pipeline );
}

static size_t AlignNonPow2( size_t x, size_t alignment ) {
	return ( ( x + alignment - 1 ) / alignment ) * alignment;
}

template< typename T >
static size_t FillDynamicGeometryBuffer( Span< const T > data, size_t alignment ) {
	size_t frame_offset = dynamic_geometry.buffer.size * FrameSlot();
	size_t aligned_offset = AlignNonPow2( frame_offset + dynamic_geometry.cursor, alignment );
	memcpy( ( ( char * ) dynamic_geometry.buffer.ptr ) + aligned_offset, data.ptr, data.num_bytes() );
	dynamic_geometry.cursor = aligned_offset + data.num_bytes() - frame_offset;
	return aligned_offset / alignment;
}

DynamicDrawData UploadDynamicGeometry( Span< const u8 > vertices, Span< const u16 > indices, const VertexDescriptor & vertex_descriptor ) {
	for( Optional< VertexAttribute > attr : vertex_descriptor.attributes ) {
		Assert( !attr.exists || attr.value.buffer == 0 );
	}

	size_t vertex_size = vertex_descriptor.buffer_strides[ 0 ];
	size_t frame_offset = dynamic_geometry.buffer.size * FrameSlot();
	size_t required_size = AlignNonPow2( AlignNonPow2( frame_offset + dynamic_geometry.cursor, vertex_size ) + vertices.num_bytes(), sizeof( u16 ) ) + indices.num_bytes() - frame_offset;
	if( required_size > DynamicGeometry::BufferSize ) {
		Com_Printf( S_COLOR_YELLOW "Too much dynamic geometry!\n" );
		Assert( is_public_build );
		return { };
	}

	size_t base_vertex = FillDynamicGeometryBuffer( vertices, vertex_size );
	size_t first_index = FillDynamicGeometryBuffer( indices, sizeof( u16 ) );

	return { vertex_descriptor, base_vertex, first_index, indices.n };
}

void DrawDynamicGeometry( const PipelineState & pipeline, const DynamicDrawData & data, Optional< size_t > override_num_vertices, size_t extra_first_index ) {
	if( data.num_vertices == 0 ) {
		return;
	}

	Mesh mesh = NewMesh( MeshConfig {
		.vertex_descriptor = data.vertex_descriptor,
		.vertex_buffers = { dynamic_geometry.buffer.buffer },
		.index_format = IndexFormat_U16,
		.index_buffer = dynamic_geometry.buffer.buffer,
	} );

	size_t num_vertices = Default( override_num_vertices, data.num_vertices );
	DrawMesh( mesh, pipeline, num_vertices, data.first_index + extra_first_index, data.base_vertex );
}

void Draw2DBox( float x, float y, float w, float h, const Material * material, Vec4 color ) {
	Vec2 half_pixel = HalfPixelSize( material );
	Draw2DBoxUV( x, y, w, h, half_pixel, 1.0f - half_pixel, material, color );
}

void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, const Material * material, Vec4 color ) {
	if( w <= 0.0f || h <= 0.0f || color.w <= 0.0f )
		return;

	RGBA8 rgba = LinearTosRGB( color );

	ImDrawList * bg = ImGui::GetBackgroundDrawList();
	bg->PushTextureID( ImGuiShaderAndTexture( material ) );
	bg->PrimReserve( 6, 4 );
	bg->PrimRectUV( Vec2( x, y ), Vec2( x + w, y + h ), topleft_uv, bottomright_uv, IM_COL32( rgba.r, rgba.g, rgba.b, rgba.a ) );
	bg->PopTextureID();
}

UniformBlock UploadModelUniforms( const Mat3x4 & M ) {
	return UploadUniformBlock( M );
}

UniformBlock UploadMaterialStaticUniforms( float specular, float shininess, float lod_bias ) {
	return UploadUniformBlock( specular, shininess, lod_bias );
}

UniformBlock UploadMaterialDynamicUniforms( const Vec4 & color ) {
	return UploadUniformBlock( color );
}

Optional< ModelRenderData > FindModelRenderData( StringHash name ) {
	const GLTFRenderData * gltf = FindGLTFRenderData( name );
	if( gltf != NULL ) {
		return ModelRenderData {
			.type = ModelType_GLTF,
			.gltf = gltf,
		};
	}

	const MapSubModelRenderData * map = FindMapSubModelRenderData( name );
	if( map != NULL ) {
		return ModelRenderData {
			.type = ModelType_Map,
			.map = map,
		};
	}

	return NONE;
}

Optional< ModelRenderData > FindModelRenderData( const char * name ) {
	return FindModelRenderData( StringHash( name ) );
}

void DrawModel( DrawModelConfig config, ModelRenderData render_data, const Mat3x4 & transform, const Vec4 & color, MatrixPalettes palettes ) {
	switch( render_data.type ) {
		case ModelType_GLTF: DrawGLTFModel( config, render_data.gltf, transform, color, palettes ); break;
		case ModelType_Map: DrawMapModel( config, render_data.map, transform, color ); break;
	}
}
