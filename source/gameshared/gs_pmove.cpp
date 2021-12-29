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

#include "qcommon/qcommon.h"
#include "gameshared/movement.h"


static constexpr s16 MAX_TBAG_TIME = 2000;
static constexpr s16 TBAG_THRESHOLD = 1000;
static constexpr s16 TBAG_AMOUNT_PER_CROUCH = 500;

//===============================================================

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

static pmove_t *pm;
static pml_t pml;
static const gs_state_t * pmove_gs;

// movement parameters

constexpr float pm_friction = 16; //  ( initially 6 )
constexpr float pm_waterfriction = 16;
constexpr float pm_wateraccelerate = 12; // user intended acceleration when swimming ( initially 6 )

constexpr float pm_accelerate = 16; // user intended acceleration when on ground or fly movement ( initially 10 )
constexpr float pm_decelerate = 16; // user intended deceleration when on ground

constexpr float pm_airaccelerate = 0.5f; // user intended aceleration when on air
constexpr float pm_airdecelerate = 1.0f; // air deceleration (not +strafe one, just at normal moving).

// special movement parameters

constexpr float pm_aircontrol = 140.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
constexpr float pm_strafebunnyaccel = 60; // forward acceleration when strafe bunny hopping
constexpr float pm_wishspeed = 30;

//
//  walking up a step should kill some velocity
//

/*
* PM_SlideMove
*
* Returns a new origin, velocity, and contact entity
* Does not modify any world state?
*/

#define MAX_CLIP_PLANES 5

static void PM_AddTouchEnt( int entNum ) {
	if( pm->numtouch >= MAXTOUCH || entNum < 0 ) {
		return;
	}

	// see if it is already added
	for( int i = 0; i < pm->numtouch; i++ ) {
		if( pm->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entNum;
	pm->numtouch++;
}


static int PM_SlideMove() {
	ZoneScoped;

	Vec3 planes[MAX_CLIP_PLANES];
	constexpr int maxmoves = 4;
	float remainingTime = pml.frametime;
	int blockedmask = 0;

	Vec3 old_velocity = pml.velocity;
	Vec3 last_valid_origin = pml.origin;

	if( pm->groundentity != -1 ) { // clip velocity to ground, no need to wait
		// if the ground is not horizontal (a ramp) clipping will slow the player down
		if( pml.groundplane.normal.z == 1.0f && pml.velocity.z < 0.0f ) {
			pml.velocity.z = 0.0f;
		}
	}

	int numplanes = 0; // clean up planes count for checking

	for( int moves = 0; moves < maxmoves; moves++ ) {
		Vec3 end = pml.origin + pml.velocity * remainingTime;

		trace_t trace;
		pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );
		if( trace.allsolid ) { // trapped into a solid
			pml.origin = last_valid_origin;
			return SLIDEMOVEFLAG_TRAPPED;
		}

		if( trace.fraction > 0 ) { // actually covered some distance
			pml.origin = trace.endpos;
			last_valid_origin = trace.endpos;
		}

		if( trace.fraction == 1 ) {
			break; // move done
		}

		// save touched entity for return output
		PM_AddTouchEnt( trace.ent );

		// at this point we are blocked but not trapped.

		blockedmask |= SLIDEMOVEFLAG_BLOCKED;
		if( trace.plane.normal.z < SLIDEMOVE_PLANEINTERACT_EPSILON ) { // is it a vertical wall?
			blockedmask |= SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		remainingTime -= trace.fraction * remainingTime;

		// we got blocked, add the plane for sliding along it

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		{
			int i;
			for( i = 0; i < numplanes; i++ ) {
				if( Dot( trace.plane.normal, planes[i] ) > ( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) ) {
					pml.velocity = trace.plane.normal + pml.velocity;
					break;
				}
			}
			if( i < numplanes ) { // found a repeated plane, so don't add it, just repeat the trace
				continue;
			}
		}

		// security check: we can't store more planes
		if( numplanes >= MAX_CLIP_PLANES ) {
			pml.velocity = Vec3( 0.0f );
			return SLIDEMOVEFLAG_TRAPPED;
		}

		// put the actual plane in the list
		planes[numplanes] = trace.plane.normal;
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//

		for( int i = 0; i < numplanes; i++ ) {
			if( Dot( pml.velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) { // would not touch it
				continue;
			}

			pml.velocity = GS_ClipVelocity( pml.velocity, planes[i], PM_OVERBOUNCE );
			// see if we enter a second plane
			for( int j = 0; j < numplanes; j++ ) {
				if( j == i ) { // it's the same plane
					continue;
				}
				if( Dot( pml.velocity, planes[j] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue; // not with this one
				}

				//there was a second one. Try to slide along it too
				pml.velocity = GS_ClipVelocity( pml.velocity, planes[j], PM_OVERBOUNCE );

				// check if the slide sent it back to the first plane
				if( Dot( pml.velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue;
				}

				// bad luck: slide the original velocity along the crease
				Vec3 dir = SafeNormalize( Cross( planes[i], planes[j] ) );
				float value = Dot( dir, pml.velocity );
				pml.velocity = dir * value;

				// check if there is a third plane, in that case we're trapped
				for( int k = 0; k < numplanes; k++ ) {
					if( j == k || i == k ) { // it's the same plane
						continue;
					}
					if( Dot( pml.velocity, planes[k] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
						continue; // not with this one
					}
					pml.velocity = Vec3( 0.0f );
					break;
				}
			}
		}
	}

	if( pm->playerState->pmove.pm_time ) {
		pml.velocity = old_velocity;
	}

	return blockedmask;
}

/*
* PM_StepSlideMove
*
* Each intersection will try to step over the obstruction instead of
* sliding along it.
*/
static void PM_StepSlideMove() {
	ZoneScoped;

	trace_t trace;

	Vec3 start_o = pml.origin;
	Vec3 start_v = pml.velocity;

	int blocked = PM_SlideMove();

	Vec3 down_o = pml.origin;
	Vec3 down_v = pml.velocity;

	Vec3 up = start_o + Vec3( 0.0f, 0.0f, STEPSIZE );

	pmove_gs->api.Trace( &trace, up, pm->mins, pm->maxs, up, pm->playerState->POVnum, pm->contentmask, 0 );
	if( trace.allsolid ) {
		return; // can't step up
	}

	// try sliding above
	pml.origin = up;
	pml.velocity = start_v;

	PM_SlideMove();

	// push down the final amount
	Vec3 down = pml.origin - Vec3( 0.0f, 0.0f, STEPSIZE );
	pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, down, pm->playerState->POVnum, pm->contentmask, 0 );
	if( !trace.allsolid ) {
		pml.origin = trace.endpos;
	}

	up = pml.origin;

	// decide which one went farther
	float down_dist = LengthSquared( down_o.xy() - start_o.xy() );
	float up_dist = LengthSquared( up.xy() - start_o.xy() );

	if( down_dist >= up_dist || trace.allsolid || ( trace.fraction != 1.0 && !ISWALKABLEPLANE( &trace.plane ) ) ) {
		pml.origin = down_o;
		pml.velocity = down_v;
		return;
	}

	// only add the stepping output when it was a vertical step (second case is at the exit of a ramp)
	if( ( blocked & SLIDEMOVEFLAG_WALL_BLOCKED ) || trace.plane.normal.z == 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
		pm->step = pml.origin.z - pml.previous_origin.z;
	}

	// Preserve speed when sliding up ramps
	float hspeed = Length( start_v.xy() );
	if( hspeed && ISWALKABLEPLANE( &trace.plane ) ) {
		if( trace.plane.normal.z >= 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
			pml.velocity = start_v;
		} else {
			Normalize2D( &pml.velocity );
			pml.velocity = Vec3( pml.velocity.xy() * hspeed, pml.velocity.z );
		}
	}

	// wsw : jal : The following line is what produces the ramp sliding.

	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity.z = down_v.z;
}

/*
* PM_Friction -- Modified for wsw
*
* Handles both ground friction and water friction
*/
static void PM_Friction() {
	float speed = LengthSquared( pml.velocity );
	if( speed < 1 ) {
		pml.velocity.x = 0.0f;
		pml.velocity.y = 0.0f;
		return;
	}

	speed = sqrtf( speed );
	float drop = 0.0f;

	// apply ground friction
	if( pm->groundentity != -1 || pml.ladder ) {
		if( pm->playerState->pmove.knockback_time <= 0 ) {
			float friction = pm_friction;
			float control = speed < pm_decelerate ? pm_decelerate : speed;
			drop += control * friction * pml.frametime;
		}
	}

	// scale the velocity
	float newspeed = Max2( 0.0f, speed - drop );
	pml.velocity *= newspeed / speed;
}

/*
* PM_Accelerate
*
* Handles user intended acceleration
*/
static void PM_Accelerate( Vec3 wishdir, float wishspeed, float accel ) {
	float currentspeed = Dot( pml.velocity, wishdir );
	float addspeed = wishspeed - currentspeed;
	if( addspeed <= 0 ) {
		return;
	}

	float accelspeed = accel * pml.frametime * wishspeed;
	if( accelspeed > addspeed ) {
		accelspeed = addspeed;
	}

	pml.velocity += wishdir * accelspeed;
}

// when using +strafe convert the inertia to forward speed.
static void PM_Aircontrol( Vec3 wishdir, float wishspeed ) {
	// accelerate
	float smove = pml.sidePush;

	if( smove != 0.0f || wishspeed == 0.0f ) {
		return; // can't control movement if not moving forward or backward
	}

	float zspeed = pml.velocity.z;
	pml.velocity.z = 0;
	float speed = Length( pml.velocity );
	pml.velocity = Normalize( pml.velocity );

	float dot = Dot( pml.velocity, wishdir );
	float k = 32.0f * pm_aircontrol * dot * dot * pml.frametime;

	if( dot > 0 ) {
		// we can't change direction while slowing down
		pml.velocity.x = pml.velocity.x * speed + wishdir.x * k;
		pml.velocity.y = pml.velocity.y * speed + wishdir.y * k;

		pml.velocity = Normalize( pml.velocity );
	}

	pml.velocity.x *= speed;
	pml.velocity.y *= speed;
	pml.velocity.z = zspeed;
}

static Vec3 PM_LadderMove( Vec3 wishvel ) {
	if( pml.ladder && Abs( pml.velocity.z ) <= DEFAULT_LADDERSPEED ) {
		if( pml.upPush > 0 ) { //jump
			wishvel.z = DEFAULT_LADDERSPEED;
		}
		else if( pml.forwardPush != 0 ) {
			wishvel.z = Lerp( -float( DEFAULT_LADDERSPEED ), Unlerp01( 15.0f, pm->playerState->viewangles[PITCH], -15.0f ), float( DEFAULT_LADDERSPEED ) );
		}
		else {
			wishvel.z = 0;
		}

		// limit horizontal speed when on a ladder
		wishvel.x = Clamp( -25.0f, wishvel.x, 25.0f );
		wishvel.y = Clamp( -25.0f, wishvel.y, 25.0f );
	}

	return wishvel;
}

static void PM_WaterMove() {
	ZoneScoped;

	// user intentions
	Vec3 wishvel = pml.forward * pml.forwardPush + pml.right * pml.sidePush;
	wishvel.z -= pm_waterfriction;

	wishvel = PM_LadderMove( wishvel );

	Vec3 wishdir = wishvel;
	float wishspeed = Length( wishdir );
	wishdir = SafeNormalize( wishdir );

	if( wishspeed > pml.maxPlayerSpeed ) {
		wishspeed = pml.maxPlayerSpeed / wishspeed;
		wishvel *= wishspeed;
		wishspeed = pml.maxPlayerSpeed;
	}

	PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );
	PM_StepSlideMove();
}

static void PM_Move() {
	ZoneScoped;

	float fmove = pml.forwardPush;
	float smove = pml.sidePush;

	Vec3 wishvel = pml.forward * fmove + pml.right * smove;
	wishvel.z = 0;

	wishvel = PM_LadderMove( wishvel );

	Vec3 wishdir = wishvel;
	float wishspeed = Length( wishdir );
	wishdir = SafeNormalize( wishdir );

	// clamp to server defined max speed

	float maxspeed;
	if( pm->playerState->pmove.crouch_time ) {
		maxspeed = pml.maxCrouchedSpeed;
	} else {
		maxspeed = pml.maxPlayerSpeed;
	}

	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		wishvel *= wishspeed;
		wishspeed = maxspeed;
	}

	if( pml.ladder ) {
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		if( wishvel.z == 0.0f ) {
			float decel = GRAVITY * pml.frametime;
			if( pml.velocity.z > 0 ) {
				pml.velocity.z = Max2( 0.0f, pml.velocity.z - decel );
			}
			else {
				pml.velocity.z = Min2( 0.0f, pml.velocity.z + decel );
			}
		}

		PM_StepSlideMove();
	}
	else if( pm->groundentity != -1 ) {
		// walking on ground
		if( pml.velocity.z > 0 ) {
			pml.velocity.z = 0; //!!! this is before the accel
		}

		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		pml.velocity.z = Min2( 0.0f, pml.velocity.z );

		if( pml.velocity.xy() == Vec2( 0.0f ) ) {
			return;
		}

		PM_StepSlideMove();
	}
	else {
		// Air Control
		float wishspeed2 = wishspeed;
		float accel;
		if( Dot( pml.velocity, wishdir ) < 0 && !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) && pm->playerState->pmove.knockback_time <= 0 ) {
			accel = pm_airdecelerate;
		} else {
			accel = pm_airaccelerate;
		}

		if( ( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) ) {
			accel = 0; // no stopmove while walljumping
		}
		if( smove != 0.0f && !fmove && pm->playerState->pmove.knockback_time <= 0 ) {
			if( wishspeed > pm_wishspeed ) {
				wishspeed = pm_wishspeed;
			}
			accel = pm_strafebunnyaccel;
		}

		// Air control
		PM_Accelerate( wishdir, wishspeed, accel );
		if( !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) && pm->playerState->pmove.knockback_time <= 0 ) { // no air ctrl while wjing
			PM_Aircontrol( wishdir, wishspeed2 );
		}

		// add gravity
		pml.velocity.z -= GRAVITY * pml.frametime;
		PM_StepSlideMove();
	}
}

/*
* PM_GroundTrace
*
* If the player hull point one-quarter unit down is solid, the player is on ground
*/
static void PM_GroundTrace( trace_t *trace ) {
	Vec3 point = pml.origin - Vec3( 0.0f, 0.0f, 0.25f );
	pmove_gs->api.Trace( trace, pml.origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );
}

static bool PM_GoodPosition( Vec3 origin, trace_t *trace ) {
	if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		return true;
	}

	pmove_gs->api.Trace( trace, origin, pm->mins, pm->maxs, origin, pm->playerState->POVnum, pm->contentmask, 0 );

	return !trace->allsolid;
}

static void PM_UnstickPosition( trace_t *trace ) {
	ZoneScoped;

	Vec3 origin = pml.origin;

	// try all combinations
	for( int j = 0; j < 8; j++ ) {
		origin = pml.origin;

		origin.x += ( j & 1 ) ? -1.0f : 1.0f;
		origin.y += ( j & 2 ) ? -1.0f : 1.0f;
		origin.z += ( j & 4 ) ? -1.0f : 1.0f;

		if( PM_GoodPosition( origin, trace ) ) {
			pml.origin = origin;
			PM_GroundTrace( trace );
			return;
		}
	}

	// go back to the last position
	pml.origin = pml.previous_origin;
}

static void PM_CategorizePosition() {
	ZoneScoped;

	if( pml.velocity.z > 180 ) { // !!ZOID changed from 100 to 180 (ramp accel)
		pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = -1;
	}
	else {
		trace_t trace;

		// see if standing on something solid
		PM_GroundTrace( &trace );

		if( trace.allsolid ) {
			// try to unstick position
			PM_UnstickPosition( &trace );
		}

		pml.groundplane = trace.plane;
		pml.groundsurfFlags = trace.surfFlags;
		pml.groundcontents = trace.contents;

		if( trace.fraction == 1 || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			pm->groundentity = -1;
			pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		}
		else {
			pm->groundentity = trace.ent;

			// hitting solid ground will end a waterjump
			if( pm->playerState->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
				pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_TELEPORT );
				pm->playerState->pmove.pm_time = 0;
			}

			if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) ) { // just hit the ground
				pm->playerState->pmove.pm_flags |= PMF_ON_GROUND;
			}
		}

		if( pm->numtouch < MAXTOUCH && trace.fraction < 1.0f ) {
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

	//
	// get waterlevel, accounting for ducking
	//
	pm->waterlevel = 0;
	pm->watertype = 0;

	int sample2 = pm->playerState->viewheight - pm->mins.z;
	int sample1 = sample2 / 2;

	Vec3 point = pml.origin;
	point.z += pm->mins.z + 1.0f;
	int cont = pmove_gs->api.PointContents( point, 0 );

	if( cont & MASK_WATER ) {
		pm->watertype = cont;
		pm->waterlevel = 1;
		point.z = pml.origin.z + pm->mins.z + sample1;
		cont = pmove_gs->api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			pm->waterlevel = 2;
			point.z = pml.origin.z + pm->mins.z + sample2;
			cont = pmove_gs->api.PointContents( point, 0 );
			if( cont & MASK_WATER ) {
				pm->waterlevel = 3;
			}
		}
	}
}



static void PM_CheckSpecialMovement() {
	int cont;

	if( pm->playerState->pmove.pm_time ) {
		return;
	}

	pml.ladder = false;

	// check for ladder
	Vec3 spot = pml.origin + pml.flatforward;
	trace_t trace;
	pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->contentmask, 0 );
	if( trace.fraction < 1 && ( trace.surfFlags & SURF_LADDER ) ) {
		pml.ladder = true;
	}

	// check for water jump
	if( pm->waterlevel != 2 ) {
		return;
	}

	spot = pml.origin + pml.flatforward * 30;
	spot.z += 4;
	cont = pmove_gs->api.PointContents( spot, 0 );
	if( !( cont & CONTENTS_SOLID ) ) {
		return;
	}

	spot.z += 16;
	cont = pmove_gs->api.PointContents( spot, 0 );
	if( cont ) {
		return;
	}
	// jump out of water
	pml.velocity = pml.flatforward * 50;
	pml.velocity.z = 350;

	pm->playerState->pmove.pm_flags |= PMF_TIME_WATERJUMP;
	pm->playerState->pmove.pm_time = 255;
}

static void PM_FlyMove( bool doclip ) {
	trace_t trace;

	float maxspeed = pml.maxPlayerSpeed * 1.5f;

	if( pm->cmd.buttons & BUTTON_SPECIAL ) {
		maxspeed *= 2;
	}

	// friction
	float speed = Length( pml.velocity );
	if( speed < 1 ) {
		pml.velocity = Vec3( 0.0f );
	} else {
		float drop = 0;

		float friction = pm_friction * 1.5f; // extra friction
		float control = speed < pm_decelerate ? pm_decelerate : speed;
		drop += control * friction * pml.frametime;

		// scale the velocity
		float newspeed = Max2( 0.0f, speed - drop );
		pml.velocity *= newspeed / speed;
	}

	// accelerate
	float fmove = pml.forwardPush;
	float smove = pml.sidePush;

	if( pm->cmd.buttons & BUTTON_SPECIAL ) {
		fmove *= 2;
		smove *= 2;
	}

	Vec3 wishvel = pml.forward * fmove + pml.right * smove;
	wishvel.z += pml.upPush;

	Vec3 wishdir = wishvel;
	float wishspeed = Length( wishdir );
	wishdir = SafeNormalize( wishdir );

	// clamp to server defined max speed
	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		wishvel *= wishspeed;
		wishspeed = maxspeed;
	}

	float currentspeed = Dot( pml.velocity, wishdir );
	float addspeed = wishspeed - currentspeed;
	if( addspeed > 0 ) {
		float accelspeed = pm_accelerate * pml.frametime * wishspeed;
		if( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}

		pml.velocity += accelspeed * wishdir;
	}

	if( doclip ) {
		Vec3 end = pml.origin + pml.frametime * pml.velocity;

		pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );

		pml.origin = trace.endpos;
	} else {
		// move
		pml.origin += pml.velocity * pml.frametime;
	}
}

static void PM_AdjustBBox() {
	trace_t trace;

	if( pm->playerState->pmove.pm_type >= PM_FREEZE ) {
		pm->playerState->pmove.crouch_time = 0;
		pm->playerState->viewheight = 0;
		return;
	}

	if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		pm->playerState->pmove.crouch_time = 0;
		pm->playerState->viewheight = playerbox_stand_viewheight;
	}

	if( pml.upPush < 0 && ( pm->playerState->pmove.features & PMFEAT_CROUCH ) ) {
		if( pm->playerState->pmove.crouch_time == 0 && pmove_gs->gameState.round_state >= RoundState_Finished ) {
			pm->playerState->pmove.tbag_time = Min2( pm->playerState->pmove.tbag_time + TBAG_AMOUNT_PER_CROUCH, int( MAX_TBAG_TIME ) );

			if( pm->playerState->pmove.tbag_time >= TBAG_THRESHOLD ) {
				float frac = Unlerp( TBAG_THRESHOLD, pm->playerState->pmove.tbag_time, MAX_TBAG_TIME );
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_TBAG, frac * 255 );
			}
		}

		pm->playerState->pmove.crouch_time = Clamp( 0, pm->playerState->pmove.crouch_time + pm->cmd.msec, CROUCHTIME );

		float crouchFrac = (float)pm->playerState->pmove.crouch_time / (float)CROUCHTIME;
		pm->mins = pm->scale * Lerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins );
		pm->maxs = pm->scale * Lerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs );
		pm->playerState->viewheight = pm->scale.z * Lerp( playerbox_stand_viewheight, crouchFrac, playerbox_crouch_viewheight );

		// it's going down, so, no need of checking for head-chomping
		return;
	}

	// it's crouched, but not pressing the crouch button anymore, try to stand up
	if( pm->playerState->pmove.crouch_time != 0 ) {
		// find the current size
		float crouchFrac = (float)pm->playerState->pmove.crouch_time / (float)CROUCHTIME;
		Vec3 curmins = pm->scale * Lerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins );
		Vec3 curmaxs = pm->scale * Lerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs );
		float curviewheight = pm->scale.z * Lerp( playerbox_stand_viewheight, crouchFrac, playerbox_crouch_viewheight );

		if( !pm->cmd.msec ) { // no need to continue
			pm->mins = curmins;
			pm->maxs = curmaxs;
			pm->playerState->viewheight = curviewheight;
			return;
		}

		// find the desired size
		int newcrouchtime = Clamp( 0, pm->playerState->pmove.crouch_time - pm->cmd.msec, CROUCHTIME );
		crouchFrac = (float)newcrouchtime / (float)CROUCHTIME;
		Vec3 wishmins = pm->scale * Lerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins );
		Vec3 wishmaxs = pm->scale * Lerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs );
		float wishviewheight = pm->scale.z * Lerp( playerbox_stand_viewheight, crouchFrac, playerbox_crouch_viewheight );

		// check that the head is not blocked
		pmove_gs->api.Trace( &trace, pml.origin, wishmins, wishmaxs, pml.origin, pm->playerState->POVnum, pm->contentmask, 0 );
		if( trace.allsolid || trace.startsolid ) {
			// can't do the uncrouching, let the time alone and use old position
			pm->mins = curmins;
			pm->maxs = curmaxs;
			pm->playerState->viewheight = curviewheight;
			return;
		}

		// can do the uncrouching, use new position and update the time
		pm->playerState->pmove.crouch_time = newcrouchtime;
		pm->mins = wishmins;
		pm->maxs = wishmaxs;
		pm->playerState->viewheight = wishviewheight;
		return;
	}

	// the player is not crouching at all
	pm->mins = pm->scale * playerbox_stand_mins;
	pm->maxs = pm->scale * playerbox_stand_maxs;
	pm->playerState->viewheight = pm->scale.z * playerbox_stand_viewheight;
}

static void PM_UpdateDeltaAngles() {
	if( pmove_gs->module != GS_MODULE_GAME ) {
		return;
	}

	for( int i = 0; i < 3; i++ ) {
		pm->playerState->pmove.delta_angles[ i ] = ANGLE2SHORT( pm->playerState->viewangles[ i ] ) - pm->cmd.angles[ i ];
	}
}

static void PM_ApplyMouseAnglesClamp() {
	for( int i = 0; i < 3; i++ ) {
		s16 temp = pm->cmd.angles[i] + pm->playerState->pmove.delta_angles[i];
		if( i == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if( temp > (short)ANGLE2SHORT( 90 ) - 1 ) {
				pm->playerState->pmove.delta_angles[i] = ( ANGLE2SHORT( 90 ) - 1 ) - pm->cmd.angles[i];
				temp = (short)ANGLE2SHORT( 90 ) - 1;
			} else if( temp < (short)ANGLE2SHORT( -90 ) + 1 ) {
				pm->playerState->pmove.delta_angles[i] = ( ANGLE2SHORT( -90 ) + 1 ) - pm->cmd.angles[i];
				temp = (short)ANGLE2SHORT( -90 ) + 1;
			}
		}

		pm->playerState->viewangles[i] = SHORT2ANGLE( (short)temp );
	}

	AngleVectors( pm->playerState->viewangles, &pml.forward, &pml.right, &pml.up );

	pml.flatforward = Normalize( Vec3( pml.forward.xy(), 0.0f ) );
}

static void PM_BeginMove() {
	// clear results
	pm->numtouch = 0;
	pm->groundentity = -1;
	pm->watertype = 0;
	pm->waterlevel = 0;
	pm->step = 0;

	// clear all pmove local vars
	memset( &pml, 0, sizeof( pml ) );

	pml.origin = pm->playerState->pmove.origin;
	pml.velocity = pm->playerState->pmove.velocity;

	// save old org in case we get stuck
	pml.previous_origin = pm->playerState->pmove.origin;
}

static void PM_EndMove() {
	pm->playerState->pmove.origin = pml.origin;
	pm->playerState->pmove.velocity = pml.velocity;
}

void Pmove( const gs_state_t * gs, pmove_t *pmove ) {
	ZoneScoped;

	if( !pmove->playerState ) {
		return;
	}

	pm = pmove;
	pmove_gs = gs;

	SyncPlayerState * ps = pm->playerState;

	// clear all pmove local vars
	PM_BeginMove();

	float fallvelocity = Max2( 0.0f, -pml.velocity.z );

	pml.frametime = pm->cmd.msec * 0.001;

	pml.maxPlayerSpeed = ps->pmove.max_speed;
	if( pml.maxPlayerSpeed < 0 ) {
		pml.maxPlayerSpeed = DEFAULT_PLAYERSPEED;
	}

	pml.jumpPlayerSpeed = (float)ps->pmove.jump_speed * GRAVITY_COMPENSATE;
	pml.jumpPlayerSpeedWater = pml.jumpPlayerSpeed * 2;

	if( pml.jumpPlayerSpeed < 0 ) {
		pml.jumpPlayerSpeed = DEFAULT_JUMPSPEED * GRAVITY_COMPENSATE;
	}

	pml.dashPlayerSpeed = ps->pmove.dash_speed;
	if( pml.dashPlayerSpeed < 0 ) {
		pml.dashPlayerSpeed = DEFAULT_DASHSPEED;
	}

	pml.maxCrouchedSpeed = DEFAULT_CROUCHEDSPEED;
	if( pml.maxCrouchedSpeed > pml.maxPlayerSpeed * 0.5f ) {
		pml.maxCrouchedSpeed = pml.maxPlayerSpeed * 0.5f;
	}

	pml.jumpCallback = PM_DefaultJump;
	pml.specialCallback = PM_DefaultSpecial;

	// assign a contentmask for the movement type
	switch( ps->pmove.pm_type ) {
		case PM_FREEZE:
		case PM_CHASECAM:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				ps->pmove.pm_flags |= PMF_NO_PREDICTION;
			}
			pm->contentmask = 0;
			break;

		case PM_SPECTATOR:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				ps->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			pm->contentmask = MASK_DEADSOLID;
			break;

		default:
		case PM_NORMAL:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				ps->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			if( ps->pmove.features & PMFEAT_GHOSTMOVE ) {
				pm->contentmask = MASK_DEADSOLID;
			} else if( ps->pmove.features & PMFEAT_TEAMGHOST ) {
				int team = pmove_gs->api.GetEntityState( ps->POVnum, 0 )->team;
				pm->contentmask = team == TEAM_ALPHA ? MASK_ALPHAPLAYERSOLID : MASK_BETAPLAYERSOLID;
			} else {
				pm->contentmask = MASK_PLAYERSOLID;
			}
			break;
	}

	if( !GS_MatchPaused( pmove_gs ) ) {
		// drop timing counters
		if( ps->pmove.pm_time ) {
			int msec;

			msec = pm->cmd.msec >> 3;
			if( !msec ) {
				msec = 1;
			}
			if( msec >= ps->pmove.pm_time ) {
				ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_TELEPORT );
				ps->pmove.pm_time = 0;
			} else {
				ps->pmove.pm_time -= msec;
			}
		}

		ps->pmove.no_shooting_time = Max2( 0, ps->pmove.no_shooting_time - pm->cmd.msec );
		ps->pmove.knockback_time = Max2( 0, ps->pmove.knockback_time - pm->cmd.msec );
		ps->pmove.dash_time = Max2( 0, ps->pmove.dash_time - pm->cmd.msec );
		ps->pmove.walljump_time = Max2( 0, ps->pmove.walljump_time - pm->cmd.msec );
		ps->pmove.tbag_time = Max2( 0, ps->pmove.tbag_time - pm->cmd.msec );
		// crouch_time is handled at PM_AdjustBBox
	}

	pml.forwardPush = pm->cmd.forwardmove * SPEEDKEY / 127.0f;
	pml.sidePush = pm->cmd.sidemove * SPEEDKEY / 127.0f;
	pml.upPush = pm->cmd.upmove * SPEEDKEY / 127.0f;

	if( ps->pmove.pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( !GS_MatchPaused( pmove_gs ) ) {
			PM_ClearDash( pm );

			PM_ClearWallJump( pm );

			ps->pmove.knockback_time = 0;
			ps->pmove.crouch_time = 0;
			ps->pmove.tbag_time = 0;
			ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_TELEPORT | PMF_SPECIAL_HELD );

			PM_AdjustBBox();
		}

		if( ps->pmove.pm_type == PM_SPECTATOR ) {
			PM_ApplyMouseAnglesClamp();

			PM_FlyMove( false );
		} else {
			pml.forwardPush = 0;
			pml.sidePush = 0;
			pml.upPush = 0;
		}

		PM_EndMove();
		return;
	}

	PM_ApplyMouseAnglesClamp();

	// set mins, maxs, viewheight amd fov
	PM_AdjustBBox();

	// set groundentity, watertype, and waterlevel
	PM_CategorizePosition();

	int oldGroundEntity = pm->groundentity;

	PM_CheckSpecialMovement();

	if( ps->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		// teleport pause stays exactly in place
	} else if( ps->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		// waterjump has no control, but falls
		pml.velocity.z -= GRAVITY * pml.frametime;
		if( pml.velocity.z < 0 ) {
			// cancel as soon as we are falling down again
			ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_TELEPORT );
			ps->pmove.pm_time = 0;
		}

		PM_StepSlideMove();
	} else {
		// Kurim
		// Keep this order !
		pml.jumpCallback( pm, &pml, pmove_gs, ps );
		pml.specialCallback( pm, &pml, pmove_gs, ps );

		PM_Friction();

		if( pm->waterlevel >= 2 ) {
			PM_WaterMove();
		} else {
			Vec3 angles = ps->viewangles;
			if( angles.x > 180 ) {
				angles.x -= 360;
			}
			angles.x /= 3;

			AngleVectors( angles, &pml.forward, &pml.right, &pml.up );

			// hack to work when looking straight up and straight down
			if( pml.forward.z == -1.0f ) {
				pml.flatforward = pml.up;
			} else if( pml.forward.z == 1.0f ) {
				pml.flatforward = -pml.up;
			} else {
				pml.flatforward = pml.forward;
			}
			pml.flatforward.z = 0.0f;
			pml.flatforward = SafeNormalize( pml.flatforward );

			PM_Move();
		}
	}

	// set groundentity, watertype, and waterlevel for final spot
	PM_CategorizePosition();

	PM_EndMove();

	// Execute the triggers that are touched.
	// We check the entire path between the origin before the pmove and the
	// current origin to ensure no triggers are missed at high velocity.
	// Note that this method assumes the movement has been linear.
	pmove_gs->api.PMoveTouchTriggers( pm, pml.previous_origin );

	PM_UpdateDeltaAngles(); // in case some trigger action has moved the view angles (like teleported).

	// touching triggers may force groundentity off
	if( !( ps->pmove.pm_flags & PMF_ON_GROUND ) && pm->groundentity != -1 ) {
		pm->groundentity = -1;
		pml.velocity.z = 0;
	}

	if( pm->groundentity != -1 ) { // remove wall-jump and dash bits when touching ground
		// always keep the dash flag 50 msecs at least (to prevent being removed at the start of the dash)
		if( ps->pmove.dash_time < PM_DASHJUMP_TIMEDELAY - 50 ) {
			ps->pmove.pm_flags &= ~PMF_DASHING;
		}

		if( ps->pmove.walljump_time < PM_WALLJUMP_TIMEDELAY - 50 ) {
			PM_ClearWallJump( pm );
		}
	}

	if( oldGroundEntity == -1 ) {
		constexpr float min_fall_velocity = 200;
		constexpr float max_fall_velocity = 800;

		float fall_delta = fallvelocity - Max2( 0.0f, -pml.velocity.z );

		// scale velocity if in water
		if( pm->waterlevel == 3 ) {
			fall_delta = 0;
		}
		if( pm->waterlevel == 2 ) {
			fall_delta *= 0.25;
		}
		if( pm->waterlevel == 1 ) {
			fall_delta *= 0.5;
		}

		float frac = Unlerp01( min_fall_velocity, fall_delta, max_fall_velocity );
		if( frac > 0 ) {
			pmove_gs->api.PredictedEvent( ps->POVnum, EV_FALL, frac * 255 );
		}
	}
}
