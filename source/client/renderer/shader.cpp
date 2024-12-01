#include "qcommon/base.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_shared.h"
#include "client/renderer/shader_variants.h"

Shaders shaders;

static void LoadShaders() {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	// standard
	LoadShader( &shaders.standard, "glsl/standard.glsl" );
	LoadShader( &shaders.standard_vertexcolors, "glsl/standard.glsl", "#define VERTEX_COLORS 1\n" );
	LoadShader( &shaders.standard_skinned, "glsl/standard.glsl", "#define SKINNED 1\n" );
	LoadShader( &shaders.standard_shaded, "glsl/standard.glsl", "#define APPLY_DLIGHTS 1\n#define SHADED 1\n" );
	LoadShader( &shaders.standard_skinned_shaded, "glsl/standard.glsl", "#define SKINNED 1\n#define APPLY_DLIGHTS 1\n#define SHADED 1\n" );

	// standard instanced
	LoadShader( &shaders.standard_instanced, "glsl/standard.glsl", "#define INSTANCED 1\n" );
	LoadShader( &shaders.standard_shaded_instanced, "glsl/standard.glsl", "#define INSTANCED 1\n#define APPLY_DLIGHTS 1\n#define SHADED 1\n" );

	// rest
	constexpr Span< const char > world_defines =
		"#define APPLY_DRAWFLAT 1\n"
		"#define APPLY_FOG 1\n"
		"#define APPLY_DECALS 1\n"
		"#define APPLY_DLIGHTS 1\n"
		"#define APPLY_SHADOWS 1\n"
		"#define SHADED 1\n";
	LoadShader( &shaders.world, "glsl/standard.glsl", world_defines );
	constexpr Span< const char > world_instanced_defines =
		"#define APPLY_DRAWFLAT 1\n"
		"#define APPLY_FOG 1\n"
		"#define APPLY_DECALS 1\n"
		"#define APPLY_DLIGHTS 1\n"
		"#define APPLY_SHADOWS 1\n"
		"#define SHADED 1\n"
		"#define INSTANCED 1\n";
	LoadShader( &shaders.world_instanced, "glsl/standard.glsl", world_instanced_defines );

	LoadShader( &shaders.depth_only, "glsl/depth_only.glsl" );
	LoadShader( &shaders.depth_only_instanced, "glsl/depth_only.glsl", "#define INSTANCED 1\n" );
	LoadShader( &shaders.depth_only_skinned, "glsl/depth_only.glsl", "#define SKINNED 1\n" );

	LoadShader( &shaders.postprocess_world_gbuffer, "glsl/postprocess_world_gbuffer.glsl" );
	LoadShader( &shaders.postprocess_world_gbuffer_msaa, "glsl/postprocess_world_gbuffer.glsl", "#define MSAA 1\n" );

	LoadShader( &shaders.write_silhouette_gbuffer, "glsl/write_silhouette_gbuffer.glsl" );
	LoadShader( &shaders.write_silhouette_gbuffer_instanced, "glsl/write_silhouette_gbuffer.glsl", "#define INSTANCED 1\n" );
	LoadShader( &shaders.write_silhouette_gbuffer_skinned, "glsl/write_silhouette_gbuffer.glsl", "#define SKINNED 1\n" );
	LoadShader( &shaders.postprocess_silhouette_gbuffer, "glsl/postprocess_silhouette_gbuffer.glsl" );

	LoadShader( &shaders.outline, "glsl/outline.glsl" );
	LoadShader( &shaders.outline_instanced, "glsl/outline.glsl", "#define INSTANCED 1\n" );
	LoadShader( &shaders.outline_skinned, "glsl/outline.glsl", "#define SKINNED 1\n" );

	LoadShader( &shaders.scope, "glsl/scope.glsl" );
	LoadShader( &shaders.skybox, "glsl/skybox.glsl" );
	LoadShader( &shaders.text, "glsl/text.glsl" );
	LoadShader( &shaders.text_alphatest, "glsl/text.glsl", "#define ALPHA_TEST 1\n" );
	LoadShader( &shaders.postprocess, "glsl/postprocess.glsl" );

	LoadComputeShader( &shaders.particle_compute, "glsl/particle_compute.glsl" );
	LoadComputeShader( &shaders.particle_setup_indirect, "glsl/particle_setup_indirect.glsl" );
	LoadShader( &shaders.particle, "glsl/particle.glsl" );

	LoadComputeShader( &shaders.tile_culling, "glsl/tile_culling.glsl" );
}

void InitShaders() {
	shaders = { };
	LoadShaders();
}

void HotloadShaders() {
	bool need_hotload = false;
	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".glsl" ) {
			need_hotload = true;
			break;
		}
	}

	if( need_hotload ) {
		LoadShaders();
	}
}
