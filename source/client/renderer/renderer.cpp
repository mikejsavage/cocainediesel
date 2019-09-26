#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/renderer/blue_noise.h"
#include "client/renderer/skybox.h"
#include "client/renderer/text.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

FrameStatic frame_static;

static Texture blue_noise;

static Mesh fullscreen_mesh;

static constexpr size_t MaxDynamicVerts = 100000;
static Mesh dynamic_geometry_mesh;
static u32 dynamic_geometry_num_vertices;
static u32 dynamic_geometry_num_indices;

static char last_screenshot_date[ 256 ];
static int same_date_count;

static u32 last_viewport_width, last_viewport_height;
static u32 last_msaa;

static void TakeScreenshot() {
	RGB8 * buf = ALLOC_MANY( sys_allocator, RGB8, frame_static.viewport_width * frame_static.viewport_height );
	defer { FREE( sys_allocator, buf ); };
	DownloadFramebuffer( buf );

	char date[ 256 ];
	Sys_FormatTime( date, sizeof( date ), "%y%m%d_%H%M%S" );

	TempAllocator temp = cls.frame_arena->temp();
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
	RenderBackendInit();

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
		config.format = TextureFormat_R_U8Norm; // TODO: wtf is u8norm
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

static void CreateFramebuffers() {
	DeleteFramebuffers();

	TextureConfig texture_config;
	texture_config.width = frame_static.viewport_width;
	texture_config.height = frame_static.viewport_height;

	{
		FramebufferConfig fb;

		texture_config.format = TextureFormat_RG_Half;
		fb.normal_attachment = texture_config;

		texture_config.format = TextureFormat_Depth;
		fb.depth_attachment = texture_config;

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

		texture_config.format = TextureFormat_RGB_U8_sRGB;
		fb.albedo_attachment = texture_config;

		texture_config.format = TextureFormat_Depth;
		fb.depth_attachment = texture_config;

		fb.msaa_samples = 0;

		frame_static.msaa_fb = NewFramebuffer( fb );
	}
}

void RendererBeginFrame( u32 viewport_width, u32 viewport_height ) {
	bool msaa = false;

	RenderBackendBeginFrame();

	dynamic_geometry_num_vertices = 0;
	dynamic_geometry_num_indices = 0;

	frame_static.viewport_width = viewport_width;
	frame_static.viewport_height = viewport_height;
	frame_static.viewport = Vec2( viewport_width, viewport_height );
	frame_static.aspect_ratio = float( viewport_width ) / float( viewport_height );

	if( viewport_width != last_viewport_width || viewport_height != last_viewport_height ) {
		CreateFramebuffers();
		last_viewport_width = viewport_width;
		last_viewport_height = viewport_height;
	}

	frame_static.ortho_view_uniforms = UploadViewUniforms( Mat4::Identity(), OrthographicProjection( 0, 0, viewport_width, viewport_height, -1, 1 ), Vec3( 0 ), -1 );
	frame_static.identity_model_uniforms = UploadUniformBlock( Mat4::Identity(), Vec4( 1 ) );

	frame_static.blue_noise_uniforms = UploadUniformBlock( Vec2( blue_noise.width, blue_noise.height ) );

	frame_static.world_write_gbuffer_pass = AddRenderPass( "Write world gbuffer", frame_static.world_gbuffer, ClearColor_Dont, ClearDepth_Do );

	if( msaa ) {
		frame_static.world_opaque_pass = AddRenderPass( "Render world opaque", frame_static.msaa_fb, ClearColor_Do, ClearDepth_Do );
		frame_static.world_postprocess_gbuffer_pass = AddRenderPass( "Postprocess world gbuffer", frame_static.msaa_fb );
	}
	else {
		frame_static.world_opaque_pass = AddRenderPass( "Render world opaque", ClearColor_Do, ClearDepth_Do );
		frame_static.world_postprocess_gbuffer_pass = AddRenderPass( "Postprocess world gbuffer" );
	}

	// frame_static.teammate_write_gbuffer_pass = AddRenderPass( "Write teammate gbuffer", ClearColor_Dont, ClearDepth_Do );
	// frame_static.teammate_postprocess_gbuffer_pass = AddRenderPass( "Postprocess teammate gbuffer" );

	frame_static.nonworld_opaque_pass = AddRenderPass( "Render nonworld opaque" );
	frame_static.transparent_pass = AddRenderPass( "Render transparent" );

	frame_static.sky_pass = AddRenderPass( "Render sky" );

	if( msaa ) {
		AddResolveMSAAPass( frame_static.msaa_fb );
	}

	frame_static.blur_pass = AddRenderPass( "Blur screen" );
	frame_static.ui_pass = AddRenderPass( "Render UI" );
}

void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov ) {
	float near_plane = 4.0f;

	frame_static.V = ViewMatrix( position, angles );
	frame_static.P = PerspectiveProjection( vertical_fov, frame_static.aspect_ratio, near_plane );
	frame_static.position = position;

	frame_static.view_uniforms = UploadViewUniforms( frame_static.V, frame_static.P, position, near_plane );
}

void RendererSubmitFrame() {
	RenderBackendSubmitFrame();
}

bool HasAlpha( TextureFormat format ) {
	return
		format == TextureFormat_A_U8
		|| format == TextureFormat_RGBA_U8
		|| format == TextureFormat_RGBA_U8_sRGB
		|| format == TextureFormat_RGBA_Float;
}

PipelineState MaterialToPipelineState( const Material * material, bool skinned ) {
	if( material == &world_material ) {
		PipelineState pipeline;
		pipeline.shader = &shaders.world;
		pipeline.pass = frame_static.world_opaque_pass;
		return pipeline;
	}

	PipelineState pipeline;
	pipeline.pass = material->blend_func == BlendFunc_Disabled ? frame_static.nonworld_opaque_pass : frame_static.transparent_pass;
	pipeline.cull_face = material->double_sided ? CullFace_Disabled : CullFace_Back;
	pipeline.blend_func = material->blend_func;
	pipeline.set_texture( "u_BaseTexture", material->textures[ 0 ].texture );
	pipeline.set_uniform( "u_Material", UploadUniformBlock( Vec4( 0 ), Vec4( 0 ), Vec2( material->textures[ 0 ].texture.width, material->textures[ 0 ].texture.height ), material->alpha_cutoff ) );

	if( material->alpha_cutoff > 0 ) {
		pipeline.shader = &shaders.standard_alphatest;
	}
	else if( skinned ) {
		pipeline.shader = &shaders.standard_skinned;
	}
	else {
		pipeline.shader = &shaders.standard;
	}

	return pipeline;
}

Texture BlueNoiseTexture() {
	return blue_noise;
}

void DrawFullscreenMesh( const PipelineState & pipeline ) {
	DrawMesh( fullscreen_mesh, pipeline );
}

u32 DynamicMeshBaseIndex() {
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

void Draw2DBox( u8 render_pass, float x, float y, float w, float h, Texture texture, Vec4 color ) {
	PipelineState pipeline;
	pipeline.pass = render_pass;
	pipeline.shader = &shaders.standard;
	pipeline.depth_func = DepthFunc_Disabled;
	pipeline.write_depth = false;

	if( HasAlpha( texture.format ) || color.w < 1 ) {
		pipeline.blend_func = BlendFunc_Blend;
	}

	Mat4 transform = Mat4Translation( x, y, 0 ) * Mat4Scale( w, h, 0 );
	pipeline.set_uniform( "u_Model", UploadModelUniforms( transform, color ) );
	pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
	pipeline.set_texture( "u_BaseTexture", texture );
	pipeline.set_uniform( "u_Material", UploadUniformBlock( Vec4( 0 ), Vec4( 0 ), Vec2( texture.width, texture.height ), 0.0f ) );

	Vec2 half_pixel = 0.5f / Vec2( texture.width, texture.height );
	RGBA8 c = RGBA8( color );
	u32 base_index = DynamicMeshBaseIndex();

	constexpr Vec3 positions[] = {
		Vec3( 0, 0, 0 ),
		Vec3( 1, 0, 0 ),
		Vec3( 0, 1, 0 ),
		Vec3( 1, 1, 0 ),
	};
	Vec2 uvs[] = {
		half_pixel,
		Vec2( half_pixel.x, 1.0f - half_pixel.y ),
		Vec2( 1.0f - half_pixel.x, half_pixel.y ),
		Vec2( 1.0f - half_pixel.x, 1.0f - half_pixel.y ),
	};
	RGBA8 colors[] = { c, c, c, c };

	u16 indices[] = { 0, 2, 1, 3, 1, 2 };
	for( u16 & idx : indices ) {
		idx += base_index;
	}

	DynamicMesh mesh;
	mesh.positions = positions;
	mesh.uvs = uvs;
	mesh.colors = colors;
	mesh.indices = indices;
	mesh.num_vertices = 4;
	mesh.num_indices = 6;

	DrawDynamicMesh( pipeline, mesh );
}

void Draw2DBox( u8 render_pass, float x, float y, float w, float h, const Material * material, Vec4 color ) {
	Draw2DBox( render_pass, x, y, w, h, material->textures[ 0 ].texture, color );
}

UniformBlock UploadViewUniforms( const Mat4 & V, const Mat4 & P, const Vec3 & camera_pos, float near_plane ) {
	return UploadUniformBlock( V, P, camera_pos, near_plane );
}

UniformBlock UploadModelUniforms( const Mat4 & M, const Vec4 & color ) {
	return UploadUniformBlock( M, color );
}
