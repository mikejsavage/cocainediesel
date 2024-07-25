#include "game/g_local.h"

void SP_speaker_wall( edict_t * speaker, const spawn_temp_t * st ) {
	speaker->s.svflags &= ~SVF_NOCLIENT;
	speaker->s.solidity = Solid_NotSolid;

	speaker->s.model = "entities/speaker/model";
	speaker->s.type = ET_SPEAKER;

	GClip_LinkEntity( speaker );
}
