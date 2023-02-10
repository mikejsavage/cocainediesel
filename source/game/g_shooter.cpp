#include "game/g_local.h"
#include "gameshared/gs_weapons.h"

static void Shooter_Think( edict_t * shooter ) {
	G_FireWeapon( shooter, shooter->s.weapon );
	const WeaponDef * weapon = GS_GetWeaponDef( shooter->s.weapon );
	shooter->nextThink = level.time + Max2( weapon->reload_time, weapon->refire_time );
}

void SP_shooter( edict_t * shooter, const spawn_temp_t * st ) {
	shooter->think = Shooter_Think;
	shooter->s.team = Team_One;
	shooter->s.svflags &= ~SVF_NOCLIENT;
	GClip_LinkEntity( shooter );
	TempAllocator temp = svs.frame_arena.temp();

	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		const WeaponDef * weapon = GS_GetWeaponDef( i );
		if( st->weapon == StringHash( weapon->short_name ) ) {
			shooter->s.weapon = i;
			shooter->s.model = StringHash( temp( "weapons/{}/model", weapon->short_name ) );
			if( weapon->speed == 0 ) {
				Com_Printf( S_COLOR_YELLOW "shooter with hitscan weapon, disabling.\n" );
			} else {
				shooter->nextThink = level.time + Max2( weapon->reload_time, weapon->refire_time );
			}
			return;
		}
	}
}
