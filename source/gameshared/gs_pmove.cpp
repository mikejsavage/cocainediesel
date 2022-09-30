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
#include "qcommon/qfiles.h"
#include "gameshared/movement.h"

static constexpr float pm_ladderspeed = 300.0f;

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

static pmove_t *pm;
static pml_t pml;
static const gs_state_t * pmove_gs;

// movement parameters

constexpr float default_friction = 16; //  ( initially 6 )
constexpr float default_accelerate = 16; // user intended acceleration when on ground or fly movement ( initially 10 )
constexpr float default_airaccelerate = 0.5f; // user intended aceleration when on air
constexpr float default_strafebunnyaccel = 60; // forward acceleration when strafe bunny hopping


constexpr float pm_decelerate = 16; // user intended deceleration when on ground
constexpr float pm_airdecelerate = 1.0f; // air deceleration (not +strafe one, just at normal moving).

constexpr float pm_specspeed = 450.0f;

// special movement parameters

constexpr float pm_aircontrol = 4440.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
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
	TracyZoneScoped;

	Vec3 planes[MAX_CLIP_PLANES];
	constexpr int maxmoves = 4;
	float remainingTime = pml.frametime;
	int blockedmask = 0;

	Vec3 old_velocity = pml.velocity;
	Vec3 last_valid_origin = pml.origin;

	if( pm->groundentity != -1 ) { // clip velocity to ground, no need to wait
		// if the ground is not horizontal (a ramp) clipping will slow the player down
		if( pml.groundplane.z == 1.0f && pml.velocity.z < 0.0f ) {
			pml.velocity.z = 0.0f;
		}
	}

	int numplanes = 0; // clean up planes count for checking

	for( int moves = 0; moves < maxmoves; moves++ ) {
		Vec3 end = pml.origin + pml.velocity * remainingTime;

		trace_t trace;
		pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->solid_mask, 0 );
		if( trace.fraction == 0.0f ) { // trapped into a solid
			pml.origin = last_valid_origin;
			return SLIDEMOVEFLAG_TRAPPED;
		}

		pml.origin = trace.endpos;
		last_valid_origin = trace.endpos;

		if( trace.fraction == 1 ) {
			break; // move done
		}

		// save touched entity for return output
		PM_AddTouchEnt( trace.ent );

		// at this point we are blocked but not trapped.

		blockedmask |= SLIDEMOVEFLAG_BLOCKED;
		if( trace.normal.z < SLIDEMOVE_PLANEINTERACT_EPSILON ) { // is it a vertical wall?
			blockedmask |= SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		remainingTime -= trace.fraction * remainingTime;

		// we got blocked, add the plane for sliding along it

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		{
			int i;
			for( i = 0; i < numplanes; i++ ) {
				if( Dot( trace.normal, planes[i] ) > ( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) ) {
					pml.velocity = trace.normal + pml.velocity;
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
		planes[numplanes] = trace.normal;
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

	return blockedmask;
}

/*
* PM_StepSlideMove
*
* Each intersection will try to step over the obstruction instead of
* sliding along it.
*/
static void PM_StepSlideMove() {
	TracyZoneScoped;

	trace_t trace;

	Vec3 start_o = pml.origin;
	Vec3 start_v = pml.velocity;

	int blocked = PM_SlideMove();

	Vec3 down_o = pml.origin;
	Vec3 down_v = pml.velocity;

	Vec3 up = start_o + Vec3( 0.0f, 0.0f, STEPSIZE );

	pmove_gs->api.Trace( &trace, up, pm->mins, pm->maxs, up, pm->playerState->POVnum, pm->solid_mask, 0 );

	// try sliding above
	pml.origin = up;
	pml.velocity = start_v;

	PM_SlideMove();

	// push down the final amount
	Vec3 down = pml.origin - Vec3( 0.0f, 0.0f, STEPSIZE );
	pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, down, pm->playerState->POVnum, pm->solid_mask, 0 );
	pml.origin = trace.endpos;

	up = pml.origin;

	// decide which one went farther
	float down_dist = LengthSquared( down_o.xy() - start_o.xy() );
	float up_dist = LengthSquared( up.xy() - start_o.xy() );

	if( down_dist >= up_dist || ( trace.fraction != 1.0f && !ISWALKABLEPLANE( trace.normal ) ) ) {
		pml.origin = down_o;
		pml.velocity = down_v;
		return;
	}

	// only add the stepping output when it was a vertical step (second case is at the exit of a ramp)
	if( ( blocked & SLIDEMOVEFLAG_WALL_BLOCKED ) || trace.normal.z == 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
		pm->step = pml.origin.z - pml.previous_origin.z;
	}

	// Preserve speed when sliding up ramps
	float hspeed = Length( start_v.xy() );
	if( hspeed && ISWALKABLEPLANE( trace.normal ) ) {
		if( trace.normal.z >= 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
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
			float control = speed < pm_decelerate ? pm_decelerate : speed;
			drop += control * pml.friction * pml.frametime;
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
	pml.velocity = SafeNormalize( pml.velocity );

	float dot = Dot( pml.velocity, wishdir );
	float k = pm_aircontrol * dot * dot * pml.frametime;

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
	if( pml.ladder == Ladder_On && Abs( pml.velocity.z ) <= pm_ladderspeed ) {
		if( pm->cmd.buttons & Button_Ability1 ) { //jump
			wishvel.z = pm_ladderspeed;
		}
		else if( pml.forwardPush > 0 ) {
			wishvel.z = Lerp( -float( pm_ladderspeed ), Unlerp01( 15.0f, pm->playerState->viewangles[PITCH], -15.0f ), float( pm_ladderspeed ) );
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

static void PM_Move() {
	TracyZoneScoped;

	float fmove = pml.forwardPush;
	float smove = pml.sidePush;

	Vec3 wishvel = pml.forward * fmove + pml.right * smove;
	wishvel.z = 0;

	wishvel = PM_LadderMove( wishvel );

	Vec3 wishdir = wishvel;
	float wishspeed = Length( wishdir );
	wishdir = SafeNormalize( wishdir );

	// clamp to server defined max speed

	float maxspeed = pml.maxSpeed;

	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		wishvel *= wishspeed;
		wishspeed = maxspeed;
	}

	if( pml.ladder ) {
		PM_Accelerate( wishdir, wishspeed, pml.groundAccel );

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

		PM_Accelerate( wishdir, wishspeed, pml.groundAccel );

		pml.velocity.z = Min2( 0.0f, pml.velocity.z );

		if( pml.velocity.xy() == Vec2( 0.0f ) ) {
			return;
		}

		PM_StepSlideMove();
	}
	else {
		// Air Control
		float wishspeed2 = wishspeed;
		float accel = 0.0f;

		if( Dot( pml.velocity, wishdir ) < 0 && pm->playerState->pmove.knockback_time <= 0 ) {
			accel = pm_airdecelerate;
		} else {
			accel = pml.airAccel;
		}

		if( smove != 0.0f && !fmove && pm->playerState->pmove.knockback_time <= 0 ) {
			if( wishspeed > pm_wishspeed ) {
				wishspeed = pm_wishspeed;
			}
			accel = pml.strafeBunnyAccel;
		}

		// Air control
		PM_Accelerate( wishdir, wishspeed, accel );
		if( pm->playerState->pmove.knockback_time <= 0 ) { // no air ctrl while wjing
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
	pmove_gs->api.Trace( trace, pml.origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->solid_mask, 0 );
}

static bool PM_GoodPosition( Vec3 origin, trace_t *trace ) {
	pmove_gs->api.Trace( trace, origin, pm->mins, pm->maxs, origin, pm->playerState->POVnum, pm->solid_mask, 0 );
	return trace->ent == -1;
}

static void PM_UnstickPosition( trace_t *trace ) {
	TracyZoneScoped;

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
	TracyZoneScoped;

	if( pml.velocity.z > 180 ) { // !!ZOID changed from 100 to 180 (ramp accel)
		pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = -1;
	}
	else {
		trace_t trace;

		// see if standing on something solid
		PM_GroundTrace( &trace );

		if( trace.fraction == 0.0f ) {
			// try to unstick position
			PM_UnstickPosition( &trace );
		}

		pml.groundplane = trace.normal;

		if( trace.fraction == 1 || !ISWALKABLEPLANE( trace.normal ) ) {
			pm->groundentity = -1;
			pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		}
		else {
			pm->groundentity = trace.ent;

			if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) ) { // just hit the ground
				pm->playerState->pmove.pm_flags |= PMF_ON_GROUND;
			}
		}

		if( pm->numtouch < MAXTOUCH && trace.fraction < 1.0f ) {
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}
}

static void PM_CheckSpecialMovement() {
	pml.ladder = Ladder_Off;

	// check for ladder
	Vec3 spot = pml.origin + pml.forward;
	trace_t trace;
	pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->solid_mask, 0 );
	if( trace.fraction < 1 && ( trace.solidity & Solid_Ladder ) ) {
		pml.ladder = Ladder_On;
	}
}

static void PM_FlyMove() {
	// accelerate
	float special = 1 + int( ( pm->cmd.buttons & Button_Attack2 ) != 0 );
	float fmove = pm->cmd.forwardmove * special / 127.0f;
	float smove = pm->cmd.sidemove * special / 127.0f;
	float umove = (int( (pm->cmd.buttons & Button_Ability1) != 0 ) - int( (pm->cmd.buttons & Button_Ability2) != 0 )) * special;

	Vec3 wishdir = pml.forward * fmove + pml.right * smove;
	wishdir.z += umove;

	float wishspeed = Length( wishdir );
	wishdir = SafeNormalize( wishdir );

	pml.velocity = pm_specspeed * wishspeed * wishdir;
	pml.origin += pml.velocity * pml.frametime;
}

static void PM_AdjustBBox() {
	if( pm->playerState->pmove.pm_type >= PM_FREEZE ) {
		pm->playerState->viewheight = 0;
		return;
	}

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

	pml.forward = Normalize( Vec3( pml.forward.xy(), 0.0f ) );
}

static void PM_BeginMove() {
	// clear results
	pm->numtouch = 0;
	pm->groundentity = -1;
	pm->step = 0;

	// clear all pmove local vars
	memset( &pml, 0, sizeof( pml ) );

	pml.origin = pm->playerState->pmove.origin;
	pml.velocity = pm->playerState->pmove.velocity;

	// save old org in case we get stuck
	pml.previous_origin = pm->playerState->pmove.origin;

	pml.frametime = pm->cmd.msec * 0.001;
	pml.forwardPush = pm->cmd.forwardmove / 127.0f;
	pml.sidePush = pm->cmd.sidemove / 127.0f;

	pml.groundAccel = default_accelerate;
	pml.airAccel = default_airaccelerate;
	pml.strafeBunnyAccel = default_strafebunnyaccel;

	pml.friction = default_friction;
}

static void PM_EndMove() {
	pm->playerState->pmove.origin = pml.origin;
	pm->playerState->pmove.velocity = pml.velocity;
}

static void PM_InitPerk() {
	switch( pm->playerState->perk ) {
		case Perk_Hooligan: PM_HooliganInit( pm, &pml ); break;
		case Perk_Midget: PM_MidgetInit( pm, &pml ); break;
		case Perk_Jetpack: PM_JetpackInit( pm, &pml ); break;
		case Perk_Ninja: PM_NinjaInit( pm, &pml ); break;
		case Perk_Boomer: PM_BoomerInit( pm, &pml ); break;
		default: PM_NinjaInit( pm, &pml ); break;
	}
}

void Pmove( const gs_state_t * gs, pmove_t * pmove ) {
	TracyZoneScoped;

	if( !pmove->playerState ) {
		return;
	}

	pm = pmove;
	pmove_gs = gs;

	SyncPlayerState * ps = pm->playerState;

	// clear all pmove local vars
	PM_BeginMove();

	pml_t old_pml = pml;

	float fallvelocity = Max2( 0.0f, -pml.velocity.z );

	PM_InitPerk();

	// assign a solidity for the movement type
	switch( ps->pmove.pm_type ) {
		case PM_FREEZE:
		case PM_CHASECAM:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				ps->pmove.pm_flags |= PMF_NO_PREDICTION;
			}
			pm->solid_mask = Solid_NotSolid;
			break;

		case PM_SPECTATOR:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				ps->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			pm->solid_mask = Solid_NotSolid;
			break;

		default:
		case PM_NORMAL:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				ps->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			pm->solid_mask = Solid_Solid;
			break;
	}

	if( !GS_MatchPaused( pmove_gs ) ) {
		ps->pmove.no_shooting_time = Max2( 0, ps->pmove.no_shooting_time - pm->cmd.msec );
		ps->pmove.knockback_time = Max2( 0, ps->pmove.knockback_time - pm->cmd.msec );
	}

	if( ps->pmove.pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( !GS_MatchPaused( pmove_gs ) ) {
			ps->pmove.knockback_time = 0;

			PM_AdjustBBox();
		}

		if( ps->pmove.pm_type == PM_SPECTATOR ) {
			PM_ApplyMouseAnglesClamp();

			PM_FlyMove();
		} else {
			pml.forwardPush = 0;
			pml.sidePush = 0;
		}

		PM_EndMove();
		return;
	}

	PM_ApplyMouseAnglesClamp();

	// set mins, maxs, viewheight amd fov
	PM_AdjustBBox();

	// set groundentity
	PM_CategorizePosition();

	int oldGroundEntity = pm->groundentity;

	PM_CheckSpecialMovement();

	if( pm->groundentity != -1 ) {
		pm->playerState->last_touch.entnum = 0;
		pm->playerState->last_touch.type = Weapon_None;
	}

	// Kurim
	// Keep this order !
	if( ps->pmove.pm_type == PM_NORMAL && ( pm->playerState->pmove.features & PMFEAT_ABILITIES ) ) {
		pml.ability1Callback( pm, &pml, pmove_gs, pm->playerState, pm->cmd.buttons & Button_Ability1 );
		pml.ability2Callback( pm, &pml, pmove_gs, pm->playerState, pm->cmd.buttons & Button_Ability2 );
	}

	PM_Friction();

	Vec3 angles = ps->viewangles;
	if( angles.x > 180 ) {
		angles.x -= 360;
	}
	angles.x /= 3;

	AngleVectors( angles, &pml.forward, &pml.right, &pml.up );

	// hack to work when looking straight up and straight down
	if( pml.forward.z == -1.0f ) {
		pml.forward = pml.up;
	} else if( pml.forward.z == 1.0f ) {
		pml.forward = -pml.up;
	} else {
		pml.forward = pml.forward;
	}
	pml.forward.z = 0.0f;
	pml.forward = SafeNormalize( pml.forward );
	PM_Move();

	// set groundentity for final spot
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

	if( oldGroundEntity == -1 && pm->groundentity != -1 ) {
		constexpr float min_fall_velocity = 200;
		constexpr float max_fall_velocity = 800;

		float fall_delta = fallvelocity - Max2( 0.0f, -pml.velocity.z );

		float frac = Unlerp01( min_fall_velocity, fall_delta, max_fall_velocity );
		if( frac > 0 ) {
			pmove_gs->api.PredictedEvent( ps->POVnum, EV_FALL, frac * 255 );
		}
	}
}
