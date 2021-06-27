#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/types.h"

struct CollisionModel;

struct Map {
	const char * name;
	u64 base_hash;

	Model * models;
	u32 num_models;

	float fog_strength;

	CollisionModel * cms;

	TextureBuffer nodeBuffer;
	TextureBuffer leafBuffer;
	TextureBuffer brushBuffer;
	TextureBuffer planeBuffer;
};

void InitMaps();
void ShutdownMaps();

void HotloadMaps();

bool AddMap( Span< const u8 > data, const char * path );

const Map * FindMap( StringHash name );
const Map * FindMap( const char * name );

