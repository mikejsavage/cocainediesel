#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

void InitDecals();
void ShutdownDecals();

void AddDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color );

void UploadDecalBuffers();
void AddDecalsToPipeline( PipelineState * pipeline );
