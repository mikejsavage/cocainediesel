#pragma once

#include "qcommon/types.h"

constexpr u32 SPIRV_MAGIC = 0x07230203

struct SPIRVHeader {
	u32 magic;
	u32 version;
	u32 generator;
	u32 bound;
	u32 zero;
};
