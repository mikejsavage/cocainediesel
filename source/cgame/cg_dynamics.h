#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

void InitDecals();
void ResetDecals();
void ShutdownDecals();

void DrawDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, float height = 0.0f );
void DrawDynamicLight( Vec3 origin, Vec3 color, float intensity );

void AddPersistentDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, Time lifetime, float height = 0.0f );
void DrawPersistentDecals();

void AddPersistentDynamicLight( Vec3 origin, Vec3 color, float intensity, Time lifetime );
void DrawPersistentDynamicLights();

void AllocateDecalBuffers();
void UploadDecalBuffers();

struct PipelineState;
void AddDynamicsToPipeline( PipelineState * pipeline );
