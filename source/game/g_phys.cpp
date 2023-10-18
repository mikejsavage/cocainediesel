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

static bool EntityOverlapsAnything( edict_t *ent ) {
	SolidBits solidity = EntitySolidity( ServerCollisionModelStorage(), &ent->s );
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	trace_t trace = G_Trace4D( ent->s.origin, bounds, ent->s.origin, ent, solidity, ent->timeDelta );
	return trace.GotNowhere();
}

static void SV_CheckVelocity( edict_t *ent ) {
	float velocity = Length( ent->velocity );
	if( velocity > g_maxvelocity->number && velocity != 0.0f ) {
		ent->velocity = ent->velocity * g_maxvelocity->number / velocity;
	}
}

/*
* SV_RunThink
*
* Runs thinking code for this frame if necessary
*/
static void SV_RunThink( edict_t *ent ) {
	TracyZoneScoped;

	if( ent->nextThink <= 0 || ent->nextThink > level.time ) {
		return;
	}

	ent->nextThink = 0;

	if( ISEVENTENTITY( &ent->s ) ) { // events do not think
		return;
	}

	G_CallThink( ent );
}

/*
* SV_Impact
*
* Two entities have touched, so run their touch functions
*/
void SV_Impact( edict_t * e1, const trace_t & trace ) {
	if( trace.HitSomething() ) {
		edict_t * e2 = &game.edicts[ trace.ent ];

		if( EntitySolidity( ServerCollisionModelStorage(), &e1->s ) != Solid_NotSolid ) {
			G_CallTouch( e1, e2, trace.normal, trace.solidity );
		}

		if( EntitySolidity( ServerCollisionModelStorage(), &e2->s ) != Solid_NotSolid ) {
			G_CallTouch( e2, e1, Vec3( 0.0f ), Solid_NotSolid );
		}
	}
}

//===============================================================================
//
//PUSHMOVE
//
//===============================================================================


/*
* SV_PushEntity
*
* Does not change the entities velocity at all
*/
static trace_t SV_PushEntity( edict_t *ent, Vec3 push ) {
	trace_t trace;
	SolidBits solidity;

	Vec3 start = ent->s.origin;
	Vec3 end = start + push;

retry:
	solidity = EntitySolidity( ServerCollisionModelStorage(), &ent->s );
	if( solidity == Solid_NotSolid ) {
		solidity = SolidMask_AnySolid;
	}

	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	trace = G_Trace4D( start, bounds, end, ent, solidity, ent->timeDelta );
	ent->s.origin = trace.endpos;

	GClip_LinkEntity( ent );

	if( trace.HitSomething() ) {
		SV_Impact( ent, trace );

		// if the pushed entity went away and the pusher is still there
		if( !game.edicts[trace.ent].r.inuse && ent->movetype == MOVETYPE_PUSH && ent->r.inuse ) {
			// move the pusher back and try again
			ent->s.origin = start;
			GClip_LinkEntity( ent );
			goto retry;
		}
	}

	if( ent->r.inuse ) {
		GClip_TouchTriggers( ent );
	}

	return trace;
}


struct pushed_t {
	edict_t *ent;
	Vec3 origin;
	Vec3 angles;
	float yaw;
	Vec3 pmove_origin;
};
static pushed_t pushed[MAX_EDICTS], *pushed_p;

static edict_t *obstacle;


/*
* SV_Push
*
* Objects need to be moved back on a failed push,
* otherwise riders would continue to slide.
*/
static bool SV_Push( edict_t *pusher, Vec3 move, Vec3 amove ) {
	TracyZoneScoped;

	int e;
	edict_t *check;
	pushed_t *p;
	mat3_t axis;
	Vec3 org, org2, move2;

	// find the bounding box
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &pusher->s );
	bounds += pusher->s.origin + move;

	// we need this for pushing things later
	org = -amove;
	AnglesToAxis( org, axis );

	// save the pusher's original position
	pushed_p->ent = pusher;
	pushed_p->origin = pusher->s.origin;
	pushed_p->angles = pusher->s.angles;
	if( pusher->r.client ) {
		pushed_p->pmove_origin = pusher->r.client->ps.pmove.velocity;
		pushed_p->yaw = pusher->r.client->ps.viewangles.y;
	}
	pushed_p++;

	// move the pusher to its final position
	pusher->s.origin += move;
	pusher->s.angles += amove;
	GClip_LinkEntity( pusher );

	// see if any solid entities are inside the final position
	check = game.edicts + 1;
	for( e = 1; e < game.numentities; e++, check++ ) {
		if( !check->r.inuse ) {
			continue;
		}
		if( check->movetype == MOVETYPE_PUSH
			|| check->movetype == MOVETYPE_STOP
			|| check->movetype == MOVETYPE_NONE
			|| check->movetype == MOVETYPE_NOCLIP ) {
			continue;
		}

		// if the entity is standing on the pusher, it will definitely be moved
		if( check->groundentity != pusher ) {
			// see if the ent needs to be tested
			MinMax3 check_bounds = EntityBounds( ServerCollisionModelStorage(), &check->s );
			check_bounds += check->s.origin;
			if( !BoundsOverlap( check_bounds, bounds ) ) {
				continue;
			}

			// see if the ent's bbox is inside the pusher's final position
			if( !EntityOverlapsAnything( check ) ) {
				continue;
			}
		}

		if( ( pusher->movetype == MOVETYPE_PUSH ) || ( check->groundentity == pusher ) ) {
			// move this entity
			pushed_p->ent = check;
			pushed_p->origin = check->s.origin;
			pushed_p->angles = check->s.angles;
			pushed_p++;

			// try moving the contacted entity
			check->s.origin = check->s.origin + move;
			if( check->r.client ) {
				// FIXME: doesn't rotate monsters?
				check->r.client->ps.pmove.origin = check->r.client->ps.pmove.origin + move;
				check->r.client->ps.viewangles.y += amove.y;
			}

			// figure movement due to the pusher's amove
			org = check->s.origin - pusher->s.origin;
			Matrix3_TransformVector( axis, org, &org2 );
			move2 = org2 - org;
			check->s.origin = check->s.origin + move2;

			if( check->movetype != MOVETYPE_BOUNCEGRENADE ) {
				// may have pushed them off an edge
				if( check->groundentity != pusher ) {
					check->groundentity = NULL;
				}
			}

			bool block = EntityOverlapsAnything( check );
			if( !block ) {
				// pushed ok
				GClip_LinkEntity( check );

				// impact?
				continue;
			} else {
				// try to fix block
				// if it is ok to leave in the old position, do it
				// this is only relevant for riding entities, not pushed
				check->s.origin = check->s.origin - move;
				check->s.origin = check->s.origin - move2;
				block = EntityOverlapsAnything( check );
				if( !block ) {
					pushed_p--;
					continue;
				}
			}
		}

		// save off the obstacle so we can call the block function
		obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for( p = pushed_p - 1; p >= pushed; p-- ) {
			p->ent->s.origin = p->origin;
			p->ent->s.angles = p->angles;
			if( p->ent->r.client ) {
				p->ent->r.client->ps.pmove.origin = p->pmove_origin;
				p->ent->r.client->ps.viewangles.y = p->yaw;
			}
			GClip_LinkEntity( p->ent );
		}
		return false;
	}

	//FIXME: is there a better way to handle this?
	// see if anything we moved has touched a trigger
	for( p = pushed_p - 1; p >= pushed; p-- )
		GClip_TouchTriggers( p->ent );

	return true;
}

/*
* SV_Physics_Pusher
*
* Bmodel objects don't interact with each other, but
* push all box objects
*/
static void SV_Physics_Pusher( edict_t *ent ) {
	TracyZoneScoped;

	pushed_p = pushed;

	bool blocked = false;

	if( ent->velocity != Vec3( 0.0f ) || ent->avelocity != Vec3( 0.0f ) ) {
		Vec3 move;
		if( ent->s.linearMovement ) {
			GS_LinearMovement( &ent->s, svs.gametime, &move );
			move -= ent->s.origin;
		}
		else {
			move = ent->velocity * FRAMETIME;
		}

		Vec3 amove = ent->avelocity * FRAMETIME;

		blocked = !SV_Push( ent, move, amove );
	}

	if( pushed_p > &pushed[MAX_EDICTS] ) {
		Fatal( "pushed_p > &pushed[MAX_EDICTS], memory corrupted" );
	}

	if( blocked ) {
		// the move failed, bump all nextthink times and back out moves
		if( ent->nextThink > 0 ) {
			ent->nextThink += game.frametime;
		}

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if( ent->moveinfo.blocked ) {
			ent->moveinfo.blocked( ent, obstacle );
		}
	}
}

//==============================================================================
//
//TOSS / BOUNCE
//
//==============================================================================

/*
* SV_Physics_Toss
*
* Toss, bounce, and fly movement.  When onground, do nothing.
*
* FIXME: This function needs a serious rewrite
*/
static void SV_Physics_Toss( edict_t *ent ) {
	TracyZoneScoped;

	// refresh the ground entity
	if( ent->movetype == MOVETYPE_BOUNCE || ent->movetype == MOVETYPE_BOUNCEGRENADE ) {
		if( ent->velocity.z > 0.1f ) {
			ent->groundentity = NULL;
		}
	}

	if( ent->groundentity && ent->groundentity != world && !ent->groundentity->r.inuse ) {
		ent->groundentity = NULL;
	}

	float oldSpeed = Length( ent->velocity );

	if( ent->groundentity ) {
		if( !oldSpeed ) {
			return;
		}

		if( ent->movetype == MOVETYPE_TOSS ) {
			if( ent->velocity.z >= 8 ) {
				ent->groundentity = NULL;
			} else {
				ent->velocity = Vec3( 0.0f );
				ent->avelocity = Vec3( 0.0f );
				G_CallStop( ent );
				return;
			}
		}
	}

	SV_CheckVelocity( ent );

	// add gravity
	if( !ent->groundentity ) {
		ent->velocity.z -= GRAVITY * FRAMETIME * ent->gravity_scale;
	}

	// move origin
	Vec3 move = ent->velocity * FRAMETIME;

	if( !ent->r.inuse ) {
		return;
	}

	trace_t trace = SV_PushEntity( ent, move );
	if( trace.HitSomething() ) {
		float restitution = ent->movetype == MOVETYPE_BOUNCE || ent->movetype == MOVETYPE_BOUNCEGRENADE ? ent->restitution : 0.0f;

		Vec3 impulse = -Dot( ent->velocity, trace.normal ) * trace.normal;
		ent->velocity += ( 1.0f + restitution ) * impulse;

		ent->num_bounces++;

		// stop if on ground

		if( ent->movetype != MOVETYPE_TOSS ) {
			// stop dead on allsolid

			// LA: hopefully will fix grenades bouncing down slopes
			// method taken from Darkplaces sourcecode
			if( trace.GotNowhere() ||
				( ISWALKABLEPLANE( trace.normal ) &&
				  Abs( Dot( trace.normal, ent->velocity ) ) < 40
				)
			) {
				ent->groundentity = &game.edicts[trace.ent];
				ent->velocity = Vec3( 0.0f );
				ent->avelocity = Vec3( 0.0f );
				G_CallStop( ent );
			}
		} else {
			// in movetype_toss things stop dead when touching ground

			// walkable or trapped inside solid brush
			if( trace.GotNowhere() || ISWALKABLEPLANE( trace.normal ) ) {
				ent->groundentity = trace.HitNothing() ? world : &game.edicts[trace.ent];
				ent->velocity = Vec3( 0.0f );
				ent->avelocity = Vec3( 0.0f );
				G_CallStop( ent );
			}
		}
	}

	// move angles
	if( ent->movetype == MOVETYPE_BOUNCEGRENADE ) {
		if( ent->velocity == Vec3( 0.0f ) ) {
			ent->s.angles.x = 0.0f;
			ent->s.angles.z = 0.0f;
		}
		else {
			ent->s.angles = VecToAngles( SafeNormalize( ent->velocity ) );
		}
	}
	else {
		ent->s.angles += ent->avelocity * FRAMETIME;
	}

	GClip_LinkEntity( ent );
}

//============================================================================

static void SV_Physics_LinearProjectile( edict_t *ent ) {
	TracyZoneScoped;

	SolidBits solidity = EntitySolidity( ServerCollisionModelStorage(), &ent->s );
	if( solidity == Solid_NotSolid ) {
		solidity = SolidMask_AnySolid;
	}

	// find its current position given the starting timeStamp
	float endFlyTime = float( svs.gametime - ent->s.linearMovementTimeStamp ) * 0.001f;
	float startFlyTime = float( Max2( s64( 0 ), game.prevServerTime - ent->s.linearMovementTimeStamp ) ) * 0.001f;

	Vec3 start = ent->s.linearMovementBegin + ent->s.linearMovementVelocity * startFlyTime;
	Vec3 end = ent->s.linearMovementBegin + ent->s.linearMovementVelocity * endFlyTime;

	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	trace_t trace = G_Trace4D( start, bounds, end, ent, solidity, ent->timeDelta );
	ent->s.origin = trace.endpos;
	GClip_LinkEntity( ent );
	SV_Impact( ent, trace );

	GClip_TouchTriggers( ent );
	ent->groundentity = NULL; // projectiles never have ground entity

	ent->s.angles += ent->avelocity * FRAMETIME;
}

//============================================================================

void G_RunEntity( edict_t *ent ) {
	TracyZoneScoped;

	if( !level.canSpawnEntities ) { // don't try to think before map entities are spawned
		return;
	}

	SV_RunThink( ent );

	switch( (int)ent->movetype ) {
		case MOVETYPE_NONE:
		case MOVETYPE_NOCLIP: // only used for clients, that use pmove
		case MOVETYPE_PLAYER:
			break;
		case MOVETYPE_PUSH:
		case MOVETYPE_STOP:
			SV_Physics_Pusher( ent );
			break;
		case MOVETYPE_BOUNCE:
		case MOVETYPE_BOUNCEGRENADE:
		case MOVETYPE_TOSS:
			SV_Physics_Toss( ent );
			break;
		case MOVETYPE_LINEARPROJECTILE:
			SV_Physics_LinearProjectile( ent );
			break;
		default:
			Fatal( "SV_Physics: bad movetype %i", (int)ent->movetype );
	}
}
