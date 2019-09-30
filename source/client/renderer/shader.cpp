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

		Span< const char > include = ParseSpan( &ptr, true );
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

void InitShaders() {
	TempAllocator temp = cls.frame_arena->temp();
	DynamicArray< const char * > srcs( &temp );
	DynamicArray< int > lengths( &temp );

	BuildShaderSrcs( "glsl/standard.glsl", NULL, &srcs, &lengths );
	shaders.standard = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define VERTEX_COLORS 1\n", &srcs, &lengths );
	shaders.standard_vertexcolors = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define SKINNED 1\n", &srcs, &lengths );
	shaders.standard_skinned = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define SKINNED 1\n#define VERTEX_COLORS 1\n", &srcs, &lengths );
	shaders.standard_skinned_vertexcolors = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/standard.glsl", "#define ALPHA_TEST 1\n", &srcs, &lengths );
	shaders.standard_alphatest = NewShader( srcs.span(), lengths.span() );

	const char * world_defines =
		"#define APPLY_DRAWFLAT 1\n"
		"#define APPLY_FOG 1\n";
	BuildShaderSrcs( "glsl/standard.glsl", world_defines, &srcs, &lengths );
	shaders.world = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/world_write_gbuffer.glsl", NULL, &srcs, &lengths );
	shaders.world_write_gbuffer = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/world_postprocess_gbuffer.glsl", NULL, &srcs, &lengths );
	shaders.world_postprocess_gbuffer = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/teammate_write_gbuffer.glsl", "#define SKINNED 1\n", &srcs, &lengths );
	shaders.teammate_write_gbuffer_skinned = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/teammate_postprocess_gbuffer.glsl", NULL, &srcs, &lengths );
	shaders.teammate_postprocess_gbuffer = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/blur.glsl", NULL, &srcs, &lengths );
	shaders.blur = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/tonemap.glsl", NULL, &srcs, &lengths );
	shaders.tonemap = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/outline.glsl", NULL, &srcs, &lengths );
	shaders.outline = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/outline.glsl", "#define SKINNED 1\n", &srcs, &lengths );
	shaders.outline_skinned = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/particle.glsl", NULL, &srcs, &lengths );
	shaders.particle = NewShader( srcs.span(), lengths.span() );

	BuildShaderSrcs( "glsl/text.glsl", NULL, &srcs, &lengths );
	shaders.text = NewShader( srcs.span(), lengths.span() );
}

void ShutdownShaders() {
	DeleteShader( shaders.text );
}
