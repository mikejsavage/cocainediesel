#pragma once

#include "qcommon/types.h"

constexpr u32 SURF_NODRAW = 0x80;

void InitMaterials();
void ShutdownMaterials();

void GetMaterialFlags( Span< const char > name, u32 * surface_flags, u32 * content_flags );
bool IsNodrawMaterial( Span< const char > name );
