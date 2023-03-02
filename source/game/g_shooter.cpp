#include "game/g_local.h"
#include "gameshared/gs_weapons.h"

static void Shooter_Think( edict_t * shooter ) {
	const WeaponDef * weapon = GS_GetWeaponDef( shooter->s.weapon );
	if( weapon->speed == 0 ) { // hitscan
		edict_t * event = G_SpawnEvent( EV_FIREWEAPON, shooter->s.weapon, &shooter->s.origin );
		event->s.ownerNum = ENTNUM( shooter );
		event->s.origin2 = shooter->s.angles;
		event->s.team = shooter->s.team;
	}
	else {
		G_FireWeapon( shooter, shooter->s.weapon );
	}
	shooter->nextThink = level.time + 2000;
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
			shooter->nextThink = level.time + 2000;
			return;
		}
	}
}
