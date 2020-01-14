#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/renderer/blue_noise.h"
#include "client/renderer/skybox.h"
#include "client/renderer/text.h"

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

static cvar_t * r_samples;

static void TakeScreenshot() {
	RGB8 * buf = ALLOC_MANY( sys_allocator, RGB8, frame_static.viewport_width * frame_static.viewport_height );
	defer { FREE( sys_allocator, buf ); };
	DownloadFramebuffer( buf );

	char date[ 256 ];
	Sys_FormatTime( date, sizeof( date ), "%y%m%d_%H%M%S" );

	TempAllocator temp = cls.frame_arena.temp();
	DynamicString filename( &temp );
	filename.append( "{}/screenshots/{}", FS_WriteDirectory(), date );

	if( strcmp( date, last_screenshot_date ) == 0 ) {
		same_date_count++;
		filename.append( "_{}", same_date_count );
	}
	else {
		same_date_count = 0;
	}

	strcpy( last_screenshot_date, date );

	filename.append( ".png" );

	stbi_flip_vertically_on_write( 1 );
	int ok = stbi_write_png( filename.c_str(), frame_static.viewport_width, frame_static.viewport_height, 3, buf, 0 );
	if( ok != 0 ) {
		Com_Printf( "Wrote %s\n", filename.c_str() );
	}
	else {
		Com_Printf( "Couldn't write %s\n", filename.c_str() );
	}
}

void InitRenderer() {
	ZoneScoped;

	RenderBackendInit();

	r_samples = Cvar_Get( "r_samples", "0", CVAR_ARCHIVE );

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
		config.positions = NewVertexBuffer( positions, sizeof( positions ) );
		config.num_vertices = 3;
		fullscreen_mesh = NewMesh( config );
	}

	{
		MeshConfig config;
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
}

static void DeleteFramebuffers() {
	DeleteFramebuffer( frame_static.world_gbuffer );
	DeleteFramebuffer( frame_static.world_outlines_fb );
	DeleteFramebuffer( frame_static.silhouette_gbuffer );
	DeleteFramebuffer( frame_static.silhouette_silhouettes_fb );
	DeleteFramebuffer( frame_static.msaa_fb );
}

void ShutdownRenderer() {
	ShutdownModels();
	ShutdownSkybox();
	ShutdownText();
	ShutdownMaterials();
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
	float tan_half_vertical_fov = tanf( DEG2RAD( vertical_fov_degrees ) / 2.0f );
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
	float pitch = DEG2RAD( angles.pitch );
	float sp = sinf( pitch );
	float cp = cosf( pitch );
	float yaw = DEG2RAD( angles.yaw );
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

static Mat4 InvertViewMatrix( const Mat4 & V, Vec3 position ) {
	return Mat4(
		// transpose rotation part
		Vec4( V.row0().xyz(), 0.0f ),
		Vec4( V.row1().xyz(), 0.0f ),
		Vec4( V.row2().xyz(), 0.0f ),

		Vec4( position, 1.0f )
	);
}

static UniformBlock UploadViewUniforms( const Mat4 & V, const Mat4 & inverse_V, const Mat4 & P, const Mat4 & inverse_P, const Vec3 & camera_pos, const Vec2 & viewport_size, float near_plane, int samples ) {
	return UploadUniformBlock( V, inverse_V, P, inverse_P, camera_pos, viewport_size, near_plane, samples );
}

static void CreateFramebuffers() {
	DeleteFramebuffers();

	TextureConfig texture_config;
	texture_config.width = frame_static.viewport_width;
	texture_config.height = frame_static.viewport_height;
	texture_config.wrap = TextureWrap_Clamp;

	{
		FramebufferConfig fb;

		texture_config.format = TextureFormat_RG_Half;
		fb.normal_attachment = texture_config;

		texture_config.format = TextureFormat_Depth;
		fb.depth_attachment = texture_config;

		fb.msaa_samples = frame_static.msaa_samples;

		frame_static.world_gbuffer = NewFramebuffer( fb );
	}

	{
		FramebufferConfig fb;

		texture_config.format = TextureFormat_R_U8;
		fb.albedo_attachment = texture_config;

		frame_static.world_outlines_fb = NewFramebuffer( fb );
	}

	{
		FramebufferConfig fb;

		texture_config.format = TextureFormat_RGBA_U8_sRGB;
		fb.albedo_attachment = texture_config;

		frame_static.silhouette_gbuffer = NewFramebuffer( fb );
		frame_static.silhouette_silhouettes_fb = NewFramebuffer( fb );
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
}

void RendererBeginFrame( u32 viewport_width, u32 viewport_height ) {
	HotloadShaders();
	HotloadMaterials();

	RenderBackendBeginFrame();

	dynamic_geometry_num_vertices = 0;
	dynamic_geometry_num_indices = 0;

	frame_static.viewport_width = Max2( u32( 1 ), viewport_width );
	frame_static.viewport_height = Max2( u32( 1 ), viewport_height );
	frame_static.viewport = Vec2( frame_static.viewport_width, frame_static.viewport_height );
	frame_static.aspect_ratio = float( viewport_width ) / float( viewport_height );
	frame_static.msaa_samples = r_samples->integer;

	if( viewport_width != last_viewport_width || viewport_height != last_viewport_height || frame_static.msaa_samples != last_msaa ) {
		CreateFramebuffers();
		last_viewport_width = viewport_width;
		last_viewport_height = viewport_height;
		last_msaa = frame_static.msaa_samples;
	}

	bool msaa = frame_static.msaa_samples;

	frame_static.ortho_view_uniforms = UploadViewUniforms( Mat4::Identity(), Mat4::Identity(), OrthographicProjection( 0, 0, viewport_width, viewport_height, -1, 1 ), Mat4::Identity(), Vec3( 0 ), frame_static.viewport, -1, frame_static.msaa_samples );
	frame_static.identity_model_uniforms = UploadModelUniforms( Mat4::Identity() );
	frame_static.identity_material_uniforms = UploadMaterialUniforms( vec4_white, Vec2( 0 ), 0.0f );

	frame_static.blue_noise_uniforms = UploadUniformBlock( Vec2( blue_noise.width, blue_noise.height ) );

	frame_static.write_world_gbuffer_pass = AddRenderPass( "Write world gbuffer", frame_static.world_gbuffer, ClearColor_Do, ClearDepth_Do );
	frame_static.postprocess_world_gbuffer_pass = AddRenderPass( "Postprocess world gbuffer", frame_static.world_outlines_fb );

	if( msaa ) {
		frame_static.world_opaque_pass = AddRenderPass( "Render world opaque", frame_static.msaa_fb, ClearColor_Do, ClearDepth_Do );
		frame_static.add_world_outlines_pass = AddRenderPass( "Render world outlines", frame_static.msaa_fb );
	}
	else {
		frame_static.world_opaque_pass = AddRenderPass( "Render world opaque", ClearColor_Do, ClearDepth_Do );
		frame_static.add_world_outlines_pass = AddRenderPass( "Render world outlines" );
	}

	frame_static.write_silhouette_gbuffer_pass = AddRenderPass( "Write silhouette gbuffer", frame_static.silhouette_gbuffer, ClearColor_Do, ClearDepth_Dont );
	frame_static.postprocess_silhouette_gbuffer_pass = AddRenderPass( "Postprocess silhouette gbuffer", frame_static.silhouette_silhouettes_fb );

	if( msaa ) {
		frame_static.nonworld_opaque_pass = AddRenderPass( "Render nonworld opaque", frame_static.msaa_fb );
		frame_static.sky_pass = AddRenderPass( "Render sky", frame_static.msaa_fb );
		frame_static.transparent_pass = AddRenderPass( "Render transparent", frame_static.msaa_fb );

		AddResolveMSAAPass( frame_static.msaa_fb );
	}
	else {
		frame_static.nonworld_opaque_pass = AddRenderPass( "Render nonworld opaque" );
		frame_static.sky_pass = AddRenderPass( "Render sky" );
		frame_static.transparent_pass = AddRenderPass( "Render transparent" );
	}

	frame_static.add_silhouettes_pass = AddRenderPass( "Render silhouettes" );
	frame_static.blur_pass = AddRenderPass( "Blur screen" );
	frame_static.ui_pass = AddUnsortedRenderPass( "Render UI" );
}

void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov ) {
	float near_plane = 4.0f;

	frame_static.V = ViewMatrix( position, angles );
	frame_static.inverse_V = InvertViewMatrix( frame_static.V, position );
	frame_static.P = PerspectiveProjection( vertical_fov, frame_static.aspect_ratio, near_plane );
	frame_static.inverse_P = InvertPerspectiveProjection( frame_static.P );
	frame_static.position = position;

	frame_static.view_uniforms = UploadViewUniforms( frame_static.V, frame_static.inverse_V, frame_static.P, frame_static.inverse_P, position, frame_static.viewport, near_plane, frame_static.msaa_samples );
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
	Vec2 half_pixel = 0.5f / Vec2( material->texture->width, material->texture->height );
	Draw2DBoxUV( x, y, w, h, half_pixel, 1.0f - half_pixel, material, color );
}

void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, const Material * material, Vec4 color ) {
	if( w <= 0.0f || h <= 0.0f )
		return;

	RGBA8 rgba = RGBA8( color );

	ImDrawList * bg = ImGui::GetBackgroundDrawList();
	bg->PushTextureID( ImGuiShaderAndMaterial( material ) );
	bg->PrimReserve( 6, 4 );
	bg->PrimRectUV( Vec2( x, y ), Vec2( x + w, y + h ), topleft_uv, bottomright_uv, IM_COL32( rgba.r, rgba.g, rgba.b, rgba.a ) );
	bg->PopTextureID();
}

UniformBlock UploadModelUniforms( const Mat4 & M ) {
	return UploadUniformBlock( M );
}

UniformBlock UploadMaterialUniforms( const Vec4 & color, const Vec2 & texture_size, float alpha_cutoff, Vec3 tcmod_row0, Vec3 tcmod_row1 ) {
	return UploadUniformBlock( color, tcmod_row0, tcmod_row1, texture_size, alpha_cutoff );
}
