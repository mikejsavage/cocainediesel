#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

struct CollisionModel;

struct Map {
	const char * name;
	u64 base_hash;

	u32 num_models;

	float fog_strength;

	CollisionModel * cms;
};

void InitMaps();
void ShutdownMaps();

bool AddMap( Span< const u8 > compressed, const char * path );

const Map * FindMap( StringHash name );
const Map * FindMap( const char * name );

