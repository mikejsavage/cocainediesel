#include "game/g_local.h"

void SP_speaker_wall( edict_t * speaker, const spawn_temp_t * st ) {
	speaker->s.svflags &= ~SVF_NOCLIENT;
	speaker->r.solid = SOLID_NOT;

	speaker->s.model = "models/speakers/speaker_wall";
	speaker->s.type = ET_SPEAKER;

	GClip_LinkEntity( speaker );
}
