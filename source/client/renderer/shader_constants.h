#pragma once

#include "qcommon/types.h"

// mirror this in BuildShaderSrcs
constexpr u32 FORWARD_PLUS_TILE_SIZE = 32;
constexpr u32 FORWARD_PLUS_TILE_CAPACITY = 50;
constexpr float DLIGHT_CUTOFF = 0.5f;
constexpr u32 SKINNED_MODEL_MAX_JOINTS = 100;

enum ParticleFlags : u32 {
	ParticleFlag_CollisionPoint = 1_u32 << 0_u32,
	ParticleFlag_CollisionSphere = 1_u32 << 1_u32,
	ParticleFlag_Rotate = 1_u32 << 2_u32,
	ParticleFlag_Stretch = 1_u32 << 3_u32,
};
