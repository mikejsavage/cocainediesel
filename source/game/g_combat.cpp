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

bool G_IsTeamDamage( const SyncEntityState * target, const SyncEntityState * attacker ) {
	return target->number != attacker->number && target->team == attacker->team;
}

static bool G_CanSplashDamage( const edict_t * targ, const edict_t * inflictor, Optional< Vec3 > normal, Vec3 pos, int timeDelta ) {
	constexpr float SPLASH_DAMAGE_TRACE_FRAC_EPSILON = 1.0f / 32.0f;

	// bmodels need special checking because their origin is 0,0,0
	if( targ->movetype == MOVETYPE_PUSH ) {
		// NOT FOR PLAYERS only for entities that can push the players
		MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &targ->s );
		Vec3 dest = targ->s.origin + Center( bounds );
		trace_t trace = G_Trace4D( pos, MinMax3( 0.0f ), dest, inflictor, Solid_WeaponClip, timeDelta );
		if( trace.fraction >= 1.0f - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
			return true;
		}

		return false;
	}

	// up by 9 units to account for stairs
	Vec3 origin = pos + Default( normal, Vec3( 0.0f ) ) * 9.0f;

	constexpr Vec3 offsets[] = {
		Vec3( 0.0f, 0.0f, 0.0f ),
		Vec3( 15.0f, 15.0f, 0.0f ),
		Vec3( -15.0f, 15.0f, 0.0f ),
		Vec3( 15.0f, -15.0f, 0.0f ),
		Vec3( -15.0f, -15.0f, 0.0f ),
	};

	for( Vec3 offset : offsets ) {
		trace_t trace = G_Trace4D( origin, MinMax3( 0.0f ), targ->s.origin + offset, inflictor, Solid_WeaponClip, timeDelta );
		if( trace.fraction >= 1.0f - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
			return true;
		}
	}

	return false;
}

void G_Killed( edict_t * targ, edict_t * inflictor, edict_t * attacker, int assistorNo, DamageType damage_type, int damage ) {
	if( targ->health < -999 ) {
		targ->health = -999;
	}

	if( targ->deadflag == DEAD_DEAD ) {
		return;
	}

	targ->deadflag = DEAD_DEAD;
	targ->enemy = attacker;

	if( targ->r.client && attacker && attacker->r.client && targ != attacker ) {
		attacker->r.client->snap.kill = true;
	}

	// count stats
	if( server_gs.gameState.match_state == MatchState_Playing ) {
		G_ClientGetStats( targ )->deaths++;

		if( !attacker || !attacker->r.client || attacker == targ || attacker == world ) {
			G_ClientGetStats( targ )->suicides++;
		} else {
			G_ClientGetStats( attacker )->kills++;
		}
	}

	GT_CallPlayerKilled( targ, attacker, inflictor );

	G_CallDie( targ, inflictor, attacker, assistorNo, damage_type, damage );
}

static void G_AddAssistDamage( edict_t * targ, edict_t * attacker, int amount ) {
	if( attacker == world || attacker == targ ) {
		return;
	}

	int attacker_entno = attacker->s.number;
	assistinfo_t * assist = NULL;

	for( size_t i = 0; i < ARRAY_COUNT( targ->recent_attackers ); i++ ) {
		// check for recent attacker or free slot first
		if( targ->recent_attackers[i].entno == attacker_entno || !targ->recent_attackers[i].entno) {
			assist = &targ->recent_attackers[i];
			break;
		}
	}

	if( assist == NULL ) {
		// no free slots, replace oldest attacker seeya pal
		for( size_t i = 0; i < ARRAY_COUNT( targ->recent_attackers ); i++ ) {
			if( assist == NULL || targ->recent_attackers[i].lastTime < assist->lastTime ) {
				assist = &targ->recent_attackers[i];
			}
		}

		assist->cumDamage = 0; // we're taking over this slot
	}

	assist->lastTime = svs.gametime;
	assist->entno = attacker_entno; // incase old re-write/empty
	assist->cumDamage += amount;
}

static int G_FindTopAssistor( edict_t * victim, edict_t * attacker ) {
	const assistinfo_t * top = NULL;

	// TODO: could weigh damage by most recent timestamp as well
	for( size_t i = 0; i < ARRAY_COUNT( victim->recent_attackers ); i++ ) {
		if(victim->recent_attackers[i].entno && (top == NULL || victim->recent_attackers[i].cumDamage > top->cumDamage)) {
			top = &victim->recent_attackers[i];
		}
	}

	// dont return last-hit killer as assistor as well. if they did a tiny amount of dmg ignore
	return top != NULL && top->entno != attacker->s.number && top->cumDamage > 9 ? top->entno : -1;
}

static void G_KnockBackPush( edict_t * targ, const edict_t * attacker, Vec3 basedir, int knockback, int dflags ) {
	if( knockback < 1 ) {
		return;
	}

	if( targ->movetype == MOVETYPE_NONE ||
		targ->movetype == MOVETYPE_PUSH ||
		targ->movetype == MOVETYPE_STOP ||
		targ->movetype == MOVETYPE_BOUNCE ) {
		return;
	}

	constexpr float MIN_KNOCKBACK_SPEED = 2.5f;
	float push = 1000.0f * float( knockback ) / float( Max2( targ->mass, 1 ) );
	if( push < MIN_KNOCKBACK_SPEED ) {
		return;
	}

	Vec3 dir = SafeNormalize( basedir );
	constexpr float VERTICAL_KNOCKBACK_SCALE = 1.25f;
	dir.z *= VERTICAL_KNOCKBACK_SCALE;

	if( targ->r.client && targ != attacker && !( dflags & DAMAGE_KNOCKBACK_SOFT ) ) {
		targ->r.client->ps.pmove.knockback_time = Clamp( 100, 3 * knockback, 250 );
	}

	targ->velocity = targ->velocity + dir * push;
	knockbackOfDeath = dir * push;
}

void SpawnDamageEvents( const edict_t * attacker, edict_t * victim, float damage, bool headshot, Vec3 pos, Vec3 dir, bool showNumbers ) {
	constexpr StringHash headshot_sound = "sounds/headshot/headshot";

	u64 parm = HEALTH_TO_INT( damage ) << 1;
	if( headshot ) {
		parm |= 1;
		G_SpawnEvent( EV_SOUND_ORIGIN, headshot_sound.hash, &victim->s.origin );
	}

	if( showNumbers ) {
		edict_t * damage_number = G_SpawnEvent( EV_DAMAGE, parm, &victim->s.origin );
		damage_number->s.svflags |= SVF_OWNERANDCHASERS;
		damage_number->s.ownerNum = ENTNUM( attacker );
	}

	edict_t * blood = G_SpawnEvent( EV_BLOOD, HEALTH_TO_INT( damage ), &pos );
	blood->s.origin2 = dir;
	blood->s.team = victim->s.team;

	s16 client_maxhp = game.clients[ PLAYERNUM( victim ) ].ps.max_health;
	if( !G_IsDead( victim ) && level.time >= victim->pain_debounce_time && client_maxhp > 0 ) { //2nd check will prevent the game from crashing on some loss
		G_AddEvent( victim, EV_PAIN, Min2( 3, int( 4 * victim->health / client_maxhp ) ), true );
		victim->pain_debounce_time = level.time + 400;
	}
}

void G_Damage( edict_t * targ, edict_t * inflictor, edict_t * attacker, Vec3 pushdir, Vec3 dmgdir, Vec3 point, float damage, float knockback, int dflags, DamageType damage_type ) {
	gclient_t *client;

	if( !targ || !targ->takedamage ) {
		return;
	}

	if( !attacker ) {
		attacker = world;
		damage_type = WorldDamage_Trigger;
	}

	damageFlagsOfDeath = dflags;

	client = targ->r.client;

	if( G_IsTeamDamage( &targ->s, &attacker->s ) ) {
		return;
	}

	WeaponType weapon;
	DamageCategory damage_category = DecodeDamageType( damage_type, &weapon, NULL, NULL );

	// dont count self-damage cause it just adds the same to both stats
	bool statDmg = attacker != targ && damage_type != WorldDamage_Telefrag && damage_type != WorldDamage_Explosion;

	// push
	G_KnockBackPush( targ, attacker, pushdir, knockback, dflags );

	float take = damage;
	if( attacker == targ ) {
		if( level.gametype.selfDamage && damage_category == DamageCategory_Weapon ) {
			take = damage * GS_GetWeaponDef( weapon )->self_damage_scale;
		}
		else {
			take = 0;
		}
	}

	// do the damage
	if( take <= 0 ) {
		return;
	}

	if( statDmg ) {
		G_ClientGetStats( targ )->total_damage_received += take;
	}

	// accumulate received damage for snapshot effects
	{
		Vec3 dorigin;

		if( point != Vec3( 0.0f ) ) {
			dorigin = point;
		} else {
			dorigin = targ->s.origin;
			dorigin.z += targ->viewheight;
		}

		if( targ->r.client && damage_type != WorldDamage_Telefrag && damage_type != WorldDamage_Suicide ) {
			if( inflictor == world || attacker == world ) {
				// for world inflicted damage use always 'frontal'
				G_ClientAddDamageIndicatorImpact( targ->r.client, take, Vec3( 0.0f ) );
			} else if( dflags & DAMAGE_RADIUS ) {
				// for splash hits the direction is from the inflictor origin
				G_ClientAddDamageIndicatorImpact( targ->r.client, take, pushdir );
			} else {   // for direct hits the direction is the projectile direction
				G_ClientAddDamageIndicatorImpact( targ->r.client, take, dmgdir );
			}
		}
	}

	targ->health = targ->health - take;

	// adding damage given/received to stats
	if( attacker->r.client && !targ->deadflag && targ->movetype != MOVETYPE_PUSH && targ->s.type != ET_CORPSE ) {
		if( statDmg ) {
			G_ClientGetStats( attacker )->total_damage_given += take;
		}

		// shotgun calls G_Damage for every bullet, so we accumulate damage
		// in W_Fire_Shotgun and send events from there instead
		if( damage_type != Weapon_Shotgun && damage_type != Weapon_DoubleBarrel ) {
			bool headshot = dflags & DAMAGE_HEADSHOT;
			SpawnDamageEvents( attacker, targ, take, headshot, point, dmgdir, statDmg );
		}
	}

	int clamped_takedmg = HEALTH_TO_INT( take );

	// accumulate given damage for hit sounds
	if( targ != attacker && client && !targ->deadflag && attacker && attacker->r.client ) {
		attacker->r.client->snap.damage_given += take;
	}

	if( G_IsDead( targ ) ) {
		if( targ->s.type != ET_CORPSE && attacker != targ ) {
			edict_t * killed = G_SpawnEvent( EV_DAMAGE, 255 << 1, &targ->s.origin );
			killed->s.svflags |= SVF_OWNERANDCHASERS;
			killed->s.ownerNum = ENTNUM( attacker );
		}

		int topAssistorNo = G_FindTopAssistor( targ, attacker );
		G_Killed( targ, inflictor, attacker, topAssistorNo, damage_type, clamped_takedmg );
	} else {
		SyncPlayerState * s = &game.edicts[ ENTNUM( targ ) ].r.client->ps;
		s->last_touch.entnum = ENTNUM( attacker );
		s->last_touch.type = damage_type;
		G_AddAssistDamage( targ, attacker, clamped_takedmg );
		G_CallPain( targ, attacker, knockback, take );
	}
}

void G_SplashFrac( const SyncEntityState * s, const entity_shared_t * r, Vec3 point, float maxradius, Vec3 * pushdir, float * frac, bool selfdamage ) {
	const Vec3 & origin = s->origin;
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), s );

	float innerradius = ( bounds.maxs.x + bounds.maxs.y - bounds.mins.x - bounds.mins.y ) * 0.25f;

	// Find the distance to the closest point in the capsule contained in the player bbox
	// modify the origin so the inner sphere acts as a capsule
	Vec3 closest_point = origin;
	closest_point.z = Clamp( ( origin.z + bounds.mins.z ) + innerradius, point.z, ( origin.z + bounds.maxs.z ) - innerradius );

	// find push intensity
	float distance = Length( point - closest_point );

	if( distance >= maxradius ) {
		if( frac ) {
			*frac = 0;
		}
		*pushdir = Vec3( 0.0f );
		return;
	}

	float refdistance = innerradius;

	maxradius -= refdistance;
	distance = Max2( distance - refdistance, 0.0f );

	if( frac != NULL ) {
		// soft sin curve
		float distance_frac = ( maxradius - distance ) / maxradius;
		*frac = Clamp01( sinf( Radians( distance_frac * 80 ) ) );
	}

	// find push direction
	Vec3 center_of_mass;
	if( selfdamage ) {
		center_of_mass = origin + Vec3( 0.0f, 0.0f, r->client->ps.viewheight );
	}
	else {
		// find real center of the box again
		center_of_mass = origin + Center( bounds );
	}

	*pushdir = SafeNormalize( center_of_mass - point );
}

void G_RadiusKnockback( float maxknockback, float minknockback, float radius, edict_t * attacker, Vec3 pos, Optional< Vec3 > normal, int timeDelta ) {
	Assert( radius >= 0.0f );
	Assert( minknockback >= 0.0f && maxknockback >= 0.0f );

	int touch[MAX_EDICTS];
	int numtouch = GClip_FindInRadius4D( pos, radius, touch, MAX_EDICTS, timeDelta );

	for( int i = 0; i < numtouch; i++ ) {
		edict_t * ent = game.edicts + touch[i];
		if( !ent->takedamage || G_IsTeamDamage( &ent->s, &attacker->s ) )
			continue;

		float frac;
		Vec3 pushDir;
		G_SplashFrac4D( ent, pos, radius, &pushDir, &frac, timeDelta, false );
		if( frac == 0.0f )
			continue;

		if( G_CanSplashDamage( ent, NULL, normal, pos, timeDelta ) ) {
			float knockback = Lerp( minknockback, frac, maxknockback );
			G_KnockBackPush( ent, attacker, pushDir, knockback, 0 );
		}
	}
}

void G_RadiusDamage( edict_t * inflictor, edict_t * attacker, Optional< Vec3 > normal, edict_t * ignore, DamageType damage_type ) {
	Assert( inflictor );

	float maxdamage = inflictor->projectileInfo.maxDamage;
	float mindamage = inflictor->projectileInfo.minDamage;
	float maxknockback = inflictor->projectileInfo.maxKnockback;
	float minknockback = inflictor->projectileInfo.minKnockback;
	float radius = inflictor->projectileInfo.radius;

	Assert( radius >= 0.0f );
	Assert( mindamage >= 0.0f && minknockback >= 0.0f && mindamage <= maxdamage );
	Assert( maxdamage >= 0.0f && maxknockback >= 0.0f && mindamage <= maxdamage );

	int touch[MAX_EDICTS];
	int numtouch = GClip_FindInRadius4D( inflictor->s.origin, radius, touch, MAX_EDICTS, inflictor->timeDelta );

	for( int i = 0; i < numtouch; i++ ) {
		edict_t * ent = game.edicts + touch[i];
		if( ent == ignore || !ent->takedamage )
			continue;

		int timeDelta;
		if( ent == attacker && ent->r.client ) {
			timeDelta = 0;
		} else {
			timeDelta = inflictor->timeDelta;
		}

		float frac;
		Vec3 pushDir;
		bool is_selfdamage = inflictor->r.client != NULL && attacker == inflictor;
		G_SplashFrac4D( ent, inflictor->s.origin, radius, &pushDir, &frac, timeDelta, is_selfdamage );
		if( frac == 0.0f )
			continue;

		if( G_CanSplashDamage( ent, inflictor, normal, inflictor->s.origin, inflictor->timeDelta ) ) {
			float damage = Lerp( mindamage, frac, maxdamage );
			float knockback = Lerp( minknockback, frac, maxknockback );
			G_Damage( ent, inflictor, attacker, pushDir, inflictor->velocity, inflictor->s.origin, damage, knockback, DAMAGE_RADIUS, damage_type );
		}
	}
}
