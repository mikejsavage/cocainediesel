/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "gameshared/q_arch.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_collision.h"
#include "gameshared/gs_public.h"

WeaponType MODToWeapon( int mod ) {
	switch( mod ) {
		case MOD_GUNBLADE: return Weapon_Knife;
		case MOD_PISTOL: return Weapon_Pistol;
		case MOD_MACHINEGUN: return Weapon_MachineGun;
		case MOD_DEAGLE: return Weapon_Deagle;
		case MOD_RIOTGUN: return Weapon_Shotgun;
		case MOD_ASSAULTRIFLE: return Weapon_AssaultRifle;
		case MOD_GRENADE: return Weapon_GrenadeLauncher;
		case MOD_ROCKET: return Weapon_RocketLauncher;
		case MOD_PLASMA: return Weapon_Plasma;
		case MOD_ELECTROBOLT: return Weapon_Railgun;
		case MOD_LASERGUN: return Weapon_Laser;
		case MOD_SNIPER: return Weapon_Sniper;
		case MOD_RIFLE: return Weapon_Rifle;
	}

	return Weapon_None;
}

void GS_TraceBullet( const gs_state_t * gs, trace_t * trace, trace_t * wallbang_trace, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up, float r, float u, int range, int ignore, int timeDelta ) {
	vec3_t end;
	VectorMA( start, range, dir, end );
	VectorMA( end, r, right, end );
	VectorMA( end, u, up, end );

	gs->api.Trace( trace, start, vec3_origin, vec3_origin, end, ignore, MASK_WALLBANG, timeDelta );

	if( wallbang_trace != NULL ) {
		gs->api.Trace( wallbang_trace, start, vec3_origin, vec3_origin, trace->endpos, ignore, MASK_SHOT, timeDelta );
	}
}

void GS_TraceLaserBeam( const gs_state_t * gs, trace_t * trace, const vec3_t origin, const vec3_t angles, float range, int ignore, int timeDelta, void ( *impact )( const trace_t * tr, const vec3_t dir ) ) {
	vec3_t dir, end;
	vec3_t mins = { -0.5, -0.5, -0.5 };
	vec3_t maxs = { 0.5, 0.5, 0.5 };

	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( origin, range, dir, end );

	trace->ent = 0;

	gs->api.Trace( trace, origin, mins, maxs, end, ignore, MASK_SHOT, timeDelta );
	if( trace->ent != -1 && impact != NULL ) {
		impact( trace, dir );
	}
}

SyncPlayerState::WeaponInfo * GS_FindWeapon( SyncPlayerState * player, WeaponType weapon ) {
	for( size_t i = 0; i < ARRAY_COUNT( player->weapons ); i++ ) {
		if( player->weapons[ i ].weapon == weapon ) {
			return &player->weapons[ i ];
		}
	}

	return NULL;
}

WeaponType GS_SelectBestWeapon( const SyncPlayerState * player ) {
	if( player->weapons[ 0 ].weapon == Weapon_Knife && player->weapons[ 1 ].weapon != Weapon_None ) {
		return player->weapons[ 1 ].weapon;
	}
	return player->weapons[ 0 ].weapon;
}

static bool GS_CheckAmmoInWeapon( SyncPlayerState * player, WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	const SyncPlayerState::WeaponInfo * found_weapon = GS_FindWeapon( player, weapon );
	if( found_weapon != NULL )
		return def->clip_size == 0 || found_weapon->ammo > 0;

	return false;
}

WeaponType GS_ThinkPlayerWeapon( const gs_state_t * gs, SyncPlayerState * player, const usercmd_t * cmd, int timeDelta ) {
	bool refire = false;

	assert( player->pending_weapon < Weapon_Count );

	if( GS_MatchPaused( gs ) ) {
		return player->weapon;
	}

	if( player->weapon == Weapon_None ) {
		player->weapon = player->pending_weapon;
		return player->weapon;
	}

	if( player->pmove.pm_type != PM_NORMAL ) {
		player->weapon_state = WeaponState_Ready;
		player->pending_weapon = Weapon_None;
		player->weapon = Weapon_None;
		player->weapon_time = 0;
		player->zoom_time = 0;
		return player->weapon;
	}

	int buttons = player->pmove.no_control_time > 0 ? 0 : cmd->buttons;

	if( cmd->msec == 0 ) {
		return player->weapon;
	}

	player->weapon_time = Max2( 0, player->weapon_time - cmd->msec );

	const WeaponDef * def = GS_GetWeaponDef( player->weapon );

	if( cmd->weaponSwitch != Weapon_None && GS_CanEquip( player, cmd->weaponSwitch ) ) {
		player->pending_weapon = cmd->weaponSwitch;
	}

	s16 last_zoom_time = player->zoom_time;
	bool can_zoom = ( player->weapon_state == WeaponState_Ready
			|| player->weapon_state == WeaponState_Firing
			|| player->weapon_state == WeaponState_FiringSemiAuto )
		&& ( player->pmove.features & PMFEAT_SCOPE );

	if( can_zoom && def->zoom_fov != 0 && ( buttons & BUTTON_SPECIAL ) != 0 ) {
		player->zoom_time = Min2( player->zoom_time + cmd->msec, ZOOMTIME );
		if( last_zoom_time == 0 ) {
			gs->api.PredictedEvent( player->POVnum, EV_ZOOM_IN, player->weapon );
		}
	}
	else {
		player->zoom_time = Max2( 0, player->zoom_time - cmd->msec );
		if( player->zoom_time == 0 && last_zoom_time != 0 ) {
			gs->api.PredictedEvent( player->POVnum, EV_ZOOM_OUT, player->weapon );
		}
	}

	if( player->weapon_state == WeaponState_FiringSemiAuto ) {
		if( ( buttons & BUTTON_ATTACK ) == 0 ) {
			player->weapon_state = WeaponState_Firing;
		}
		else if( player->weapon_time > 0 ) {
			return player->weapon;
		}
	}

	// during cool-down time it can shoot again or go into reload time
	if( player->weapon_state == WeaponState_Firing ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		refire = true;

		player->weapon_state = WeaponState_Ready;
	}

	// there is a weapon to be changed
	if( player->weapon != player->pending_weapon ) {
		if( player->weapon_state == WeaponState_Ready || player->weapon_state == WeaponState_SwitchingIn || player->weapon_state == WeaponState_Reloading || ( player->weapon_state == WeaponState_FiringSemiAuto && player->weapon_time == 0 ) ) {
			player->weapon_state = WeaponState_SwitchingOut;
			player->weapon_time = def->weapondown_time;
			gs->api.PredictedEvent( player->POVnum, EV_WEAPONDROP, 0 );
		}
	}

	SyncPlayerState::WeaponInfo * selected_weapon = GS_FindWeapon( player, player->weapon );

	if( player->weapon_state == WeaponState_Reloading ) {
		if( player->weapon_time > 0 ) {
			if( ( buttons & BUTTON_ATTACK ) != 0 && GS_CheckAmmoInWeapon( player, player->weapon ) ) {
				player->weapon_time = 0;
				player->weapon_state = WeaponState_Ready;
			}
			else {
				return selected_weapon->weapon;
			}
		}
		else if( def->staged_reloading ) {
			selected_weapon->ammo++;

			if( selected_weapon->ammo == def->clip_size ) {
				player->weapon_state = WeaponState_Ready;
			}
			else {
				player->weapon_time = def->reload_time;
			}
		}
		else {
			selected_weapon->ammo = def->clip_size;
			player->weapon_state = WeaponState_Ready;
			gs->api.PredictedEvent( player->POVnum, EV_WEAPONACTIVATE, player->weapon << 1 );
		}
	}

	// do the change
	if( player->weapon_state == WeaponState_SwitchingOut ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		bool had_weapon_before = player->weapon != Weapon_None;
		player->weapon = player->pending_weapon;

		// update the firedef
		def = GS_GetWeaponDef( player->weapon );
		player->weapon_state = WeaponState_SwitchingIn;
		player->weapon_time = def->weaponup_time;

		u64 parm = player->weapon << 1;
		if( !had_weapon_before )
			parm |= 1;

		gs->api.PredictedEvent( player->POVnum, EV_WEAPONACTIVATE, parm );
	}

	if( player->weapon_state == WeaponState_SwitchingIn ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		player->weapon_state = WeaponState_Ready;
	}

	if( player->weapon_state == WeaponState_Ready ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		if( def->clip_size != 0 && selected_weapon->ammo == 0 ) {
			player->weapon_time = def->reload_time;
			player->weapon_state = WeaponState_Reloading;
		}

		if( !GS_ShootingDisabled( gs ) ) {
			if( buttons & BUTTON_ATTACK ) {
				if( GS_CheckAmmoInWeapon( player, player->weapon ) ) {
					player->weapon_time = def->refire_time;
					player->weapon_state = def->mode == FiringMode_SemiAuto ? WeaponState_FiringSemiAuto : WeaponState_Firing;

					if( refire && def->mode == FiringMode_Smooth ) {
						gs->api.PredictedEvent( player->POVnum, EV_SMOOTHREFIREWEAPON, player->weapon );
					}
					else {
						gs->api.PredictedFireWeapon( player->POVnum, player->weapon );
					}

					if( def->clip_size > 0 ) {
						selected_weapon->ammo--;
						if( selected_weapon->ammo == 0 ) {
							gs->api.PredictedEvent( player->POVnum, EV_NOAMMOCLICK, 0 );
						}
					}
				}
			}
			else if( ( buttons & BUTTON_RELOAD ) && def->clip_size != 0 && selected_weapon->ammo < def->clip_size ) {
				player->weapon_time = def->reload_time;
				player->weapon_state = WeaponState_Reloading;
			}
		}
	}

	return player->weapon;
}

bool GS_CanEquip( SyncPlayerState * player, WeaponType weapon ) {
	if( GS_FindWeapon( player, weapon ) != NULL )
		return ( player->pmove.features & PMFEAT_WEAPONSWITCH );

	return false;
}
