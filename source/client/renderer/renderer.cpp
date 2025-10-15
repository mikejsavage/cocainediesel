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
#include "client/renderer/private.h"
// #include "client/renderer/rgb_noise.h"
#include "client/renderer/skybox.h"
#include "client/renderer/text.h"

#include "client/maps.h"
#include "cgame/cg_dynamics.h"
#include "cgame/cg_particles.h"

#include "imgui/imgui.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

static PoolHandle< Texture > rgb_noise;
static PoolHandle< Texture > blue_noise;

static Mesh fullscreen_mesh;

static char last_screenshot_date[ 256 ];
static int same_date_count;

static u32 last_viewport_width, last_viewport_height;
static u32 last_msaa;
static ShadowQuality last_shadow_quality;

static Cvar * r_samples;
static Cvar * r_shadow_quality;

static void TakeScreenshot() {
	// RGB8 * framebuffer = AllocMany< RGB8 >( sys_allocator, frame_static.viewport_width * frame_static.viewport_height );
	// defer { Free( sys_allocator, framebuffer ); };
	// DownloadFramebuffer( framebuffer );
    //
	// stbi_flip_vertically_on_write( 1 );
    //
	// int ok = stbi_write_png_to_func( []( void * context, void * png, int png_size ) {
	// 	char date[ 256 ];
	// 	FormatCurrentTime( date, sizeof( date ), "%y%m%d_%H%M%S" );
    //
	// 	TempAllocator temp = cls.frame_arena.temp();
    //
	// 	DynamicString path( &temp, "{}/screenshots/{}", HomeDirPath(), date );
    //
	// 	if( StrEqual( date, last_screenshot_date ) ) {
	// 		same_date_count++;
	// 		path.append( "_{}", same_date_count );
	// 	}
	// 	else {
	// 		same_date_count = 0;
	// 	}
	// 	strcpy( last_screenshot_date, date );
    //
	// 	path.append( ".png" );
    //
	// 	if( WriteFile( &temp, path.c_str(), png, png_size ) ) {
	// 		Com_Printf( "Wrote %s\n", path.c_str() );
	// 	}
	// 	else {
	// 		Com_Printf( "Couldn't write %s\n", path.c_str() );
	// 	}
	// }, NULL, frame_static.viewport_width, frame_static.viewport_height, 3, framebuffer, 0 );
    //
	// if( ok == 0 ) {
	// 	Com_Printf( "Couldn't convert screenshot to PNG\n" );
	// }
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
	last_viewport_width = 0;
	last_viewport_height = 0;
	last_msaa = 0;
	last_shadow_quality = ShadowQuality_Count;

	// {
	// 	int w, h;
	// 	u8 * img = stbi_load_from_memory( rgb_noise_png, rgb_noise_png_len, &w, &h, NULL, 4 );
	// 	Assert( img != NULL );
    //
	// 	rgb_noise = NewTexture( TextureConfig {
	// 		.format = TextureFormat_RGBA_U8_sRGB,
	// 		.width = u32( w ),
	// 		.height = u32( h ),
	// 		.data = img,
	// 	} );
    //
	// 	stbi_image_free( img );
	// }

	{
		int w, h;
		u8 * img = stbi_load_from_memory( blue_noise_png, blue_noise_png_len, &w, &h, NULL, 1 );
		Assert( img != NULL );

		blue_noise = NewTexture( TextureConfig {
			.name = "Blue noise",
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

		fullscreen_mesh = { };
		fullscreen_mesh.vertex_descriptor.attributes[ VertexAttribute_Position ] = { VertexFormat_Floatx3 };
		fullscreen_mesh.vertex_descriptor.buffer_strides[ 0 ] = sizeof( Vec3 );
		fullscreen_mesh.vertex_buffers[ 0 ] = NewBuffer( "Fullscreen triangle vertices", positions );
	}

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

	ShutdownRenderBackend();
	ShutdownMaterials();

	RemoveCommand( "screenshot" );
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

static void CreateRenderTargets( bool first_time ) {
	// NOMERGE use NewRenderTargetTexture
	frame_static.render_targets.silhouette_mask = NewTexture( TextureConfig {
		.name = "Silhouette mask RT",
		.format = TextureFormat_RGBA_U8_sRGB,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
		.dedicated_allocation = true,
	}, first_time ? NONE : MakeOptional( frame_static.render_targets.silhouette_mask ) );

	frame_static.render_targets.curved_surface_mask = NewTexture( TextureConfig {
		.name = "Curved surface mask RT",
		.format = TextureFormat_R_UI8,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
		.msaa_samples = frame_static.msaa_samples,
		.dedicated_allocation = true,
	}, first_time ? NONE : MakeOptional( frame_static.render_targets.curved_surface_mask ) );

	if( frame_static.msaa_samples > 1 ) {
		frame_static.render_targets.msaa_color = NewTexture( TextureConfig {
			.name = "MSAA color RT",
			.format = TextureFormat_RGBA_U8_sRGB,
			.width = frame_static.viewport_width,
			.height = frame_static.viewport_height,
			.msaa_samples = frame_static.msaa_samples,
			.dedicated_allocation = true,
		}, frame_static.render_targets.msaa_color );

		frame_static.render_targets.msaa_depth = NewTexture( TextureConfig {
			.name = "MSAA depth RT",
			.format = TextureFormat_Depth,
			.width = frame_static.viewport_width,
			.height = frame_static.viewport_height,
			.msaa_samples = frame_static.msaa_samples,
			.dedicated_allocation = true,
		}, frame_static.render_targets.msaa_depth );
	}
	else {
		frame_static.render_targets.msaa_color = NONE;
		frame_static.render_targets.msaa_depth = NONE;
	}

	frame_static.render_targets.resolved_color = NewTexture( TextureConfig {
		.name = "Color RT",
		.format = TextureFormat_RGBA_U8_sRGB,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
		.dedicated_allocation = true,
	}, first_time ? NONE : MakeOptional( frame_static.render_targets.resolved_color ) );

	frame_static.render_targets.resolved_depth = NewTexture( TextureConfig {
		.name = "Depth RT",
		.format = TextureFormat_Depth,
		.width = frame_static.viewport_width,
		.height = frame_static.viewport_height,
		.dedicated_allocation = true,
	}, first_time ? NONE : MakeOptional( frame_static.render_targets.resolved_depth ) );

	frame_static.render_targets.shadowmap = NewTexture( TextureConfig {
		.name = "Shadowmap RT",
		.format = TextureFormat_Depth,
		.width = frame_static.shadow_parameters.resolution,
		.height = frame_static.shadow_parameters.resolution,
		.num_layers = frame_static.shadow_parameters.num_cascades,
		.dedicated_allocation = true,
	} );
}

void RendererBeginFrame( u32 viewport_width, u32 viewport_height ) {
	HotloadShaders();
	HotloadMaterials();
	HotloadGLTFModels();
	HotloadMaps();
	HotloadVisualEffects();

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
		CreateRenderTargets( last_viewport_width == 0 );
	}

	last_viewport_width = viewport_width;
	last_viewport_height = viewport_height;
	last_msaa = frame_static.msaa_samples;
	last_shadow_quality = frame_static.shadow_quality;

	frame_static.ortho_view_uniforms = NewTempBuffer( ViewUniforms {
		.V = Mat3x4::Identity(),
		.inverse_V = Mat3x4::Identity(),
		.P = OrthographicProjection( 0.0f, 0.0f, viewport_width, viewport_height, -1.0f, 1.0f ),
		.inverse_P = Mat4::Identity(),
		.camera_pos = Vec3( 0.0f ),
		.viewport_size = frame_static.viewport,
		.near_clip = -1.0f,
		.msaa_samples = frame_static.msaa_samples,
		.sun_direction = Vec3( 0.0f ),
	} );
	frame_static.identity_model_transform_uniforms = NewTempBuffer( Mat3x4::Identity() );
	frame_static.identity_material_properties_uniforms = NewTempBuffer( MaterialProperties { .shininess = 64.0f } );
	frame_static.identity_material_color_uniforms = NewTempBuffer( white.vec4 );
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

static void SetupShadowCascades() {
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
		shadow_camera_positions[ i ] = frustum_centers[ i ] - frame_static.sun_direction * radius;

		shadow_views[ i ] = ViewMatrix( shadow_camera_positions[ i ], frame_static.sun_direction );
		shadow_projections[ i ] = OrthographicProjection( -radius, radius, radius, -radius, 0.0f, radius * 2.0f );
	}

	ShadowmapUniforms uniforms = {
		.VP = shadow_views[ 0 ],
		.num_cascades = frame_static.shadow_parameters.num_cascades,
	};

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
		frame_static.shadowmap_view_uniforms[ i ] = NewTempBuffer( ViewUniforms {
			.V = shadow_view,
			.P = shadow_projection,
			.camera_pos = shadow_camera_position,
			.near_clip = cascade_dist[ i ],
			.sun_direction = frame_static.sun_direction,
		} );

		Mat4 inv_shadow_projection = InverseScaleTranslation( shadow_projection );

		Mat4 tex_scale_bias = Mat4( Mat4Translation( 0.5f, 0.5f, 0.0f ) * Mat4Scale( 0.5f, 0.5f, 1.0f ) );
		Mat4 inv_tex_scale_bias = InverseScaleTranslation( tex_scale_bias );

		Mat4 inv_cascade = Mat4( shadow_views[ 0 ] * inv_shadow_view ) * inv_shadow_projection * inv_tex_scale_bias;
		Vec3 cascade_corner = ( inv_cascade * Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xyz();
		Vec3 other_corner = ( inv_cascade * Vec4( 1.0f, 1.0f, 1.0f, 1.0f ) ).xyz();

		uniforms.cascades[ i ] = {
			.plane = cascade_dist[ i ],
			.offset = -cascade_corner,
			.scale = 1.0f / ( other_corner - cascade_corner ),
		};
	}

	frame_static.shadow_uniforms = NewTempBuffer( uniforms );
}

void RendererSetView( Vec3 position, EulerDegrees3 angles, float vertical_fov ) {
	TempAllocator temp = cls.frame_arena.temp();

	constexpr float near_plane = 4.0f;

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
	AngleVectors( sun_angles, &frame_static.sun_direction, NULL, NULL );

	SetupShadowCascades();

	frame_static.view_uniforms = NewTempBuffer( ViewUniforms {
		.V = frame_static.V,
		.inverse_V = frame_static.inverse_V,
		.P = frame_static.P,
		.inverse_P = frame_static.inverse_P,
		.camera_pos = position,
		.viewport_size = frame_static.viewport,
		.near_clip = near_plane,
		.msaa_samples = frame_static.msaa_samples,
		.sun_direction = frame_static.sun_direction,
	} );

	DynamicsResources dynamics_resources = GetDynamicsResources();

	BoundedDynamicArray< BufferBinding, 16 > standard_buffers = {
		{ "u_View", frame_static.view_uniforms },
		{ "u_Shadowmap", frame_static.shadow_uniforms },
		{ "u_TileCounts", dynamics_resources.tile_counts },
		{ "u_DecalTiles", dynamics_resources.decal_tiles },
		{ "u_LightTiles", dynamics_resources.light_tiles },
		{ "u_Decals", dynamics_resources.decals },
		{ "u_Lights", dynamics_resources.lights },
	};

	BoundedDynamicArray< GPUBindings::TextureBinding, 4 > standard_textures = {
		{ "u_BlueNoiseTexture", BlueNoiseTexture() },
		{ "u_ShadowmapTextureArray", frame_static.render_targets.shadowmap },
		{ "u_SpriteAtlas", SpriteAtlasTexture() },
	};

	BoundedDynamicArray< GPUBindings::SamplerBinding, 2 > standard_samplers = {
		{ "u_StandardSampler", Sampler_Standard },
		{ "u_ShadowmapSampler", Sampler_Shadowmap },
	};

	GPUBindings standard_bindings = {
		.buffers = standard_buffers.span(),
		.textures = standard_textures.span(),
		.samplers = standard_samplers.span(),
	};

	Vec4 clear_color = Vec4( 0.0f );
	float clear_depth = 1.0f;

	// RenderPass_ParticleUpdate
	// RenderPass_ParticleSetupIndirect
	// RenderPass_TileCulling

	for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
		frame_static.render_passes[ RenderPass_ShadowmapCascade0 + i ] = NewRenderPass( RenderPassConfig {
			.name = temp( "Shadowmap cascade {}", i ),
			.depth_target = RenderPassConfig::DepthTarget {
				.texture = frame_static.render_targets.shadowmap,
				.layer = i,
				.preserve_contents = false,
				.clear = clear_depth,
			},
			.representative_shader = shaders.depth_only,
			.bindings = {
				.buffers = { { "u_View", frame_static.shadowmap_view_uniforms[ i ] } },
			},
		} );
	}

	bool msaa = frame_static.msaa_samples > 1;
	PoolHandle< Texture > color_target = msaa ? Unwrap( frame_static.render_targets.msaa_color ) : frame_static.render_targets.resolved_color;
	PoolHandle< Texture > depth_target = msaa ? Unwrap( frame_static.render_targets.msaa_depth ) : frame_static.render_targets.resolved_depth;

	frame_static.render_passes[ RenderPass_WorldOpaqueZPrepass ] = NewRenderPass( RenderPassConfig {
		.name = "World Z-prepass",
		.depth_target = RenderPassConfig::DepthTarget {
			.texture = depth_target,
			.preserve_contents = false,
			.clear = clear_depth,
		},
		.representative_shader = shaders.depth_only,
		.bindings = {
			.buffers = { { "u_View", frame_static.view_uniforms } },
		},
	} );

	frame_static.render_passes[ RenderPass_WorldOpaque ] = NewRenderPass( RenderPassConfig {
		.name = "World opaque",
		.color_targets = {
			RenderPassConfig::ColorTarget {
				.texture = color_target,
				.preserve_contents = false,
				.clear = clear_color,
			},
			RenderPassConfig::ColorTarget {
				.texture = frame_static.render_targets.curved_surface_mask,
				.preserve_contents = false,
				.clear = clear_color,
			},
		},
		.depth_target = RenderPassConfig::DepthTarget { .texture = depth_target },
		.representative_shader = shaders.world,
		.bindings = standard_bindings,
	} );

	// RenderPass_Sky
	// RenderPass_SilhouetteGBuffer

	frame_static.render_passes[ RenderPass_NonworldOpaqueOutlined ] = NewRenderPass( RenderPassConfig {
		.name = "Nonworld opaque outlined",
		.color_targets = {
			RenderPassConfig::ColorTarget { .texture = color_target },
			RenderPassConfig::ColorTarget { .texture = frame_static.render_targets.curved_surface_mask },
		},
		.depth_target = RenderPassConfig::DepthTarget { .texture = depth_target },
		.representative_shader = shaders.world,
		.bindings = standard_bindings,
	} );

	// RenderPass_AddOutlines

	frame_static.render_passes[ RenderPass_NonworldOpaque ] = NewRenderPass( RenderPassConfig {
		.name = "Nonworld opaque",
		.color_targets = {
			RenderPassConfig::ColorTarget {
				.texture = frame_static.render_targets.resolved_color,
				.resolve_from = msaa ? frame_static.render_targets.msaa_color : NONE,
			},
		},
		.depth_target = RenderPassConfig::DepthTarget {
			.texture = frame_static.render_targets.resolved_depth,
			.resolve_from = msaa ? MakeOptional( frame_static.render_targets.resolved_depth ) : NONE,
		},
		.representative_shader = shaders.world,
		.bindings = standard_bindings,
	} );

	frame_static.render_passes[ RenderPass_Transparent ] = NewRenderPass( RenderPassConfig {
		.name = "Transparent",
		.color_targets = { RenderPassConfig::ColorTarget { .texture = frame_static.render_targets.resolved_color } },
		// .barrier = true, particle rendering happens here
	} );

	// RenderPass_AddSilhouettes

	frame_static.render_passes[ RenderPass_UIBeforePostprocessing ] = NewRenderPass( RenderPassConfig {
		.name = "UI before postprocessing",
		.color_targets = {
			RenderPassConfig::ColorTarget {
				.texture = frame_static.render_targets.resolved_color,
				.preserve_contents = false,
			},
		},
		.representative_shader = shaders.imgui,
		.bindings = {
			.buffers = { { "u_View", frame_static.ortho_view_uniforms } },
		},
	} );

	// RenderPass_Postprocessing

	frame_static.render_passes[ RenderPass_UIAfterPostprocessing ] = NewRenderPass( RenderPassConfig {
		.name = "UI after postprocessing",
		.color_targets = { RenderPassConfig::ColorTarget { .texture = NONE } },
		.representative_shader = shaders.imgui,
		.bindings = {
			.buffers = { { "u_View", frame_static.ortho_view_uniforms } },
		},
	} );
}

void EncodeComputeCall( RenderPass render_pass, PoolHandle< ComputePipeline > pipeline, u32 x, u32 y, u32 z, Span< const BufferBinding > buffers ) {
	EncodeComputeCall( frame_static.render_passes[ render_pass ], pipeline, x, y, z, buffers );
}

void EncodeIndirectComputeCall( RenderPass render_pass, PoolHandle< ComputePipeline > pipeline, GPUBuffer indirect_args, Span< const BufferBinding > buffers ) {
	EncodeIndirectComputeCall( frame_static.render_passes[ render_pass ], pipeline, indirect_args, buffers );
}

void Draw( RenderPass render_pass, const PipelineState & pipeline, Mesh mesh, Span< const BufferBinding > buffers, DrawCallExtras extras ) {
	EncodeDrawCall( frame_static.render_passes[ render_pass ], pipeline, mesh, buffers, extras );
}

void EncodeScissor( RenderPass render_pass, Optional< Scissor > scissor ) {
	EncodeScissor( frame_static.render_passes[ render_pass ], scissor );
}

void RendererEndFrame() {
	RenderBackendEndFrame();
}

PoolHandle< Texture > RGBNoiseTexture() {
	return rgb_noise;
}

PoolHandle< Texture > BlueNoiseTexture() {
	return blue_noise;
}

Mesh FullscreenMesh() {
	return fullscreen_mesh;
}

void Draw2DBox( float x, float y, float w, float h, PoolHandle< Material2 > material, Vec4 color ) {
	Vec2 half_pixel = HalfPixelSize( material );
	Draw2DBoxUV( x, y, w, h, half_pixel, 1.0f - half_pixel, material, color );
}

void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, PoolHandle< Material2 > material, Vec4 color ) {
	if( w <= 0.0f || h <= 0.0f || color.w <= 0.0f )
		return;

	RGBA8 rgba = LinearTosRGB( color );

	ImDrawList * bg = ImGui::GetBackgroundDrawList();
	bg->PushTextureID( ImGuiShaderAndMaterial( material ) );
	bg->PrimReserve( 6, 4 );
	bg->PrimRectUV( Vec2( x, y ), Vec2( x + w, y + h ), topleft_uv, bottomright_uv, IM_COL32( rgba.r, rgba.g, rgba.b, rgba.a ) );
	bg->PopTextureID();
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
