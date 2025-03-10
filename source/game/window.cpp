#include "game/g_local.h"

static void BreakWindow( edict_t * ent, edict_t * inflictor, edict_t * attacker, int topAssistorEntNo, DamageType damage_type, int damage ) {
	G_PositionedSound( ent->s.origin, "entities/window/shatter" );
	G_FreeEdict( ent );
}

static void TouchWindow( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	// if( other->s.type == ET_PLAYER && other->velocity > 1 ) {
	// }
}

void SP_window( edict_t * ent, const spawn_temp_t * st ) {
	ent->s.svflags &= ~SVF_NOCLIENT;
	ent->s.solidity = Solid_World | Solid_WeaponClip | Solid_PlayerClip;
	ent->takedamage = true;
	ent->health = 1;
	ent->touch = TouchWindow;
	ent->die = BreakWindow;
	GClip_LinkEntity( ent );
}
