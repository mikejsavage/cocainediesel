#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"

Shaders shaders;

static const char * FindInclude( Span< const char > str ) {
	while( true ) {
		const char * before_hash = ( const char * ) memchr( str.ptr, '#', str.n );
		if( before_hash == NULL )
			break;

		str += before_hash - str.ptr;
		if( StartsWith( str, "#include" ) )
			return str.ptr;

		if( str.n == 0 )
			break;
		str++;
	}

	return NULL;
}

static Span< Span< const char > > BuildShaderSrcs( TempAllocator * temp, const char * path, const char * defines ) {
	TracyZoneScoped;
	TracyZoneText( path, strlen( path ) );

	NonRAIIDynamicArray< Span< const char > > srcs( temp );

	if( defines != NULL ) {
		srcs.add( MakeSpan( defines ) );
	}

	Span< const char > glsl = AssetString( path );
	if( glsl.ptr == NULL ) {
		// TODO
	}

	Span< const char > cursor = glsl;

	srcs.add( MakeSpan( "#line 1\n" ) );

	while( true ) {
		const char * before_include = FindInclude( cursor );
		if( before_include == NULL )
			break;

		size_t length_to_include = before_include - cursor.ptr;
		if( length_to_include > 0 ) {
			srcs.add( cursor.slice( 0, length_to_include ) );
		}

		cursor += length_to_include + strlen( "#include" );

		Span< const char > include = ParseToken( &cursor, Parse_StopOnNewLine );
		StringHash hash = StringHash( Hash64( include.ptr, include.n, Hash64( "glsl/" ) ) );

		Span< const char > contents = AssetString( hash );
		if( contents.ptr == NULL ) {
			// TODO
		}

		srcs.add( MakeSpan( "#line 1\n" ) );
		srcs.add( contents );
		srcs.add( MakeSpan( "#line 1\n" ) );
	}

	if( cursor.n > 0 ) {
		srcs.add( cursor );
	}

	return srcs.span();
}

static void LoadShader( Shader * shader, const char * path, const char * defines = NULL, bool particle_vertex_attribs = false ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();
	Span< Span< const char > > srcs = BuildShaderSrcs( &temp, path, defines );

	Shader new_shader;
	if( !NewShader( &new_shader, srcs, particle_vertex_attribs ) )
		return;

	DeleteShader( *shader );
	*shader = new_shader;
}

static void LoadComputeShader( Shader * shader, const char * path, const char * defines = NULL ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();
	Span< Span< const char > > srcs = BuildShaderSrcs( &temp, path, defines );

	Shader new_shader;
	if( !NewComputeShader( &new_shader, srcs ) )
		return;

	DeleteShader( *shader );
	*shader = new_shader;
}

static void LoadShaders() {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	// standard
	LoadShader( &shaders.standard, "glsl/standard.glsl" );
	LoadShader( &shaders.standard_vertexcolors, "glsl/standard.glsl", "#define VERTEX_COLORS 1\n" );
	LoadShader( &shaders.standard_skinned, "glsl/standard.glsl", "#define SKINNED 1\n" );
	LoadShader( &shaders.standard_skinned_vertexcolors, "glsl/standard.glsl", "#define SKINNED 1\n#define VERTEX_COLORS 1\n" );

	const char * standard_shaded_defines = temp(
		"#define APPLY_DLIGHTS 1\n"
		"#define SHADED 1\n"
		"#define TILE_SIZE {}\n"
		"#define DLIGHT_CUTOFF {}\n", TILE_SIZE, DLIGHT_CUTOFF );
	LoadShader( &shaders.standard_shaded, "glsl/standard.glsl", standard_shaded_defines );

	const char * standard_skinned_shaded_defines = temp(
		"#define SKINNED 1\n"
		"#define APPLY_DLIGHTS 1\n"
		"#define SHADED 1\n"
		"#define TILE_SIZE {}\n"
		"#define DLIGHT_CUTOFF {}\n", TILE_SIZE, DLIGHT_CUTOFF );
	LoadShader( &shaders.standard_skinned_shaded, "glsl/standard.glsl", standard_skinned_shaded_defines );

	// standard instanced
	LoadShader( &shaders.standard_instanced, "glsl/standard.glsl", "#define INSTANCED 1\n" );
	LoadShader( &shaders.standard_vertexcolors_instanced, "glsl/standard.glsl", "#define VERTEX_COLORS 1\n" );

	const char * standard_shaded_instanced_defines = temp(
		"#define INSTANCED 1\n"
		"#define APPLY_DLIGHTS 1\n"
		"#define SHADED 1\n"
		"#define TILE_SIZE {}\n"
		"#define DLIGHT_CUTOFF {}\n", TILE_SIZE, DLIGHT_CUTOFF );
	LoadShader( &shaders.standard_shaded_instanced, "glsl/standard.glsl", standard_shaded_instanced_defines );

	// rest
	const char * world_defines = temp(
		"#define APPLY_DRAWFLAT 1\n"
		"#define APPLY_FOG 1\n"
		"#define APPLY_DECALS 1\n"
		"#define APPLY_DLIGHTS 1\n"
		"#define APPLY_SHADOWS 1\n"
		"#define SHADED 1\n"
		"#define TILE_SIZE {}\n"
		"#define DLIGHT_CUTOFF {}\n", TILE_SIZE, DLIGHT_CUTOFF );
	LoadShader( &shaders.world, "glsl/standard.glsl", world_defines );

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
	LoadShader( &shaders.blur, "glsl/blur.glsl" );
	LoadShader( &shaders.postprocess, "glsl/postprocess.glsl" );

	LoadComputeShader( &shaders.particle_compute, "glsl/particle_compute.glsl", NULL );
	LoadComputeShader( &shaders.particle_setup_indirect, "glsl/particle_setup_indirect.glsl", NULL );
	LoadShader( &shaders.particle, "glsl/particle.glsl", NULL, true );
	LoadShader( &shaders.particle_model, "glsl/particle.glsl", "#define MODEL 1\n", true );

	const char * culling_defines = temp( "#define TILE_SIZE {}\n", TILE_SIZE );
	LoadComputeShader( &shaders.tile_culling, "glsl/tile_culling.glsl", culling_defines );
}

void InitShaders() {
	shaders = { };
	LoadShaders();
}

void HotloadShaders() {
	bool need_hotload = false;
	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".glsl" ) {
			need_hotload = true;
			break;
		}
	}

	if( need_hotload ) {
		LoadShaders();
	}
}

void ShutdownShaders() {
	DeleteShader( shaders.text );
}
