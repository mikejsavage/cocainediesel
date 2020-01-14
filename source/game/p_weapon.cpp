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

#include "game/g_local.h"

void SV_Physics_LinearProjectile( edict_t *ent );

#define PLASMAHACK // ffs : hack for the plasmagun

#ifdef PLASMAHACK
void W_Plasma_Backtrace( edict_t *ent, const vec3_t start );
#endif

/*
* G_ProjectileDistancePrestep
*/
static void G_ProjectileDistancePrestep( edict_t *projectile, float distance ) {
	float speed;
	vec3_t dir, dest;
	int mask, i;
	trace_t trace;
#ifdef PLASMAHACK
	vec3_t plasma_hack_start;
#endif

	if( projectile->movetype != MOVETYPE_TOSS
		&& projectile->movetype != MOVETYPE_LINEARPROJECTILE
		&& projectile->movetype != MOVETYPE_BOUNCE
		&& projectile->movetype != MOVETYPE_BOUNCEGRENADE ) {
		return;
	}

	if( !distance ) {
		return;
	}

	if( ( speed = VectorNormalize2( projectile->velocity, dir ) ) == 0.0f ) {
		return;
	}

	mask = ( projectile->r.clipmask ) ? projectile->r.clipmask : MASK_SHOT; // race trick should come set up inside clipmask

#ifdef PLASMAHACK
	VectorCopy( projectile->s.origin, plasma_hack_start );
#endif

	VectorMA( projectile->s.origin, distance, dir, dest );
	G_Trace4D( &trace, projectile->s.origin, projectile->r.mins, projectile->r.maxs, dest, projectile->r.owner, mask, projectile->timeDelta );

	for( i = 0; i < 3; i++ )
		projectile->s.origin[i] = projectile->olds.origin[i] = trace.endpos[i];

	GClip_LinkEntity( projectile );
	SV_Impact( projectile, &trace );

	// set initial water state
	if( !projectile->r.inuse ) {
		return;
	}

	projectile->waterlevel = ( G_PointContents4D( projectile->s.origin, projectile->timeDelta ) & MASK_WATER ) ? true : false;

	// ffs : hack for the plasmagun
#ifdef PLASMAHACK
	if( projectile->s.type == ET_PLASMA ) {
		W_Plasma_Backtrace( projectile, plasma_hack_start );
	}
#endif
}

static void G_Fire_Gunblade_Knife( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Knife );
	W_Fire_Blade( owner, def->range, origin, angles, def->damage, def->knockback, timeDelta );
}

static edict_t *G_Fire_Rocket( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_RocketLauncher );
	return W_Fire_Rocket( owner, origin, angles, def->speed, def->damage,
		def->minknockback, def->knockback, def->mindamage,
		def->splash_radius, def->range, timeDelta );
}

static void G_Fire_Machinegun( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_MachineGun );
	W_Fire_MG( owner, origin, angles, def->range, def->spread, def->damage, def->knockback, timeDelta );
}

static void G_Fire_Riotgun( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );
	W_Fire_Riotgun( owner, origin, angles, def->range, def->spread,
		def->projectile_count, def->damage, def->knockback, timeDelta );
}

static edict_t *G_Fire_Grenade( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_GrenadeLauncher );
	return W_Fire_Grenade( owner, origin, angles, def->speed, def->damage,
		def->minknockback, def->knockback, def->mindamage,
		def->splash_radius, def->range, timeDelta, true );
}

static edict_t *G_Fire_Plasma( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Plasma );
	return W_Fire_Plasma( owner, origin, angles, def->damage,
		def->minknockback, def->knockback, def->mindamage,
		def->splash_radius, def->speed, def->range, timeDelta );
}

static edict_t *G_Fire_Lasergun( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Laser );
	return W_Fire_Lasergun( owner, origin, angles, def->damage, def->knockback, def->range, timeDelta );
}

static void G_Fire_Bolt( vec3_t origin, vec3_t angles, edict_t *owner, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );
	W_Fire_Electrobolt( owner, origin, angles, def->damage,
		def->knockback, def->range, timeDelta );
}

/*
* G_FireWeapon
*/
void G_FireWeapon( edict_t *ent, int parm ) {
	vec3_t origin, angles;
	vec3_t viewoffset = { 0, 0, 0 };
	int timeDelta = 0;

	// find this shot projection source
	if( ent->r.client != NULL ) {
		viewoffset[2] += ent->r.client->ps.viewheight;
		timeDelta = ent->r.client->timeDelta;
		VectorCopy( ent->r.client->ps.viewangles, angles );
	}
	else {
		VectorCopy( ent->s.angles, angles );
	}

	VectorAdd( ent->s.origin, viewoffset, origin );

	// shoot
	edict_t * projectile = NULL;

	switch( parm ) {
		default:
			return;

		case Weapon_Knife:
			G_Fire_Gunblade_Knife( origin, angles, ent, timeDelta );
			break;

		case Weapon_MachineGun:
			G_Fire_Machinegun( origin, angles, ent, timeDelta );
			break;

		case Weapon_Shotgun:
			G_Fire_Riotgun( origin, angles, ent, timeDelta );
			break;

		case Weapon_GrenadeLauncher:
			projectile = G_Fire_Grenade( origin, angles, ent, timeDelta );
			break;

		case Weapon_RocketLauncher:
			projectile = G_Fire_Rocket( origin, angles, ent, timeDelta );
			break;

		case Weapon_Plasma:
			projectile = G_Fire_Plasma( origin, angles, ent, timeDelta );
			break;

		case Weapon_Laser:
			projectile = G_Fire_Lasergun( origin, angles, ent, timeDelta );
			break;

		case Weapon_Railgun:
			G_Fire_Bolt( origin, angles, ent, timeDelta );
			break;
	}

	// add stats
	if( ent->r.client != NULL ) {
		ent->r.client->level.stats.accuracy_shots[ parm ] += GS_GetWeaponDef( parm )->projectile_count;
	}

	if( projectile != NULL ) {
		G_ProjectileDistancePrestep( projectile, g_projectile_prestep->value );
		if( projectile->s.linearMovement )
			VectorCopy( projectile->s.origin, projectile->s.linearMovementBegin );
	}
}
