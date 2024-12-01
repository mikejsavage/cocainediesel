#pragma once

#include "qcommon/types.h"

struct CompileShadersSettings {
	bool optimize;
};

struct ArenaAllocator;
bool CompileShaders( ArenaAllocator * a, const CompileShadersSettings & settings );

struct GraphicsShaderDescriptor;
struct ComputeShaderDescriptor;
Span< const char > ShaderFilename( Allocator * a, const GraphicsShaderDescriptor & shader );
Span< const char > ShaderFilename( Allocator * a, const ComputeShaderDescriptor & shader );
