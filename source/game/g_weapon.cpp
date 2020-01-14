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

#include "qcommon/base.h"
#include "game/g_local.h"

static bool CanHit( const edict_t * projectile, const edict_t * target ) {
	return target == world || target != projectile->r.owner;
}

static void ForgotToSetProjectileTouch( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	assert( false );
}

/*
* W_Fire_LinearProjectile - Spawn a generic linear projectile without a model, touch func, sound nor mod
*/
static edict_t *W_Fire_LinearProjectile( edict_t *self, vec3_t start, vec3_t angles, int speed,
										 float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta ) {
	edict_t *projectile;
	vec3_t dir;

	projectile = G_Spawn();
	VectorCopy( start, projectile->s.origin );
	VectorCopy( start, projectile->olds.origin );

	VectorCopy( angles, projectile->s.angles );
	AngleVectors( angles, dir, NULL, NULL );
	VectorScale( dir, speed, projectile->velocity );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;

	projectile->r.solid = SOLID_YES;
	projectile->r.clipmask = ( !GS_RaceGametype( &server_gs ) ) ? MASK_SHOT : MASK_SOLID;

	projectile->r.svflags = SVF_PROJECTILE;

	// enable me when drawing exception is added to cgame
	VectorClear( projectile->r.mins );
	VectorClear( projectile->r.maxs );
	projectile->s.modelindex = 0;
	projectile->r.owner = self;
	projectile->s.ownerNum = ENTNUM( self );
	projectile->touch = ForgotToSetProjectileTouch;
	projectile->nextThink = level.time + timeout;
	projectile->think = G_FreeEdict;
	projectile->classname = NULL; // should be replaced after calling this func.
	projectile->s.sound = 0;
	projectile->timeStamp = level.time;
	projectile->timeDelta = timeDelta;

	projectile->projectileInfo.minDamage = min( minDamage, damage );
	projectile->projectileInfo.maxDamage = damage;
	projectile->projectileInfo.minKnockback = min( minKnockback, maxKnockback );
	projectile->projectileInfo.maxKnockback = maxKnockback;
	projectile->projectileInfo.radius = radius;

	GClip_LinkEntity( projectile );

	// update some data required for the transmission
	projectile->s.linearMovement = true;
	VectorCopy( projectile->s.origin, projectile->s.linearMovementBegin );
	VectorCopy( projectile->velocity, projectile->s.linearMovementVelocity );
	projectile->s.linearMovementTimeStamp = svs.gametime;
	projectile->s.team = self->s.team;
	projectile->s.modelindex2 = Min2( Abs( timeDelta ), 255 );
	return projectile;
}

/*
* W_Fire_TossProjectile - Spawn a generic projectile without a model, touch func, sound nor mod
*/
static edict_t *W_Fire_TossProjectile( edict_t *self, vec3_t start, vec3_t angles, int speed,
									   float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta ) {
	edict_t *projectile;
	vec3_t dir;

	projectile = G_Spawn();
	VectorCopy( start, projectile->s.origin );
	VectorCopy( start, projectile->olds.origin );

	VectorCopy( angles, projectile->s.angles );
	AngleVectors( angles, dir, NULL, NULL );
	VectorScale( dir, speed, projectile->velocity );

	projectile->movetype = MOVETYPE_BOUNCEGRENADE;

	// make missile fly through players in race
	if( GS_RaceGametype( &server_gs ) ) {
		projectile->r.clipmask = MASK_SOLID;
	} else {
		projectile->r.clipmask = MASK_SHOT;
	}

	projectile->r.solid = SOLID_YES;
	projectile->r.svflags = SVF_PROJECTILE;
	VectorClear( projectile->r.mins );
	VectorClear( projectile->r.maxs );

	//projectile->s.modelindex = trap_ModelIndex ("models/objects/projectile/plasmagun/proj_plasmagun2.md3");
	projectile->s.modelindex = 0;
	projectile->r.owner = self;
	projectile->touch = ForgotToSetProjectileTouch;
	projectile->nextThink = level.time + timeout;
	projectile->think = G_FreeEdict;
	projectile->classname = NULL; // should be replaced after calling this func.
	projectile->s.sound = 0;
	projectile->timeStamp = level.time;
	projectile->timeDelta = timeDelta;
	projectile->s.team = self->s.team;

	projectile->projectileInfo.minDamage = min( minDamage, damage );
	projectile->projectileInfo.maxDamage = damage;
	projectile->projectileInfo.minKnockback = min( minKnockback, maxKnockback );
	projectile->projectileInfo.maxKnockback = maxKnockback;
	projectile->projectileInfo.radius = radius;

	GClip_LinkEntity( projectile );

	return projectile;
}


//	------------ the actual weapons --------------


/*
* W_Fire_Blade
*/
void W_Fire_Blade( edict_t *self, int range, vec3_t start, vec3_t angles,  float damage, int knockback, int timeDelta ) {
	int traces = 6;
	float slash_angle = 45.0f;

	int mask = MASK_SHOT;
	int dmgflags = 0;

	if( GS_RaceGametype( &server_gs ) ) {
		mask = MASK_SOLID;
	}

	for( int i = 0; i < traces; i++ ) {
		vec3_t end, dir;
		trace_t trace;
		vec3_t new_angles;

		new_angles[0] = angles[0];
		new_angles[1] = angles[1] + Lerp( -slash_angle, float( i ) / float( traces - 1 ), slash_angle );
		new_angles[2] = angles[2];

		AngleVectors( new_angles, dir, NULL, NULL );
		VectorMA( start, range, dir, end );

		G_Trace4D( &trace, start, NULL, NULL, end, self, mask, timeDelta );
		if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
			G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, damage, knockback, dmgflags, MOD_GUNBLADE );
			break;
		}
	}
}

/*
* W_Fire_MG
*/
void W_Fire_MG( edict_t *self, vec3_t start, vec3_t angles, int range, int spread,
					float damage, int knockback, int timeDelta ) {
	vec3_t dir;
	AngleVectors( angles, dir, NULL, NULL );

	// send the event
	edict_t *event = G_SpawnEvent( EV_FIRE_MG, 0, start );
	event->s.ownerNum = ENTNUM( self );
	VectorScale( dir, 4096, event->s.origin2 ); // DirToByte is too inaccurate
	event->s.weapon = Weapon_MachineGun;

	vec3_t right, up;
	ViewVectors( dir, right, up );

	trace_t trace;
	GS_TraceBullet( &server_gs, &trace, start, dir, right, up, 0, 0, range, ENTNUM( self ), timeDelta );
	if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
		int dmgflags = DAMAGE_KNOCKBACK_SOFT;
		G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, damage, knockback, dmgflags, MOD_MACHINEGUN );
	}
}

// Sunflower spiral with Fibonacci numbers
static void G_Fire_SunflowerPattern( edict_t *self, vec3_t start, vec3_t dir, int count,
									 int spread, int range, float damage, int kick, int dflags, int timeDelta ) {
	vec3_t right, up;
	ViewVectors( dir, right, up );

	int hits[MAX_CLIENTS + 1] = { };
	for( int i = 0; i < count; i++ ) {
		float fi = i * 2.4f; //magic value creating Fibonacci numbers
		float r = cosf( fi ) * spread * sqrtf( fi );
		float u = sinf( fi ) * spread * sqrtf( fi );

		trace_t trace;
		GS_TraceBullet( &server_gs, &trace, start, dir, right, up, r, u, range, ENTNUM( self ), timeDelta );
		if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
			G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, damage, kick, dflags, MOD_RIOTGUN );
			if( !G_IsTeamDamage( &game.edicts[trace.ent].s, &self->s ) && trace.ent <= MAX_CLIENTS ) {
				hits[trace.ent]++;
			}
		}
	}

	for( int i = 1; i <= MAX_CLIENTS; i++ ) {
		if( hits[i] == 0 )
			continue;
		edict_t * target = &game.edicts[i];
		edict_t * ev = G_SpawnEvent( EV_DAMAGE, 0, target->s.origin );
		ev->r.svflags |= SVF_ONLYOWNER;
		ev->s.ownerNum = ENTNUM( self );
		ev->s.damage = HEALTH_TO_INT( hits[i] * damage );
	}
}

void W_Fire_Riotgun( edict_t *self, vec3_t start, vec3_t angles, int range, int spread,
					 int count, float damage, int knockback, int timeDelta ) {
	vec3_t dir;
	edict_t *event;
	int dmgflags = 0;

	AngleVectors( angles, dir, NULL, NULL );

	// send the event
	event = G_SpawnEvent( EV_FIRE_RIOTGUN, 0, start );
	event->s.ownerNum = ENTNUM( self );
	VectorScale( dir, 4096, event->s.origin2 ); // DirToByte is too inaccurate
	event->s.weapon = Weapon_Shotgun;

	G_Fire_SunflowerPattern( self, start, dir, count, spread,
		range, damage, knockback, dmgflags, timeDelta );
}

/*
* W_Grenade_ExplodeDir
*/
static void W_Grenade_ExplodeDir( edict_t *ent, vec3_t normal ) {
	vec3_t origin;
	int radius;
	edict_t *event;
	vec3_t up = { 0, 0, 1 };
	float *dir = normal ? normal : up;

	G_RadiusDamage( ent, ent->r.owner, NULL, ent->enemy, MOD_GRENADE_SPLASH );

	radius = ( ( ent->projectileInfo.radius * 1 / 8 ) > 127 ) ? 127 : ( ent->projectileInfo.radius * 1 / 8 );
	VectorMA( ent->s.origin, -0.02, ent->velocity, origin );
	event = G_SpawnEvent( EV_GRENADE_EXPLOSION, ( dir ? DirToByte( dir ) : 0 ), ent->s.origin );
	event->s.weapon = radius;
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

/*
* W_Grenade_Explode
*/
static void W_Grenade_Explode( edict_t *ent ) {
	W_Grenade_ExplodeDir( ent, NULL );
}

/*
* W_Touch_Grenade
*/
static void W_Touch_Grenade( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	// don't explode on doors and plats that take damage
	if( !other->takedamage || ISBRUSHMODEL( other->s.modelindex ) ) {
		G_AddEvent( ent, EV_GRENADE_BOUNCE, 0, true );
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_GRENADE );
	}

	ent->enemy = other;
	W_Grenade_ExplodeDir( ent, plane ? plane->normal : NULL );
}

/*
* W_Fire_Grenade
*/
edict_t *W_Fire_Grenade( edict_t *self, vec3_t start, vec3_t angles, int speed, float damage,
						 int minKnockback, int maxKnockback, int minDamage, float radius,
						 int timeout, int timeDelta, bool aim_up ) {
	if( aim_up ) {
		angles[PITCH] -= 5.0f * cosf( DEG2RAD( angles[PITCH] ) ); // aim some degrees upwards from view dir
	}

	edict_t * grenade = W_Fire_TossProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );
	VectorClear( grenade->s.angles );
	grenade->s.type = ET_GRENADE;
	grenade->movetype = MOVETYPE_BOUNCEGRENADE;
	grenade->touch = W_Touch_Grenade;
	grenade->use = NULL;
	grenade->think = W_Grenade_Explode;
	grenade->classname = "grenade";
	grenade->enemy = NULL;
	VectorSet( grenade->avelocity, 300, 300, 300 );

	grenade->s.modelindex = trap_ModelIndex( PATH_GRENADE_MODEL );

	GClip_LinkEntity( grenade );

	return grenade;
}

/*
* W_Touch_Rocket
*/
static void W_Touch_Rocket( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_ROCKET );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, MOD_ROCKET_SPLASH );

	// spawn the explosion
	vec3_t explosion_origin;
	VectorMA( ent->s.origin, -0.02, ent->velocity, explosion_origin );

	edict_t *event = G_SpawnEvent( EV_ROCKET_EXPLOSION, DirToByte( plane ? plane->normal : NULL ), explosion_origin );
	event->s.weapon = min( ent->projectileInfo.radius / 8, 255 );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

/*
* W_Fire_Rocket
*/
edict_t *W_Fire_Rocket( edict_t *self, vec3_t start, vec3_t angles, int speed, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta ) {
	edict_t *rocket;

	rocket = W_Fire_LinearProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );

	rocket->s.type = ET_ROCKET; //rocket trail sfx
	rocket->s.modelindex = trap_ModelIndex( PATH_ROCKET_MODEL );
	rocket->s.sound = trap_SoundIndex( S_WEAPON_ROCKET_FLY );
	rocket->s.attenuation = ATTN_STATIC;
	rocket->touch = W_Touch_Rocket;
	rocket->think = G_FreeEdict;
	rocket->classname = "rocket";

	return rocket;
}

/*
* W_Touch_Plasma
*/
static void W_Touch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, DAMAGE_KNOCKBACK_SOFT, MOD_PLASMA );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, MOD_PLASMA_SPLASH );

	edict_t *event = G_SpawnEvent( EV_PLASMA_EXPLOSION, DirToByte( plane ? plane->normal : NULL ), ent->s.origin );
	event->s.weapon = min( ent->projectileInfo.radius / 8, 127 );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

/*
* W_Plasma_Backtrace
*/
void W_Plasma_Backtrace( edict_t *ent, const vec3_t start ) {
	trace_t tr;
	vec3_t oldorigin;
	vec3_t mins = { -2, -2, -2 }, maxs = { 2, 2, 2 };

	if( GS_RaceGametype( &server_gs ) ) {
		return;
	}

	VectorCopy( ent->s.origin, oldorigin );
	VectorCopy( start, ent->s.origin );

	do {
		G_Trace4D( &tr, ent->s.origin, mins, maxs, oldorigin, ent, CONTENTS_BODY, ent->timeDelta );

		VectorCopy( tr.endpos, ent->s.origin );

		if( tr.ent == -1 ) {
			break;
		}
		if( tr.allsolid || tr.startsolid ) {
			W_Touch_Plasma( ent, &game.edicts[tr.ent], NULL, 0 );
		} else if( tr.fraction != 1.0 ) {
			W_Touch_Plasma( ent, &game.edicts[tr.ent], &tr.plane, tr.surfFlags );
		} else {
			break;
		}
	} while( ent->r.inuse && ent->s.type == ET_PLASMA && !VectorCompare( ent->s.origin, oldorigin ) );

	if( ent->r.inuse && ent->s.type == ET_PLASMA ) {
		VectorCopy( oldorigin, ent->s.origin );
	}
}

/*
* W_Think_Plasma
*/
static void W_Think_Plasma( edict_t *ent ) {
	vec3_t start;

	if( ent->timeout < level.time ) {
		G_FreeEdict( ent );
		return;
	}

	if( ent->r.inuse ) {
		ent->nextThink = level.time + 1;
	}

	VectorMA( ent->s.origin, -( game.frametime * 0.001 ), ent->velocity, start );

	W_Plasma_Backtrace( ent, start );
}

/*
* W_AutoTouch_Plasma
*/
static void W_AutoTouch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	W_Think_Plasma( ent );
	if( !ent->r.inuse || ent->s.type != ET_PLASMA ) {
		return;
	}

	W_Touch_Plasma( ent, other, plane, surfFlags );
}

/*
* W_Fire_Plasma
*/
edict_t *W_Fire_Plasma( edict_t *self, vec3_t start, vec3_t angles, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int speed, int timeout, int timeDelta ) {
	edict_t * plasma = W_Fire_LinearProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );
	plasma->s.type = ET_PLASMA;
	plasma->classname = "plasma";

	plasma->think = W_Think_Plasma;
	plasma->touch = W_AutoTouch_Plasma;
	plasma->nextThink = level.time + 1;
	plasma->timeout = level.time + timeout;

	plasma->s.modelindex = trap_ModelIndex( PATH_PLASMA_MODEL );
	plasma->s.sound = trap_SoundIndex( S_WEAPON_PLASMAGUN_FLY );
	plasma->s.attenuation = ATTN_STATIC;

	return plasma;
}

void W_Fire_Electrobolt( edict_t *self, vec3_t start, vec3_t angles, float damage, int knockback, int range, int timeDelta ) {
	vec3_t from, end, dir;

	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( start, range, dir, end );
	VectorCopy( start, from );

	edict_t * ignore = self;

	trace_t tr;
	tr.ent = -1;
	while( ignore ) {
		G_Trace4D( &tr, from, NULL, NULL, end, ignore, MASK_SHOT, timeDelta );

		VectorCopy( tr.endpos, from );
		ignore = NULL;

		if( tr.ent == -1 ) {
			break;
		}

		// some entity was touched
		edict_t * hit = &game.edicts[tr.ent];
		int hit_movetype = hit->movetype; // backup the original movetype as the entity may "die"
		if( hit == world ) { // stop dead if hit the world
			break;
		}

		// allow trail to go through BBOX entities (players, gibs, etc)
		if( !ISBRUSHMODEL( hit->s.modelindex ) ) {
			ignore = hit;
		}

		if( hit != self && hit->takedamage ) {
			G_Damage( hit, self, self, dir, dir, tr.endpos, damage, knockback, 0, MOD_ELECTROBOLT );

			// spawn a impact event on each damaged ent
			edict_t * event = G_SpawnEvent( EV_BOLT_EXPLOSION, DirToByte( tr.plane.normal ), tr.endpos );
			event->s.team = self->s.team;

			// if we hit a teammate stop the trace
			if( G_IsTeamDamage( &hit->s, &self->s ) ) {
				break;
			}
		}

		if( hit_movetype == MOVETYPE_NONE || hit_movetype == MOVETYPE_PUSH ) {
			break;
		}
	}

	// send the weapon fire effect
	edict_t * fire_event = G_SpawnEvent( EV_ELECTROTRAIL, ENTNUM( self ), start );
	VectorCopy( dir, fire_event->s.origin2 );
}

/*
* G_HideLaser
*/
static void G_HideLaser( edict_t *ent ) {
	ent->s.modelindex = 0;
	ent->s.sound = 0;
	ent->r.svflags = SVF_NOCLIENT;

	// give it 100 msecs before freeing itself, so we can relink it if we start firing again
	ent->think = G_FreeEdict;
	ent->nextThink = level.time + 100;
}

/*
* G_Laser_Think
*/
static void G_Laser_Think( edict_t *ent ) {
	edict_t *owner;

	if( ent->s.ownerNum < 1 || ent->s.ownerNum > server_gs.maxclients ) {
		G_FreeEdict( ent );
		return;
	}

	owner = &game.edicts[ent->s.ownerNum];

	if( G_ISGHOSTING( owner ) || owner->s.weapon != Weapon_Laser ||
		trap_GetClientState( PLAYERNUM( owner ) ) < CS_SPAWNED ||
		owner->r.client->ps.weapon_state != WEAPON_STATE_REFIRE ) {
		G_HideLaser( ent );
		return;
	}

	ent->nextThink = level.time + 1;
}

static float laser_damage;
static int laser_knockback;
static int laser_attackerNum;

static void _LaserImpact( trace_t *trace, vec3_t dir ) {
	edict_t *attacker;

	if( !trace || trace->ent <= 0 ) {
		return;
	}
	if( trace->ent == laser_attackerNum ) {
		return; // should not be possible theoretically but happened at least once in practice

	}
	attacker = &game.edicts[laser_attackerNum];

	if( game.edicts[trace->ent].takedamage ) {
		G_Damage( &game.edicts[trace->ent], attacker, attacker, dir, dir, trace->endpos, laser_damage, laser_knockback, DAMAGE_KNOCKBACK_SOFT, MOD_LASERGUN );
	}
}

static edict_t *_FindOrSpawnLaser( edict_t *owner, int entType ) {
	int i, ownerNum;
	edict_t *e, *laser;

	// first of all, see if we already have a beam entity for this laser
	laser = NULL;
	ownerNum = ENTNUM( owner );
	for( i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		e = &game.edicts[i];
		if( !e->r.inuse ) {
			continue;
		}

		if( e->s.ownerNum == ownerNum && e->s.type == ET_LASERBEAM ) {
			laser = e;
			break;
		}
	}

	// if no ent was found we have to create one
	if( !laser || laser->s.type != entType || !laser->s.modelindex ) {
		if( !laser ) {
			laser = G_Spawn();
		}

		laser->s.type = entType;
		laser->s.ownerNum = ownerNum;
		laser->movetype = MOVETYPE_NONE;
		laser->r.solid = SOLID_NOT;
		laser->s.modelindex = 255; // needs to have some value so it isn't filtered by the server culling
		laser->r.svflags &= ~SVF_NOCLIENT;
	}

	return laser;
}

/*
* W_Fire_Lasergun
*/
edict_t *W_Fire_Lasergun( edict_t *self, vec3_t start, vec3_t angles, float damage, int knockback, int range, int timeDelta ) {
	edict_t *laser;
	trace_t tr;
	vec3_t dir;

	laser = _FindOrSpawnLaser( self, ET_LASERBEAM );

	laser_damage = damage;
	laser_knockback = knockback;
	laser_attackerNum = ENTNUM( self );

	GS_TraceLaserBeam( &server_gs, &tr, start, angles, range, ENTNUM( self ), timeDelta, _LaserImpact );

	laser->r.svflags |= SVF_FORCEOWNER;
	VectorCopy( start, laser->s.origin );
	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( laser->s.origin, range, dir, laser->s.origin2 );

	laser->think = G_Laser_Think;
	laser->nextThink = level.time + 100;

	// calculate laser's mins and maxs for linkEntity
	G_SetBoundsForSpanEntity( laser, 8 );

	GClip_LinkEntity( laser );

	return laser;
}
