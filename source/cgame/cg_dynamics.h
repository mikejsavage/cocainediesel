#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

void InitDecals();
void ShutdownDecals();

void DrawDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, float height = 0.0f );
void DrawDynamicLight( Vec3 origin, Vec4 color, float intensity );

void AddPersistentDecal( Vec3 origin, Vec3 normal, float radius, float angle, StringHash name, Vec4 color, s64 duration, float height = 0.0f );
void DrawPersistentDecals();

void AddPersistentDynamicLight( Vec3 origin, Vec4 color, float intensity, s64 duration );
void DrawPersistentDynamicLights();

void AllocateDecalBuffers();
void UploadDecalBuffers();
void AddDynamicsToPipeline( PipelineState * pipeline );
