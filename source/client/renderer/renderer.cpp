#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/time.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/renderer/blue_noise.h"
#include "client/renderer/skybox.h"
#include "client/renderer/text.h"

#include "client/maps.h"
#include "cgame/cg_particles.h"

#include "imgui/imgui.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#include "tracy/Tracy.hpp"

FrameStatic frame_static;
static u64 frame_counter;

static Texture blue_noise;

static Mesh fullscreen_mesh;

struct DynamicGeometry {
	static constexpr size_t MaxVerts = U16_MAX;

	StreamingBuffer positions_buffer;
	StreamingBuffer uvs_buffer;
	StreamingBuffer colors_buffer;
	StreamingBuffer index_buffer;

	Mesh mesh;
	u16 num_vertices;
	u16 num_indices;
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

	r_samples = NewCvar( "r_samples", "0", CvarFlag_Archive );
	r_shadow_quality = NewCvar( "r_shadow_quality", "1", CvarFlag_Archive );

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

		MeshConfig config = { };
		config.name = "Fullscreen triangle";
		config.set_attribute( VertexAttribute_Position, NewGPUBuffer( positions, sizeof( positions ), "Fullscreen triangle vertices" ) );
		config.num_vertices = 3;
		fullscreen_mesh = NewMesh( config );
	}

	{
		dynamic_geometry.positions_buffer = NewStreamingBuffer( sizeof( Vec3 ) * DynamicGeometry::MaxVerts, "Dynamic geometry positions" );
		dynamic_geometry.uvs_buffer = NewStreamingBuffer( sizeof( Vec2 ) * DynamicGeometry::MaxVerts, "Dynamic geometry uvs" );
		dynamic_geometry.colors_buffer = NewStreamingBuffer( sizeof( RGBA8 ) * DynamicGeometry::MaxVerts, "Dynamic geometry colors" );
		dynamic_geometry.index_buffer = NewStreamingBuffer( sizeof( u16 ) * DynamicGeometry::MaxVerts, "Dynamic geometry indices" );

		MeshConfig config = { };
		config.name = "Dynamic geometry";
		config.set_attribute( VertexAttribute_Position, dynamic_geometry.positions_buffer.buffer );
		config.set_attribute( VertexAttribute_TexCoord, dynamic_geometry.uvs_buffer.buffer );
		config.set_attribute( VertexAttribute_Color, dynamic_geometry.colors_buffer.buffer );
		config.index_buffer = dynamic_geometry.index_buffer.buffer;
		dynamic_geometry.mesh = NewMesh( config );
	}

	AddCommand( "screenshot", TakeScreenshot );
	strcpy( last_screenshot_date, "" );
	same_date_count = 0;

	InitShaders();
	InitMaterials();
	InitText();
	InitSkybox();
	InitModels();
	InitVisualEffects();
}

static void DeleteRenderTargets() {
	DeleteRenderTargetAndTextures( frame_static.render_targets.silhouette_mask );
	DeleteRenderTarget( frame_static.render_targets.postprocess );
	DeleteRenderTarget( frame_static.render_targets.msaa );
	DeleteRenderTargetAndTextures( frame_static.render_targets.postprocess_masked );
	DeleteRenderTargetAndTextures( frame_static.render_targets.msaa_masked );
	DeleteRenderTarget( frame_static.render_targets.postprocess_onlycolor );
	DeleteRenderTarget( frame_static.render_targets.msaa_onlycolor );

	DeleteTexture( frame_static.render_targets.shadowmaps[ 0 ].depth_attachment );
	for( u32 i = 0; i < 4; i++ ) {
		DeleteRenderTarget( frame_static.render_targets.shadowmaps[ i ] );
	}

	frame_static.render_targets = { };
}

void ShutdownRenderer() {
	TracyZoneScoped;

	ShutdownVisualEffects();
	ShutdownModels();
	ShutdownSkybox();
	ShutdownText();
	ShutdownMaterials();
	ShutdownShaders();

	DeleteTexture( blue_noise );
	DeleteMesh( fullscreen_mesh );
	DeleteRenderTargets();

	DeleteStreamingBuffer( dynamic_geometry.positions_buffer );
	DeleteStreamingBuffer( dynamic_geometry.uvs_buffer );
	DeleteStreamingBuffer( dynamic_geometry.colors_buffer );
	DeleteStreamingBuffer( dynamic_geometry.index_buffer );

	// this is a hack, ordinarily Mesh owns its vertex buffers but not here
	for( GPUBuffer & buffer : dynamic_geometry.mesh.vertex_buffers ) {
		buffer = { };
	}
	dynamic_geometry.mesh.index_buffer = { };

	DeleteMesh( dynamic_geometry.mesh );

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

static Mat4 ViewMatrix( Vec3 position, EulerDegrees3 angles ) {
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

static void CreateRenderTargets() {
	DeleteRenderTargets();

	{
		TextureConfig albedo_desc;
		albedo_desc.format = TextureFormat_RGBA_U8_sRGB;
		albedo_desc.width = frame_static.viewport_width;
		albedo_desc.height = frame_static.viewport_height;
		albedo_desc.wrap = TextureWrap_Clamp;

		RenderTargetConfig rt;
		rt.color_attachments[ FragmentShaderOutput_Albedo ] = { NewTexture( albedo_desc ) };

		frame_static.render_targets.silhouette_mask = NewRenderTarget( rt );
	}

	if( frame_static.msaa_samples > 1 ) {
		TextureConfig albedo_desc;
		albedo_desc.format = TextureFormat_RGBA_U8_sRGB;
		albedo_desc.width = frame_static.viewport_width;
		albedo_desc.height = frame_static.viewport_height;
		albedo_desc.msaa_samples = frame_static.msaa_samples;
		Texture albedo = NewTexture( albedo_desc );

		TextureConfig curved_surface_mask_desc;
		curved_surface_mask_desc.format = TextureFormat_R_UI8;
		curved_surface_mask_desc.width = frame_static.viewport_width;
		curved_surface_mask_desc.height = frame_static.viewport_height;
		curved_surface_mask_desc.msaa_samples = frame_static.msaa_samples;
		curved_surface_mask_desc.filter = TextureFilter_Point;
		Texture curved_surface_mask = NewTexture( curved_surface_mask_desc );

		TextureConfig depth_desc;
		depth_desc.format = TextureFormat_Depth;
		depth_desc.width = frame_static.viewport_width;
		depth_desc.height = frame_static.viewport_height;
		depth_desc.msaa_samples = frame_static.msaa_samples;
		Texture depth = NewTexture( depth_desc );

		{
			RenderTargetConfig rt;
			rt.color_attachments[ FragmentShaderOutput_Albedo ] = { albedo };
			rt.color_attachments[ FragmentShaderOutput_CurvedSurfaceMask ] = { curved_surface_mask };
			rt.depth_attachment = { depth };
			frame_static.render_targets.msaa_masked = NewRenderTarget( rt );
		}

		{
			RenderTargetConfig rt;
			rt.color_attachments[ FragmentShaderOutput_Albedo ] = { albedo };
			rt.depth_attachment = { depth };
			frame_static.render_targets.msaa = NewRenderTarget( rt );
		}

		{
			RenderTargetConfig rt;
			rt.color_attachments[ FragmentShaderOutput_Albedo ] = { albedo };
			frame_static.render_targets.msaa_onlycolor = NewRenderTarget( rt );
		}
	}

	{
		TextureConfig albedo_desc;
		albedo_desc.format = TextureFormat_RGBA_U8_sRGB;
		albedo_desc.width = frame_static.viewport_width;
		albedo_desc.height = frame_static.viewport_height;
		Texture albedo = NewTexture( albedo_desc );

		TextureConfig curved_surface_mask_desc;
		curved_surface_mask_desc.format = TextureFormat_R_UI8;
		curved_surface_mask_desc.width = frame_static.viewport_width;
		curved_surface_mask_desc.height = frame_static.viewport_height;
		curved_surface_mask_desc.filter = TextureFilter_Point;
		Texture curved_surface_mask = NewTexture( curved_surface_mask_desc );

		TextureConfig depth_desc;
		depth_desc.format = TextureFormat_Depth;
		depth_desc.width = frame_static.viewport_width;
		depth_desc.height = frame_static.viewport_height;
		Texture depth = NewTexture( depth_desc );

		{
			RenderTargetConfig rt;
			rt.color_attachments[ FragmentShaderOutput_Albedo ] = { albedo };
			rt.color_attachments[ FragmentShaderOutput_CurvedSurfaceMask ] = { curved_surface_mask };
			rt.depth_attachment = { depth };
			frame_static.render_targets.postprocess_masked = NewRenderTarget( rt );
		}

		{
			RenderTargetConfig rt;
			rt.color_attachments[ FragmentShaderOutput_Albedo ] = { albedo };
			rt.depth_attachment = { depth };
			frame_static.render_targets.postprocess = NewRenderTarget( rt );
		}

		{
			RenderTargetConfig rt;
			rt.color_attachments[ FragmentShaderOutput_Albedo ] = { albedo };
			frame_static.render_targets.postprocess_onlycolor = NewRenderTarget( rt );
		}
	}

	{
		TextureConfig shadowmap_desc;
		shadowmap_desc.format = TextureFormat_Shadow;
		shadowmap_desc.width = frame_static.shadow_parameters.resolution;
		shadowmap_desc.height = frame_static.shadow_parameters.resolution;
		shadowmap_desc.num_layers = frame_static.shadow_parameters.num_cascades;
		shadowmap_desc.wrap = TextureWrap_Border;
		shadowmap_desc.border_color = Vec4( 0.0f );
		Texture shadowmap = NewTexture( shadowmap_desc );

		for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
			RenderTargetConfig rt;
			rt.depth_attachment = { shadowmap, i };
			frame_static.render_targets.shadowmaps[ i ] = NewRenderTarget( rt );
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

	ClearMaterialStaticUniforms();

	RenderBackendBeginFrame();

	dynamic_geometry.num_vertices = 0;
	dynamic_geometry.num_indices = 0;

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

	frame_static.ortho_view_uniforms = UploadViewUniforms( Mat4::Identity(), Mat4::Identity(), OrthographicProjection( 0, 0, viewport_width, viewport_height, -1, 1 ), Mat4::Identity(), Vec3( 0 ), frame_static.viewport, -1, frame_static.msaa_samples, Vec3() );
	frame_static.identity_model_uniforms = UploadModelUniforms( Mat4::Identity() );
	frame_static.identity_material_static_uniforms = UploadMaterialStaticUniforms( Vec2( 0 ), 0.0f, 64.0f );
	frame_static.identity_material_dynamic_uniforms = UploadMaterialDynamicUniforms( vec4_white );

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
	frame_static.particle_setup_indirect_pass = AddBarrierRenderPass( &particle_setup_indirect_tracy );
	frame_static.tile_culling_pass = AddRenderPass( &tile_culling_tracy );

	for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
		frame_static.shadowmap_pass[ i ] = AddRenderPass( &write_shadowmap_tracy, frame_static.render_targets.shadowmaps[ i ], ClearColor_Dont, ClearDepth_Do );
	}

	bool msaa = frame_static.msaa_samples;
	if( msaa ) {
		frame_static.world_opaque_prepass_pass = AddRenderPass( &world_opaque_prepass_tracy, frame_static.render_targets.msaa, ClearColor_Do, ClearDepth_Do );
		frame_static.world_opaque_pass = AddBarrierRenderPass( &world_opaque_tracy, frame_static.render_targets.msaa_masked );
		frame_static.sky_pass = AddRenderPass( &sky_tracy, frame_static.render_targets.msaa );
	}
	else {
		frame_static.world_opaque_prepass_pass = AddRenderPass( &world_opaque_prepass_tracy, frame_static.render_targets.postprocess, ClearColor_Do, ClearDepth_Do );
		frame_static.world_opaque_pass = AddBarrierRenderPass( &world_opaque_tracy, frame_static.render_targets.postprocess_masked );
		frame_static.sky_pass = AddRenderPass( &sky_tracy, frame_static.render_targets.postprocess );
	}

	frame_static.write_silhouette_gbuffer_pass = AddRenderPass( &write_silhouette_buffer_tracy, frame_static.render_targets.silhouette_mask, ClearColor_Do, ClearDepth_Dont );

	if( msaa ) {
		frame_static.nonworld_opaque_outlined_pass = AddRenderPass( &nonworld_opaque_outlined_tracy, frame_static.render_targets.msaa_masked );
		frame_static.add_outlines_pass = AddRenderPass( &add_outlines_tracy, frame_static.render_targets.msaa_onlycolor );
		frame_static.nonworld_opaque_pass = AddRenderPass( &nonworld_opaque_tracy, frame_static.render_targets.msaa );
		AddResolveMSAAPass( &msaa_tracy, frame_static.render_targets.msaa, frame_static.render_targets.postprocess, ClearColor_Do, ClearDepth_Do );
	}
	else {
		frame_static.nonworld_opaque_outlined_pass = AddRenderPass( &nonworld_opaque_outlined_tracy, frame_static.render_targets.postprocess_masked );
		frame_static.add_outlines_pass = AddRenderPass( &add_outlines_tracy, frame_static.render_targets.postprocess_onlycolor );
		frame_static.nonworld_opaque_pass = AddRenderPass( &nonworld_opaque_tracy, frame_static.render_targets.postprocess );
	}

	frame_static.transparent_pass = AddBarrierRenderPass( &transparent_tracy, frame_static.render_targets.postprocess );
	frame_static.add_silhouettes_pass = AddRenderPass( &silhouettes_tracy, frame_static.render_targets.postprocess );
	frame_static.ui_pass = AddUnsortedRenderPass( &ui_tracy, frame_static.render_targets.postprocess );
	frame_static.postprocess_pass = AddRenderPass( &postprocess_tracy, ClearColor_Do );
	frame_static.post_ui_pass = AddUnsortedRenderPass( &post_ui_tracy );
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
	const float near_plane = 4.0f;
	float cascade_dist[ 5 ];
	const u32 num_planes = ARRAY_COUNT( cascade_dist );
	const u32 num_cascades = num_planes - 1;
	const u32 num_corners = 4;

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
			u32 shadowmap_size = frame_static.shadow_parameters.resolution;
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

	float t = 1.0f;
	if( client_gs.gameState.sun_moved_from != client_gs.gameState.sun_moved_to ) {
		t = Unlerp01( client_gs.gameState.sun_moved_from, cls.gametime, client_gs.gameState.sun_moved_to );
	}
	Vec3 sun_angles = LerpAngles( client_gs.gameState.sun_angles_from, t, client_gs.gameState.sun_angles_to );
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

const VertexDescriptor & FullscreenMeshVertexDescriptor() {
	return fullscreen_mesh.vertex_descriptor;
}

void DrawDynamicMesh( const PipelineState & pipeline, const DynamicMesh & mesh ) {
	if( dynamic_geometry.num_vertices + mesh.num_vertices > DynamicGeometry::MaxVerts ) {
		Com_Printf( S_COLOR_YELLOW "Too much dynamic geometry!\n" );
		return;
	}

	WriteAndFlushStreamingBuffer( dynamic_geometry.positions_buffer, mesh.positions, mesh.num_vertices, dynamic_geometry.num_vertices );
	WriteAndFlushStreamingBuffer( dynamic_geometry.uvs_buffer, mesh.uvs, mesh.num_vertices, dynamic_geometry.num_vertices );
	WriteAndFlushStreamingBuffer( dynamic_geometry.colors_buffer, mesh.colors, mesh.num_vertices, dynamic_geometry.num_vertices );
	WriteAndFlushStreamingBuffer( dynamic_geometry.index_buffer, mesh.indices, mesh.num_indices, dynamic_geometry.num_indices );

	size_t first_index = dynamic_geometry.num_indices + FrameSlot() * DynamicGeometry::MaxVerts;
	size_t base_vertex = dynamic_geometry.num_vertices + FrameSlot() * DynamicGeometry::MaxVerts;
	DrawMesh( dynamic_geometry.mesh, pipeline, mesh.num_indices, first_index, base_vertex );

	dynamic_geometry.num_vertices += mesh.num_vertices;
	dynamic_geometry.num_indices += mesh.num_indices;
}

const VertexDescriptor & DynamicMeshVertexDescriptor() {
	return dynamic_geometry.mesh.vertex_descriptor;
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
	bg->PushTextureID( ImGuiShaderAndMaterial( material ) );
	bg->PrimReserve( 6, 4 );
	bg->PrimRectUV( Vec2( x, y ), Vec2( x + w, y + h ), topleft_uv, bottomright_uv, IM_COL32( rgba.r, rgba.g, rgba.b, rgba.a ) );
	bg->PopTextureID();
}

UniformBlock UploadModelUniforms( const Mat4 & M ) {
	return UploadUniformBlock( M );
}

UniformBlock UploadMaterialStaticUniforms( const Vec2 & texture_size, float specular, float shininess ) {
	return UploadUniformBlock( texture_size, specular, shininess );
}

UniformBlock UploadMaterialDynamicUniforms( const Vec4 & color, Vec3 tcmod_row0, Vec3 tcmod_row1 ) {
	return UploadUniformBlock( color, tcmod_row0, tcmod_row1 );
}
