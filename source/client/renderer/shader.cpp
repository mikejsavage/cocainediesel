#include "qcommon/base.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_shared.h"
#include "client/renderer/shader_variants.h"

Shaders shaders;

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
}

void HotloadShaders() {
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
