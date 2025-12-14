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
#include "qcommon/array.h"
#include "gameshared/collision.h"
#include "gameshared/intersection_tests.h"
#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"

static constexpr float pm_ladderspeed = 300.0f;

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

static pmove_t *pm;
static pml_t pml;
static const gs_state_t * pmove_gs;

// movement parameters

constexpr float default_strafebunnyaccel = 60; // forward acceleration when strafe bunny hopping


constexpr float pm_decelerate = 16; // user intended deceleration when on ground
constexpr float pm_airdecelerate = 1.0f; // air deceleration (not +strafe one, just at normal moving).

constexpr float pm_specspeed = 450.0f;

// special movement parameters

constexpr float pm_aircontrol = 4440.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
constexpr float pm_wishspeed = 30;

constexpr float SLIDEMOVE_PLANEINTERACT_EPSILON = 0.05f;

#define SLIDEMOVEFLAG_BLOCKED       	( 1 << 0 )   // it was blocked at some point, doesn't mean it didn't slide along the blocking object
#define SLIDEMOVEFLAG_TRAPPED       	( 1 << 1 )
#define SLIDEMOVEFLAG_WALL_BLOCKED  	( 1 << 2 )

/*
* PM_SlideMove
*
* Returns a new origin, velocity, and contact entity
* Does not modify any world state?
*/

static void PM_AddTouchEnt( int entNum, pmove_t * my_pm ) {
	if( my_pm->numtouch >= ARRAY_COUNT( my_pm->touchents ) ) {
		return;
	}

	// see if it is already added
	for( int i = 0; i < my_pm->numtouch; i++ ) {
		if( my_pm->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	my_pm->touchents[my_pm->numtouch] = entNum;
	my_pm->numtouch++;
}

static int PM_SlideMove( const gs_module_api_t & api, pmove_t * my_pm, pml_t * my_pml ) {
	constexpr size_t MAX_CLIP_PLANES = 5;

	TracyZoneScoped;

	Vec3 planes[MAX_CLIP_PLANES];
	constexpr int maxmoves = 4;
	float remainingTime = my_pml->frametime;
	int blockedmask = 0;

	Vec3 last_valid_origin = my_pml->origin;

	if( my_pm->groundentity != -1 ) { // clip velocity to ground, no need to wait
		// if the ground is not horizontal (a ramp) clipping will slow the player down
		if( my_pml->groundplane.z == 1.0f && my_pml->velocity.z < 0.0f ) {
			my_pml->velocity.z = 0.0f;
		}
	}

	int numplanes = 0; // clean up planes count for checking

	for( int moves = 0; moves < maxmoves; moves++ ) {
		Vec3 end = my_pml->origin + my_pml->velocity * remainingTime;

		trace_t trace = api.Trace( my_pml->origin, my_pm->bounds, end, my_pm->playerState->POVnum, my_pm->solid_mask, 0 );
		if( trace.GotNowhere() ) { // trapped into a solid
			my_pml->origin = last_valid_origin;
			return SLIDEMOVEFLAG_TRAPPED;
		}

		if( trace.GotSomewhere() ) {
			my_pml->origin = trace.endpos;
			last_valid_origin = trace.endpos;
		}

		if( trace.HitNothing() ) {
			break; // move done
		}

		// save touched entity for return output
		PM_AddTouchEnt( trace.ent, my_pm );

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
					my_pml->velocity = trace.normal + my_pml->velocity;
					break;
				}
			}
			if( i < numplanes ) { // found a repeated plane, so don't add it, just repeat the trace
				continue;
			}
		}

		// security check: we can't store more planes
		if( numplanes >= MAX_CLIP_PLANES ) {
			my_pml->velocity = Vec3( 0.0f );
			return SLIDEMOVEFLAG_TRAPPED;
		}

		// put the actual plane in the list
		planes[numplanes] = trace.normal;
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//

		for( int i = 0; i < numplanes; i++ ) {
			if( Dot( my_pml->velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) { // would not touch it
				continue;
			}

			my_pml->velocity = GS_ClipVelocity( my_pml->velocity, planes[i] );
			// see if we enter a second plane
			for( int j = 0; j < numplanes; j++ ) {
				if( j == i ) { // it's the same plane
					continue;
				}
				if( Dot( my_pml->velocity, planes[j] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue; // not with this one
				}

				//there was a second one. Try to slide along it too
				my_pml->velocity = GS_ClipVelocity( my_pml->velocity, planes[j] );

				// check if the slide sent it back to the first plane
				if( Dot( my_pml->velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue;
				}

				// bad luck: slide the original velocity along the crease
				Vec3 dir = SafeNormalize( Cross( planes[i], planes[j] ) );
				float value = Dot( dir, my_pml->velocity );
				my_pml->velocity = dir * value;

				// check if there is a third plane, in that case we're trapped
				for( int k = 0; k < numplanes; k++ ) {
					if( j == k || i == k ) { // it's the same plane
						continue;
					}
					if( Dot( my_pml->velocity, planes[k] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
						continue; // not with this one
					}
					my_pml->velocity = Vec3( 0.0f );
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

	Vec3 initial_origin = pml.origin;
	Vec3 initial_velocity = pml.velocity;

	int blocked = PM_SlideMove( pmove_gs->api, pm, &pml );
	if( blocked == 0 )
		return;

	// try to step over the obstruction, try n candidates up to STEPSIZE height and take the one that goes the furthest
	// NOTE(mike 20250913): we need to try multiple candidates so you can walk through doors that have a step
	struct Candidate {
		Vec3 endpos;
		Vec3 velocity;
		Vec3 normal;
	};

	constexpr size_t num_step_over_candidates = 4;
	BoundedDynamicArray< Candidate, num_step_over_candidates + 1 > candidates = { };
	candidates.must_add( { pml.origin, pml.velocity } );

	for( size_t i = 0; i < num_step_over_candidates; i++ ) {
		float step_size = ( float( i + 1 ) / float( num_step_over_candidates ) ) * STEPSIZE;

		trace_t up_trace = pmove_gs->api.Trace( initial_origin, pm->bounds, initial_origin + Vec3::Z( step_size ), pm->playerState->POVnum, pm->solid_mask, 0 );
		if( i == 0 && up_trace.GotNowhere() )
			break;

		pml.origin = up_trace.endpos;
		pml.velocity = initial_velocity;

		PM_SlideMove( pmove_gs->api, pm, &pml );

		trace_t down_trace = pmove_gs->api.Trace( pml.origin, pm->bounds, pml.origin - Vec3::Z( step_size ), pm->playerState->POVnum, pm->solid_mask, 0 );
		if( !down_trace.HitSomething() || ISWALKABLEPLANE( down_trace.normal ) ) {
			candidates.must_add( {
				.endpos = down_trace.endpos,
				.velocity = pml.velocity,
				.normal = down_trace.normal,
			} );
		}

		if( up_trace.HitSomething() )
			break;
	}

	size_t best_candidate = 0;
	float best_sqdistance = LengthSquared( candidates[ 0 ].endpos - initial_origin );
	for( size_t i = 1; i < candidates.size(); i++ ) {
		float d = LengthSquared( candidates[ i ].endpos - initial_origin );
		if( d > best_sqdistance ) {
			best_candidate = i;
			best_sqdistance = d;
		}
	}

	pml.origin = candidates[ best_candidate ].endpos;
	pml.velocity = candidates[ best_candidate ].velocity;

	if( best_candidate == 0 ) {
		return;
	}

	// only add the stepping output when it was a vertical step (second case is at the exit of a ramp)
	Vec3 normal = candidates[ best_candidate ].normal;
	if( ( blocked & SLIDEMOVEFLAG_WALL_BLOCKED ) || normal.z == 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
		pm->step = pml.origin.z - pml.previous_origin.z;
	}

	// Preserve speed when sliding up ramps
	float hspeed = Length( initial_velocity.xy() );
	if( hspeed && ISWALKABLEPLANE( normal ) ) {
		if( normal.z >= 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
			pml.velocity = initial_velocity;
		} else {
			Normalize2D( &pml.velocity );
			pml.velocity = Vec3( pml.velocity.xy() * hspeed, pml.velocity.z );
		}
	}

	// wsw : jal : The following line is what produces the ramp sliding.

	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity.z = candidates[ 0 ].velocity.z;
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
		if( pm->playerState->pmove.no_friction_time <= 0 ) {
			float control = speed < pm_decelerate ? pm_decelerate : speed;
			drop += control * pml.groundFriction * pml.frametime;
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
	if( pm->playerState->pmove.no_friction_time > 0 )
		return;

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
			wishvel.z = Lerp( -float( pm_ladderspeed ), Unlerp01( 15.0f, pm->playerState->viewangles.pitch, -15.0f ), float( pm_ladderspeed ) );
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
	float zoom_maxspeed = pml.maxSpeed * GetWeaponDefProperties( pm->playerState->weapon )->zoom_movement_speed;
	maxspeed = Lerp( maxspeed, float( pm->playerState->zoom_time ) / float( ZOOMTIME ), zoom_maxspeed );


	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		wishvel *= wishspeed;
		wishspeed = maxspeed;
	}

	if( pml.ladder != Ladder_Off ) {
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

		if( Dot( pml.velocity, wishdir ) < 0 && pm->playerState->pmove.no_friction_time <= 0 ) {
			accel = pm_airdecelerate;
		} else {
			accel = pml.airAccel;
		}

		if( smove != 0.0f && !fmove && pm->playerState->pmove.no_friction_time <= 0 ) {
			if( wishspeed > pm_wishspeed ) {
				wishspeed = pm_wishspeed;
			}
			accel = pml.strafeBunnyAccel;
		}

		// Air control
		PM_Accelerate( wishdir, wishspeed, accel );
		if( pm->playerState->pmove.no_friction_time <= 0 ) {
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
static trace_t PM_GroundTrace() {
	Vec3 point = pml.origin - Vec3::Z( 0.25f );
	return pmove_gs->api.Trace( pml.origin, pm->bounds, point, pm->playerState->POVnum, pm->solid_mask, 0 );
}

static Optional< trace_t > PM_UnstickPosition() {
	TracyZoneScoped;

	Vec3 origin = pml.origin;

	// try all combinations
	for( int j = 0; j < 8; j++ ) {
		origin = pml.origin;

		origin.x += ( j & 1 ) ? -1.0f : 1.0f;
		origin.y += ( j & 2 ) ? -1.0f : 1.0f;
		origin.z += ( j & 4 ) ? -1.0f : 1.0f;

		trace_t inside_solid_trace = pmove_gs->api.Trace( origin, pm->bounds, origin, pm->playerState->POVnum, pm->solid_mask, 0 );
		if( inside_solid_trace.GotSomewhere() ) {
			pml.origin = origin;
			return PM_GroundTrace();
		}
	}

	// go back to the last position
	pml.origin = pml.previous_origin;
	return NONE;
}

static void PM_CategorizePosition() {
	TracyZoneScoped;

	if( pml.velocity.z > 180 ) { // !!ZOID changed from 100 to 180 (ramp accel)
		pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = -1;
	}
	else {
		// see if standing on something solid
		trace_t trace = PM_GroundTrace();

		if( trace.GotNowhere() ) {
			// try to unstick position
			trace = Default( PM_UnstickPosition(), trace );
		}

		pml.groundplane = trace.normal;

		if( trace.HitNothing() || !ISWALKABLEPLANE( trace.normal ) ) {
			pm->groundentity = -1;
			pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		}
		else {
			pm->groundentity = trace.ent;

			if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) ) { // just hit the ground
				pm->playerState->pmove.pm_flags |= PMF_ON_GROUND;
			}
		}

		if( pm->numtouch < ARRAY_COUNT( pm->touchents ) && trace.HitSomething() ) {
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}
}

static void PM_CheckSpecialMovement() {
	pml.ladder = Ladder_Off;

	// check for ladder
	Vec3 spot = pml.origin + pml.forward;
	trace_t trace = pmove_gs->api.Trace( pml.origin, pm->bounds, spot, pm->playerState->POVnum, pm->solid_mask, 0 );
	if( trace.HitSomething() && ( trace.solidity & Solid_Ladder ) ) {
		pml.ladder = Ladder_On;
	}
}

static void PM_FlyMove() {
	// accelerate
	float special = 1 + int( ( pm->cmd.buttons & Button_Attack2 ) != 0 );
	Vec3 fwd, right;
	AngleVectors( pm->playerState->viewangles, &fwd, &right, NULL );

	Vec3 wishdir = pml.forwardPush * fwd + pml.sidePush * right;
	wishdir = SafeNormalize( wishdir );

	pml.velocity = wishdir * pm_specspeed * special;
	pml.velocity.z += (int( (pm->cmd.buttons & Button_Ability1) != 0 ) - int( (pm->cmd.buttons & Button_Ability2) != 0 )) * pm_specspeed * special;

	Vec3 origin = pml.origin;
	Vec3 velocity = pml.velocity;

	int blocked = PM_SlideMove( pmove_gs->api, pm, &pml );

	if( blocked & SLIDEMOVEFLAG_TRAPPED ) { //noclip if we're blocked
		pml.origin = origin + velocity * pml.frametime;
	} else {
		pml.origin = origin;
		pml.velocity = velocity;
		PM_StepSlideMove();
	}
}

static void PM_AdjustBBox() {
	if( pm->playerState->pmove.pm_type >= PM_FREEZE ) {
		pm->playerState->viewheight = 0;
		return;
	}

	pm->bounds = playerbox_stand * pm->scale;
	pm->playerState->viewheight = pm->scale.z * playerbox_stand_viewheight;
}

static void PM_UpdateDeltaAngles() {
	if( pmove_gs->module != GS_MODULE_GAME ) {
		return;
	}

	pm->playerState->pmove.angles = EulerDegrees3( pm->cmd.angles );
}

static void PM_ApplyMouseAnglesClamp() {
	pm->playerState->viewangles = EulerDegrees3( pm->cmd.angles );
	pm->playerState->viewangles.pitch = Clamp( -90.0f, pm->playerState->viewangles.pitch, 90.0f );

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

	pml.strafeBunnyAccel = default_strafebunnyaccel;
}

static void PM_EndMove() {
	pm->playerState->pmove.origin = pml.origin;
	pm->playerState->pmove.velocity = pml.velocity;
}

static void PM_InitPerk() {
	switch( pm->playerState->perk ) {
		case Perk_Hooligan: PM_HooliganInit( pm, &pml ); break;
		case Perk_Midget: PM_MidgetInit( pm, &pml ); break;
		case Perk_Wheel: PM_WheelInit( pm, &pml ); break;
		case Perk_Jetpack: PM_JetpackInit( pm, &pml ); break;
		case Perk_Ninja: PM_NinjaInit( pm, &pml ); break;
		default: PM_BoomerInit( pm, &pml ); break;
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

			SolidBits inverse_team_solidity = SolidBits( ~( Solid_PlayerTeamOne << ( pm->team - Team_One ) ) & SolidMask_Player );
			pm->solid_mask = Solid_PlayerClip | inverse_team_solidity;
			break;
	}

	if( !pmove_gs->gameState.paused ) {
		ps->pmove.no_shooting_time = Max2( 0, ps->pmove.no_shooting_time - pm->cmd.msec );
		ps->pmove.no_friction_time = Max2( 0, ps->pmove.no_friction_time - pm->cmd.msec );
	}

	if( ps->pmove.pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( !pmove_gs->gameState.paused ) {
			ps->pmove.no_friction_time = 0;

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

	EulerDegrees3 angles = ps->viewangles;
	angles.pitch = AngleNormalize180( angles.pitch ) / 3.0f;
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

bool SweptShapeVsMapModel( const MapData * map, const MapModel * model, Ray ray, const Shape & shape, SolidBits solid_mask, Intersection * intersection );
trace_t MakeTrace( const Ray & ray, const Shape & shape, const Intersection & intersection, const SyncEntityState * ent );

static trace_t TestTrace( Vec3 start, MinMax3 bounds, Vec3 end, int ignore, SolidBits solid_mask, int timeDelta ) {
	solid_mask = SolidBits( 1 );

	Ray ray = MakeRayStartEnd( start, end );

	Shape aabb = {
		.type = ShapeType_AABB,
		.aabb = ToCenterExtents( bounds ),
	};

	/*Plane plane = {
		.normal = Vec3( 1.0f, 0.0f, 0.0f ),
		.distance = 0.0f,
	};*/

	BoundedDynamicArray< u32, 1 > brush_indices = { 0 };

	BoundedDynamicArray< Plane, 6 > brush_planes = {
		Plane { .normal = Vec3( 1.0f, 0.0f, 0.0f ), .distance = 0.0f },
		Plane { .normal = Vec3( -1.0f, 0.0f, 0.0f ), .distance = 1.0f },
		Plane { .normal = Vec3( 0.0f, 1.0f, 0.0f ), .distance = 1000.0f },
		Plane { .normal = Vec3( 0.0f, -1.0f, 0.0f ), .distance = 1000.0f },
		Plane { .normal = Vec3( 0.0f, 0.0f, 1.0f ), .distance = 1000.0f },
		Plane { .normal = Vec3( 0.0f, 0.0f, -1.0f ), .distance = 1000.0f },
	};

	MapBrush brush = {
		.bounds = MinMax3( Vec3( -1.0f, -1000.0f, -1000.0f ), Vec3( 0.0f, 1000.0f, 1000.0f ) ),
		.first_plane = 0,
		.num_planes = 6,
		.solidity = solid_mask,
	};

	MapKDTreeNode leaf = {
		.leaf = {
			.first_brush = 0,
			.is_leaf = MapKDTreeNode::LEAF,
			.num_brushes = 1,
		},
	};

	MapModel model = {
		.bounds = brush.bounds,
		.solidity = solid_mask,
		.root_node = 0,
	};

	MapData map = {
		.models = Span< const MapModel >( &model, 1 ),
		.nodes = Span< const MapKDTreeNode >( &leaf, 1 ),
		.brushes = Span< const MapBrush >( &brush, 1 ),
		.brush_indices = brush_indices.span(),
		.brush_planes = brush_planes.span(),
	};

	Intersection intersection;
	bool hit = SweptShapeVsMapModel( &map, &model, ray, aabb, solid_mask, &intersection );

	SyncEntityState ent = { };
	return hit ? MakeTrace( ray, aabb, intersection, &ent ) : MakeMissedTrace( ray );
}

TEST( "Trivial miss slidemove" ) {
	SyncPlayerState player = { };

	pmove_t test_pm = {
		.playerState = &player,
		.bounds = MinMax3( Vec3( -1.0f ), Vec3( 1.0f ) ),
		.groundentity = -1,
	};

	pml_t test_pml = {
		.origin = Vec3( 10.0f, 0.0f, 0.0f ),
		.velocity = Vec3( -5.0f, 0.0f, 0.0f ),
		.frametime = 1.0f,
	};

	gs_module_api_t api = { .Trace = TestTrace };

	int blocked = PM_SlideMove( api, &test_pm, &test_pml );

	Vec3 expected_endpos = Vec3( 5.0f, 0.0f, 0.0f );
	Vec3 expected_velocity = Vec3( -5.0f, 0.0f, 0.0f );

	return blocked == 0 && Length( expected_endpos - test_pml.origin ) < 0.05f && Length( expected_velocity - test_pml.velocity ) < 0.01f;
}

TEST( "Directly into plane slidemove" ) {
	SyncPlayerState player = { };

	pmove_t test_pm = {
		.playerState = &player,
		.bounds = MinMax3( Vec3( -1.0f ), Vec3( 1.0f ) ),
		.groundentity = -1,
	};

	pml_t test_pml = {
		.origin = Vec3( 10.0f, 0.0f, 0.0f ),
		.velocity = Vec3( -20.0f, 0.0f, 0.0f ),
		.frametime = 1.0f,
	};

	gs_module_api_t api = { .Trace = TestTrace };

	int blocked = PM_SlideMove( api, &test_pm, &test_pml );

	Vec3 expected_endpos = Vec3( 1.0f, 0.0f, 0.0f );
	Vec3 expected_velocity = Vec3( 0.0f );

	return blocked != 0 && Length( expected_endpos - test_pml.origin ) < 0.05f && Length( expected_velocity - test_pml.velocity ) < 0.01f;
}

// TODO: contact is set incorrectly for this test
// TODO: "modify original_velocity so it parallels all of the clip planes" loop seems busted
// TODO: the backoff in MakeTrace steps back spatially but not temporarily so slidey slidemoves are slightly shorter than they should be
TEST( "Single plane slide slidemove" ) {
	SyncPlayerState player = { };

	pmove_t test_pm = {
		.playerState = &player,
		.bounds = MinMax3( Vec3( -1.0f ), Vec3( 1.0f ) ),
		.groundentity = -1,
	};

	pml_t test_pml = {
		.origin = Vec3( 10.0f, 0.0f, 0.0f ),
		.velocity = Vec3( -20.0f, 0.0f, 20.0f ),
		.frametime = 1.0f,
	};

	gs_module_api_t api = { .Trace = TestTrace };

	int blocked = PM_SlideMove( api, &test_pm, &test_pml );

	Vec3 expected_endpos = Vec3( 1.0f, 0.0f, 20.0f );
	Vec3 expected_velocity = Vec3( 0.0f, 0.0f, 20.0f );

	return blocked != 0 && Length( expected_endpos - test_pml.origin ) < 0.05f && Length( expected_velocity - test_pml.velocity ) < 0.01f;
}

TEST( "Many frame single plane slide slidemove" ) {
	SyncPlayerState player = { };

	pmove_t test_pm = {
		.playerState = &player,
		.bounds = MinMax3( Vec3( -1.0f ), Vec3( 1.0f ) ),
		.groundentity = -1,
	};

	pml_t test_pml = {
		.origin = Vec3( 10.0f, 0.0f, 0.0f ),
		.velocity = Vec3( -20.0f, 0.0f, 20.0f ),
		.frametime = 0.05f,
	};

	gs_module_api_t api = { .Trace = TestTrace };

	int num_collisions = 0;
	for( int i = 0; i < 20; i++ ) {
		int blocked = PM_SlideMove( api, &test_pm, &test_pml );
		if( blocked != 0 ) {
			num_collisions++;
		}
	}

	Vec3 expected_endpos = Vec3( 1.0f, 0.0f, 20.0f );
	Vec3 expected_velocity = Vec3( 0.0f, 0.0f, 20.0f );

	return num_collisions == 1 && Length( expected_endpos - test_pml.origin ) < 0.05f && Length( expected_velocity - test_pml.velocity ) < 0.01f;
}

TEST( "Many frame single plane holding direction slide slidemove" ) {
	SyncPlayerState player = { };

	pmove_t test_pm = {
		.playerState = &player,
		.bounds = MinMax3( Vec3( -1.0f ), Vec3( 1.0f ) ),
		.groundentity = -1,
	};

	pml_t test_pml = {
		.origin = Vec3( 10.0f, 0.0f, 0.0f ),
		.frametime = 0.05f,
	};

	gs_module_api_t api = { .Trace = TestTrace };

	int num_collisions = 0;
	for( int i = 0; i < 20; i++ ) {
		test_pml.velocity = Vec3( -20.0f, 0.0f, 500.0f );
		int blocked = PM_SlideMove( api, &test_pm, &test_pml );
		if( blocked != 0 ) {
			num_collisions++;
		}
	}

	Vec3 expected_endpos = Vec3( 1.0f, 0.0f, 500.0f );
	Vec3 expected_velocity = Vec3( 0.0f, 0.0f, 500.0f );

	return num_collisions == 12 && Length( expected_endpos - test_pml.origin ) < 10.0f && Length( expected_velocity - test_pml.velocity ) < 0.01f;
}
