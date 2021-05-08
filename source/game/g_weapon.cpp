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
	if( target == world )
		return true;
	if( target == projectile->r.owner )
		return false;

	constexpr s64 projectile_ignore_teammates_time = 50;
	if( projectile->s.team != TEAM_PLAYERS && projectile->s.team == target->s.team && level.time - projectile->timeStamp < projectile_ignore_teammates_time )
		return false;

	return true;
}

static void W_Explode_Plasma( edict_t *ent, edict_t *other, cplane_t *plane ) {
	if( other != NULL && other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, DAMAGE_KNOCKBACK_SOFT, ent->projectileInfo.mod );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, ent->projectileInfo.mod );

	edict_t * event = G_SpawnEvent( ent->s.type == ET_PLASMA ? EV_PLASMA_EXPLOSION : EV_BUBBLE_EXPLOSION, DirToU64( plane ? plane->normal : Vec3( 0.0f ) ), &ent->s.origin );
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

static void W_Plasma_Backtrace( edict_t *ent, Vec3 start ) {
	trace_t tr;
	Vec3 mins( -2.0f ), maxs( 2.0f );

	if( GS_RaceGametype( &server_gs ) ) {
		return;
	}

	Vec3 oldorigin = ent->s.origin;
	ent->s.origin = start;

	// workaround to stop this hanging when you shoot a teammate
	int iter = 0;
	do {
		G_Trace4D( &tr, ent->s.origin, mins, maxs, oldorigin, ent, CONTENTS_BODY, ent->timeDelta );

		ent->s.origin = tr.endpos;

		if( tr.fraction == 1.0f )
			break;

		if( tr.allsolid || tr.startsolid ) {
			W_Touch_Plasma( ent, &game.edicts[tr.ent], NULL, 0 );
		}
		else {
			W_Touch_Plasma( ent, &game.edicts[tr.ent], &tr.plane, tr.surfFlags );
		}

		iter++;
	} while( ent->r.inuse && ent->s.origin != oldorigin && iter < 5 );

	if( ent->r.inuse ) {
		ent->s.origin = oldorigin;
	}
}

static void W_Think_Plasma( edict_t *ent ) {
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

	Vec3 start = ent->s.origin - ent->velocity * game.frametime * 0.001f;

	W_Plasma_Backtrace( ent, start );
}

static void W_AutoTouch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	W_Think_Plasma( ent );
	if( ent->r.inuse ) {
		W_Touch_Plasma( ent, other, plane, surfFlags );
	}
}

static void G_ProjectileDistancePrestep( edict_t *projectile, float distance ) {
	assert( projectile->movetype == MOVETYPE_TOSS
		|| projectile->movetype == MOVETYPE_LINEARPROJECTILE
		|| projectile->movetype == MOVETYPE_BOUNCE
		|| projectile->movetype == MOVETYPE_BOUNCEGRENADE );

	if( !distance ) {
		return;
	}

	float speed = Length( projectile->velocity );
	Vec3 dir = Normalize( projectile->velocity );
	if( speed == 0.0f ) {
		return;
	}

	int mask = projectile->r.clipmask;

#ifdef PLASMAHACK
	Vec3 plasma_hack_start = projectile->s.origin;
#endif

	Vec3 dest = projectile->s.origin + dir * distance;

	trace_t trace;
	G_Trace4D( &trace, projectile->s.origin, projectile->r.mins, projectile->r.maxs, dest, projectile->r.owner, mask, projectile->timeDelta );

	projectile->s.origin = trace.endpos;
	projectile->olds.origin = trace.endpos;

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
	Vec3 start, Vec3 angles,
	int timeDelta,
	WeaponType weapon, EdictTouchCallback touch, int ent_type, int clipmask
) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	edict_t * projectile = G_Spawn();
	projectile->s.origin = start;
	projectile->olds.origin = start;
	projectile->s.angles = angles;

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );

	projectile->velocity = dir * def->speed;

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;

	projectile->r.solid = SOLID_YES;
	projectile->r.clipmask = !GS_RaceGametype( &server_gs ) ? clipmask : MASK_SOLID;
	projectile->r.svflags = SVF_PROJECTILE;

	projectile->r.mins = Vec3( 0.0f );
	projectile->r.maxs = Vec3( 0.0f );

	projectile->r.owner = owner;
	projectile->touch = touch;
	projectile->nextThink = level.time + def->range;
	projectile->think = G_FreeEdict;
	projectile->timeout = level.time + def->range;
	projectile->timeStamp = level.time;
	projectile->timeDelta = timeDelta;
	projectile->s.team = owner->s.team;
	projectile->s.type = ent_type;

	projectile->projectileInfo.minDamage = Min2( float( def->mindamage ), def->damage );
	projectile->projectileInfo.maxDamage = def->damage;
	projectile->projectileInfo.minKnockback = Min2( def->minknockback, def->knockback );
	projectile->projectileInfo.maxKnockback = def->knockback;
	projectile->projectileInfo.radius = def->splash_radius;
	projectile->projectileInfo.mod = MeanOfDeath( weapon );

	G_ProjectileDistancePrestep( projectile, g_projectile_prestep->value );

	return projectile;
}

static edict_t * FireLinearProjectile(
		edict_t * owner,
		Vec3 start, Vec3 angles,
		int timeDelta,
		WeaponType weapon, EdictTouchCallback touch, int ent_type, int clipmask
) {
	edict_t * projectile = FireProjectile( owner, start, angles, timeDelta, weapon, touch, ent_type, clipmask );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;
	projectile->s.linearMovement = true;
	projectile->s.linearMovementBegin = projectile->s.origin;
	projectile->s.linearMovementVelocity = projectile->velocity;
	projectile->s.linearMovementTimeStamp = svs.gametime;
	projectile->s.linearMovementTimeDelta = Min2( Abs( timeDelta ), 255 );

	return projectile;
}

static void W_Fire_Blade( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Knife );

	int traces = def->projectile_count;
	float slash_angle = def->spread;

	int mask = MASK_SHOT;
	int dmgflags = 0;

	if( GS_RaceGametype( &server_gs ) ) {
		mask = MASK_SOLID;
	}

	for( int i = 0; i < traces; i++ ) {
		Vec3 new_angles = angles;
		angles.y += Lerp( -slash_angle, float( i ) / float( traces - 1 ), slash_angle );

		Vec3 dir;
		AngleVectors( new_angles, &dir, NULL, NULL );
		Vec3 end = start + dir * def->range;

		trace_t trace;
		G_Trace4D( &trace, start, Vec3( 0.0f ), Vec3( 0.0f ), end, self, mask, timeDelta );
		if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
			G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, def->damage, def->knockback, dmgflags, Weapon_Knife );
			break;
		}
	}
}

static void W_Fire_Bullet( edict_t * self, Vec3 start, Vec3 angles, int timeDelta, WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	float spreadness = def->spread;

	if( def->zoom_spread > 0.0f && self->r.client != NULL ) {
		spreadness += ZoomSpreadness( self->r.client->ps.zoom_time, def );
	}

	Vec2 spread = RandomSpreadPattern( self->r.client->ucmd.entropy, spreadness );

	trace_t trace, wallbang;
	GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, spread, def->range, ENTNUM( self ), timeDelta );
	if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
		int dmgflags = DAMAGE_KNOCKBACK_SOFT;

		if( IsHeadshot( trace.ent, trace.endpos, timeDelta ) ) {
			dmgflags |= DAMAGE_HEADSHOT;
		}

		if( trace.endpos != wallbang.endpos ) {
			dmgflags |= DAMAGE_WALLBANG;
		}

		G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, def->damage, def->knockback, dmgflags, weapon );
	}
}

static void W_Fire_Shotgun( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	float damage_dealt[ MAX_CLIENTS + 1 ] = { };
	for( int i = 0; i < def->projectile_count; i++ ) {
		Vec2 spread = FixedSpreadPattern( i, def->spread );

		trace_t trace, wallbang;
		GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, spread, def->range, ENTNUM( self ), timeDelta );
		if( trace.ent != -1 && game.edicts[ trace.ent ].takedamage ) {
			int dmgflags = trace.endpos == wallbang.endpos ? 0 : DAMAGE_WALLBANG;
			G_Damage( &game.edicts[ trace.ent ], self, self, dir, dir, trace.endpos, def->damage, def->knockback, dmgflags, Weapon_Shotgun );

			if( !G_IsTeamDamage( &game.edicts[ trace.ent ].s, &self->s ) && trace.ent <= MAX_CLIENTS ) {
				damage_dealt[ trace.ent ] += def->damage;
			}
		}
	}

	for( int i = 1; i <= MAX_CLIENTS; i++ ) {
		if( damage_dealt[ i ] == 0 )
			continue;
		edict_t * target = &game.edicts[ i ];
		edict_t * ev = G_SpawnEvent( EV_DAMAGE, HEALTH_TO_INT( damage_dealt[ i ] ) << 1, &target->s.origin );
		ev->r.svflags |= SVF_OWNERANDCHASERS;
		ev->s.ownerNum = ENTNUM( self );
	}
}

static void W_Grenade_ExplodeDir( edict_t *ent, Vec3 normal ) {
	Vec3 dir = normal != Vec3( 0.0f ) ? normal : Vec3( 0.0f, 0.0f, 1.0f );

	G_RadiusDamage( ent, ent->r.owner, NULL, ent->enemy, Weapon_GrenadeLauncher );

	int radius = ( ( ent->projectileInfo.radius * 1 / 8 ) > 127 ) ? 127 : ( ent->projectileInfo.radius * 1 / 8 );
	edict_t * event = G_SpawnEvent( EV_GRENADE_EXPLOSION, DirToU64( dir ), &ent->s.origin );
	event->s.weapon = radius;
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Grenade_Explode( edict_t *ent ) {
	W_Grenade_ExplodeDir( ent, Vec3( 0.0f ) );
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
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_GrenadeLauncher );
	}

	ent->enemy = other;
	W_Grenade_ExplodeDir( ent, plane ? plane->normal : Vec3( 0.0f ) );
}

static void W_Fire_Grenade( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * grenade = FireProjectile( self, start, angles, timeDelta, Weapon_GrenadeLauncher, W_Touch_Grenade, ET_GRENADE, MASK_SHOT );

	grenade->classname = "grenade";
	grenade->movetype = MOVETYPE_BOUNCEGRENADE;
	grenade->s.model = "weapons/gl/grenade";
	// grenade->s.sound = "weapons/gl/trail";

	grenade->think = W_Grenade_Explode;
}

static void W_Touch_Stake( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_StakeGun );
		ent->enemy = other;
		edict_t * event = G_SpawnEvent( EV_STAKE_IMPALE, DirToU64( -SafeNormalize( ent->velocity ) ), &ent->s.origin );
		event->s.team = ent->s.team;
		G_FreeEdict( ent );
	}
	else {
		ent->s.type = ET_GENERIC;
		// ent->think = G_FreeEdict;
		// ent->nextThink = level.time + def->range;
		ent->movetype = MOVETYPE_NONE;
		ent->s.sound = "";
		edict_t * event = G_SpawnEvent( EV_STAKE_IMPACT, DirToU64( -SafeNormalize( ent->velocity ) ), &ent->s.origin );
		event->s.team = ent->s.team;
	}
}

static void W_Fire_Stake( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * stake = FireProjectile( self, start, angles, timeDelta, Weapon_StakeGun, W_Touch_Stake, ET_STAKE, MASK_SHOT );

	stake->classname = "stake";
	stake->movetype = MOVETYPE_BOUNCEGRENADE;
	stake->s.model = "weapons/stake/stake";
	stake->s.sound = "weapons/stake/trail";
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
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_RocketLauncher );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, Weapon_RocketLauncher );

	edict_t * event = G_SpawnEvent( EV_ROCKET_EXPLOSION, DirToU64( plane ? plane->normal : Vec3( 0.0f ) ), &ent->s.origin );
	event->s.weapon = Min2( ent->projectileInfo.radius / 8, 255 );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Fire_Rocket( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * rocket = FireLinearProjectile( self, start, angles, timeDelta, Weapon_RocketLauncher, W_Touch_Rocket, ET_ROCKET, MASK_SHOT );

	rocket->classname = "rocket";
	rocket->s.model = "weapons/rl/rocket";
	rocket->s.sound = "weapons/rl/trail";
}

static void W_Fire_Plasma( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * plasma = FireLinearProjectile( self, start, angles, timeDelta, Weapon_Plasma, W_AutoTouch_Plasma, ET_PLASMA, MASK_SHOT );

	plasma->classname = "plasma";
	plasma->s.sound = "weapons/pg/trail";

	plasma->think = W_Think_Plasma;
	plasma->nextThink = level.time + 1;
}

static void FireBubble( edict_t * owner, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * bubble = FireLinearProjectile( owner, start, angles, timeDelta, Weapon_BubbleGun, W_AutoTouch_Plasma, ET_BUBBLE, MASK_SHOT );

	bubble->classname = "bubble";
	bubble->s.sound = "weapons/bg/trail";

	bubble->think = W_Think_Plasma;
	bubble->nextThink = level.time + 1;
}

void W_Fire_BubbleGun( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	constexpr int bubble_spacing = 0;
	const WeaponDef * def = GS_GetWeaponDef( Weapon_BubbleGun );

	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	FireBubble( self, start, angles, timeDelta );

	int n = def->projectile_count - 1;
	float base_angle = random_float01( &svs.rng ) * 2.0f * PI;

	for( int i = 0; i < n; i++ ) {
		float angle = base_angle + 2.0f * PI * float( i ) / float( n );

		Vec3 pos = start;
		pos += right * cosf( angle ) * bubble_spacing;
		pos += up * sinf( angle ) * bubble_spacing;

		Vec3 new_dir = dir;
		new_dir += right * cosf( angle + 0.5f * PI ) * def->spread;
		new_dir += up * sinf( angle + 0.5f * PI ) * def->spread;
		new_dir = Normalize( new_dir );

		Vec3 new_angles = VecToAngles( new_dir );
		FireBubble( self, pos, new_angles, timeDelta );
	}
}

static void W_Fire_Railgun( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );
	Vec3 end = start + dir * def->range;
	Vec3 from = start;

	edict_t * ignore = self;

	trace_t tr;
	tr.ent = -1;

	while( ignore ) {
		G_Trace4D( &tr, from, Vec3( 0.0f ), Vec3( 0.0f ), end, ignore, MASK_WALLBANG, timeDelta );

		from = tr.endpos;
		ignore = NULL;

		if( tr.ent == -1 ) {
			break;
		}

		// some entity was touched
		edict_t * hit = &game.edicts[tr.ent];
		int hit_movetype = hit->movetype; // backup the original movetype as the entity may "die"
		if( hit == world ) { // stop dead if hit the world
			G_RadiusKnockback( def, self, tr.endpos, &tr.plane, Weapon_Railgun, timeDelta );
			break;
		}

		// allow trail to go through BBOX entities (players, gibs, etc)
		if( !CM_IsBrushModel( CM_Server, hit->s.model ) ) {
			ignore = hit;
		}

		if( hit != self && hit->takedamage ) {
			int dmgflags = 0;
			if( IsHeadshot( tr.ent, tr.endpos, timeDelta ) ) {
				dmgflags |= DAMAGE_HEADSHOT;
			}

			G_Damage( hit, self, self, dir, dir, tr.endpos, def->damage, def->knockback, dmgflags, Weapon_Railgun );

			// spawn a impact event on each damaged ent
			edict_t * event = G_SpawnEvent( EV_BOLT_EXPLOSION, DirToU64( tr.plane.normal ), &tr.endpos );
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

static void G_Laser_Think( edict_t * ent ) {
	edict_t * owner = &game.edicts[ent->s.ownerNum];

	if( G_ISGHOSTING( owner ) || owner->s.weapon != Weapon_Laser ||
		PF_GetClientState( PLAYERNUM( owner ) ) < CS_SPAWNED ||
		owner->r.client->ps.weapon_state != WeaponState_FiringSmooth ) {
		G_FreeEdict( ent );
		return;
	}

	ent->nextThink = level.time + 1;
}

struct LaserBeamTraceData {
	float damage;
	int knockback;
	int attacker;
};

static void LaserImpact( const trace_t * trace, Vec3 dir, void * data ) {
	LaserBeamTraceData * beam = ( LaserBeamTraceData * ) data;

	if( trace->ent == beam->attacker ) {
		return; // should not be possible theoretically but happened at least once in practice
	}

	edict_t * attacker = &game.edicts[ beam->attacker ];
	G_Damage( &game.edicts[ trace->ent ], attacker, attacker, dir, dir, trace->endpos, beam->damage, beam->knockback, DAMAGE_KNOCKBACK_SOFT, Weapon_Laser );
}

static edict_t * FindOrSpawnLaser( edict_t * owner ) {
	int ownerNum = ENTNUM( owner );

	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		edict_t * e = &game.edicts[i];
		if( !e->r.inuse ) {
			continue;
		}

		if( e->s.ownerNum == ownerNum && e->s.type == ET_LASERBEAM ) {
			return e;
		}
	}

	edict_t * laser = G_Spawn();
	laser->s.type = ET_LASERBEAM;
	laser->s.ownerNum = ownerNum;
	laser->movetype = MOVETYPE_NONE;
	laser->r.solid = SOLID_NOT;
	laser->r.svflags &= ~SVF_NOCLIENT;
	return laser;
}

static void W_Fire_Lasergun( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Laser );

	edict_t * laser = FindOrSpawnLaser( self );

	LaserBeamTraceData data;
	data.damage = def->damage;
	data.knockback = def->knockback;
	data.attacker = ENTNUM( self );

	trace_t tr;
	GS_TraceLaserBeam( &server_gs, &tr, start, angles, def->range, ENTNUM( self ), timeDelta, LaserImpact, &data );

	laser->r.svflags |= SVF_FORCEOWNER;

	Vec3 dir;
	laser->s.origin = start;
	AngleVectors( angles, &dir, NULL, NULL );
	laser->s.origin2 = laser->s.origin + dir * def->range;

	laser->think = G_Laser_Think;
	laser->nextThink = level.time + 1;

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

	edict_t * event = G_SpawnEvent( EV_RIFLEBULLET_IMPACT, DirToU64( plane ? plane->normal : Vec3( 0.0f ) ), &ent->s.origin );
	event->s.team = ent->s.team;

	if( other->takedamage && ent->enemy != other ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_Rifle );
		ent->enemy = other;
	} else {
		G_FreeEdict( ent );
	}
}

void W_Fire_RifleBullet( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * bullet = FireLinearProjectile( self, start, angles, timeDelta, Weapon_Rifle, W_Touch_RifleBullet, ET_RIFLEBULLET, MASK_WALLBANG );

	bullet->classname = "riflebullet";
	bullet->s.model = "weapons/rifle/bullet";
	bullet->s.sound = "weapons/bullet_whizz";
}

static void W_Touch_Blast( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		edict_t * event = G_SpawnEvent( EV_BLAST_IMPACT, DirToU64( plane ? plane->normal : Vec3( 0.0f ) ), &ent->s.origin );
		event->s.team = ent->s.team;
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, ent->projectileInfo.mod );
		ent->enemy = other;
		G_FreeEdict( ent );
		return;
	}

	edict_t * event = G_SpawnEvent( EV_BLAST_BOUNCE, DirToU64( plane ? plane->normal : Vec3( 0.0f ) ), &ent->s.origin );
	event->s.team = ent->s.team;

	if( ent->num_bounces >= 5 ) {
		G_FreeEdict( ent );
	}
}

void W_Fire_Blast( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_MasterBlaster );
	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	for( int i = 0; i < def->projectile_count; i++ ) {
		Vec2 spread = FixedSpreadPattern( i, def->spread );

		Vec3 blast_dir = dir * def->range + right * spread.x + up * spread.y;
		Vec3 blast_angles = VecToAngles( blast_dir );

		edict_t * blast = FireProjectile( self, start, blast_angles, timeDelta, Weapon_MasterBlaster, W_Touch_Blast, ET_BLAST, MASK_SHOT );

		blast->classname = "blast";
		blast->movetype = MOVETYPE_BOUNCEGRENADE;
		blast->stop = G_FreeEdict;
		blast->s.sound = "weapons/mb/trail";
	}
}

void W_Fire_Road( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * bullet = FireProjectile( self, start, angles, timeDelta, Weapon_RoadGun, W_Touch_Blast, ET_BLAST, MASK_SHOT );

	bullet->classname = "zorg";
	bullet->movetype = MOVETYPE_BOUNCEGRENADE;
	bullet->stop = G_FreeEdict;
	bullet->s.sound = "weapons/road/trail";
}

void G_FireWeapon( edict_t * ent, u64 weap ) {
	Vec3 origin, angles;
	Vec3 viewoffset = Vec3( 0.0f );
	int timeDelta = 0;

	// find this shot projection source
	if( ent->r.client != NULL ) {
		viewoffset.z += ent->r.client->ps.viewheight;
		timeDelta = ent->r.client->timeDelta;
		angles = ent->r.client->ps.viewangles;
	}
	else {
		angles = ent->s.angles;
	}

	origin = ent->s.origin + viewoffset;

	switch( weap ) {
		default:
			return;

		case Weapon_Knife:
			W_Fire_Blade( ent, origin, angles, timeDelta );
			break;

		case Weapon_Pistol:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Pistol );
			break;

		case Weapon_MachineGun:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_MachineGun );
			break;

		case Weapon_Deagle:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Deagle );
			break;

		case Weapon_Shotgun:
			W_Fire_Shotgun( ent, origin, angles, timeDelta );
			break;

		case Weapon_AssaultRifle:
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_AssaultRifle );
			break;

		case Weapon_StakeGun:
			W_Fire_Stake( ent, origin, angles, timeDelta );
			break;

		case Weapon_GrenadeLauncher:
			W_Fire_Grenade( ent, origin, angles, timeDelta );
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
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Sniper );
			break;

		case Weapon_Railgun:
			W_Fire_Railgun( ent, origin, angles, timeDelta );
			break;

		case Weapon_Rifle:
			W_Fire_RifleBullet( ent, origin, angles, timeDelta );
			break;

		case Weapon_MasterBlaster:
			W_Fire_Blast( ent, origin, angles, timeDelta );
			break;

		case Weapon_RoadGun:
			W_Fire_Road( ent, origin, angles, timeDelta );
			break;

		case Weapon_Minigun: {
			W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Minigun );

			Vec3 dir;
			AngleVectors( angles, &dir, NULL, NULL );
			ent->velocity -= dir * GS_GetWeaponDef( Weapon_Minigun )->knockback;
		} break;
	}

	// add stats
	if( ent->r.client != NULL ) {
		G_ClientGetStats( ent )->accuracy_shots[ weap ] += GS_GetWeaponDef( weap )->projectile_count;
	}
}
