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

static void SpawnFX( const edict_t * ent, Optional< Vec3 > normal, StringHash vfx, StringHash sfx ) {
	edict_t * event = G_SpawnEvent( EV_FX, DirToU64( Default( normal, Vec3( 0.0f ) ) ), &ent->s.origin );
	event->s.team = ent->s.team;
	event->s.model = vfx;
	event->s.model2 = sfx;
}

static void Explode( edict_t * ent, edict_t * other, Optional< Vec3 > normal ) {
	if( other != NULL && other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, DAMAGE_KNOCKBACK_SOFT, ent->projectileInfo.damage_type );
	}

	G_RadiusDamage( ent, ent->r.owner, normal, other, ent->projectileInfo.damage_type );

	StringHash vfx = ent->projectileInfo.explosion_vfx;
	StringHash sfx = ent->projectileInfo.explosion_sfx;
	SpawnFX( ent, normal, vfx, sfx );

	G_FreeEdict( ent );
}

static void W_Touch_ARBullet( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	Explode( ent, other, normal );
}

static void W_ARBullet_Backtrace( edict_t * ent, Vec3 start ) {
	trace_t tr;

	Vec3 oldorigin = ent->s.origin;
	ent->s.origin = start;

	// workaround to stop this hanging when you shoot a teammate
	int iter = 0;
	do {
		tr = G_Trace4D( ent->s.origin, MinMax3( 2.0f ), oldorigin, ent, SolidMask_Player, ent->timeDelta );

		ent->s.origin = tr.endpos;

		if( tr.HitNothing() )
			break;

		W_Touch_ARBullet( ent, &game.edicts[ tr.ent ], tr.normal, tr.solidity );

		iter++;
	} while( ent->r.inuse && ent->s.origin != oldorigin && iter < 5 );

	if( ent->r.inuse ) {
		ent->s.origin = oldorigin;
	}
}

static void W_Think_ARBullet( edict_t * ent ) {
	if( ent->timeout < level.time ) {
		if( ent->s.type == ET_BUBBLE )
			Explode( ent, NULL, NONE );
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

static void W_AutoTouch_ARBullet( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	W_Think_ARBullet( ent );
	if( ent->r.inuse ) {
		W_Touch_ARBullet( ent, other, normal, solid_mask );
	}
}

static void G_ProjectileDistancePrestep( edict_t * projectile, float distance ) {
	Assert( projectile->movetype == MOVETYPE_TOSS || projectile->movetype == MOVETYPE_LINEARPROJECTILE || projectile->movetype == MOVETYPE_BOUNCE || projectile->movetype == MOVETYPE_BOUNCEGRENADE );

	if( !distance ) {
		return;
	}

#ifdef ARBULLETHACK
	Vec3 arbullet_hack_start = projectile->s.origin;
#endif

	Vec3 dir = Normalize( projectile->velocity );
	Vec3 dest = projectile->s.origin + dir * distance;

	SolidBits solid_mask = EntitySolidity( ServerCollisionModelStorage(), &projectile->s );
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &projectile->s );
	trace_t trace = G_Trace4D( projectile->s.origin, bounds, dest, projectile->r.owner, solid_mask, projectile->timeDelta );

	projectile->s.origin = trace.endpos;
	projectile->olds.origin = trace.endpos;

	GClip_LinkEntity( projectile );
	SV_Impact( projectile, trace );

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
	s64 timeout;
	float gravity_scale;
	float restitution;
	int splash_radius;
	DamageType damage_type;
};

static ProjectileStats WeaponProjectileStats( WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	return ProjectileStats {
		.min_damage = def->min_damage,
		.max_damage = def->damage,
		.min_knockback = def->min_knockback,
		.max_knockback = def->knockback,
		.speed = def->speed,
		.timeout = def->range,
		.gravity_scale = def->gravity_scale,
		.restitution = def->restitution,
		.splash_radius = def->splash_radius,
		.damage_type = weapon,
	};
}

static ProjectileStats GadgetProjectileStats( GadgetType gadget ) {
	const GadgetDef * def = GetGadgetDef( gadget );

	return ProjectileStats {
		.min_damage = def->min_damage,
		.max_damage = def->damage,
		.min_knockback = def->min_knockback,
		.max_knockback = def->knockback,
		.speed = def->speed,
		.timeout = def->timeout,
		.gravity_scale = def->gravity_scale,
		.restitution = def->restitution,
		.splash_radius = def->splash_radius,
		.damage_type = gadget,
	};
}

static edict_t * GenEntity( edict_t * owner, Vec3 pos, EulerDegrees3 angles, int timeout ) {
	edict_t * ent = G_Spawn();
	ent->s.origin = pos;
	ent->olds.origin = pos;
	ent->s.angles = angles;

	ent->s.solidity = Solid_NotSolid;

	ent->r.owner = owner;
	ent->s.ownerNum = owner->s.number;
	ent->nextThink = level.time + timeout;
	ent->think = G_FreeEdict;
	ent->timeout = level.time + timeout;
	ent->timeStamp = level.time;
	ent->s.team = owner->s.team;

	return ent;
}

struct FireProjectileConfig {
	edict_t * owner;
	Vec3 start;
	EulerDegrees3 angles;
};

static edict_t * FireProjectile(
	edict_t * owner,
	Vec3 start, EulerDegrees3 angles,
	int timeDelta,
	ProjectileStats stats
) {
	edict_t * projectile = GenEntity( owner, start, angles, stats.timeout );

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );

	projectile->velocity = dir * stats.speed;

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;

	projectile->s.override_collision_model = CollisionModelAABB( MinMax3( Vec3( 0.0f ), Vec3( 0.0f ) ) );
	projectile->s.solidity = SolidMask_Shot;
	projectile->s.svflags &= ~SVF_NOCLIENT;
	projectile->gravity_scale = stats.gravity_scale;
	projectile->restitution = stats.restitution;

	projectile->timeDelta = timeDelta;

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
	Vec3 start, EulerDegrees3 angles,
	int timeDelta,
	ProjectileStats stats
) {
	edict_t * projectile = FireProjectile( owner, start, angles, timeDelta, stats );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;
	projectile->s.linearMovement = true;
	projectile->s.linearMovementBegin = projectile->s.origin;
	projectile->s.linearMovementVelocity = projectile->velocity;
	projectile->s.linearMovementTimeStamp = svs.gametime;
	projectile->s.linearMovementTimeDelta = Min2( Abs( timeDelta ), 255 );

	return projectile;
}

static void HitWithSpread( edict_t * self, Vec3 start, EulerDegrees3 angles, float range, float spread, int traces, float damage, float knockback, WeaponType weapon, int timeDelta ) {
	Vec3 forward;
	AngleVectors( angles, &forward, NULL, NULL );

	for( int i = 0; i < traces; i++ ) {
		EulerDegrees3 new_angles = angles;
		new_angles.yaw += Lerp( -spread, float( i ) / float( traces - 1 ), spread );
		Vec3 dir;
		AngleVectors( new_angles, &dir, NULL, NULL );
		Vec3 end = start + dir * range;

		trace_t trace = G_Trace4D( start, MinMax3( 0.0f ), end, self, SolidMask_Shot, timeDelta );
		if( trace.HitSomething() && game.edicts[ trace.ent ].takedamage ) {
			G_Damage( &game.edicts[ trace.ent ], self, self, forward, forward, trace.endpos, damage, knockback, 0, weapon );
			break;
		}
	}
}

static void HitOrStickToWall( edict_t * ent, edict_t * other, DamageType weapon, StringHash player_fx, StringHash wall_fx ) {
	if( other->takedamage ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, weapon );
		ent->enemy = other;

		SpawnFX( ent, -SafeNormalize( ent->velocity ), player_fx, player_fx );

		G_FreeEdict( ent );
	}
	else {
		ent->s.type = ET_GENERIC;
		ent->movetype = MOVETYPE_NONE;
		ent->s.sound = EMPTY_HASH;
		ent->avelocity = EulerDegrees3( 0.0f, 0.0f, 0.0f );
		ent->s.linearMovement = false;

		SpawnFX( ent, -SafeNormalize( ent->velocity ), wall_fx, wall_fx );

		ent->velocity = Vec3( 0.0f );
	}
}

static void W_Fire_Blade( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Knife );

	HitWithSpread( self, start, angles, def->range, def->spread, def->projectile_count, def->damage, def->knockback, Weapon_Knife, timeDelta );
}

static void W_Fire_Bat( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Bat );

	float charge = Min2( 1.0f, float( self->r.client->ps.weapon_state_time ) / float( def->reload_time ) );
	float damage = Lerp( float( def->min_damage ), charge, float( def->damage ) );
	float knockback = def->knockback * charge;

	HitWithSpread( self, start, angles, def->range, def->spread, def->projectile_count, damage, knockback, Weapon_Bat, timeDelta );
}

static void W_Fire_Bullet( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta, WeaponType weapon ) {
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
	if( trace.HitSomething() && game.edicts[ trace.ent ].takedamage ) {
		int dmgflags = DAMAGE_KNOCKBACK_SOFT;
		float damage = def->damage;

		if( IsHeadshot( trace.ent, trace.endpos, timeDelta ) ) {
			dmgflags |= DAMAGE_HEADSHOT;
		}

		if( wallbang.HitSomething() ) {
			dmgflags |= DAMAGE_WALLBANG;
			damage *= def->wallbang_damage_scale;
		}

		G_Damage( &game.edicts[ trace.ent ], self, self, dir, dir, trace.endpos, damage, def->knockback, dmgflags, weapon );
	}
}

static void W_Fire_Shotgun( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta, WeaponType weapon ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	float damage_dealt[ MAX_CLIENTS + 1 ] = { };
	Vec3 hit_locations[ MAX_CLIENTS + 1 ] = { }; // arbitrary trace end pos to use as blood origin

	for( int i = 0; i < def->projectile_count; i++ ) {
		Vec2 spread = FixedSpreadPattern( i, def->spread );

		trace_t trace, wallbang;
		GS_TraceBullet( &server_gs, &trace, &wallbang, start, dir, right, up, spread, def->range, ENTNUM( self ), timeDelta );
		if( trace.HitSomething() && game.edicts[ trace.ent ].takedamage ) {
			int dmgflags = trace.endpos == wallbang.endpos ? 0 : DAMAGE_WALLBANG;
			float damage = def->damage;

			if( trace.endpos != wallbang.endpos ) {
				dmgflags |= DAMAGE_WALLBANG;
				damage *= def->wallbang_damage_scale;
			}

			G_Damage( &game.edicts[ trace.ent ], self, self, dir, dir, trace.endpos, damage, def->knockback, dmgflags, weapon );

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

static void W_Grenade_Explode( edict_t * ent ) {
	Explode( ent, NULL, NONE );
}

static void W_Touch_Grenade( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_GrenadeLauncher );

	if( !CanHit( ent, other ) ) {
		return;
	}

	if( !other->takedamage ) {
		Vec3 parallel = Project( ent->velocity, normal );
		Vec3 perpendicular = ent->velocity - parallel;

		float friction = 0.1f;
		float velocity = Length( parallel ) + Length( perpendicular ) * friction;

		u16 volume = Lerp( u16( 0 ), Unlerp01( 0.0f, velocity, float( def->speed ) ), U16_MAX );
		G_AddEvent( ent, EV_GRENADE_BOUNCE, volume, true );

		return;
	}

	Explode( ent, other, normal );
}

static void W_Fire_Grenade( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta, bool altfire ) {
	edict_t * grenade = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_GrenadeLauncher ) );

	if( altfire ) {
		grenade->velocity *= 0.5;
	}

	grenade->s.type = ET_GRENADE;
	grenade->classname = "grenade";
	grenade->movetype = MOVETYPE_BOUNCEGRENADE;
	grenade->s.model = "weapons/gl/grenade";
	grenade->projectileInfo.explosion_vfx = "vfx/explosion";
	grenade->projectileInfo.explosion_sfx = "weapons/gl/explode";
	grenade->think = W_Grenade_Explode;
	grenade->touch = W_Touch_Grenade;
}

static void W_Touch_Stake( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	HitOrStickToWall( ent, other, Weapon_StakeGun, "weapons/stake/impale", "weapons/stake/hit" );
}

static void W_Fire_Stake( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * stake = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_StakeGun ) );

	stake->s.type = ET_STAKE;
	stake->classname = "stake";
	stake->movetype = MOVETYPE_BOUNCEGRENADE;
	stake->s.model = "weapons/stake/stake";
	stake->s.sound = "weapons/stake/trail";
	stake->touch = W_Touch_Stake;
}

static void W_Touch_Rocket( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	Explode( ent, other, normal );
}

static void W_Fire_Rocket( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta, bool altfire ) {
	edict_t * rocket;

	if( altfire ) {
		rocket = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_RocketLauncher ) );
		rocket->movetype = MOVETYPE_BOUNCE;
	}
	else {
		rocket = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_RocketLauncher ) );
	}

	rocket->s.type = ET_ROCKET;
	rocket->classname = "rocket";
	rocket->s.model = "weapons/rl/rocket";
	rocket->s.sound = "weapons/rl/trail";
	rocket->projectileInfo.explosion_vfx = "vfx/explosion";
	rocket->projectileInfo.explosion_sfx = "weapons/rl/explode";
	rocket->touch = W_Touch_Rocket;
}

static void W_Fire_ARBullet( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * arbullet = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_AssaultRifle ) );

	arbullet->s.type = ET_ARBULLET;
	arbullet->classname = "arbullet";
	arbullet->s.model = "weapons/ar/projectile";
	arbullet->s.sound = "weapons/ar/trail";
	arbullet->projectileInfo.explosion_vfx = "weapons/ar/explosion";
	arbullet->projectileInfo.explosion_sfx = "weapons/ar/explode";

	arbullet->touch = W_AutoTouch_ARBullet;
	arbullet->think = W_Think_ARBullet;
	arbullet->nextThink = level.time + 1;
}

static void FireBubble( edict_t * owner, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * bubble = FireLinearProjectile( owner, start, angles, timeDelta, WeaponProjectileStats( Weapon_BubbleGun ) );

	bubble->s.type = ET_BUBBLE;
	bubble->classname = "bubble";
	bubble->s.sound = "weapons/bg/trail";
	bubble->projectileInfo.explosion_vfx = "weapons/bg/explosion";
	bubble->projectileInfo.explosion_sfx = "weapons/bg/explode";

	bubble->touch = W_AutoTouch_ARBullet;
	bubble->think = W_Think_ARBullet;
	bubble->nextThink = level.time + 1;
}

void W_Fire_BubbleGun( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
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

		EulerDegrees3 new_angles = VecToAngles( new_dir );
		FireBubble( self, pos, new_angles, timeDelta );
	}
}

static void W_Fire_Railgun( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );
	Vec3 end = start + dir * def->range;
	Vec3 from = start;

	const edict_t * ignore = self;

	trace_t tr;
	tr.ent = -1;

	while( ignore != NULL ) {
		tr = G_Trace4D( from, MinMax3( 0.0f ), end, ignore, SolidMask_WallbangShot, timeDelta );

		from = tr.endpos;
		ignore = NULL;

		if( tr.HitNothing() ) {
			break;
		}

		// some entity was touched
		edict_t * hit = &game.edicts[ tr.ent ];
		int hit_movetype = hit->movetype; // backup the original movetype as the entity may "die"
		if( hit == world ) { // stop dead if hit the world
			break;
		}

		if( hit != self && hit->takedamage ) {
			int dmgflags = 0;
			if( IsHeadshot( tr.ent, tr.endpos, timeDelta ) ) {
				dmgflags |= DAMAGE_HEADSHOT;
			}

			G_Damage( hit, self, self, dir, dir, tr.endpos, def->damage, def->knockback, dmgflags, Weapon_Railgun );

			// spawn a impact event on each damaged ent
			edict_t * event = G_SpawnEvent( EV_BOLT_EXPLOSION, DirToU64( tr.normal ), &tr.endpos );
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
	edict_t * event = G_SpawnEvent( EV_RAIL_ALTFIRE, 0, &ent->s.origin );
	event->s.ownerNum = ent->s.ownerNum;
	event->s.angles = ent->s.angles;

	edict_t * owner = &game.edicts[ ent->s.ownerNum ];
	W_Fire_Railgun( owner, ent->s.origin, ent->s.angles, 0 );

	G_FreeEdict( ent );
}

static void W_Fire_RailgunAlt( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );
	edict_t * ent = GenEntity( self, start, angles, def->reload_time );
	ent->s.type = ET_RAILALT;
	ent->classname = "railgunalt";
	ent->think = RailgunAltDeploy;

	edict_t * event = G_SpawnEvent( EV_RAIL_ALTENT, 0, &ent->s.origin );
	event->s.ownerNum = ent->s.ownerNum;
	event->s.angles = ent->s.angles;
	event->s.team = self->s.team;
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

static void LaserImpact( const trace_t & trace, Vec3 dir, int damage, int knockback, edict_t * attacker ) {
	if( trace.ent == ENTNUM( attacker ) ) {
		return; // should not be possible theoretically but happened at least once in practice
	}

	G_Damage( &game.edicts[ trace.ent ], attacker, attacker, dir, dir, trace.endpos, damage, knockback, DAMAGE_KNOCKBACK_SOFT, Weapon_Laser );
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
	laser->s.solidity = Solid_NotSolid;
	laser->s.svflags &= ~SVF_NOCLIENT;
	return laser;
}

static void W_Fire_Lasergun( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Laser );

	edict_t * laser = FindOrSpawnLaser( self );

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );

	trace_t trace = GS_TraceLaserBeam( &server_gs, start, angles, def->range, ENTNUM( self ), timeDelta );
	if( trace.HitSomething() ) {
		LaserImpact( trace, dir, def->damage, def->knockback, self );
	}

	laser->s.svflags |= SVF_FORCEOWNER;

	laser->s.origin = start;
	laser->s.origin2 = laser->s.origin + dir * def->range;

	laser->think = G_Laser_Think;
	laser->nextThink = level.time + 1;
}

static void W_Touch_RifleBullet( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	SpawnFX( ent, normal, "vfx/bulletsparks", "weapons/bullet_impact" );

	if( other->takedamage && ent->enemy != other ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_Rifle );
		ent->enemy = other;
	}
	else {
		G_FreeEdict( ent );
	}
}

void W_Fire_RifleBullet( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * bullet = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_Rifle ) );

	bullet->s.type = ET_RIFLEBULLET;
	bullet->classname = "riflebullet";
	bullet->s.model = "weapons/rifle/bullet";
	bullet->s.sound = "weapons/bullet_whizz";
	bullet->s.solidity = SolidMask_WallbangShot;
	bullet->touch = W_Touch_RifleBullet;
}

static void StickyExplodeNormal( edict_t * ent, Vec3 normal, bool silent ) {
	G_RadiusDamage( ent, ent->r.owner, normal, ent->enemy, Weapon_StickyGun );

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

static void W_Touch_Sticky( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		Vec3 push_dir;
		G_SplashFrac4D( other, ent->s.origin, ent->projectileInfo.radius, &push_dir, NULL, ent->timeDelta, false );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Weapon_StickyGun );
		ent->enemy = other;

		StickyExplodeNormal( ent, normal, false );
	}
	else {
		const WeaponDef * def = GS_GetWeaponDef( Weapon_StickyGun );
		ent->s.linearMovementBegin = ent->s.origin;
		ent->s.linearMovementVelocity = Vec3( 0.0f );
		ent->avelocity = EulerDegrees3( 0.0f, 0.0f, 0.0f );
		ent->nextThink = level.time + def->spread; //gg

		SpawnFX( ent, normal, "weapons/sticky/impact", "weapons/sticky/impact" );
	}
}

void W_Fire_Sticky( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_StickyGun );

	Vec2 spread = UniformSampleInsideCircle( &svs.rng ) * def->zoom_spread;
	angles.pitch += spread.x;
	angles.yaw += spread.y;

	edict_t * bullet = FireLinearProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_StickyGun ) );

	bullet->s.type = ET_ROCKET; // TODO: need a new entity type here
	bullet->classname = "sticky";
	bullet->s.model = "weapons/sticky/bullet";
	bullet->s.sound = "weapons/sticky/fuse";
	bullet->avelocity = EulerDegrees3( UniformSampleInsideSphere( &svs.rng ) * 1800.0f );
	bullet->touch = W_Touch_Sticky;
	bullet->think = StickyExplode;
}

static bool BouncingProjectile( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask, u32 max_bounces, StringHash hit_fx, StringHash bounce_fx ) {
	if( !CanHit( ent, other ) ) {
		return false;
	}

	if( other->takedamage ) {
		SpawnFX( ent, normal, hit_fx, hit_fx );
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, ent->projectileInfo.damage_type );
		G_FreeEdict( ent );
		return false;
	}

	SpawnFX( ent, normal, bounce_fx, bounce_fx );

	return ent->num_bounces >= max_bounces;
}

static void W_Touch_Blast( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( BouncingProjectile( ent, other, normal, solid_mask, 5, "weapons/mb/hit", "weapons/mb/bounce" ) ) {
		G_FreeEdict( ent );
	}
}

void W_Fire_Blast( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_MasterBlaster );
	Vec3 dir, right, up;
	AngleVectors( angles, &dir, &right, &up );

	for( int i = 0; i < def->projectile_count; i++ ) {
		Vec2 spread = FixedSpreadPattern( i, def->spread );

		Vec3 blast_dir = dir * def->range + right * spread.x + up * spread.y;
		EulerDegrees3 blast_angles = VecToAngles( blast_dir );

		edict_t * blast = FireProjectile( self, start, blast_angles, timeDelta, WeaponProjectileStats( Weapon_MasterBlaster ) );

		blast->s.type = ET_BLAST;
		blast->classname = "blast";
		blast->movetype = MOVETYPE_BOUNCE;
		blast->s.sound = "weapons/mb/trail";
		blast->touch = W_Touch_Blast;
		blast->stop = G_FreeEdict;
	}
}

static void W_Touch_Pistol( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( BouncingProjectile( ent, other, normal, solid_mask, 3, "weapons/pistol/bullet_impact", "weapons/pistol/bullet_impact" ) ) {
		G_FreeEdict( ent );
	} else {
		ent->gravity_scale = GS_GetWeaponDef( Weapon_Pistol )->gravity_scale;
	}
}

void W_Fire_Pistol( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * bullet = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_Pistol ) );

	bullet->s.type = ET_PISTOLBULLET;
	bullet->classname = "pistol_bullet";
	bullet->s.model = "weapons/pistol/bullet";
	bullet->movetype = MOVETYPE_BOUNCE;
	bullet->s.sound = "weapons/bullet_whizz";
	bullet->touch = W_Touch_Pistol;
	bullet->stop = G_FreeEdict;
	bullet->gravity_scale = 0.0;
}

static void W_Touch_Sawblade( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( BouncingProjectile( ent, other, normal, solid_mask, 7, "weapons/sawblade/blade_impact", "weapons/sawblade/bounce" ) ) {
		HitOrStickToWall( ent, other, Weapon_Sawblade, "weapons/sawblade/blade_impact", "weapons/sawblade/blade_impact" );
	}
}

void W_Fire_Sawblade( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * blade = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_Sawblade ) );

	blade->s.type = ET_SAWBLADE;
	blade->classname = "sawblade";
	blade->s.model = "weapons/sawblade/bullet";
	blade->movetype = MOVETYPE_BOUNCE;
	blade->s.sound = "weapons/sawblade/trail";
	blade->avelocity = EulerDegrees3( 0.0f, -360.0f, 0.0 );
	blade->touch = W_Touch_Sawblade;
	blade->stop = G_FreeEdict;
}

void W_Fire_Road( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * bullet = FireProjectile( self, start, angles, timeDelta, WeaponProjectileStats( Weapon_RoadGun ) );

	bullet->s.type = ET_BLAST;
	bullet->classname = "zorg";
	bullet->movetype = MOVETYPE_BOUNCE;
	bullet->s.sound = "weapons/road/trail";
	bullet->touch = W_Touch_Blast;
	bullet->stop = G_FreeEdict;
}

static void CallFireWeapon( edict_t * ent, u64 parm, bool alt ) {
	WeaponType weapon = WeaponType( parm & 0xFF );

	Vec3 origin;
	EulerDegrees3 angles;
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

		case Weapon_9mm:
		case Weapon_MachineGun:
		case Weapon_Deagle:
		case Weapon_BurstRifle:
		case Weapon_Sniper:
		case Weapon_AutoSniper:
			W_Fire_Bullet( ent, origin, angles, timeDelta, weapon );
			break;

		case Weapon_Pistol:
			W_Fire_Pistol( ent, origin, angles, timeDelta );
			break;

		case Weapon_Shotgun:
		case Weapon_DoubleBarrel:
			W_Fire_Shotgun( ent, origin, angles, timeDelta, weapon );
			break;

		case Weapon_StakeGun:
			W_Fire_Stake( ent, origin, angles, timeDelta );
			break;

		case Weapon_GrenadeLauncher:
			W_Fire_Grenade( ent, origin, angles, timeDelta, alt );
			break;

		case Weapon_RocketLauncher:
			W_Fire_Rocket( ent, origin, angles, timeDelta, alt );
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

		case Weapon_Sawblade:
			W_Fire_Sawblade( ent, origin, angles, timeDelta );
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

static void TouchThrowingAxe( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	HitOrStickToWall( ent, other, Gadget_ThrowingAxe, "gadgets/hatchet/hit", "gadgets/hatchet/impact" );
}

static void UseThrowingAxe( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta, u64 charge_time ) {
	const GadgetDef * def = GetGadgetDef( Gadget_ThrowingAxe );

	ProjectileStats stats = GadgetProjectileStats( Gadget_ThrowingAxe );
	float cook_frac = Unlerp01( u64( 0 ), charge_time, u64( def->cook_time ) );
	stats.max_damage = Lerp( def->min_damage, cook_frac, def->damage );
	stats.speed = Lerp( def->min_speed, cook_frac, def->speed );

	edict_t * axe = FireProjectile( self, start, angles, timeDelta, stats );
	axe->s.type = ET_THROWING_AXE;
	axe->classname = "throwing axe";
	axe->movetype = MOVETYPE_BOUNCE;
	axe->s.model = "gadgets/hatchet/model";
	axe->s.sound = "gadgets/hatchet/trail";
	axe->avelocity = EulerDegrees3( 360.0f * 4.0f, 0.0f, 0.0f );
	axe->touch = TouchThrowingAxe;
}

static void ExplodeStunGrenade( edict_t * grenade ) {
	const GadgetDef * def = GetGadgetDef( Gadget_StunGrenade );
	static constexpr float flash_distance = 2000.0f;

	SpawnFX( grenade, NONE, "gadgets/flash/explode", "gadgets/flash/explode" );

	int touch[ MAX_EDICTS ];
	int numtouch = GClip_FindInRadius4D( grenade->s.origin, def->splash_radius + playerbox_stand_viewheight, touch, ARRAY_COUNT( touch ), grenade->timeDelta );

	for( int i = 0; i < numtouch; i++ ) {
		edict_t * other = &game.edicts[ touch[ i ] ];
		if( other->s.type != ET_PLAYER )
			 continue;

		SyncPlayerState * ps = &other->r.client->ps;
		Vec3 eye = other->s.origin + Vec3( 0.0f, 0.0f, ps->viewheight );

		trace_t grenade_to_eye = G_Trace4D( grenade->s.origin, MinMax3( 0.0f ), eye, grenade, SolidMask_AnySolid, grenade->timeDelta );
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
			}
		}
	}

	G_RadiusKnockback( def->knockback, def->min_knockback, def->splash_radius, grenade, grenade->s.origin, NONE, 0 );

	G_FreeEdict( grenade );
}

static void TouchStunGrenade( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	if( other->takedamage ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, Gadget_StunGrenade );
		ExplodeStunGrenade( ent );
	}
}

static void UseStunGrenade( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta, u64 charge_time, bool dead ) {
	const GadgetDef * def = GetGadgetDef( Gadget_StunGrenade );

	ProjectileStats stats = GadgetProjectileStats( Gadget_StunGrenade );

	if( dead ) {
		stats.speed = 1;
	} else {
		stats.speed = Lerp( def->min_speed, Unlerp01( u64( 0 ), charge_time, u64( def->cook_time ) ), def->speed );
	}

	edict_t * grenade = FireProjectile( self, start, angles, timeDelta, stats );
	grenade->s.type = ET_STUNGRENADE;
	grenade->classname = "stun grenade";
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->s.model = "gadgets/flash/model";
	grenade->avelocity = EulerDegrees3( 360.0f, 0.0f, 0.0f );
	grenade->touch = TouchStunGrenade;
	grenade->think = ExplodeStunGrenade;
}

static void TouchShuriken( edict_t * ent, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( !CanHit( ent, other ) ) {
		return;
	}

	HitOrStickToWall( ent, other, Gadget_Shuriken, "gadgets/shuriken/hit", "gadgets/hatchet/impact" );
}

static void UseRocket( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * rocket = FireProjectile( self, start, angles, timeDelta, GadgetProjectileStats( Gadget_Rocket ) );

	rocket->s.type = ET_ROCKET;
	rocket->movetype = MOVETYPE_BOUNCE;
	rocket->classname = "rocket";
	rocket->s.model = "gadgets/rocket/model";
	rocket->s.sound = "gadgets/rocket/trail";
	rocket->projectileInfo.explosion_vfx = "gadgets/rocket/explode";
	rocket->projectileInfo.explosion_sfx = "gadgets/rocket/explode";
	rocket->touch = W_Touch_Rocket;
}

static void UseShuriken( edict_t * self, Vec3 start, EulerDegrees3 angles, int timeDelta ) {
	edict_t * shuriken = FireLinearProjectile( self, start, angles, timeDelta, GadgetProjectileStats( Gadget_Shuriken ) );
	shuriken->s.type = ET_SHURIKEN;
	shuriken->classname = "shuriken";
	shuriken->s.model = "gadgets/shuriken/model";
	shuriken->s.sound = "gadgets/shuriken/trail";
	shuriken->avelocity = EulerDegrees3( 0.0f, 360.0f, 0.0f );
	shuriken->touch = TouchShuriken;
}

void G_UseGadget( edict_t * ent, GadgetType gadget, u64 parm, bool dead ) {
	if( !GetGadgetDef( gadget )->drop_on_death && dead )
		return;

	Vec3 origin = ent->s.origin;
	origin.z += ent->r.client->ps.viewheight;

	EulerDegrees3 angles = ent->r.client->ps.viewangles;

	int timeDelta = ent->r.client->timeDelta;

	switch( gadget ) {
		case Gadget_ThrowingAxe:
			UseThrowingAxe( ent, origin, angles, timeDelta, parm );
			break;

		case Gadget_StunGrenade:
			UseStunGrenade( ent, origin, angles, timeDelta, parm, dead );
			break;

		case Gadget_Rocket:
			UseRocket( ent, origin, angles, timeDelta );
			break;

		case Gadget_Shuriken:
			UseShuriken( ent, origin, angles, timeDelta );
			break;
	}
}
