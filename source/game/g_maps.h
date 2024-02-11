#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

void InitServerCollisionModels();
void ShutdownServerCollisionModels();

bool LoadServerMap( const char * name );

struct MapData;
struct MapSubModelCollisionData;

const MapData * FindServerMap( StringHash name );
const MapSubModelCollisionData * FindServerMapSubModelCollisionData( StringHash name );

struct SyncEntityState;
MinMax3 ServerEntityBounds( const SyncEntityState * ent );

struct CollisionModelStorage;
const CollisionModelStorage * ServerCollisionModelStorage();
