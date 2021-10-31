#pragma once

#include "qcommon/types.h"

void InitSprays();
void AddSpray( Vec3 origin, Vec3 normal, Vec3 angles, float scale, u64 entropy );
void DrawSprays();
