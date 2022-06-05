#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/cdmap.h"
#include "gameshared/cdmap.h"

struct CollisionModel;

struct Map {
	const char * name;
	u64 base_hash;
	MapData data;
	MapSharedRenderData render_data;

	CollisionModel * cms;
};

void InitMaps();
void ShutdownMaps();

void HotloadMaps();

bool AddMap( Span< const u8 > data, const char * path );

const Map * FindMap( StringHash name );
const Map * FindMap( const char * name );

const MapSubModelRenderData * FindMapSubModelRenderData( StringHash name );
