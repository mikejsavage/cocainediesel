#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/renderer/blue_noise.h"
#include "client/renderer/skybox.h"
#include "client/renderer/srgb.h"
#include "client/renderer/text.h"

#include "client/maps.h"
#include "cgame/cg_particles.h"

#include "imgui/imgui.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

FrameStatic frame_static;

static Texture blue_noise;

static Mesh fullscreen_mesh;

static constexpr size_t MaxDynamicVerts = U16_MAX;
static Mesh dynamic_geometry_mesh;
static u16 dynamic_geometry_num_vertices;
static u16 dynamic_geometry_num_indices;

static char last_screenshot_date[ 256 ];
static int same_date_count;

static u32 last_viewport_width, last_viewport_height;
static int last_msaa;
static ShadowQuality last_shadow_quality;

static cvar_t * r_samples;
static cvar_t * r_shadow_quality;

static void TakeScreenshot() {
	RGB8 * framebuffer = ALLOC_MANY( sys_allocator, RGB8, frame_static.viewport_width * frame_static.viewport_height );
	defer { FREE( sys_allocator, framebuffer ); };
	DownloadFramebuffer( framebuffer );

	stbi_flip_vertically_on_write( 1 );

	int ok = stbi_write_png_to_func( []( void * context, void * png, int png_size ) {
		char date[ 256 ];
		Sys_FormatTime( date, sizeof( date ), "%y%m%d_%H%M%S" );

		TempAllocator temp = cls.frame_arena.temp();

		char * dir = temp( "{}/screenshots", HomeDirPath() );
		DynamicString path( &temp, "{}/{}", dir, date );

		if( strcmp( date, last_screenshot_date ) == 0 ) {
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
		case ShadowQuality_Low:    return { 2, 256.0f, 2048.0f, 2048.0f, 2048.0f, 1024, 2 };
		case ShadowQuality_Medium: return { 4, 256.0f, 768.0f, 2304.0f, 6912.0f, 1024, 2 };
		case ShadowQuality_High:   return { 4, 256.0f, 768.0f, 2304.0f, 6912.0f, 2048, 4 };
		case ShadowQuality_Ultra:  return { 4, 256.0f, 768.0f, 2304.0f, 6912.0f, 4096, 4 };
	}

	assert( false );
	return { };
}

void InitRenderer() {
	ZoneScoped;

	RenderBackendInit();

	r_samples = Cvar_Get( "r_samples", "0", CVAR_ARCHIVE );
	r_shadow_quality = Cvar_Get( "r_shadow_quality", "1", CVAR_ARCHIVE );

	frame_static = { };
	last_viewport_width = U32_MAX;
	last_viewport_height = U32_MAX;
	last_msaa = 0;

	{
		int w, h;
		u8 * img = stbi_load_from_memory( blue_noise_png, blue_noise_png_len, &w, &h, NULL, 1 );
		assert( img != NULL );

		TextureConfig config;
		config.width = w;
		config.height = h;
		config.data = img;
		config.format = TextureFormat_R_S8;
		blue_noise = NewTexture( config );

		stbi_image_free( img );
	}

	{
		constexpr Vec3 positions[] = {
			Vec3( -1.0f, -1.0f, 0.0f ),
			Vec3( 3.0f, -1.0f, 0.0f ),
			Vec3( -1.0f, 3.0f, 0.0f ),
		};

		MeshConfig config;
		config.name = "Fullscreen triangle";
		config.positions = NewVertexBuffer( positions, sizeof( positions ) );
		config.num_vertices = 3;
		fullscreen_mesh = NewMesh( config );
	}

	{
		MeshConfig config;
		config.name = "Dynamic geometry";
		config.positions = NewVertexBuffer( sizeof( Vec3 ) * 4 * MaxDynamicVerts );
		config.tex_coords = NewVertexBuffer( sizeof( Vec2 ) * 4 * MaxDynamicVerts );
		config.colors = NewVertexBuffer( sizeof( RGBA8 ) * 4 * MaxDynamicVerts );
		config.indices = NewIndexBuffer( sizeof( u16 ) * 6 * MaxDynamicVerts );
		dynamic_geometry_mesh = NewMesh( config );
	}

	Cmd_AddCommand( "screenshot", TakeScreenshot );
	strcpy( last_screenshot_date, "" );
	same_date_count = 0;

	InitShaders();
	InitMaterials();
	InitText();
	InitSkybox();
	InitModels();
	InitVisualEffects();
}

static void DeleteFramebuffers() {
	DeleteFramebuffer( frame_static.silhouette_gbuffer );
	DeleteFramebuffer( frame_static.postprocess_fb );
	DeleteFramebuffer( frame_static.msaa_fb );
	DeleteFramebuffer( frame_static.postprocess_fb_onlycolor );
	DeleteFramebuffer( frame_static.msaa_fb_onlycolor );
	for( u32 i = 0; i < 4; i++ ) {
		DeleteFramebuffer( frame_static.shadowmap_fb[ i ] );
	}
}

void ShutdownRenderer() {
	ShutdownModels();
	ShutdownSkybox();
	ShutdownText();
	ShutdownMaterials();
	ShutdownVisualEffects();
	ShutdownShaders();

	DeleteTexture( blue_noise );
	DeleteMesh( fullscreen_mesh );
	DeleteFramebuffers();

	Cmd_RemoveCommand( "screenshot" );

	RenderBackendShutdown();
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
	float epsilon = 2.4e-6f;

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

static Mat4 ViewMatrix( Vec3 position, EulerDegrees3 angles ) {
	float pitch = Radians( angles.pitch );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	float yaw = Radians( angles.yaw );
	float sy = sinf( yaw );
	float cy = cosf( yaw );

	Vec3 forward = Vec3( cp * cy, cp * sy, -sp );
	Vec3 right = Vec3( sy, -cy, 0 );
	Vec3 up = Vec3( sp * cy, sp * sy, cp );

	Mat4 rotation(
		right.x, right.y, right.z, 0,
		up.x, up.y, up.z, 0,
		-forward.x, -forward.y, -forward.z, 0,
		0, 0, 0, 1
	);
	return rotation * Mat4Translation( -position );
}

static Mat4 ViewMatrix( Vec3 position, Vec3 forward ) {
	Vec3 right, up;
	ViewVectors( forward, &right, &up );
	Mat4 rotation(
		right.x, right.y, right.z, 0,
		up.x, up.y, up.z, 0,
		-forward.x, -forward.y, -forward.z, 0,
		0, 0, 0, 1
	);
	return rotation * Mat4Translation( -position );
}

static Mat4 InvertViewMatrix( const Mat4 & V, Vec3 position ) {
	return Mat4(
		// transpose rotation part
		Vec4( V.row0().xyz(), 0.0f ),
		Vec4( V.row1().xyz(), 0.0f ),
		Vec4( V.row2().xyz(), 0.0f ),

		Vec4( position, 1.0f )
	);
}

static UniformBlock UploadViewUniforms( const Mat4 & V, const Mat4 & inverse_V, const Mat4 & P, const Mat4 & inverse_P, Vec3 camera_pos, Vec2 viewport_size, float near_plane, int samples, Vec3 light_dir ) {
	return UploadUniformBlock( V, inverse_V, P, inverse_P, camera_pos, viewport_size, near_plane, samples, light_dir );
}

static void CreateFramebuffers() {
	DeleteFramebuffers();

	TextureConfig texture_config;
	texture_config.width = frame_static.viewport_width;
	texture_config.height = frame_static.viewport_height;
	texture_config.wrap = TextureWrap_Clamp;

	{
		FramebufferConfig fb;

		texture_config.format = TextureFormat_RGBA_U8_sRGB;
		fb.albedo_attachment = texture_config;

		frame_static.silhouette_gbuffer = NewFramebuffer( fb );
	}

	if( frame_static.msaa_samples > 1 ) {
		FramebufferConfig fb;

		texture_config.format = TextureFormat_RGB_U8_sRGB;
		fb.albedo_attachment = texture_config;

		texture_config.format = TextureFormat_Depth;
		fb.depth_attachment = texture_config;

		fb.msaa_samples = frame_static.msaa_samples;

		frame_static.msaa_fb = NewFramebuffer( fb );
	}

	{
		FramebufferConfig fb;

		texture_config.format = TextureFormat_RGB_U8_sRGB;
		fb.albedo_attachment = texture_config;

		texture_config.format = TextureFormat_Depth;
		fb.depth_attachment = texture_config;

		frame_static.postprocess_fb = NewFramebuffer( fb );
	}

	frame_static.postprocess_fb_onlycolor = NewFramebuffer( &frame_static.postprocess_fb.albedo_texture, NULL, NULL );
	if( frame_static.msaa_samples > 1 ) {
		frame_static.msaa_fb_onlycolor = NewFramebuffer( &frame_static.msaa_fb.albedo_texture, NULL, NULL );
	}

	{
		FramebufferConfig fb;

		u32 shadowmap_res = frame_static.shadow_parameters.shadowmap_res;
		texture_config.width = shadowmap_res;
		texture_config.height = shadowmap_res;
		texture_config.format = TextureFormat_Shadow;
		fb.depth_attachment = texture_config;

		for( u32 i = 0; i < 4; i++ ) {
			frame_static.shadowmap_fb[ i ] = NewFramebuffer( fb );
		}
	}
}

#if !TRACY_ENABLE
namespace tracy {
struct SourceLocationData {
	const char * name;
	const char * function;
	const char * file;
	uint32_t line;
	uint32_t color;
};
}
#endif

void RendererBeginFrame( u32 viewport_width, u32 viewport_height ) {
	HotloadShaders();
	HotloadMaterials();
	HotloadModels();
	HotloadMaps();
	HotloadVisualEffects();

	RenderBackendBeginFrame();

	dynamic_geometry_num_vertices = 0;
	dynamic_geometry_num_indices = 0;

	if( !IsPowerOf2( r_samples->integer ) || r_samples->integer > 16 || r_samples->integer == 1 ) {
		Com_Printf( "Invalid r_samples value (%d), resetting\n", r_samples->integer );
		Cvar_Set( "r_samples", r_samples->dvalue );
	}

	if( r_shadow_quality->integer < ShadowQuality_Low || r_shadow_quality->integer > ShadowQuality_Ultra ) {
		Com_Printf( "Invalid r_shadow_quality value (%d), resetting\n", r_shadow_quality->integer );
		Cvar_Set( "r_shadow_quality", r_shadow_quality->dvalue );
	}

	frame_static.viewport_width = Max2( u32( 1 ), viewport_width );
	frame_static.viewport_height = Max2( u32( 1 ), viewport_height );
	frame_static.viewport = Vec2( frame_static.viewport_width, frame_static.viewport_height );
	frame_static.aspect_ratio = float( viewport_width ) / float( viewport_height );
	frame_static.msaa_samples = r_samples->integer;
	frame_static.shadow_quality = ShadowQuality( r_shadow_quality->integer );
	frame_static.shadow_parameters = GetShadowParameters( frame_static.shadow_quality );

	if( viewport_width != last_viewport_width || viewport_height != last_viewport_height || frame_static.msaa_samples != last_msaa || frame_static.shadow_quality != last_shadow_quality ) {
		CreateFramebuffers();
		last_viewport_width = viewport_width;
		last_viewport_height = viewport_height;
		last_msaa = frame_static.msaa_samples;
		last_shadow_quality = frame_static.shadow_quality;
	}

	bool msaa = frame_static.msaa_samples;

	frame_static.ortho_view_uniforms = UploadViewUniforms( Mat4::Identity(), Mat4::Identity(), OrthographicProjection( 0, 0, viewport_width, viewport_height, -1, 1 ), Mat4::Identity(), Vec3( 0 ), frame_static.viewport, -1, frame_static.msaa_samples, Vec3() );
	frame_static.identity_model_uniforms = UploadModelUniforms( Mat4::Identity() );
	frame_static.identity_material_uniforms = UploadMaterialUniforms( vec4_white, Vec2( 0 ), 0.0f, 64.0f );

	frame_static.blue_noise_uniforms = UploadUniformBlock( Vec2( blue_noise.width, blue_noise.height ) );

#define TRACY_HACK( name ) { name, __FUNCTION__, __FILE__, uint32_t( __LINE__ ), 0 }
	static const tracy::SourceLocationData particle_update_tracy = TRACY_HACK( "Update particles" );
	static const tracy::SourceLocationData write_shadowmap_tracy = TRACY_HACK( "Write shadowmap" );
	static const tracy::SourceLocationData world_opaque_prepass_tracy = TRACY_HACK( "World z-prepass" );
	static const tracy::SourceLocationData world_opaque_tracy = TRACY_HACK( "Render world opaque" );
	static const tracy::SourceLocationData add_world_outlines_tracy = TRACY_HACK( "Render world outlines" );
	static const tracy::SourceLocationData write_silhouette_buffer_tracy = TRACY_HACK( "Write silhouette buffer" );
	static const tracy::SourceLocationData nonworld_opaque_tracy = TRACY_HACK( "Render nonworld opaque" );
	static const tracy::SourceLocationData msaa_tracy = TRACY_HACK( "Resolve MSAA" );
	static const tracy::SourceLocationData sky_tracy = TRACY_HACK( "Render sky" );
	static const tracy::SourceLocationData transparent_tracy = TRACY_HACK( "Render transparent" );
	static const tracy::SourceLocationData silhouettes_tracy = TRACY_HACK( "Render silhouettes" );
	static const tracy::SourceLocationData ui_tracy = TRACY_HACK( "Render game HUD" );
	static const tracy::SourceLocationData postprocess_tracy = TRACY_HACK( "Postprocess" );
	static const tracy::SourceLocationData post_ui_tracy = TRACY_HACK( "Render non-game UI" );
#undef TRACY_HACK

	frame_static.particle_update_pass = AddRenderPass( "Particle Update", &particle_update_tracy );

	for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
		frame_static.shadowmap_pass[ i ] = AddRenderPass( "Write shadowmap", &write_shadowmap_tracy, frame_static.shadowmap_fb[ i ], ClearColor_Dont, ClearDepth_Do );
	}

	if( msaa ) {
		frame_static.world_opaque_prepass_pass = AddRenderPass( "Render world opaque Prepass", &world_opaque_prepass_tracy, frame_static.msaa_fb, ClearColor_Do, ClearDepth_Do );
		frame_static.world_opaque_pass = AddRenderPass( "Render world opaque", &world_opaque_tracy, frame_static.msaa_fb );
		frame_static.sky_pass = AddRenderPass( "Render sky", &sky_tracy, frame_static.msaa_fb );
		frame_static.add_world_outlines_pass = AddRenderPass( "Render world outlines", &add_world_outlines_tracy, frame_static.msaa_fb_onlycolor );
	}
	else {
		frame_static.world_opaque_prepass_pass = AddRenderPass( "Render world opaque Prepass", &world_opaque_prepass_tracy, frame_static.postprocess_fb, ClearColor_Do, ClearDepth_Do );
		frame_static.world_opaque_pass = AddRenderPass( "Render world opaque", &world_opaque_tracy, frame_static.postprocess_fb );
		frame_static.sky_pass = AddRenderPass( "Render sky", &sky_tracy, frame_static.postprocess_fb );
		frame_static.add_world_outlines_pass = AddRenderPass( "Render world outlines", &add_world_outlines_tracy, frame_static.postprocess_fb_onlycolor );
	}

	frame_static.write_silhouette_gbuffer_pass = AddRenderPass( "Write silhouette gbuffer", &write_silhouette_buffer_tracy, frame_static.silhouette_gbuffer, ClearColor_Do, ClearDepth_Dont );

	if( msaa ) {
		frame_static.nonworld_opaque_pass = AddRenderPass( "Render nonworld opaque", &nonworld_opaque_tracy, frame_static.msaa_fb );
		AddResolveMSAAPass( "Resolve MSAA", &msaa_tracy, frame_static.msaa_fb, frame_static.postprocess_fb, ClearColor_Do, ClearDepth_Do );
	}
	else {
		frame_static.nonworld_opaque_pass = AddRenderPass( "Render nonworld opaque", &nonworld_opaque_tracy, frame_static.postprocess_fb );
	}

	frame_static.transparent_pass = AddRenderPass( "Render transparent", &transparent_tracy, frame_static.postprocess_fb );
	frame_static.add_silhouettes_pass = AddRenderPass( "Render silhouettes", &silhouettes_tracy, frame_static.postprocess_fb );
	frame_static.ui_pass = AddUnsortedRenderPass( "Render UI", &ui_tracy, frame_static.postprocess_fb );
	frame_static.postprocess_pass = AddRenderPass( "Postprocess", &postprocess_tracy, ClearColor_Do );
	frame_static.post_ui_pass = AddUnsortedRenderPass( "Render Post UI", &post_ui_tracy );
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
	ZoneScoped;
	const float near_plane = 4.0f;
	// const float cascade_dist[ 5 ] = { near_plane, 256.0f, 768.0f, 2304.0f, 6912.0f };
	// const float cascade_dist[ 5 ] = { near_plane, 16.0f, 64.0f, 512.0f, 4096.0f };
	float cascade_dist[ 5 ];

	cascade_dist[ 0 ] = near_plane;
	for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
		cascade_dist[ i + 1 ] = frame_static.shadow_parameters.cascade_dists[ i ];
	}

	const u32 num_planes = ARRAY_COUNT( cascade_dist );
	const u32 num_cascades = num_planes - 1;
	const u32 num_corners = 4;

	const Vec4 frustum_direction_corners[] = {
		Vec4( -1.0f,  1.0f, 1.0f, 1.0f ),
		Vec4(  1.0f,  1.0f, 1.0f, 1.0f ),
		Vec4(  1.0f, -1.0f, 1.0f, 1.0f ),
		Vec4( -1.0f, -1.0f, 1.0f, 1.0f ),
	};

	Vec3 frustum_directions[ num_corners ];
	for( u32 i = 0; i < num_corners; i++ ) {
		Vec4 corner = frame_static.inverse_V * frame_static.inverse_P * frustum_direction_corners[ i ];
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
	Mat4 shadow_views[ num_planes ];
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
		Mat4 & shadow_view = shadow_views[ i + 1 ];
		Vec3 & shadow_camera_position = shadow_camera_positions[ i + 1 ];
		Mat4 shadow_matrix = shadow_projection * shadow_view;

		{
			u32 shadowmap_size = frame_static.shadowmap_fb[ i ].width;
			Vec2 shadow_origin = ( shadow_matrix * Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xy();
			shadow_origin *= shadowmap_size / 2.0f;
			Vec2 rounded_origin = Vec2( roundf( shadow_origin.x ), roundf( shadow_origin.y ) );
			Vec2 rounded_offset = ( rounded_origin - shadow_origin ) * ( 2.0f / shadowmap_size );
			shadow_projection.col3.x += rounded_offset.x;
			shadow_projection.col3.y += rounded_offset.y;
		}

		frame_static.shadowmap_view_uniforms[ i ] = UploadViewUniforms( shadow_view, Mat4::Identity(), shadow_projection, Mat4::Identity(), shadow_camera_position, Vec2(), cascade_dist[ i ], 0, frame_static.light_direction );

		Mat4 inv_shadow_view = InvertViewMatrix( shadow_view, shadow_camera_position );
		Mat4 inv_shadow_projection = InverseScaleTranslation( shadow_projection );

		Mat4 tex_scale_bias = Mat4Translation( 0.5f, 0.5f, 0.0f ) * Mat4Scale( 0.5f, 0.5f, 1.0f );
		Mat4 inv_tex_scale_bias = InverseScaleTranslation( tex_scale_bias );

		Mat4 inv_cascade = shadow_views[ 0 ] * inv_shadow_view * inv_shadow_projection * inv_tex_scale_bias;
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

	frame_static.light_direction = Normalize( Vec3( 1.0f, 2.0f, -3.0f ) );
	// frame_static.light_direction.x = cosf( float( cls.monotonicTime ) * 0.0001f ) * 2.0f;
	// frame_static.light_direction.y = sinf( float( cls.monotonicTime ) * 0.0001f ) * 2.0f;
	// frame_static.light_direction = Normalize( frame_static.light_direction );

	SetupShadowCascades();

	frame_static.view_uniforms = UploadViewUniforms( frame_static.V, frame_static.inverse_V, frame_static.P, frame_static.inverse_P, position, frame_static.viewport, near_plane, frame_static.msaa_samples, frame_static.light_direction );
}

void RendererSubmitFrame() {
	RenderBackendSubmitFrame();
}

const Texture * BlueNoiseTexture() {
	return &blue_noise;
}

void DrawFullscreenMesh( const PipelineState & pipeline ) {
	DrawMesh( fullscreen_mesh, pipeline );
}

u16 DynamicMeshBaseIndex() {
	return dynamic_geometry_num_vertices;
}

void DrawDynamicMesh( const PipelineState & pipeline, const DynamicMesh & mesh ) {
	WriteVertexBuffer( dynamic_geometry_mesh.positions, mesh.positions, mesh.num_vertices * sizeof( mesh.positions[ 0 ] ), dynamic_geometry_num_vertices * sizeof( mesh.positions[ 0 ] ) );
	WriteVertexBuffer( dynamic_geometry_mesh.tex_coords, mesh.uvs, mesh.num_vertices * sizeof( mesh.uvs[ 0 ] ), dynamic_geometry_num_vertices * sizeof( mesh.uvs[ 0 ] ) );
	WriteVertexBuffer( dynamic_geometry_mesh.colors, mesh.colors, mesh.num_vertices * sizeof( mesh.colors[ 0 ] ), dynamic_geometry_num_vertices * sizeof( mesh.colors[ 0 ] ) );
	WriteIndexBuffer( dynamic_geometry_mesh.indices, mesh.indices, mesh.num_indices * sizeof( mesh.indices[ 0 ] ), dynamic_geometry_num_indices * sizeof( mesh.indices[ 0 ] ) );

	DrawMesh( dynamic_geometry_mesh, pipeline, mesh.num_indices, dynamic_geometry_num_indices * sizeof( mesh.indices[ 0 ] ) );

	dynamic_geometry_num_vertices += mesh.num_vertices;
	dynamic_geometry_num_indices += mesh.num_indices;
}

void Draw2DBox( float x, float y, float w, float h, const Material * material, Vec4 color ) {
	Vec2 half_pixel = HalfPixelSize( material );
	Draw2DBoxUV( x, y, w, h, half_pixel, 1.0f - half_pixel, material, color );
}

void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, const Material * material, Vec4 color ) {
	if( w <= 0.0f || h <= 0.0f )
		return;

	RGBA8 rgba = LinearTosRGB( color );

	ImDrawList * bg = ImGui::GetBackgroundDrawList();
	bg->PushTextureID( ImGuiShaderAndMaterial( material ) );
	bg->PrimReserve( 6, 4 );
	bg->PrimRectUV( Vec2( x, y ), Vec2( x + w, y + h ), topleft_uv, bottomright_uv, IM_COL32( rgba.r, rgba.g, rgba.b, rgba.a ) );
	bg->PopTextureID();
}

UniformBlock UploadModelUniforms( const Mat4 & M ) {
	return UploadUniformBlock( M );
}

UniformBlock UploadMaterialUniforms( const Vec4 & color, const Vec2 & texture_size, float specular, float shininess, Vec3 tcmod_row0, Vec3 tcmod_row1 ) {
	return UploadUniformBlock( color, tcmod_row0, tcmod_row1, texture_size, specular, shininess );
}
