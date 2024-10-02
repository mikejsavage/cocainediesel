#include "game/g_local.h"

void SP_cinematic_mapname( edict_t * mapname, const spawn_temp_t * st ) {
	mapname->s.svflags &= ~SVF_NOCLIENT;
	mapname->s.solidity = Solid_NotSolid;
	mapname->s.type = ET_CINEMATIC_MAPNAME;
	GClip_LinkEntity( mapname );
}
