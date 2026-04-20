#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_shared.h"
#include "client/renderer/shader_variants.h"

Shaders shaders;
static FSChangeMonitor * fs_change_monitor;

static Span< const char > ShaderFilename( Allocator * a, Span< const char > src_filename, Span< Span< const char > > features ) {
       DynamicString filename( a, "shaders/{}", src_filename );
       for( Span< const char > feature : features ) {
               filename.append( "_{}", feature );
       }
       return CloneSpan( a, filename.span() );
}

static void LoadShaders( const ShaderDescriptors & desc, bool hotload ) {
	TempAllocator temp = cls.frame_arena.temp();

	for( const GraphicsShaderDescriptor & shader : desc.graphics_shaders ) {
		shaders.*shader.field = NewRenderPipeline( RenderPipelineConfig {
			.path = ShaderFilename( &temp, shader.src, shader.features ),
			.output_format = shader.output_format,
			.blend_func = shader.blend_func,
			.clamp_depth = shader.clamp_depth,
			.alpha_to_coverage = shader.alpha_to_coverage,
			.mesh_variants = shader.mesh_variants,
		}, hotload ? Optional( shaders.*shader.field ) : NONE );
	}

	for( const ComputeShaderDescriptor & shader : desc.compute_shaders ) {
		shaders.*shader.field = NewComputePipeline( StripExtension( temp( "shaders/{}", shader.src ) ), hotload ? Optional( shaders.*shader.field ) : NONE );
	}
}

void InitShaders() {
	shaders = { };
	VisitShaderDescriptors< void >( LoadShaders, false );

	fs_change_monitor = NewFSChangeMonitor( sys_allocator, "source/client/renderer/hlsl" );
}

void ShutdownShaders() {
	DeleteFSChangeMonitor( sys_allocator, fs_change_monitor );
}

void HotloadShaders() {
	if( Cvar_Bool( "cl_hotloadAssets" ) ) {
		// run the build system if a shader source file changes
		TempAllocator temp = cls.frame_arena.temp();
		const char * buf[ 1024 ];
		Span< const char * > changes = PollFSChangeMonitor( &temp, fs_change_monitor, buf, ARRAY_COUNT( buf ) );

		if( changes.n > 0 ) {
#if PLATFORM_WINDOWS
			constexpr const char * ninja_path = ".\\ggbuild\\ninja.exe";
#elif PLATFORM_MACOS
			constexpr const char * ninja_path = "./ggbuild/ninja.macos";
#elif PLATFORM_LINUX
			constexpr const char * ninja_path = "./ggbuild/ninja.linux";
#else
#error new platform
#endif
			Com_Printf( "Recompiling shaders...\n" );
			if( system( ninja_path ) == -1 ) {
				Com_Printf( "%s failed\n", ninja_path );
			}
		}
	}

	bool hotload = false;
	for( Span< const char > path : ModifiedAssetPaths() ) {
		constexpr Span< const char > ShaderExtension = IFDEF( PLATFORM_MACOS ) ? ".metallib"_sp : ".spv"_sp;
		if( StrEqual( FileExtension( path ), ShaderExtension ) ) {
			hotload = true;
			break;
		}
	}

	if( hotload ) {
		VisitShaderDescriptors< void >( LoadShaders, true );
	}
}
