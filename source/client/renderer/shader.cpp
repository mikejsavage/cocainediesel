#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/array.h"
#include "client/client.h"
#include "client/renderer/renderer.h"

Shaders shaders;

static void BuildShaderSrcs( const char * path, const char * defines, DynamicArray< const char * > * srcs, DynamicArray< int > * lengths ) {
	srcs->clear();
	lengths->clear();

	if( defines != NULL ) {
		srcs->add( defines );
		lengths->add( -1 );
	}

	Span< const char > glsl = AssetString( path );
	const char * ptr = glsl.ptr;
	if( ptr == NULL ) {
		// TODO
	}

	while( true ) {
		const char * before_include = strstr( ptr, "#include" );
		if( before_include == NULL )
			break;

		if( before_include != ptr ) {
			srcs->add( ptr );
			lengths->add( before_include - ptr );
		}

		ptr = before_include + strlen( "#include" );

		Span< const char > include = ParseToken( &ptr, Parse_StopOnNewLine );
		StringHash hash = StringHash( Hash64( include.ptr, include.n, Hash64( "glsl/" ) ) );

		Span< const char > contents = AssetString( hash );
		if( contents.ptr == NULL ) {
			// TODO
		}

		srcs->add( contents.ptr );
		lengths->add( -1 );
	}

	srcs->add( ptr );
	lengths->add( -1 );
}

static void ReplaceShader( Shader * shader, Span< const char * > srcs, Span< int > lens ) {
	Shader new_shader;
	if( !NewShader( &new_shader, srcs, lens ) )
		return;

	DeleteShader( *shader );
	*shader = new_shader;
}

static void LoadShaders() {
	TempAllocator temp = cls.frame_arena.temp();
	DynamicArray< const char * > srcs( &temp );
	DynamicArray< int > lengths( &temp );

	BuildShaderSrcs( "glsl/standard.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.standard, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define VERTEX_COLORS 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.standard_vertexcolors, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define SKINNED 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.standard_skinned, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define SKINNED 1\n#define VERTEX_COLORS 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.standard_skinned_vertexcolors, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define ALPHA_TEST 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.standard_alphatest, srcs.span(), lengths.span() );

	const char * world_defines =
		"#define APPLY_DRAWFLAT 1\n"
		"#define APPLY_FOG 1\n";
	BuildShaderSrcs( "glsl/standard.glsl", world_defines, &srcs, &lengths );
	ReplaceShader( &shaders.world, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/write_world_gbuffer.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.write_world_gbuffer, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/postprocess_world_gbuffer.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.postprocess_world_gbuffer, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/postprocess_world_gbuffer.glsl", "#define MSAA 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.postprocess_world_gbuffer_msaa, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/write_silhouette_gbuffer.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.write_silhouette_gbuffer, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/write_silhouette_gbuffer.glsl", "#define SKINNED 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.write_silhouette_gbuffer_skinned, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/postprocess_silhouette_gbuffer.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.postprocess_silhouette_gbuffer, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/blur.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.blur, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/outline.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.outline, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/outline.glsl", "#define SKINNED 1\n", &srcs, &lengths );
	ReplaceShader( &shaders.outline_skinned, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/particle.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.particle, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/skybox.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.skybox, srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/text.glsl", NULL, &srcs, &lengths );
	ReplaceShader( &shaders.text, srcs.span(), lengths.span() );
}

void InitShaders() {
	shaders = { };
	LoadShaders();
}

void HotloadShaders() {
	bool need_hotload = false;
	for( const char * path : ModifiedAssetPaths() ) {
		const char * ext = COM_FileExtension( path );
		if( ext != NULL && strcmp( ext, ".glsl" ) == 0 ) {
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
