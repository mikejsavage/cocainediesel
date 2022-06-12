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

#include "gameshared/trace.h"

#define ARBULLETHACK // ffs : hack for the assault rifle

static bool CanHit( const edict_t * projectile, const edict_t * target ) {
	if( target == world )
		return true;
	if( target == projectile->r.owner )
		return false;

	constexpr s64 projectile_ignore_teammates_time = 50;
	if( projectile->s.team != Team_None && projectile->s.team == target->s.team && level.time - projectile->timeStamp < projectile_ignore_teammates_time )
		return false;

	return true;
}

static void W_Explode_ARBullet( edict_t * ent, edict_t * other, Plane * plane ) {
	if( other != NULL && other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, DAMAGE_KNOCKBACK_SOFT, ent->projectileInfo.damage_type );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, ent->projectileInfo.damage_type );

	edict_t * event = G_SpawnEvent( ent->s.type == ET_ARBULLET ? EV_ARBULLET_EXPLOSION : EV_BUBBLE_EXPLOSION, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Touch_ARBullet( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	W_Explode_ARBullet( ent, other, plane );
}

static void W_ARBullet_Backtrace( edict_t * ent, Vec3 start ) {
	trace_t tr;
	Vec3 mins( -2.0f ), maxs( 2.0f );

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
			W_Touch_ARBullet( ent, &game.edicts[ tr.ent ], NULL, 0 );
		}
		else {
			W_Touch_ARBullet( ent, &game.edicts[ tr.ent ], &tr.plane, tr.surfFlags );
		}

		iter++;
	} while( ent->r.inuse && ent->s.origin != oldorigin && iter < 5 );

	if( ent->r.inuse ) {
		ent->s.origin = oldorigin;
	}
}

static void W_Think_ARBullet( edict_t * ent ) {
	if( ent->timeout < level.time ) {
		if( ent->s.type == ET_BUBBLE )
			W_Explode_ARBullet( ent, NULL, NULL );
		else
			G_FreeEdict( ent );
		return;
	}

	if( ent->r.inuse ) {
		ent->nextThink = level.time + 1;
	}

	Vec3 start = ent->s.origin - ent->velocity * game.frametime * 0.001f;

	W_ARBullet_Backtrace( ent, start );
}

static void W_AutoTouch_ARBullet( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	W_Think_ARBullet( ent );
	if( ent->r.inuse ) {
		W_Touch_ARBullet( ent, other, plane, surfFlags );
	}
}

static void G_ProjectileDistancePrestep( edict_t * projectile, float distance ) {
	assert( projectile->movetype == MOVETYPE_TOSS || projectile->movetype == MOVETYPE_LINEARPROJECTILE || projectile->movetype == MOVETYPE_BOUNCE || projectile->movetype == MOVETYPE_BOUNCEGRENADE );

	if( !distance ) {
		return;
	}

#ifdef ARBULLETHACK
	Vec3 arbullet_hack_start = projectile->s.origin;
#endif

	Vec3 dir = Normalize( projectile->velocity );
	Vec3 dest = projectile->s.origin + dir * distance;

	trace_t trace;
	G_Trace4D( &trace, projectile->s.origin, projectile->r.mins, projectile->r.maxs, dest, projectile->r.owner, projectile->r.clipmask, projectile->timeDelta );

	projectile->s.origin = trace.endpos;
	projectile->olds.origin = trace.endpos;

	GClip_LinkEntity( projectile );
	SV_Impact( projectile, &trace );

	if( !projectile->r.inuse ) {
		return;
	}

	// ffs : hack for the assault rifle
#ifdef ARBULLETHACK
	if( projectile->s.type == ET_ARBULLET || projectile->s.type == ET_BUBBLE ) {
		W_ARBullet_Backtrace( projectile, arbullet_hack_start );
	}
#endif
}

struct ProjectileStats {
	int min_damage;
	int max_damage;
	int min_knockback;
	int max_knockback;
	int speed;
	int timeout;
	int splash_radius;
	DamageType damage_type;
};

static ProjectileStats WeaponProjectileStats( WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	ProjectileStats stats;
	stats.min_damage = def->min_damage;
	stats.max_damage = def->damage;
	stats.min_knockback = def->min_knockback;
	stats.max_knockback = def->knockback;
	stats.speed = def->speed;
	stats.timeout = def->range;
	stats.splash_radius = def->splash_radius;
	stats.damage_type = weapon;

	return stats;
}

static ProjectileStats GadgetProjectileStats( GadgetType gadget ) {
	const GadgetDef * def = GetGadgetDef( gadget );

	ProjectileStats stats;
	stats.min_damage = def->min_damage;
	stats.max_damage = def->damage;
	stats.min_knockback = def->min_knockback;
	stats.max_knockback = def->knockback;
	stats.speed = def->speed;
	stats.timeout = def->timeout;
	stats.splash_radius = def->splash_radius;
	stats.damage_type = gadget;

	return stats;
}

static edict_t * GenEntity( edict_t * owner, Vec3 pos, Vec3 angles, EntityType ent_type, int timeout ) {
	edict_t * ent = G_Spawn();
	ent->s.origin = pos;
	ent->olds.origin = pos;
	ent->s.angles = angles;

	ent->r.solid = SOLID_NOT;

	ent->r.mins = Vec3( 0.0f );
	ent->r.maxs = Vec3( 0.0f );

	ent->r.owner = owner;
	ent->s.ownerNum = owner->s.number;
	ent->nextThink = level.time + timeout;
	ent->think = G_FreeEdict;
	ent->timeout = level.time + timeout;
	ent->timeStamp = level.time;
	ent->s.team = owner->s.team;
	ent->s.type = ent_type;

	return ent;
}

static edict_t * FireProjectile(
	edict_t * owner,
	Vec3 start, Vec3 angles,
	int timeDelta,
	ProjectileStats stats, EdictTouchCallback touch, EntityType ent_type, int clipmask ) {
	edict_t * projectile = GenEntity( owner, start, angles, ent_type, stats.timeout );

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );

	projectile->velocity = dir * stats.speed;

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;

	projectile->r.solid = SOLID_YES;
	projectile->r.clipmask = clipmask;
	projectile->s.svflags = SVF_PROJECTILE;

	projectile->timeDelta = timeDelta;
	projectile->touch = touch;

	projectile->projectileInfo.minDamage = stats.min_damage;
	projectile->projectileInfo.maxDamage = stats.max_damage;
	projectile->projectileInfo.minKnockback = stats.min_knockback;
	projectile->projectileInfo.maxKnockback = stats.max_knockback;
	projectile->projectileInfo.radius = stats.splash_radius;
	projectile->projectileInfo.damage_type = stats.damage_type;

	G_ProjectileDistancePrestep( projectile, g_projectile_prestep->number );

	return projectile;
}

static edict_t * FireLinearProjectile(
	edict_t * owner,
	Vec3 start, Vec3 angles,
	int timeDelta,
	ProjectileStats stats, EdictTouchCallback touch, EntityType ent_type, int clipmask
) {
	edict_t * projectile = FireProjectile( owner, start, angles, timeDelta, stats, touch, ent_type, clipmask );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;
	projectile->s.linearMovement = true;
	projectile->s.linearMovementBegin = projectile->s.origin;
	projectile->s.linearMovementVelocity = projectile->velocity;
	projectile->s.linearMovementTimeStamp = svs.gametime;
	projectile->s.linearMovementTimeDelta = Min2( Abs( timeDelta ), 255 );

	return projectile;
}

static void HitWithSpread( edict_t * self, Vec3 start, Vec3 angles, float range, float spread, int traces, float damage, float knockback, WeaponType weapon, int timeDelta ) {
	Vec3 forward;
	AngleVectors( angles, &forward, NULL, NULL );

	for( int i = 0; i < traces; i++ ) {
		Vec3 new_angles = angles;
		new_angles.y += Lerp( -spread, float( i ) / float( traces - 1 ), spread );
		Vec3 dir;
		AngleVectors( new_angles, &dir, NULL, NULL );
		Vec3 end = start + dir * range;

		trace_t trace;
		G_Trace4D( &trace, start, Vec3( 0.0f ), Vec3( 0.0f ), end, self, MASK_SHOT, timeDelta );
		if( trace.ent != -1 && game.edicts[ trace.ent ].takedamage ) {
			G_Damage( &game.edicts[ trace.ent ], self, self, forward, forward, trace.endpos, damage, knockback, 0, weapon );
			break;
		}
	}
}

static void W_Fire_Blade( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Knife );

	HitWithSpread( self, start, angles, def->range, def->spread, def->projectile_count, def->damage, def->knockback, Weapon_Knife, timeDelta );
}

static void W_Fire_Bat( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Bat );

	float charge = Min2( 1.0f, float( self->r.client->ps.weapon_state_time ) / float( def->reload_time ) );
	float damage = Lerp( float( def->min_damage ), charge, float( def->damage ) );
	float knockback = def->knockback * charge;

	HitWithSpread( self, start, angles, def->range, def->spread, def->projectile_count, damage, knockback, Weapon_Bat, timeDelta );
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


	// {
	// 	Ray ray;
	// 	ray.origin = start;
	// 	ray.direction = dir;
	// 	ray.t_max = def->range;
	// 	Intersection enter, leave;
	// 	bool hit = Trace( &svs.map, &svs.map.models[ 0 ], ray, enter, leave );
	// 	if( hit ) {
	// 		Vec3 pos = start + dir * enter.t;
	// 		Com_GGPrint( "hit {} {}", pos, enter.normal );
	// 	}
	// 	else {
	// 		Com_GGPrint( "not hit" );
	// 	}
	// }


	trace_t trace, wallbang;
	GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, spread, def->range, ENTNUM( self ), timeDelta );
	if( trace.ent != -1 && game.edicts[ trace.ent ].takedamage ) {
		int dmgflags = DAMAGE_KNOCKBACK_SOFT;
		float damage = def->damage;

		if( IsHeadshot( trace.ent, trace.endpos, timeDelta ) ) {
			dmgflags |= DAMAGE_HEADSHOT;
		}

		if( trace.endpos != wallbang.endpos ) {
			dmgflags |= DAMAGE_WALLBANG;
			damage *= def->wallbangdamage;
		}

		G_Damage( &game.edicts[ trace.ent ], self, self, dir, dir, trace.endpos, damage, def->knockback, dmgflags, weapon );
	}
}

static void W_Fire_Shotgun( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	float damage_dealt[ MAX_CLIENTS + 1 ] = { };
	Vec3 hit_locations[ MAX_CLIENTS + 1 ] = { }; // arbitrary trace end pos to use as blood origin

	for( int i = 0; i < def->projectile_count; i++ ) {
		Vec2 spread = FixedSpreadPattern( i, def->spread );

		trace_t trace, wallbang;
		GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, spread, def->range, ENTNUM( self ), timeDelta );
		if( trace.ent != -1 && game.edicts[ trace.ent ].takedamage ) {
			int dmgflags = trace.endpos == wallbang.endpos ? 0 : DAMAGE_WALLBANG;
			float damage = def->damage;

			if( trace.endpos != wallbang.endpos ) {
				dmgflags |= DAMAGE_WALLBANG;
				damage *= def->wallbangdamage;
			}

			G_Damage( &game.edicts[ trace.ent ], self, self, dir, dir, trace.endpos, damage, def->knockback, dmgflags, Weapon_Shotgun );

			if( !G_IsTeamDamage( &game.edicts[ trace.ent ].s, &self->s ) && trace.ent <= MAX_CLIENTS ) {
				damage_dealt[ trace.ent ] += damage;
				hit_locations[ trace.ent ] = trace.endpos;
			}
		}
	}

	for( int i = 1; i <= MAX_CLIENTS; i++ ) {
		if( damage_dealt[ i ] == 0 )
			continue;
		SpawnDamageEvents( self, &game.edicts[ i ], damage_dealt[ i ], false, hit_locations[ i ], dir, true );
	}
}

static void W_Grenade_ExplodeDir( edict_t * ent, Vec3 normal ) {
	Vec3 dir = normal != Vec3( 0.0f ) ? normal : Vec3( 0.0f, 0.0f, 1.0f );

	G_RadiusDamage( ent, ent->r.owner, NULL, ent->enemy, Weapon_GrenadeLauncher );

	edict_t * event = G_SpawnEvent( EV_GRENADE_EXPLOSION, DirToU64( dir ), &ent->s.origin );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Grenade_Explode( edict_t * ent ) {
	W_Grenade_ExplodeDir( ent, Vec3( 0.0f ));
}

static void W_Touch_Grenade( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_GrenadeLauncher );

	if( !CanHit( ent, other ) ) {
		return;
	}

	// don't explode on doors and plats that take damage
	if( !other->takedamage || CM_IsBrushModel( CM_Server, other->s.model ) ) {
		Vec3 parallel = Project( ent->velocity, plane->normal );
		Vec3 perpendicular = ent->velocity - parallel;

		float friction = 0.1f;
		float velocity = Length( parallel ) + Length( perpendicular ) * friction;

		u16 volume = Lerp( u16( 0 ), Unlerp01( 0.0f, velocity, float( def->speed ) ), U16_MAX );
		G_AddEvent( ent, EV_GRENADE_BOUNCE, volume, true );

		return;
	}

	if( other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_GrenadeLauncher );
	}

	ent->enemy = other;
	W_Grenade_ExplodeDir( ent, plane ? plane->normal : Vec3( 0.0f ));
}

static void W_Fire_Grenade( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * grenade = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_GrenadeLauncher ), W_Touch_Grenade, ET_GRENADE, MASK_SHOT );

	grenade->classname = "grenade";
	grenade->movetype = MOVETYPE_BOUNCEGRENADE;
	grenade->s.model = "weapons/gl/grenade";
	// grenade->s.sound = "weapons/gl/trail";

	grenade->think = W_Grenade_Explode;
}

static void W_Touch_Stake( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_StakeGun );
		ent->enemy = other;
		edict_t * event = G_SpawnEvent( EV_STAKE_IMPALE, DirToU64( -SafeNormalize( ent->velocity )), &ent->s.origin );
		event->s.team = ent->s.team;
		G_FreeEdict( ent );
	}
	else {
		ent->s.type = ET_GENERIC;
		// ent->think = G_FreeEdict;
		// ent->nextThink = level.time + def->range;
		ent->movetype = MOVETYPE_NONE;
		ent->s.sound = EMPTY_HASH;
		edict_t * event = G_SpawnEvent( EV_STAKE_IMPACT, DirToU64( -SafeNormalize( ent->velocity )), &ent->s.origin );
		event->s.team = ent->s.team;
	}
}

static void W_Fire_Stake( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * stake = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_StakeGun ), W_Touch_Stake, ET_STAKE, MASK_SHOT );

	stake->classname = "stake";
	stake->movetype = MOVETYPE_BOUNCEGRENADE;
	stake->s.model = "weapons/stake/stake";
	stake->s.sound = "weapons/stake/trail";
}

static void W_Touch_Rocket( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_RocketLauncher );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, Weapon_RocketLauncher );

	edict_t * event = G_SpawnEvent( EV_ROCKET_EXPLOSION, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

static void W_Fire_Rocket( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * rocket = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_RocketLauncher ), W_Touch_Rocket, ET_ROCKET, MASK_SHOT );

	rocket->classname = "rocket";
	rocket->s.model = "weapons/rl/rocket";
	rocket->s.sound = "weapons/rl/trail";
}

static void W_Fire_ARBullet( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * arbullet = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_AssaultRifle ), W_AutoTouch_ARBullet, ET_ARBULLET, MASK_SHOT );

	arbullet->classname = "arbullet";
	arbullet->s.model = "weapons/ar/projectile";
	arbullet->s.sound = "weapons/ar/trail";

	arbullet->think = W_Think_ARBullet;
	arbullet->nextThink = level.time + 1;
}

static void FireBubble( edict_t * owner, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * bubble = FireLinearProjectile( owner, start, angles, timeDelta, WeaponProjectileStats( Weapon_BubbleGun ), W_AutoTouch_ARBullet, ET_BUBBLE, MASK_SHOT );

	bubble->classname = "bubble";
	bubble->s.sound = "weapons/bg/trail";

	bubble->think = W_Think_ARBullet;
	bubble->nextThink = level.time + 1;
}

void W_Fire_BubbleGun( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	constexpr int bubble_spacing = 0;
	const WeaponDef * def = GS_GetWeaponDef( Weapon_BubbleGun );

	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	FireBubble( self, start, angles, timeDelta );

	int n = def->projectile_count - 1;
	float base_angle = RandomFloat01( &svs.rng ) * 2.0f * PI;

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
		edict_t * hit = &game.edicts[ tr.ent ];
		int hit_movetype = hit->movetype; // backup the original movetype as the entity may "die"
		if( hit == world ) { // stop dead if hit the world
			break;
		}

		// allow trail to go through BBOX entities ( players, gibs, etc )
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

static void RailgunAltDeploy( edict_t * ent ) {
	edict_t * event = G_SpawnEvent( EV_RAIL_ALT, 0, &ent->s.origin );
	event->s.ownerNum = ent->s.ownerNum;
	event->s.angles = ent->s.angles;

	edict_t * owner = &game.edicts[ ent->s.ownerNum ];
	W_Fire_Railgun( owner, ent->s.origin, ent->s.angles, 0 );

	G_FreeEdict( ent );
}

static void W_Fire_RailgunAlt( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );
	edict_t * ent = GenEntity( self, start, angles, ET_RAILGUN, def->reload_time );

	ent->classname = "railgunalt";
	ent->think = RailgunAltDeploy;
}


static void G_Laser_Think( edict_t * ent ) {
	edict_t * owner = &game.edicts[ ent->s.ownerNum ];

	if( G_ISGHOSTING( owner ) || owner->s.weapon != Weapon_Laser ||
		PF_GetClientState( PLAYERNUM( owner )) < CS_SPAWNED ||
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
	LaserBeamTraceData * beam = ( LaserBeamTraceData * )data;

	if( trace->ent == beam->attacker ) {
		return; // should not be possible theoretically but happened at least once in practice
	}

	edict_t * attacker = &game.edicts[ beam->attacker ];
	G_Damage( &game.edicts[ trace->ent ], attacker, attacker, dir, dir, trace->endpos, beam->damage, beam->knockback, DAMAGE_KNOCKBACK_SOFT, Weapon_Laser );
}

static edict_t * FindOrSpawnLaser( edict_t * owner ) {
	int ownerNum = ENTNUM( owner );

	for( int i = server_gs.maxclients + 1; i < game.maxentities; i++ ) {
		edict_t * e = &game.edicts[ i ];
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
	laser->s.svflags &= ~SVF_NOCLIENT;
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

	laser->s.svflags |= SVF_FORCEOWNER;

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

static void W_Touch_RifleBullet( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	edict_t * event = G_SpawnEvent( EV_RIFLEBULLET_IMPACT, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
	event->s.team = ent->s.team;

	if( other->takedamage && ent->enemy != other ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_Rifle );
		ent->enemy = other;
	}
	else {
		G_FreeEdict( ent );
	}
}

void W_Fire_RifleBullet( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * bullet = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_Rifle ), W_Touch_RifleBullet, ET_RIFLEBULLET, MASK_WALLBANG );

	bullet->classname = "riflebullet";
	bullet->s.model = "weapons/rifle/bullet";
	bullet->s.sound = "weapons/bullet_whizz";
}

static void StickyExplodeNormal( edict_t * ent, Vec3 normal, bool silent ) {
	G_RadiusDamage( ent, ent->r.owner, NULL, ent->enemy, Weapon_StickyGun );

	edict_t * event = G_SpawnEvent( EV_STICKY_EXPLOSION, silent ? 1 : 0, &ent->s.origin );
	event->s.team = ent->s.team;

	float radius = ent->projectileInfo.radius;
	Vec3 origin = ent->s.origin;
	int timeDelta = ent->timeDelta;

	G_FreeEdict( ent );

	int nearby[ MAX_EDICTS ];
	int n = GClip_FindInRadius4D( origin, radius, nearby, ARRAY_COUNT( nearby ), timeDelta );
	for( int i = 0; i < n; i++ ) {
		edict_t * other = game.edicts + nearby[ i ];
		if( other->classname == "sticky" ) {
			StickyExplodeNormal( other, Vec3( 0.0f ), true );
		}
	}
}

static void StickyExplode( edict_t * ent ) {
	StickyExplodeNormal( ent, Vec3( 0.0f ), false );
}

static void W_Touch_Sticky( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_StickyGun );
		ent->enemy = other;

		StickyExplodeNormal( ent, plane ? plane->normal : Vec3( 0.0f ), false );
	}
	else {
		const WeaponDef * def = GS_GetWeaponDef( Weapon_StickyGun );
		ent->s.linearMovementBegin = ent->s.origin;
		ent->s.linearMovementVelocity = Vec3( 0.0f );
		ent->avelocity = Vec3( 0.0f );
		ent->nextThink = level.time + def->spread; //gg
		G_SpawnEvent( EV_STICKY_IMPACT, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
	}
}

void W_Fire_Sticky( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_StickyGun );

	float spreadness = def->zoom_spread * ( 1.0f - float( self->r.client->ps.zoom_time ) / float( ZOOMTIME ) );
	Vec2 spread = UniformSampleInsideCircle( &svs.rng ) * spreadness;
	angles.x += spread.x;
	angles.y += spread.y;

	edict_t * bullet = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_StickyGun ), W_Touch_Sticky, ET_ROCKET, MASK_SHOT );

	bullet->classname = "sticky";
	bullet->s.model = "weapons/sticky/bullet";
	bullet->s.sound = "weapons/sticky/fuse";
	bullet->avelocity = UniformSampleInsideSphere( &svs.rng ) * 1800.0f;

	bullet->think = StickyExplode;
}

static void W_Touch_Blast( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		edict_t * event = G_SpawnEvent( EV_BLAST_IMPACT, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
		event->s.team = ent->s.team;
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, ent->projectileInfo.damage_type );
		ent->enemy = other;
		G_FreeEdict( ent );
		return;
	}

	edict_t * event = G_SpawnEvent( EV_BLAST_BOUNCE, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
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

		edict_t * blast = FireProjectile( self, start, blast_angles, timeDelta, WeaponProjectileStats( Weapon_MasterBlaster ), W_Touch_Blast, ET_BLAST, MASK_SHOT );

		blast->classname = "blast";
		blast->movetype = MOVETYPE_BOUNCEGRENADE;
		blast->stop = G_FreeEdict;
		blast->s.sound = "weapons/mb/trail";
	}
}

void W_Fire_Road( edict_t * self, Vec3 start, Vec3 angles, int timeDelta ) {
	edict_t * bullet = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_RoadGun ), W_Touch_Blast, ET_BLAST, MASK_SHOT );

	bullet->classname = "zorg";
	bullet->movetype = MOVETYPE_BOUNCEGRENADE;
	bullet->stop = G_FreeEdict;
	bullet->s.sound = "weapons/road/trail";
}

static void CallFireWeapon( edict_t * ent, u64 parm, bool alt ) {
	WeaponType weapon = WeaponType( parm & 0xFF );

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

	switch( weapon ) {
		default:
			return;

		case Weapon_Knife:
			W_Fire_Blade( ent, origin, angles, timeDelta );
			break;

		case Weapon_Bat:
			W_Fire_Bat( ent, origin, angles, timeDelta );
			break;

		case Weapon_Pistol:
		case Weapon_MachineGun:
		case Weapon_Deagle:
		case Weapon_BurstRifle:
		case Weapon_Sniper:
		case Weapon_AutoSniper:
			W_Fire_Bullet( ent, origin, angles, timeDelta, weapon );
			break;

		case Weapon_Shotgun:
			W_Fire_Shotgun( ent, origin, angles, timeDelta );
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

		case Weapon_AssaultRifle:
			W_Fire_ARBullet( ent, origin, angles, timeDelta );
			break;

		case Weapon_BubbleGun:
			W_Fire_BubbleGun( ent, origin, angles, timeDelta );
			break;

		case Weapon_Laser:
			W_Fire_Lasergun( ent, origin, angles, timeDelta );
			break;

		case Weapon_Railgun:
			if( alt ) {
				W_Fire_RailgunAlt( ent, origin, angles, timeDelta );
			}
			else {
				W_Fire_Railgun( ent, origin, angles, timeDelta );
			}
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

		case Weapon_StickyGun:
			W_Fire_Sticky( ent, origin, angles, timeDelta );
			break;

			// case Weapon_Minigun: {
			// 	W_Fire_Bullet( ent, origin, angles, timeDelta, Weapon_Minigun );
			//
			// 	Vec3 dir;
			// 	AngleVectors( angles, &dir, NULL, NULL );
			// 	ent->velocity -= dir * GS_GetWeaponDef( Weapon_Minigun )->knockback;
			// } break;
	}
}

void G_FireWeapon( edict_t * ent, u64 parm ) {
	CallFireWeapon( ent, parm, false );
}

void G_AltFireWeapon( edict_t * ent, u64 parm ) {
	CallFireWeapon( ent, parm, true );
}

static void TouchThrowingAxe( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	// edict_t * event = G_SpawnEvent( EV_AXE_IMPACT, DirToU64( plane ? plane->normal : Vec3( 0.0f )), &ent->s.origin );
	// event->s.team = ent->s.team;

	G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Gadget_ThrowingAxe );
	G_FreeEdict( ent );
}

static void UseThrowingAxe( edict_t * self, Vec3 start, Vec3 angles, int timeDelta, u64 charge_time ) {
	const GadgetDef * def = GetGadgetDef( Gadget_ThrowingAxe );

	ProjectileStats stats = GadgetProjectileStats( Gadget_ThrowingAxe );
	float cook_frac = Unlerp01( u64( 0 ), charge_time, u64( def->cook_time ) );
	stats.max_damage = Lerp( def->min_damage, cook_frac, def->damage );
	stats.speed = Lerp( def->min_speed, cook_frac, def->speed );

	edict_t * axe = FireProjectile( self, start, angles, timeDelta, stats, TouchThrowingAxe, ET_THROWING_AXE, MASK_SHOT );
	axe->classname = "throwing axe";
	axe->movetype = MOVETYPE_BOUNCE;
	axe->s.model = "gadgets/hatchet/model";
	axe->s.sound = "gadgets/hatchet/trail";
	axe->avelocity = Vec3( 360.0f * 4, 0.0f, 0.0f );
}

static void ExplodeStunGrenade( edict_t * grenade ) {
	const GadgetDef * def = GetGadgetDef( Gadget_StunGrenade );
	static constexpr float flash_distance = 2000.0f;

	edict_t * event = G_SpawnEvent( EV_STUN_GRENADE_EXPLOSION, 0, &grenade->s.origin );
	event->s.team = grenade->s.team;

	int touch[ MAX_EDICTS ];
	int numtouch = GClip_FindInRadius4D( grenade->s.origin, def->splash_radius + playerbox_stand_viewheight, touch, ARRAY_COUNT( touch ), grenade->timeDelta );

	for( int i = 0; i < numtouch; i++ ) {
		edict_t * other = &game.edicts[ touch[ i ] ];
		if( other->s.type != ET_PLAYER )
			 continue;

		SyncPlayerState * ps = &other->r.client->ps;
		Vec3 eye = other->s.origin + Vec3( 0.0f, 0.0f, ps->viewheight );

		trace_t grenade_to_eye;
		G_Trace4D( &grenade_to_eye, grenade->s.origin, Vec3( 0.0f ), Vec3( 0.0f ), eye, grenade, MASK_SOLID, grenade->timeDelta );
		if( grenade_to_eye.fraction == 1.0f ) {
			float distance = Length( eye - grenade->s.origin );
			u16 distance_flash = Lerp( u16( 0 ), Unlerp01( float( flash_distance ), distance, float( def->min_damage ) ), U16_MAX );

			Vec3 forward;
			AngleVectors( ps->viewangles, &forward, NULL, NULL );
			float theta = Dot( SafeNormalize( grenade->s.origin - eye ), forward );
			float angle_scale = Lerp( 0.25f, Unlerp( -1.0f, theta, 1.0f ), 1.0f );

			ps->flashed = Min2( u32( ps->flashed ) + u32( distance_flash * angle_scale ), u32( U16_MAX ) );
			if( other->s.team == grenade->s.team ) {
				ps->flashed *= 0.25;
			} else {
				G_RadiusKnockback( def->knockback, def->min_knockback, def->splash_radius, grenade, grenade->s.origin, NULL, 0 );
			}
		}
	}

	G_FreeEdict( grenade );
}

static void TouchStunGrenade( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( ENTNUM( other ) != 0 ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Gadget_StunGrenade );
		ExplodeStunGrenade( ent );
	}
}

static void UseStunGrenade( edict_t * self, Vec3 start, Vec3 angles, int timeDelta, u64 charge_time ) {
	const GadgetDef * def = GetGadgetDef( Gadget_StunGrenade );

	ProjectileStats stats = GadgetProjectileStats( Gadget_StunGrenade );
	stats.speed = Lerp( def->min_speed, Unlerp01( u64( 0 ), charge_time, u64( def->cook_time ) ), def->speed );

	edict_t * grenade = FireProjectile( self, start, angles, timeDelta, stats, TouchStunGrenade, ET_GENERIC, MASK_SHOT );
	grenade->classname = "stun grenade";
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->s.model = "gadgets/flash/model";
	grenade->avelocity = Vec3( 360.0f, 0.0f, 0.0f );
	grenade->think = ExplodeStunGrenade;
}

void G_UseGadget( edict_t * ent, GadgetType gadget, u64 parm ) {
	Vec3 origin = ent->s.origin;
	origin.z += ent->r.client->ps.viewheight;

	Vec3 angles = ent->r.client->ps.viewangles;

	int timeDelta = ent->r.client->timeDelta;

	switch( gadget ) {
		case Gadget_ThrowingAxe:
			UseThrowingAxe( ent, origin, angles, timeDelta, parm );
			break;

		case Gadget_StunGrenade:
			UseStunGrenade( ent, origin, angles, timeDelta, parm );
			break;
	}
}
