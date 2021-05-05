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
	if( !level.gametype.isTeamBased )
		return false;
	return targ->number != attacker->number && targ->team == attacker->team;
}

/*
* G_CanSplashDamage
*/
#define SPLASH_DAMAGE_TRACE_FRAC_EPSILON 1.0 / 32.0f
static bool G_CanSplashDamage( edict_t *targ, edict_t *inflictor, cplane_t *plane, Vec3 pos, int timeDelta ) {
	Vec3 dest, origin;
	trace_t trace;
	int solidmask = MASK_SOLID;

	if( !targ ) {
		return false;
	}

	if( !plane ) {
		origin = pos;
	}

	// bmodels need special checking because their origin is 0,0,0
	if( targ->movetype == MOVETYPE_PUSH ) {
		// NOT FOR PLAYERS only for entities that can push the players
		dest = ( targ->r.absmin + targ->r.absmax ) * 0.5f;
		G_Trace4D( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), dest, inflictor, solidmask, timeDelta );
		if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
			return true;
		}

		return false;
	}

	if( plane ) {
		// up by 9 units to account for stairs
		origin = pos + plane->normal * 9;
	}

	// This is for players
	G_Trace4D( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), targ->s.origin, inflictor, solidmask, timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	dest = targ->s.origin;
	dest.x += 15.0;
	dest.y += 15.0;
	G_Trace4D( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), dest, inflictor, solidmask, timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	dest = targ->s.origin;
	dest.x += 15.0;
	dest.y -= 15.0;
	G_Trace4D( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), dest, inflictor, solidmask, timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	dest = targ->s.origin;
	dest.x -= 15.0;
	dest.y += 15.0;
	G_Trace4D( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), dest, inflictor, solidmask, timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	dest = targ->s.origin;
	dest.x -= 15.0;
	dest.y -= 15.0;
	G_Trace4D( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), dest, inflictor, solidmask, timeDelta );
	if( trace.fraction >= 1.0 - SPLASH_DAMAGE_TRACE_FRAC_EPSILON || trace.ent == ENTNUM( targ ) ) {
		return true;
	}

	return false;
}

/*
* G_Killed
*/
void G_Killed( edict_t *targ, edict_t *inflictor, edict_t *attacker, int assistorNo, int damage, Vec3 point, int mod ) {
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
		G_ClientGetStats( targ )->deaths++;

		if( !attacker || !attacker->r.client || attacker == targ || attacker == world ) {
			G_ClientGetStats( targ )->suicides++;
		} else {
			G_ClientGetStats( attacker )->kills++;
		}
	}

	G_ClientGetStats( targ )->alive = false;
	server_gs.gameState.teams[ targ->s.team ].numalive--;
	G_Gametype_ScoreEvent( attacker ? attacker->r.client : NULL, "kill", va( "%i %i %i %i", targ->s.number, ( inflictor == world ) ? -1 : ENTNUM( inflictor ), ENTNUM( attacker ), mod ) );

	G_CallDie( targ, inflictor, attacker, assistorNo, damage, point );
}

/*
* G_BlendFrameDamage
*/
static void G_BlendFrameDamage( edict_t *ent, float damage, float *old_damage, const Vec3 * point, Vec3 basedir, Vec3 * old_point, Vec3 * old_dir ) {
	Vec3 offset;

	if( !point ) {
		offset = Vec3( 0, 0, ent->viewheight );
	} else {
		offset = *point - ent->s.origin;
	}

	Vec3 dir = SafeNormalize( basedir );

	if( *old_damage == 0 ) {
		*old_point = offset;
		*old_dir = dir;
		*old_damage = damage;
		return;
	}

	float frac = damage / ( damage + *old_damage );
	*old_point = Lerp( *old_point, frac, offset );
	*old_dir = Lerp( *old_dir, frac, dir );

	*old_damage += damage;
}

static void G_AddAssistDamage( edict_t* targ, edict_t* attacker, int amount ) {
	if( attacker == world || attacker == targ ) {
		return;
	}

	int attacker_entno = attacker->s.number;
	assistinfo_t *assist = NULL;

	for ( int i = 0; i < MAX_ASSIST_INFO; ++i ) {
		// check for recent attacker or free slot first
		if( targ->recent_attackers[i].entno == attacker_entno || !targ->recent_attackers[i].entno) {
			assist = &targ->recent_attackers[i];
			break;
		}
	}

	if( assist == NULL ) {
		// no free slots, replace oldest attacker seeya pal
		for ( int i = 0; i < MAX_ASSIST_INFO; ++i ) {
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

static int G_FindTopAssistor( edict_t* victim, edict_t* attacker ) {
	assistinfo_t *top = NULL;

	// TODO: could weigh damage by most recent timestamp as well
	for (int i = 0; i < MAX_ASSIST_INFO; ++i) {
		if(victim->recent_attackers[i].entno && (top == NULL || victim->recent_attackers[i].cumDamage > top->cumDamage)) {
			top = &victim->recent_attackers[i];
		}
	}

	// dont return last-hit killer as assistor as well. if they did a tiny amount of dmg ignore
	return top != NULL && top->entno != attacker->s.number && top->cumDamage > 9 ? top->entno : -1;
}

#define MIN_KNOCKBACK_SPEED 2.5

/*
* G_KnockBackPush
*/
static void G_KnockBackPush( edict_t *targ, edict_t *attacker, Vec3 basedir, int knockback, int dflags ) {
	if( targ->flags & FL_NO_KNOCKBACK ) {
		return;
	}

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

	Vec3 dir = Normalize( basedir );
	constexpr float VERTICAL_KNOCKBACK_SCALE = 1.25f;
	dir.z *= VERTICAL_KNOCKBACK_SCALE;

	if( targ->r.client && targ != attacker && !( dflags & DAMAGE_KNOCKBACK_SOFT ) ) {
		targ->r.client->ps.pmove.knockback_time = Clamp( 100, 3 * knockback, 250 );
	}

	targ->velocity = targ->velocity + dir * push;
	knockbackOfDeath = dir * push;
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
void G_Damage( edict_t *targ, edict_t *inflictor, edict_t *attacker, Vec3 pushdir, Vec3 dmgdir, Vec3 point, float damage, float knockback, int dflags, int mod ) {
	gclient_t *client;

	if( !targ || !targ->takedamage ) {
		return;
	}

	if( !attacker ) {
		attacker = world;
		mod = MeanOfDeath_Trigger;
	}

	meansOfDeath = mod;
	damageFlagsOfDeath = dflags;

	client = targ->r.client;

	// Cgg - race mode: players don't interact with one another
	if( GS_RaceGametype( &server_gs ) && attacker->r.client && targ->r.client && attacker != targ ) {
		return;
	}

	if( G_IsTeamDamage( &targ->s, &attacker->s ) ) {
		return;
	}

	// dont count self-damage cause it just adds the same to both stats
	bool statDmg = attacker != targ && mod != MeanOfDeath_Telefrag && mod != MeanOfDeath_Explosion;

	// push
	G_KnockBackPush( targ, attacker, pushdir, knockback, dflags );

	float take = damage;

	// check for cases where damage is protected
	if( !( dflags & DAMAGE_NO_PROTECTION ) ) {
		// check for godmode
		if( targ->flags & FL_GODMODE ) {
			take = 0;
		}
		// never damage in timeout
		else if( GS_MatchPaused( &server_gs ) ) {
			take = 0;
		}
		else if( attacker == targ ) {
			if( level.gametype.selfDamage ) {
				take = damage * GS_GetWeaponDef( mod )->selfdamage;
			}
			else {
				take = 0;
			}
		}
	}

	// do the damage
	if( take <= 0 ) {
		return;
	}

	// adding damage given/received to stats
	if( statDmg && attacker->r.client && !targ->deadflag && targ->movetype != MOVETYPE_PUSH && targ->s.type != ET_CORPSE ) {
		G_ClientGetStats( attacker )->total_damage_given += take;

		// shotgun calls G_Damage for every bullet, so we accumulate damage
		// in W_Fire_Shotgun and show one number there instead
		if( mod != Weapon_Shotgun ) {
			u64 parm = HEALTH_TO_INT( take ) << 1;
			if( dflags & DAMAGE_HEADSHOT ) {
				parm |= 1;
				G_SpawnEvent( EV_HEADSHOT, 0, &targ->s.origin );
			}

			edict_t * ev = G_SpawnEvent( EV_DAMAGE, parm, &targ->s.origin );
			ev->r.svflags |= SVF_OWNERANDCHASERS;
			ev->s.ownerNum = ENTNUM( attacker );
		}
	}

	G_Gametype_ScoreEvent( attacker->r.client, "dmg", va( "%i %f %i", targ->s.number, damage, attacker->s.number ) );

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

		G_BlendFrameDamage( targ, take, &targ->snap.damage_taken, &dorigin, dmgdir, &targ->snap.damage_at, &targ->snap.damage_dir );

		if( targ->r.client && mod != MeanOfDeath_Telefrag && mod != MeanOfDeath_Suicide ) {
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

	int clamped_takedmg = HEALTH_TO_INT( take );

	// add damage done to stats
	if( statDmg && mod < Weapon_Count && client && attacker->r.client ) {
		G_ClientGetStats( attacker )->accuracy_hits[ mod ]++;
		G_ClientGetStats( attacker )->accuracy_damage[ mod ] += damage;
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
			edict_t * killed = G_SpawnEvent( EV_DAMAGE, 255 << 1, &targ->s.origin );
			killed->r.svflags |= SVF_OWNERANDCHASERS;
			killed->s.ownerNum = ENTNUM( attacker );
		}

		int topAssistorNo = G_FindTopAssistor( targ, attacker );
		G_Killed( targ, inflictor, attacker, topAssistorNo, clamped_takedmg, point, mod );
	} else {
		G_AddAssistDamage( targ, attacker, clamped_takedmg );
		G_CallPain( targ, attacker, knockback, take );
	}
}

/*
* G_SplashFrac
*/
void G_SplashFrac( const SyncEntityState *s, const entity_shared_t *r, Vec3 point, float maxradius, Vec3 * pushdir, float *frac, bool selfdamage ) {
	const Vec3 & origin = s->origin;
	const Vec3 & mins = r->mins;
	const Vec3 & maxs = r->maxs;

	float innerradius = ( maxs.x + maxs.y - mins.x - mins.y ) * 0.25f;

	// Find the distance to the closest point in the capsule contained in the player bbox
	// modify the origin so the inner sphere acts as a capsule
	Vec3 closest_point = origin;
	closest_point.z = Clamp( ( origin.z + mins.z ) + innerradius, point.z, ( origin.z + maxs.z ) - innerradius );

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
		center_of_mass = origin + 0.5f * ( maxs + mins );
	}

	*pushdir = Normalize( center_of_mass - point );
}


/*
* G_RadiusKnockback
*/

void G_RadiusKnockback( const WeaponDef * def, edict_t *attacker, Vec3 pos, cplane_t *plane, int mod, int timeDelta ) {
	float maxknockback = def->knockback;
	float minknockback = def->minknockback;
	float radius = def->splash_radius;

	assert( radius >= 0.0f );
	assert( minknockback >= 0.0f && maxknockback >= 0.0f );

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

		if( G_CanSplashDamage( ent, NULL, plane, pos, timeDelta ) ) {
			float knockback = Lerp( minknockback, frac, maxknockback );
			G_KnockBackPush( ent, attacker, pushDir, knockback, 0 );
		}
	}
}


/*
* G_RadiusDamage
*/
void G_RadiusDamage( edict_t *inflictor, edict_t *attacker, cplane_t *plane, edict_t *ignore, int mod ) {
	assert( inflictor );

	float maxdamage = inflictor->projectileInfo.maxDamage;
	float mindamage = inflictor->projectileInfo.minDamage;
	float maxknockback = inflictor->projectileInfo.maxKnockback;
	float minknockback = inflictor->projectileInfo.minKnockback;
	float radius = inflictor->projectileInfo.radius;

	assert( radius >= 0.0f );
	assert( mindamage >= 0.0f && minknockback >= 0.0f && mindamage <= maxdamage );
	assert( maxdamage >= 0.0f && maxknockback >= 0.0f && mindamage <= maxdamage );

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

		if( G_CanSplashDamage( ent, inflictor, plane, inflictor->s.origin, inflictor->timeDelta ) ) {
			float damage = Lerp( mindamage, frac, maxdamage );
			float knockback = Lerp( minknockback, frac, maxknockback );
			G_Damage( ent, inflictor, attacker, pushDir, inflictor->velocity, inflictor->s.origin, damage, knockback, DAMAGE_RADIUS, mod );
		}
	}
}
