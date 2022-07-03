#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"
#include "client/renderer/cdmap.h"
#include "gameshared/cdmap.h"

struct Map {
	const char * name;
	StringHash base_hash;
	MapData data;
	MapSharedRenderData render_data;
};

void InitMaps();
void ShutdownMaps();

void HotloadMaps();

bool AddMap( Span< const u8 > data, const char * path );

const Map * FindMap( StringHash name );
const Map * FindMap( const char * name );

const MapSubModelRenderData * FindMapSubModelRenderData( StringHash name );

struct MapSharedCollisionData;
const MapSharedCollisionData * FindClientMapSharedCollisionData( StringHash name );

struct MapSubModelCollisionData;
const MapSubModelCollisionData * FindClientMapSubModelCollisionData( StringHash name );

struct CollisionModelStorage;
const CollisionModelStorage * ClientCollisionModelStorage();
