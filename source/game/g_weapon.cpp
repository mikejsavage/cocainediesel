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
#include "qcommon/cmodel.h"
#include "game/g_local.h"

#define PLASMAHACK // ffs : hack for the plasmagun

static bool CanHit( const edict_t * projectile, const edict_t * target ) {
	return target == world || target != projectile->r.owner;
}

static void W_Explode_Plasma( edict_t *ent, edict_t *other, cplane_t *plane ) {
	if( other != NULL && other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, DAMAGE_KNOCKBACK_SOFT, MOD_PLASMA );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, ent->s.type == ET_PLASMA ? MOD_PLASMA : MOD_BUBBLEGUN );

	edict_t *event = G_SpawnEvent( ent->s.type == ET_PLASMA ? EV_PLASMA_EXPLOSION : EV_BUBBLE_EXPLOSION, DirToByte( plane ? plane->normal : NULL ), ent->s.origin );
	event->s.weapon = Min2( ent->projectileInfo.radius / 8, 127 );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Touch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	W_Explode_Plasma( ent, other, plane );
}

static void W_Plasma_Backtrace( edict_t *ent, const vec3_t start ) {
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
	} while( ent->r.inuse && !VectorCompare( ent->s.origin, oldorigin ) );

	if( ent->r.inuse ) {
		VectorCopy( oldorigin, ent->s.origin );
	}
}

static void W_Think_Plasma( edict_t *ent ) {
	vec3_t start;

	if( ent->timeout < level.time ) {
		if( ent->s.type == ET_BUBBLE )
			W_Explode_Plasma( ent, NULL, NULL );
		else
			G_FreeEdict( ent );
		return;
	}

	if( ent->r.inuse ) {
		ent->nextThink = level.time + 1;
	}

	VectorMA( ent->s.origin, -( game.frametime * 0.001 ), ent->velocity, start );

	W_Plasma_Backtrace( ent, start );
}

static void W_AutoTouch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	W_Think_Plasma( ent );
	W_Touch_Plasma( ent, other, plane, surfFlags );
}

static void G_ProjectileDistancePrestep( edict_t *projectile, float distance ) {
	assert( projectile->movetype == MOVETYPE_TOSS
		|| projectile->movetype == MOVETYPE_LINEARPROJECTILE
		|| projectile->movetype == MOVETYPE_BOUNCE
		|| projectile->movetype == MOVETYPE_BOUNCEGRENADE );

	if( !distance ) {
		return;
	}

	vec3_t dir;
	float speed = VectorNormalize2( projectile->velocity, dir );
	if( speed == 0.0f ) {
		return;
	}

	int mask = projectile->r.clipmask;

#ifdef PLASMAHACK
	vec3_t plasma_hack_start;
	VectorCopy( projectile->s.origin, plasma_hack_start );
#endif

	vec3_t dest;
	VectorMA( projectile->s.origin, distance, dir, dest );

	trace_t trace;
	G_Trace4D( &trace, projectile->s.origin, projectile->r.mins, projectile->r.maxs, dest, projectile->r.owner, mask, projectile->timeDelta );

	VectorCopy( trace.endpos, projectile->s.origin );
	VectorCopy( trace.endpos, projectile->olds.origin );

	GClip_LinkEntity( projectile );
	SV_Impact( projectile, &trace );

	// set initial water state
	if( !projectile->r.inuse ) {
		return;
	}

	projectile->waterlevel = ( G_PointContents4D( projectile->s.origin, projectile->timeDelta ) & MASK_WATER ) ? true : false;

	// ffs : hack for the plasmagun
#ifdef PLASMAHACK
	if( projectile->s.type == ET_PLASMA || projectile->s.type == ET_BUBBLE ) {
		W_Plasma_Backtrace( projectile, plasma_hack_start );
	}
#endif
}

static edict_t * FireProjectile(
		edict_t * owner,
		const vec3_t start, const vec3_t angles,
		int timeDelta,
		const WeaponDef * def, EdictTouchCallback touch, int event_type, int clipmask
) {
	edict_t * projectile = G_Spawn();
	VectorCopy( start, projectile->s.origin );
	VectorCopy( start, projectile->olds.origin );
	VectorCopy( angles, projectile->s.angles );

	vec3_t dir;
	AngleVectors( angles, dir, NULL, NULL );

	VectorScale( dir, def->speed, projectile->velocity );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;

	projectile->r.solid = SOLID_YES;
	projectile->r.clipmask = !GS_RaceGametype( &server_gs ) ? clipmask : MASK_SOLID;
	projectile->r.svflags = SVF_PROJECTILE;

	VectorClear( projectile->r.mins );
	VectorClear( projectile->r.maxs );

	projectile->r.owner = owner;
	projectile->touch = touch;
	projectile->nextThink = level.time + def->range;
	projectile->think = G_FreeEdict;
	projectile->timeout = level.time + def->range;
	projectile->timeStamp = level.time;
	projectile->timeDelta = timeDelta;
	projectile->s.team = owner->s.team;
	projectile->s.type = event_type;

	projectile->projectileInfo.minDamage = Min2( float( def->mindamage ), def->damage );
	projectile->projectileInfo.maxDamage = def->damage;
	projectile->projectileInfo.minKnockback = Min2( def->minknockback, def->knockback );
	projectile->projectileInfo.maxKnockback = def->knockback;
	projectile->projectileInfo.radius = def->splash_radius;

	G_ProjectileDistancePrestep( projectile, g_projectile_prestep->value );

	return projectile;
}

static edict_t * FireLinearProjectile(
		edict_t * owner,
		const vec3_t start, const vec3_t angles,
		int timeDelta,
		const WeaponDef * def, EdictTouchCallback touch, int event_type, int clipmask
) {
	edict_t * projectile = FireProjectile( owner, start, angles, timeDelta, def, touch, event_type, clipmask );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;
	projectile->s.linearMovement = true;
	VectorCopy( projectile->s.origin, projectile->s.linearMovementBegin );
	VectorCopy( projectile->velocity, projectile->s.linearMovementVelocity );
	projectile->s.linearMovementTimeStamp = svs.gametime;
	projectile->s.linearMovementTimeDelta = Min2( Abs( timeDelta ), 255 );

	return projectile;
}

static void W_Fire_Blade( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Knife );

	int traces = def->projectile_count;
	float slash_angle = def->spread;

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
		VectorMA( start, def->range, dir, end );

		G_Trace4D( &trace, start, NULL, NULL, end, self, mask, timeDelta );
		if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
			G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, def->damage, def->knockback, dmgflags, MOD_GUNBLADE );
			break;
		}
	}
}

static void W_Fire_Bullet( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta, WeaponType weapon, int mod ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	vec3_t dir, right, up;
	AngleVectors( angles, dir, right, up );

	float x_spread = 0.0f;
	float y_spread = 0.0f;
	if( self->r.client != NULL && self->r.client->ps.zoom_time < ZOOMTIME ) {
		float frac = 1.0f - float( self->r.client->ps.zoom_time ) / float( ZOOMTIME );
		float spread = frac * def->range * atanf( DEG2RAD( def->zoom_spread ) );
		x_spread = random_float11( &svs.rng ) * spread;
		y_spread = random_float11( &svs.rng ) * spread;
	}

	trace_t trace, wallbang;
	GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, x_spread, y_spread, def->range, ENTNUM( self ), timeDelta );
	if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
		int dmgflags = DAMAGE_KNOCKBACK_SOFT;

		if( IsHeadshot( trace.ent, FromQF3( trace.endpos ), timeDelta ) ) {
			dmgflags |= DAMAGE_HEADSHOT;
		}

		G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, def->damage, def->knockback, dmgflags, mod );
	}
}

static void W_Fire_Shotgun( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	vec3_t dir, right, up;
	AngleVectors( angles, dir, right, up );

	//Sunflower pattern
	float damage_dealt[ MAX_CLIENTS + 1 ] = { };
	for( int i = 0; i < def->projectile_count; i++ ) {
		float fi = i * 2.4f; //magic value creating Fibonacci numbers
		float r = cosf( fi ) * def->spread * sqrtf( fi );
		float u = sinf( fi ) * def->spread * sqrtf( fi );

		trace_t trace, wallbang;
		GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, r, u, def->range, ENTNUM( self ), timeDelta );
		if( trace.ent != -1 && game.edicts[ trace.ent ].takedamage ) {
			G_Damage( &game.edicts[ trace.ent ], self, self, dir, dir, trace.endpos, def->damage, def->knockback, 0, MOD_SHOTGUN );

			if( !G_IsTeamDamage( &game.edicts[ trace.ent ].s, &self->s ) && trace.ent <= MAX_CLIENTS ) {
				damage_dealt[ trace.ent ] += def->damage;
			}
		}
	}

	for( int i = 1; i <= MAX_CLIENTS; i++ ) {
		if( damage_dealt[ i ] == 0 )
			continue;
		edict_t * target = &game.edicts[ i ];
		edict_t * ev = G_SpawnEvent( EV_DAMAGE, HEALTH_TO_INT( damage_dealt[ i ] ) << 1, target->s.origin );
		ev->r.svflags |= SVF_ONLYOWNER;
		ev->s.ownerNum = ENTNUM( self );
	}
}

static void W_Grenade_ExplodeDir( edict_t *ent, vec3_t normal ) {
	vec3_t origin;
	int radius;
	edict_t *event;
	vec3_t up = { 0, 0, 1 };
	float *dir = normal ? normal : up;

	G_RadiusDamage( ent, ent->r.owner, NULL, ent->enemy, MOD_GRENADE );

	radius = ( ( ent->projectileInfo.radius * 1 / 8 ) > 127 ) ? 127 : ( ent->projectileInfo.radius * 1 / 8 );
	VectorMA( ent->s.origin, -0.02, ent->velocity, origin );
	event = G_SpawnEvent( EV_GRENADE_EXPLOSION, ( dir ? DirToByte( dir ) : 0 ), ent->s.origin );
	event->s.weapon = radius;
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Grenade_Explode( edict_t *ent ) {
	W_Grenade_ExplodeDir( ent, NULL );
}

static void W_Touch_Grenade( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	// don't explode on doors and plats that take damage
	if( !other->takedamage || CM_IsBrushModel( CM_Server, other->s.model ) ) {
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

static void W_Fire_Grenade( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta, bool aim_up ) {
	vec3_t new_angles;
	VectorCopy( angles, new_angles );
	if( aim_up ) {
		new_angles[PITCH] -= 5.0f * cosf( DEG2RAD( new_angles[PITCH] ) ); // aim some degrees upwards from view dir
	}

	edict_t * grenade = FireProjectile( self, start, new_angles, timeDelta, GS_GetWeaponDef( Weapon_GrenadeLauncher ), W_Touch_Grenade, ET_GRENADE, MASK_SHOT );

	grenade->classname = "grenade";
	grenade->movetype = MOVETYPE_BOUNCEGRENADE;
	grenade->s.model = "weapons/gl/grenade";
	// grenade->s.sound = "weapons/gl/trail";

	grenade->think = W_Grenade_Explode;

	VectorClear( grenade->s.angles );
	VectorSet( grenade->avelocity, 300, 300, 300 );
}

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

	G_RadiusDamage( ent, ent->r.owner, plane, other, MOD_ROCKET );

	// spawn the explosion
	vec3_t explosion_origin;
	VectorMA( ent->s.origin, -0.02, ent->velocity, explosion_origin );

	edict_t *event = G_SpawnEvent( EV_ROCKET_EXPLOSION, DirToByte( plane ? plane->normal : NULL ), explosion_origin );
	event->s.weapon = Min2( ent->projectileInfo.radius / 8, 255 );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Fire_Rocket( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	edict_t * rocket = FireLinearProjectile( self, start, angles, timeDelta, GS_GetWeaponDef( Weapon_RocketLauncher ), W_Touch_Rocket, ET_ROCKET, MASK_SHOT );

	rocket->classname = "rocket";
	rocket->s.model = "weapons/rl/rocket";
	rocket->s.sound = "weapons/rl/trail";
}

static void W_Fire_Plasma( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	edict_t * plasma = FireLinearProjectile( self, start, angles, timeDelta, GS_GetWeaponDef( Weapon_Plasma ), W_AutoTouch_Plasma, ET_PLASMA, MASK_SHOT );

	plasma->classname = "plasma";
	plasma->s.model = "weapons/pg/cell";
	plasma->s.sound = "weapons/pg/trail";
}

static void FireBubble( edict_t * owner, const vec3_t start, const vec3_t angles, const WeaponDef * def, int timeDelta ) {
	edict_t * bubble = FireLinearProjectile( owner, start, angles, timeDelta, def, W_AutoTouch_Plasma, ET_BUBBLE, MASK_SHOT );

	bubble->classname = "bubble";
	bubble->s.model = "weapons/bg/cell";
	bubble->s.sound = "weapons/bg/trail";

	bubble->think = W_Think_Plasma;
	bubble->nextThink = level.time + 1;
}

void W_Fire_BubbleGun( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	constexpr int bubble_spacing = 25;
	const WeaponDef * def = GS_GetWeaponDef( Weapon_BubbleGun );

	vec3_t dir, right, up;
	AngleVectors( angles, dir, right, up );

	FireBubble( self, start, angles, def, timeDelta );

	int n = def->projectile_count - 1;
	float base_angle = random_float01( &svs.rng ) * 2.0f * PI;

	for( int i = 0; i < n; i++ ) {
		float angle = base_angle + ( 2.0f * PI ) * float( i ) / float( n );

		vec3_t pos;
		VectorCopy( start, pos );
		VectorMA( pos, cosf( angle ) * bubble_spacing, right, pos );
		VectorMA( pos, sinf( angle ) * bubble_spacing, up, pos );

		vec3_t new_dir;
		VectorCopy( dir, new_dir );
		VectorMA( new_dir, cosf( angle + 0.5f * PI ) * def->spread, right, new_dir );
		VectorMA( new_dir, sinf( angle + 0.5f * PI ) * def->spread, up, new_dir );
		VectorNormalize( new_dir );

		vec3_t new_angles;
		VecToAngles( new_dir, new_angles );
		FireBubble( self, pos, new_angles, def, timeDelta );
	}
}

static void W_Fire_Railgun( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );

	vec3_t from, end, dir;

	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( start, def->range, dir, end );
	VectorCopy( start, from );

	edict_t * ignore = self;

	trace_t tr;
	tr.ent = -1;
	while( ignore ) {
		G_Trace4D( &tr, from, NULL, NULL, end, ignore, MASK_WALLBANG, timeDelta );

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
		if( !CM_IsBrushModel( CM_Server, hit->s.model ) ) {
			ignore = hit;
		}

		if( hit != self && hit->takedamage ) {
			int dmgflags = DAMAGE_KNOCKBACK_SOFT;

			if( IsHeadshot( tr.ent, FromQF3( tr.endpos ), timeDelta ) ) {
				dmgflags |= DAMAGE_HEADSHOT;
			}

			G_Damage( hit, self, self, dir, dir, tr.endpos, def->damage, def->knockback, dmgflags, MOD_RAILGUN );


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
}

static void G_HideLaser( edict_t *ent ) {
	ent->r.svflags = SVF_NOCLIENT;

	// give it 100 msecs before freeing itself, so we can relink it if we start firing again
	ent->think = G_FreeEdict;
	ent->nextThink = level.time + 100;
}

static void G_Laser_Think( edict_t *ent ) {
	edict_t *owner;

	if( ent->s.ownerNum < 1 || ent->s.ownerNum > server_gs.maxclients ) {
		G_FreeEdict( ent );
		return;
	}

	owner = &game.edicts[ent->s.ownerNum];

	if( G_ISGHOSTING( owner ) || owner->s.weapon != Weapon_Laser ||
		trap_GetClientState( PLAYERNUM( owner ) ) < CS_SPAWNED ||
		owner->r.client->ps.weapon_state != WeaponState_Firing ) {
		G_HideLaser( ent );
		return;
	}

	ent->nextThink = level.time + 1;
}

static float laser_damage;
static int laser_knockback;
static int laser_attackerNum;

static void _LaserImpact( const trace_t *trace, const vec3_t dir ) {
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

static edict_t *FindOrSpawnLaser( edict_t * owner ) {
	// first of all, see if we already have a beam entity for this laser
	edict_t * laser = NULL;
	int ownerNum = ENTNUM( owner );
	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		edict_t * e = &game.edicts[i];
		if( !e->r.inuse ) {
			continue;
		}

		if( e->s.ownerNum == ownerNum && e->s.type == ET_LASERBEAM ) {
			laser = e;
			break;
		}
	}

	// if no ent was found we have to create one
	if( !laser ) {
		laser = G_Spawn();
		laser->s.type = ET_LASERBEAM;
		laser->s.ownerNum = ownerNum;
		laser->movetype = MOVETYPE_NONE;
		laser->r.solid = SOLID_NOT;
		laser->r.svflags &= ~SVF_NOCLIENT;
	}

	return laser;
}

static void W_Fire_Lasergun( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Laser );

	edict_t * laser = FindOrSpawnLaser( self );

	laser_damage = def->damage;
	laser_knockback = def->knockback;
	laser_attackerNum = ENTNUM( self );

	trace_t tr;
	GS_TraceLaserBeam( &server_gs, &tr, start, angles, def->range, ENTNUM( self ), timeDelta, _LaserImpact );

	laser->r.svflags |= SVF_FORCEOWNER;

	vec3_t dir;
	VectorCopy( start, laser->s.origin );
	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( laser->s.origin, def->range, dir, laser->s.origin2 );

	laser->think = G_Laser_Think;
	laser->nextThink = level.time + 100;

	// calculate laser's mins and maxs for linkEntity
	G_SetBoundsForSpanEntity( laser, 8 );

	GClip_LinkEntity( laser );
}

static void W_Touch_RifleBullet( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_RIFLE );
	}

	edict_t * event = G_SpawnEvent( EV_RIFLEBULLET_IMPACT, DirToByte( plane ? plane->normal : NULL ), ent->s.origin );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

void W_Fire_RifleBullet( edict_t * self, const vec3_t start, const vec3_t angles, int timeDelta ) {
	edict_t * bullet = FireLinearProjectile( self, start, angles, timeDelta, GS_GetWeaponDef( Weapon_Rifle ), W_Touch_RifleBullet, ET_RIFLEBULLET, MASK_WALLBANG );

	bullet->classname = "riflebullet";
	bullet->s.model = "weapons/rifle/bullet";
	bullet->s.sound = "weapons/bullet_whizz";
}

void G_FireWeapon( edict_t *ent, u64 weap ) {
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

	switch( weap ) {
		default:
			return;

		case Weapon_Knife:
			W_Fire_Blade( ent, origin, angles, timeDelta );
			break;

		case Weapon_Pistol:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Pistol, MOD_PISTOL );
			break;

		case Weapon_MachineGun:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_MachineGun, MOD_MACHINEGUN );
			break;

		case Weapon_Deagle:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Deagle, MOD_DEAGLE );
			break;

		case Weapon_Shotgun:
			W_Fire_Shotgun( ent, origin, angles, timeDelta );
			break;

		case Weapon_AssaultRifle:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_AssaultRifle, MOD_ASSAULTRIFLE );
			break;

		case Weapon_GrenadeLauncher:
			W_Fire_Grenade( ent, origin, angles, timeDelta, true );
			break;

		case Weapon_RocketLauncher:
			W_Fire_Rocket( ent, origin, angles, timeDelta );
			break;

		case Weapon_Plasma:
			W_Fire_Plasma( ent, origin, angles, timeDelta );
			break;

		case Weapon_BubbleGun:
			W_Fire_BubbleGun( ent, origin, angles, timeDelta );
			break;

		case Weapon_Laser:
			W_Fire_Lasergun( ent, origin, angles, timeDelta );
			break;

		case Weapon_Sniper:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Sniper, MOD_SNIPER );
			break;

		case Weapon_Railgun:
			W_Fire_Railgun( ent, origin, angles, timeDelta );
			break;

		case Weapon_Rifle:
			W_Fire_RifleBullet( ent, origin, angles, timeDelta );
			break;
	}

	// add stats
	if( ent->r.client != NULL ) {
		ent->r.client->level.stats.accuracy_shots[ weap ] += GS_GetWeaponDef( weap )->projectile_count;
	}
}
