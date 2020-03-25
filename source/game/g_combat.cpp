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

bool G_IsTeamDamage( SyncEntityState *targ, SyncEntityState *attacker ) {
	if( !GS_TeamBasedGametype( &server_gs ) )
		return false;
	return targ->number != attacker->number && targ->team == attacker->team;
}

/*
* G_CanSplashDamage
*/
#define SPLASH_DAMAGE_TRACE_FRAC_EPSILON 1.0 / 32.0f
static bool G_CanSplashDamage( edict_t *targ, edict_t *inflictor, cplane_t *plane ) {
	vec3_t dest, origin;
	trace_t trace;
	int solidmask = MASK_SOLID;

	if( !targ ) {
		return false;
	}

	if( !plane ) {
		VectorCopy( inflictor->s.origin, origin );
	}

	// bmodels need special checking because their origin is 0,0,0
	if( targ->movetype == MOVETYPE_PUSH ) {
		// NOT FOR PLAYERS only for entities that can push the players
		VectorAdd( targ->r.absmin, targ->r.absmax, dest );
		VectorScale( dest, 0.5, dest );
		G_Trace4D( &trace, origin, vec3_origin, vec3_origin, dest, inflictor, solidmask, inflictor->timeDelta );
		if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
			return true;
		}

		return false;
	}

	if( plane ) {
		// up by 9 units to account for stairs
		VectorMA( inflictor->s.origin, 9, plane->normal, origin );
	}

	// This is for players
	G_Trace4D( &trace, origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, solidmask, inflictor->timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	VectorCopy( targ->s.origin, dest );
	dest[0] += 15.0;
	dest[1] += 15.0;
	G_Trace4D( &trace, origin, vec3_origin, vec3_origin, dest, inflictor, solidmask, inflictor->timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	VectorCopy( targ->s.origin, dest );
	dest[0] += 15.0;
	dest[1] -= 15.0;
	G_Trace4D( &trace, origin, vec3_origin, vec3_origin, dest, inflictor, solidmask, inflictor->timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	VectorCopy( targ->s.origin, dest );
	dest[0] -= 15.0;
	dest[1] += 15.0;
	G_Trace4D( &trace, origin, vec3_origin, vec3_origin, dest, inflictor, solidmask, inflictor->timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	VectorCopy( targ->s.origin, dest );
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	G_Trace4D( &trace, origin, vec3_origin, vec3_origin, dest, inflictor, solidmask, inflictor->timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	return false;
}

/*
* G_Killed
*/
void G_Killed( edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point, int mod ) {
	if( targ->health < -999 ) {
		targ->health = -999;
	}

	if( targ->deadflag == DEAD_DEAD ) {
		return;
	}

	targ->deadflag = DEAD_DEAD;
	targ->enemy = attacker;

	if( targ->r.client && attacker && targ != attacker ) {
		attacker->snap.kill = true;
	}

	// count stats
	if( GS_MatchState( &server_gs ) == MATCH_STATE_PLAYTIME ) {
		targ->r.client->level.stats.deaths++;

		if( !attacker || !attacker->r.client || attacker == targ || attacker == world ) {
			targ->r.client->level.stats.suicides++;
		} else {
			attacker->r.client->level.stats.frags++;
		}
	}

	G_Gametype_ScoreEvent( attacker ? attacker->r.client : NULL, "kill", va( "%i %i %i %i", targ->s.number, ( inflictor == world ) ? -1 : ENTNUM( inflictor ), ENTNUM( attacker ), mod ) );

	G_CallDie( targ, inflictor, attacker, damage, point );
}

/*
* G_BlendFrameDamage
*/
static void G_BlendFrameDamage( edict_t *ent, float damage, float *old_damage, const vec3_t point, const vec3_t basedir, vec3_t old_point, vec3_t old_dir ) {
	vec3_t offset;
	float frac;
	vec3_t dir;
	int i;

	if( !point ) {
		VectorSet( offset, 0, 0, ent->viewheight );
	} else {
		VectorSubtract( point, ent->s.origin, offset );
	}

	VectorNormalize2( basedir, dir );

	if( *old_damage == 0 ) {
		VectorCopy( offset, old_point );
		VectorCopy( dir, old_dir );
		*old_damage = damage;
		return;
	}

	frac = damage / ( damage + *old_damage );
	for( i = 0; i < 3; i++ ) {
		old_point[i] = ( old_point[i] * ( 1.0f - frac ) ) + offset[i] * frac;
		old_dir[i] = ( old_dir[i] * ( 1.0f - frac ) ) + dir[i] * frac;
	}
	*old_damage += damage;
}

#define MIN_KNOCKBACK_SPEED 2.5

/*
* G_KnockBackPush
*/
static void G_KnockBackPush( edict_t *targ, edict_t *attacker, const vec3_t basedir, int knockback, int dflags ) {
	if( targ->flags & FL_NO_KNOCKBACK ) {
		return;
	}

	knockback *= g_knockback_scale->value;

	if( knockback < 1 ) {
		return;
	}

	if( targ->movetype == MOVETYPE_NONE ||
		targ->movetype == MOVETYPE_PUSH ||
		targ->movetype == MOVETYPE_STOP ||
		targ->movetype == MOVETYPE_BOUNCE ) {
		return;
	}

	int mass = Max2( targ->mass, 75 );
	float push = 1000.0f * float( knockback ) / float( mass );
	if( push < MIN_KNOCKBACK_SPEED ) {
		return;
	}

	vec3_t dir;
	VectorNormalize2( basedir, dir );
	constexpr float VERTICAL_KNOCKBACK_SCALE = 1.25f;
	dir[ 2 ] *= VERTICAL_KNOCKBACK_SCALE;

	if( targ->r.client && targ != attacker && !( dflags & DAMAGE_KNOCKBACK_SOFT ) ) {
		targ->r.client->ps.pmove.knockback_time = Clamp( 100, 3 * knockback, 250 );
	}

	VectorMA( targ->velocity, push, dir, targ->velocity );
	VectorScale( dir, push, knockbackOfDeath );
}

/*
* G_Damage
* targ		entity that is being damaged
* inflictor	entity that is causing the damage
* attacker	entity that caused the inflictor to damage targ
* example: targ=enemy, inflictor=rocket, attacker=player
*
* dir			direction of the attack
* point		point at which the damage is being inflicted
* normal		normal vector from that point
* damage		amount of damage being inflicted
* knockback	force to be applied against targ as a result of the damage
*
* dflags		these flags are used to control how T_Damage works
*/
void G_Damage( edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t pushdir, const vec3_t dmgdir, const vec3_t point, float damage, float knockback, int dflags, int mod ) {
	gclient_t *client;

	if( !targ || !targ->takedamage ) {
		return;
	}

	if( !attacker ) {
		attacker = world;
		mod = MOD_TRIGGER_HURT;
	}

	meansOfDeath = mod;

	client = targ->r.client;

	// Cgg - race mode: players don't interact with one another
	if( GS_RaceGametype( &server_gs ) && attacker->r.client && targ->r.client && attacker != targ ) {
		return;
	}

	if( G_IsTeamDamage( &targ->s, &attacker->s ) ) {
		return;
	}

	// dont count self-damage cause it just adds the same to both stats
	bool statDmg = ( attacker != targ ) && ( mod != MOD_TELEFRAG );

	// push
	G_KnockBackPush( targ, attacker, pushdir, knockback, dflags );

	// apply handicap on the damage given
	if( statDmg && attacker->r.client ) {
		// handicap is a percentage value
		if( attacker->r.client->handicap != 0 ) {
			damage *= 1.0 - ( attacker->r.client->handicap * 0.01f );
		}
	}

	float take = damage;
	float save = 0;

	// check for cases where damage is protected
	if( !( dflags & DAMAGE_NO_PROTECTION ) ) {
		// check for godmode
		if( targ->flags & FL_GODMODE ) {
			take = 0;
			save = damage;
		}
		// never damage in timeout
		else if( GS_MatchPaused( &server_gs ) ) {
			take = save = 0;
		}
		else if( attacker == targ ) {
			if( level.gametype.selfDamage ) {
				take = damage * GS_GetWeaponDef( MODToWeapon( mod ) )->selfdamage;
				save = damage - take;
			}
			else {
				take = save = 0;
			}
		}
	}

	// do the damage
	if( take <= 0 ) {
		return;
	}

	// adding damage given/received to stats
	if( statDmg && attacker->r.client && !targ->deadflag && targ->movetype != MOVETYPE_PUSH && targ->s.type != ET_CORPSE ) {
		attacker->r.client->level.stats.total_damage_given += take;

		// RG calls G_Damage for every bullet, so we accumulate damage
		// in G_Fire_SunflowerPattern and show one number there instead
		if( mod != MOD_RIOTGUN ) {
			edict_t * ev = G_SpawnEvent( EV_DAMAGE, 0, targ->s.origin );
			ev->r.svflags |= SVF_ONLYOWNER;
			ev->s.ownerNum = ENTNUM( attacker );
			ev->s.damage = HEALTH_TO_INT( take );
		}
	}

	G_Gametype_ScoreEvent( attacker->r.client, "dmg", va( "%i %f %i", targ->s.number, damage, attacker->s.number ) );

	if( statDmg && client ) {
		client->level.stats.total_damage_received += take;
	}

	// accumulate received damage for snapshot effects
	{
		vec3_t dorigin;

		if( point[0] != 0.0f || point[1] != 0.0f || point[2] != 0.0f ) {
			VectorCopy( point, dorigin );
		} else {
			VectorSet( dorigin,
					   targ->s.origin[0],
					   targ->s.origin[1],
					   targ->s.origin[2] + targ->viewheight );
		}

		G_BlendFrameDamage( targ, take, &targ->snap.damage_taken, dorigin, dmgdir, targ->snap.damage_at, targ->snap.damage_dir );
		G_BlendFrameDamage( targ, save, &targ->snap.damage_saved, dorigin, dmgdir, targ->snap.damage_at, targ->snap.damage_dir );

		if( targ->r.client && mod != MOD_TELEFRAG && mod != MOD_SUICIDE ) {
			if( inflictor == world || attacker == world ) {
				// for world inflicted damage use always 'frontal'
				G_ClientAddDamageIndicatorImpact( targ->r.client, take + save, NULL );
			} else if( dflags & DAMAGE_RADIUS ) {
				// for splash hits the direction is from the inflictor origin
				G_ClientAddDamageIndicatorImpact( targ->r.client, take + save, pushdir );
			} else {   // for direct hits the direction is the projectile direction
				G_ClientAddDamageIndicatorImpact( targ->r.client, take + save, dmgdir );
			}
		}
	}

	targ->health = targ->health - take;

	// add damage done to stats
	if( statDmg && MODToWeapon( mod ) != Weapon_None && client && attacker->r.client ) {
		attacker->r.client->level.stats.accuracy_hits[ MODToWeapon( mod ) ]++;
		attacker->r.client->level.stats.accuracy_damage[ MODToWeapon( mod ) ] += damage;
	}

	// accumulate given damage for hit sounds
	if( targ != attacker && client && !targ->deadflag && attacker ) {
		attacker->snap.damage_given += take;
	}

	if( G_IsDead( targ ) ) {
		if( client ) {
			targ->flags |= FL_NO_KNOCKBACK;
		}

		if( targ->s.type != ET_CORPSE && attacker != targ ) {
			edict_t * killed = G_SpawnEvent( EV_DAMAGE, 0, targ->s.origin );
			killed->r.svflags |= SVF_ONLYOWNER;
			killed->s.ownerNum = ENTNUM( attacker );
			killed->s.damage = 255;
		}

		G_Killed( targ, inflictor, attacker, HEALTH_TO_INT( take ), point, mod );
	} else {
		G_CallPain( targ, attacker, knockback, take );
	}
}

/*
* G_SplashFrac
*/
void G_SplashFrac( const SyncEntityState *s, const entity_shared_t *r, const vec3_t point, float maxradius, vec3_t pushdir, float *frac, bool selfdamage ) {
	const vec3_t & origin = s->origin;
	const vec3_t & mins = r->mins;
	const vec3_t & maxs = r->maxs;

	float innerradius = ( maxs[0] + maxs[1] - mins[0] - mins[1] ) * 0.25;

	// Find the distance to the closest point in the capsule contained in the player bbox
	// modify the origin so the inner sphere acts as a capsule
	vec3_t closest_point;
	VectorCopy( origin, closest_point );
	closest_point[2] = Clamp( ( origin[2] + mins[2] ) + innerradius, point[2], ( origin[2] + maxs[2] ) - innerradius );

	// find push intensity
	float distance = Distance( closest_point, point );

	if( distance >= maxradius ) {
		if( frac ) {
			*frac = 0;
		}
		if( pushdir ) {
			VectorClear( pushdir );
		}
		return;
	}

	float refdistance = innerradius;

	maxradius -= refdistance;
	distance = max( distance - refdistance, 0 );

	if( frac != NULL ) {
		// soft sin curve
		float distance_frac = ( maxradius - distance ) / maxradius;
		*frac = Clamp01( sinf( DEG2RAD( distance_frac * 80 ) ) );
	}

	// find push direction
	if( pushdir != NULL ) {
		vec3_t center_of_mass;

		if( selfdamage ) {
			center_of_mass[0] = origin[0];
			center_of_mass[1] = origin[1];
			center_of_mass[2] = origin[2] + r->client->ps.viewheight;
		}
		else {
			// find real center of the box again
			for( int i = 0; i < 3; i++ )
				center_of_mass[i] = origin[i] + 0.5f * ( maxs[i] + mins[i] );
		}

		VectorSubtract( center_of_mass, point, pushdir );
		VectorNormalize( pushdir );
	}
}

/*
* G_RadiusDamage
*/
void G_RadiusDamage( edict_t *inflictor, edict_t *attacker, cplane_t *plane, edict_t *ignore, int mod ) {
	int i, numtouch;
	int touch[MAX_EDICTS];
	edict_t *ent = NULL;
	float frac, damage, knockback;
	vec3_t pushDir;
	int timeDelta;

	assert( inflictor );

	float maxdamage = inflictor->projectileInfo.maxDamage;
	float mindamage = inflictor->projectileInfo.minDamage;
	float maxknockback = inflictor->projectileInfo.maxKnockback;
	float minknockback = inflictor->projectileInfo.minKnockback;
	float radius = inflictor->projectileInfo.radius;

	if( radius <= 1.0f || ( maxdamage <= 0.0f && maxknockback <= 0.0f ) ) {
		return;
	}

	mindamage = Min2( mindamage, maxdamage );
	minknockback = Min2( minknockback, maxknockback );

	numtouch = GClip_FindInRadius4D( inflictor->s.origin, radius, touch, MAX_EDICTS, inflictor->timeDelta );
	for( i = 0; i < numtouch; i++ ) {
		ent = game.edicts + touch[i];
		if( ent == ignore || !ent->takedamage ) {
			continue;
		}

		if( ent == attacker && ent->r.client ) {
			timeDelta = 0;
		} else {
			timeDelta = inflictor->timeDelta;
		}

		bool is_selfdamage = inflictor->r.client != NULL && attacker == inflictor;
		G_SplashFrac4D( ent, inflictor->s.origin, radius, pushDir, &frac, timeDelta, is_selfdamage );

		damage = max( 0, mindamage + ( ( maxdamage - mindamage ) * frac ) );
		knockback = max( 0, minknockback + ( ( maxknockback - minknockback ) * frac ) );

		if( knockback < 1.0f ) {
			knockback = 0.0f;
		}

		if( damage <= 0.0f && knockback <= 0.0f ) {
			continue;
		}

		if( G_CanSplashDamage( ent, inflictor, plane ) ) {
			G_Damage( ent, inflictor, attacker, pushDir, inflictor->velocity, inflictor->s.origin, damage, knockback, DAMAGE_RADIUS, mod );
		}
	}
}
