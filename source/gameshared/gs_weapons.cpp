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

/*
* GS_TraceBullet
*/
trace_t *GS_TraceBullet( const gs_state_t * gs, trace_t *trace, vec3_t start, vec3_t dir, vec3_t right, vec3_t up, float r, float u, int range, int ignore, int timeDelta ) {
	vec3_t end;
	bool water = false;
	vec3_t water_start;
	int content_mask = MASK_SHOT | MASK_WATER;
	static trace_t water_trace;

	assert( trace );

	VectorMA( start, range, dir, end );
	if( r ) {
		VectorMA( end, r, right, end );
	}
	if( u ) {
		VectorMA( end, u, up, end );
	}

	if( gs->api.PointContents( start, timeDelta ) & MASK_WATER ) {
		water = true;
		VectorCopy( start, water_start );
		content_mask &= ~MASK_WATER;
	}

	gs->api.Trace( trace, start, vec3_origin, vec3_origin, end, ignore, content_mask, timeDelta );

	// see if we hit water
	if( trace->contents & MASK_WATER ) {
		water_trace = *trace;

		VectorCopy( trace->endpos, water_start );

		// re-trace ignoring water this time
		gs->api.Trace( trace, water_start, vec3_origin, vec3_origin, end, ignore, MASK_SHOT, timeDelta );

		return &water_trace;
	}

	if( water ) {
		water_trace = *trace;
		VectorCopy( water_start, water_trace.endpos );
		return &water_trace;
	}

	return NULL;
}

void GS_TraceLaserBeam( const gs_state_t * gs, trace_t *trace, vec3_t origin, vec3_t angles, float range, int ignore, int timeDelta, void ( *impact )( trace_t *tr, vec3_t dir ) ) {
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


//============================================================
//
//		PLAYER WEAPON THINKING
//
//============================================================

WeaponType GS_SelectBestWeapon( const SyncPlayerState * player ) {
	for( int weapon = Weapon_Count - 1; weapon >= 0; weapon-- ) {
		if( !player->weapons[ weapon ].owned )
			continue;
		return WeaponType( weapon );
	}
	return Weapon_Count;
}

static bool GS_CheckAmmoInWeapon( const SyncPlayerState * player, WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );
	return def->clip_size == 0 || player->weapons[ weapon ].ammo > 0;
}

/*
* GS_ThinkPlayerWeapon
*/
int GS_ThinkPlayerWeapon( const gs_state_t * gs, SyncPlayerState * player, int buttons, int msecs, int timeDelta ) {
	bool refire = false;

	assert( player->pending_weapon <= Weapon_Count );

	if( GS_MatchPaused( gs ) ) {
		return player->weapon;
	}

	if( player->weapon == Weapon_Count ) {
		player->weapon = player->pending_weapon;
		return player->weapon;
	}

	if( player->pmove.pm_type != PM_NORMAL ) {
		player->weapon_state = WEAPON_STATE_READY;
		player->pending_weapon = Weapon_Count;
		player->weapon = Weapon_Count;
		player->weapon_time = 0;
		return player->weapon;
	}

	if( player->pmove.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
		buttons = 0;
	}

	if( !msecs ) {
		return player->weapon;
	}

	player->weapon_time = Max2( 0, player->weapon_time - msecs );

	const WeaponDef * def = GS_GetWeaponDef( player->weapon );

	// during cool-down time it can shoot again or go into reload time
	if( player->weapon_state == WEAPON_STATE_REFIRE ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		refire = true;

		player->weapon_state = WEAPON_STATE_READY;
	}

	// there is a weapon to be changed
	if( player->weapon != player->pending_weapon ) {
		if( player->weapon_state == WEAPON_STATE_READY || player->weapon_state == WEAPON_STATE_ACTIVATING || player->weapon_state == WEAPON_STATE_RELOADING ) {
			player->weapon_state = WEAPON_STATE_DROPPING;
			player->weapon_time = def->weapondown_time;

			if( def->weapondown_time ) {
				gs->api.PredictedEvent( player->POVnum, EV_WEAPONDROP, 0 );
			}
		}
	}

	if( player->weapon_state == WEAPON_STATE_RELOADING ) {
		if( player->weapon_time > 0 ) {
			if( ( buttons & BUTTON_ATTACK ) != 0 && GS_CheckAmmoInWeapon( player, player->weapon ) ) {
				player->weapon_time = 0;
				player->weapon_state = WEAPON_STATE_READY;
			}
			else {
				return player->weapon;
			}
		}
		else {
			player->weapons[ player->weapon ].ammo = def->clip_size;
			player->weapon_state = WEAPON_STATE_READY;
			gs->api.PredictedEvent( player->POVnum, EV_WEAPONACTIVATE, player->weapon << 1 );
		}
	}

	// do the change
	if( player->weapon_state == WEAPON_STATE_DROPPING ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		bool had_weapon_before = player->weapon != Weapon_Count;
		player->weapon = player->pending_weapon;

		// update the firedef
		def = GS_GetWeaponDef( player->weapon );
		player->weapon_state = WEAPON_STATE_ACTIVATING;
		player->weapon_time = def->weaponup_time;

		int parm = player->weapon << 1;
		if( !had_weapon_before )
			parm |= 1;

		gs->api.PredictedEvent( player->POVnum, EV_WEAPONACTIVATE, parm );
	}

	if( player->weapon_state == WEAPON_STATE_ACTIVATING ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		if( player->weapons[ player->weapon ].ammo == 0 ) {
			player->weapon_time = def->reload_time;
			player->weapon_state = WEAPON_STATE_RELOADING;
		}
		else {
			player->weapon_state = WEAPON_STATE_READY;
		}
	}

	if( player->weapon_state == WEAPON_STATE_READY ) {
		if( player->weapon_time > 0 ) {
			return player->weapon;
		}

		if( !GS_ShootingDisabled( gs ) ) {
			if( buttons & BUTTON_ATTACK ) {
				if( GS_CheckAmmoInWeapon( player, player->weapon ) ) {
					player->weapon_time = def->refire_time;
					player->weapon_state = WEAPON_STATE_REFIRE;

					if( refire && def->smooth_refire ) {
						gs->api.PredictedEvent( player->POVnum, EV_SMOOTHREFIREWEAPON, player->weapon );
					} else {
						gs->api.PredictedEvent( player->POVnum, EV_FIREWEAPON, player->weapon );
					}

					if( def->clip_size > 0 ) {
						player->weapons[ player->weapon ].ammo--;
						if( player->weapons[ player->weapon ].ammo == 0 ) {
							gs->api.PredictedEvent( player->POVnum, EV_NOAMMOCLICK, 0 );
							player->weapon_time = def->reload_time;
							player->weapon_state = WEAPON_STATE_RELOADING;
						}
					}
				}
			}
			else if( buttons & BUTTON_RELOAD ) {
				player->weapon_time = def->reload_time;
				player->weapon_state = WEAPON_STATE_RELOADING;
			}
		}
	}

	return player->weapon;
}
