#pragma once

#include "qcommon/types.h"

struct CompileShadersSettings {
	bool optimize;
};

bool CompileShaders( ArenaAllocator * a, const CompileShadersSettings & settings );
