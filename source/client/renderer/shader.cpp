#include "qcommon/base.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_constants.h"

Shaders shaders;

static Span< const char > FindInclude( Span< const char > str ) {
	while( true ) {
		const char * before_hash = StrChr( str, '#' );
		if( before_hash == NULL )
			break;

		str += before_hash - str.ptr;
		if( StartsWith( str, "#include" ) )
			return str;

		if( str.n == 0 )
			break;
		str++;
	}

	return { };
}

static void BuildShaderSrcs( DynamicString * src, Span< const char > path, Span< const char > variant_switches ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	src->append( "#define FORWARD_PLUS_TILE_SIZE {}\n", FORWARD_PLUS_TILE_SIZE );
	src->append( "#define FORWARD_PLUS_TILE_CAPACITY {}\n", FORWARD_PLUS_TILE_CAPACITY );
	src->append( "#define DLIGHT_CUTOFF {}\n", DLIGHT_CUTOFF );
	src->append( "#define SKINNED_MODEL_MAX_JOINTS {}\n", SKINNED_MODEL_MAX_JOINTS );

	src->append( "#define PARTICLE_COLLISION_POINT {}u\n", ParticleFlag_CollisionPoint );
	src->append( "#define PARTICLE_COLLISION_SPHERE {}u\n", ParticleFlag_CollisionSphere );
	src->append( "#define PARTICLE_ROTATE {}u\n", ParticleFlag_Rotate );
	src->append( "#define PARTICLE_STRETCH {}u\n", ParticleFlag_Stretch );

	src->append( "const int VertexAttribute_Position = {};\n", VertexAttribute_Position );
	src->append( "const int VertexAttribute_Normal = {};\n", VertexAttribute_Normal );
	src->append( "const int VertexAttribute_TexCoord = {};\n", VertexAttribute_TexCoord );
	src->append( "const int VertexAttribute_Color = {};\n", VertexAttribute_Color );
	src->append( "const int VertexAttribute_JointIndices = {};\n", VertexAttribute_JointIndices );
	src->append( "const int VertexAttribute_JointWeights = {};\n", VertexAttribute_JointWeights );

	src->append( "const int FragmentShaderOutput_Albedo = {};\n", FragmentShaderOutput_Albedo );
	src->append( "const int FragmentShaderOutput_CurvedSurfaceMask = {};\n", FragmentShaderOutput_CurvedSurfaceMask );

	src->append( "{}", variant_switches );

	Span< const char > glsl = AssetString( path );
	if( glsl.ptr == NULL ) {
		// TODO
	}

	Span< const char > cursor = glsl;

	while( true ) {
		Span< const char > before_include = FindInclude( cursor );
		if( before_include.ptr == NULL )
			break;

		size_t length_up_to_include = before_include.ptr - cursor.ptr;
		if( length_up_to_include > 0 ) {
			src->append( "{}", cursor.slice( 0, length_up_to_include ) );
		}

		cursor += length_up_to_include + strlen( "#include" );

		Span< const char > include = ParseToken( &cursor, Parse_StopOnNewLine );
		StringHash hash = StringHash( Hash64( include.ptr, include.n, Hash64( "glsl/" ) ) );

		Span< const char > contents = AssetString( hash );
		if( contents.ptr == NULL ) {
			// TODO
		}

		src->append( "{}", contents );
	}

	if( cursor.n > 0 ) {
		src->append( "{}", cursor );
	}
}

static void LoadShader( Shader * shader, Span< const char > path, Span< const char > variant_switches = "" ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();
	DynamicString src( &temp );
	BuildShaderSrcs( &src, path, variant_switches );

	Shader new_shader;
	if( !NewShader( &new_shader, src.span(), path ) )
		return;

	DeleteShader( *shader );
	*shader = new_shader;
}

static void LoadComputeShader( Shader * shader, Span< const char > path, Span< const char > variant_switches = "" ) {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();
	DynamicString src( &temp );
	BuildShaderSrcs( &src, path, variant_switches );

	Shader new_shader;
	if( !NewComputeShader( &new_shader, src.span(), path ) )
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

void ShutdownShaders() {
	DeleteShader( shaders.standard );
	DeleteShader( shaders.standard_shaded );
	DeleteShader( shaders.standard_vertexcolors );

	DeleteShader( shaders.standard_skinned );
	DeleteShader( shaders.standard_skinned_shaded );

	DeleteShader( shaders.standard_instanced );
	DeleteShader( shaders.standard_shaded_instanced );

	DeleteShader( shaders.depth_only );
	DeleteShader( shaders.depth_only_instanced );
	DeleteShader( shaders.depth_only_skinned );

	DeleteShader( shaders.world );
	DeleteShader( shaders.postprocess_world_gbuffer );
	DeleteShader( shaders.postprocess_world_gbuffer_msaa );

	DeleteShader( shaders.write_silhouette_gbuffer );
	DeleteShader( shaders.write_silhouette_gbuffer_instanced );
	DeleteShader( shaders.write_silhouette_gbuffer_skinned );
	DeleteShader( shaders.postprocess_silhouette_gbuffer );

	DeleteShader( shaders.outline );
	DeleteShader( shaders.outline_instanced );
	DeleteShader( shaders.outline_skinned );

	DeleteShader( shaders.scope );

	DeleteShader( shaders.particle_compute );
	DeleteShader( shaders.particle_setup_indirect );

	DeleteShader( shaders.particle );

	DeleteShader( shaders.tile_culling );

	DeleteShader( shaders.skybox );

	DeleteShader( shaders.text );

	DeleteShader( shaders.postprocess );
}
