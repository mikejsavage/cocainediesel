#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/api.h"

void InitDecals();
void ResetDecals();

void DrawDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, float height = 0.0f );
void DrawLight( Vec3 origin, Vec3 color, float intensity );

void AddPersistentDecal( Vec3 origin, Quaternion orientation, float radius, StringHash name, Vec4 color, Time lifetime, float height = 0.0f );
void DrawPersistentDecals();

void AddPersistentLight( Vec3 origin, Vec3 color, float intensity, Time lifetime );
void DrawPersistentLights();

void AllocateDecalBuffers();
void UploadDecalBuffers();

struct DynamicsBuffers {
	GPUBuffer tile_counts;
	GPUBuffer decals;
	GPUBuffer lights;
	GPUBuffer decal_tiles;
	GPUBuffer light_tiles;
};

DynamicsBuffers GetDynamicsBuffers();
