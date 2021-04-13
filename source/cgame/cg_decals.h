#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

void InitDecals();
void ShutdownDecals();

void DrawDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, float height = 0.0f );

void AddPersistentDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, s64 duration, float height = 0.0f );
void DrawPersistentDecals();

void AllocateDecalBuffers();
void UploadDecalBuffers();
void AddDecalsToPipeline( PipelineState * pipeline );
