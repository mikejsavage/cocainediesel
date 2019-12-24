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

// g_weapon.c

#include "g_local.h"

void SV_Physics_LinearProjectile( edict_t *ent );

#define PLASMAHACK // ffs : hack for the plasmagun

#ifdef PLASMAHACK
void W_Plasma_Backtrace( edict_t *ent, const vec3_t start );
#endif

/*
* Use_Weapon
*/
void Use_Weapon( edict_t *ent, const gsitem_t *item ) {
	// invalid weapon item
	if( item->tag < WEAP_NONE || item->tag >= WEAP_TOTAL ) {
		return;
	}

	// see if we're already changing to it
	if( ent->r.client->ps.stats[STAT_PENDING_WEAPON] == item->tag ) {
		return;
	}

	// change to this weapon when down
	ent->r.client->ps.stats[STAT_PENDING_WEAPON] = item->tag;
}

//======================================================================
//
// WEAPON FIRING
//
//======================================================================

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

/*
* G_Fire_Gunblade_Knife
*/
static void G_Fire_Gunblade_Knife( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int range = firedef->timeout;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	W_Fire_Blade( owner, range, origin, angles, damage, knockback, timeDelta );
}


/*
* G_Fire_Rocket
*/
static edict_t *G_Fire_Rocket( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int speed = firedef->speed;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	int minDamage = firedef->mindamage;
	int minKnockback = firedef->minknockback;
	int radius = firedef->splash_radius;

	return W_Fire_Rocket( owner, origin, angles, speed, damage, minKnockback, knockback, minDamage,
		radius, firedef->timeout, timeDelta );
}

/*
* G_Fire_Machinegun
*/
static void G_Fire_Machinegun( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int range = firedef->timeout;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	W_Fire_MG( owner, origin, angles, range, firedef->spread, firedef->v_spread, damage, knockback, timeDelta );
}

/*
* G_Fire_Riotgun
*/
static void G_Fire_Riotgun( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int range = firedef->timeout;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	W_Fire_Riotgun( owner, origin, angles, range, firedef->spread, firedef->v_spread,
		firedef->projectile_count, damage, knockback, timeDelta );
}

/*
* G_Fire_Grenade
*/
static edict_t *G_Fire_Grenade( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int speed = firedef->speed;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	int minDamage = firedef->mindamage;
	int minKnockback = firedef->minknockback;
	int radius = firedef->splash_radius;
	return W_Fire_Grenade( owner, origin, angles, speed, damage, minKnockback, knockback,
		minDamage, radius, firedef->timeout, timeDelta, true );
}

/*
* G_Fire_Plasma
*/
static edict_t *G_Fire_Plasma( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int speed = firedef->speed;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	int minDamage = firedef->mindamage;
	int minKnockback = firedef->minknockback;
	int radius = firedef->splash_radius;
	return W_Fire_Plasma( owner, origin, angles, damage, minKnockback, knockback, minDamage, radius,
		speed, firedef->timeout, timeDelta );
}

/*
* G_Fire_Lasergun
*/
static edict_t *G_Fire_Lasergun( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int range = firedef->timeout;
	float damage = firedef->damage;
	int knockback = firedef->knockback;
	return W_Fire_Lasergun( owner, origin, angles, damage, knockback, range, timeDelta );
}

/*
* G_Fire_Bolt
*/
static void G_Fire_Bolt( vec3_t origin, vec3_t angles, firedef_t *firedef, edict_t *owner ) {
	int timeDelta = 0;
	if( owner && owner->r.client ) {
		timeDelta = owner->r.client->timeDelta;
	}

	int minDamageRange = firedef->timeout;
	float maxdamage = firedef->damage;
	int mindamage = firedef->mindamage;
	int maxknockback = firedef->knockback;
	int minknockback = firedef->minknockback;
	W_Fire_Electrobolt_FullInstant( owner, origin, angles, maxdamage, mindamage,
		maxknockback, minknockback, ELECTROBOLT_RANGE, minDamageRange, timeDelta );
}

/*
* G_FireWeapon
*/
void G_FireWeapon( edict_t *ent, int parm ) {
	gs_weapon_definition_t *weapondef;
	firedef_t *firedef;
	vec3_t origin, angles;
	vec3_t viewoffset = { 0, 0, 0 };

	weapondef = GS_GetWeaponDef( ( parm >> 1 ) & 0x3f );
	firedef = &weapondef->firedef;

	// find this shot projection source
	if( ent->r.client ) {
		viewoffset[2] += ent->r.client->ps.viewheight;
		VectorCopy( ent->r.client->ps.viewangles, angles );
	} else {
		VectorCopy( ent->s.angles, angles );
	}

	VectorAdd( ent->s.origin, viewoffset, origin );

	// shoot

	edict_t *projectile = NULL;

	switch( weapondef->weapon_id ) {
		default:
		case WEAP_NONE:
			break;

		case WEAP_GUNBLADE:
			G_Fire_Gunblade_Knife( origin, angles, firedef, ent );
			break;

		case WEAP_MACHINEGUN:
			G_Fire_Machinegun( origin, angles, firedef, ent );
			break;

		case WEAP_RIOTGUN:
			G_Fire_Riotgun( origin, angles, firedef, ent );
			break;

		case WEAP_GRENADELAUNCHER:
			projectile = G_Fire_Grenade( origin, angles, firedef, ent );
			break;

		case WEAP_ROCKETLAUNCHER:
			projectile = G_Fire_Rocket( origin, angles, firedef, ent );
			break;

		case WEAP_PLASMAGUN:
			projectile = G_Fire_Plasma( origin, angles, firedef, ent );
			break;

		case WEAP_LASERGUN:
			projectile = G_Fire_Lasergun( origin, angles, firedef, ent );
			break;

		case WEAP_ELECTROBOLT:
			G_Fire_Bolt( origin, angles, firedef, ent );
			break;
	}

	// add stats
	if( ent->r.client && weapondef->weapon_id != WEAP_NONE ) {
		ent->r.client->level.stats.accuracy_shots[firedef->ammo_id - AMMO_GUNBLADE] += firedef->projectile_count;
	}

	if( projectile ) {
		G_ProjectileDistancePrestep( projectile, g_projectile_prestep->value );
		if( projectile->s.linearMovement )
			VectorCopy( projectile->s.origin, projectile->s.linearMovementBegin );
	}
}
