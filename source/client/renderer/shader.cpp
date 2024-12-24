#include "qcommon/base.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_shared.h"
#include "client/renderer/shader_variants.h"
#include "tools/shadercompiler/api.h"

Shaders shaders;

static void LoadShaders( const ShaderDescriptors & desc ) {
	TempAllocator temp = cls.frame_arena.temp();

	// TODO: hotloading
	for( const GraphicsShaderDescriptor & shader : desc.graphics_shaders ) {
		shaders.*shader.field = NewRenderPipeline( RenderPipelineConfig {
			.path = ShaderFilename( &temp, shader ),
			.output_format = { }, // TODO
			.blend_func = { }, // TODO
			.alpha_to_coverage = false,
			.mesh_variants = shader.mesh_variants,
		} );
	}

	for( const ComputeShaderDescriptor & shader : desc.compute_shaders ) {
		shaders.*shader.field = NewComputePipeline( ShaderFilename( shader ) );
	}
}

void InitShaders() {
	shaders = { };
	VisitShaderDescriptors< void >( LoadShaders );
}

void HotloadShaders() {
	bool need_hotload = false;
	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".hlsl" ) {
			need_hotload = true;
			break;
		}
	}

	if( need_hotload ) {
		VisitShaderDescriptors< void >( LoadShaders );
	}
}
