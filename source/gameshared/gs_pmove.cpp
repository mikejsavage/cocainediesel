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

#include "gameshared/q_arch.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_collision.h"
#include "gameshared/gs_public.h"

#define SPEEDKEY    500.0f

#define PM_DASHJUMP_TIMEDELAY 1000 // delay in milliseconds
#define PM_WALLJUMP_TIMEDELAY   1300
#define PM_SPECIAL_CROUCH_INHIBIT 400
#define PM_AIRCONTROL_BOUNCE_DELAY 200
#define PM_OVERBOUNCE       1.01f

static constexpr s16 MAX_TBAG_TIME = 2000;
static constexpr s16 TBAG_THRESHOLD = 1000;
static constexpr s16 TBAG_AMOUNT_PER_CROUCH = 500;

//===============================================================

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct {
	vec3_t origin;          // full float precision
	vec3_t velocity;        // full float precision

	vec3_t forward, right, up;
	vec3_t flatforward;     // normalized forward without z component, saved here because it needs
	// special handling for looking straight up or down
	float frametime;

	int groundsurfFlags;
	cplane_t groundplane;
	int groundcontents;

	vec3_t previous_origin;
	bool ladder;

	float forwardPush, sidePush, upPush;

	float maxPlayerSpeed;
	float maxWalkSpeed;
	float maxCrouchedSpeed;
	float jumpPlayerSpeed;
	float jumpPlayerSpeedWater;
	float dashPlayerSpeed;
} pml_t;

static pmove_t *pm;
static pml_t pml;
static const gs_state_t * pmove_gs;

// movement parameters

#define DEFAULT_WALKSPEED 160.0f
#define DEFAULT_CROUCHEDSPEED 100.0f
#define DEFAULT_LADDERSPEED 250.0f

const float pm_friction = 8; //  ( initially 6 )
const float pm_waterfriction = 60;
const float pm_wateraccelerate = 6; // user intended acceleration when swimming ( initially 6 )

const float pm_accelerate = 12; // user intended acceleration when on ground or fly movement ( initially 10 )
const float pm_decelerate = 12; // user intended deceleration when on ground

const float pm_airaccelerate = 1; // user intended aceleration when on air
const float pm_airdecelerate = 2.0f; // air deceleration (not +strafe one, just at normal moving).

// special movement parameters

const float pm_aircontrol = 150.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
const float pm_strafebunnyaccel = 70; // forward acceleration when strafe bunny hopping
const float pm_wishspeed = 30;

const float pm_dashupspeed = ( 174.0f * GRAVITY_COMPENSATE );

const float pm_wjupspeed = ( 330.0f * GRAVITY_COMPENSATE );
const float pm_wjbouncefactor = 0.3f;
#define pm_wjminspeed ( ( pml.maxWalkSpeed + pml.maxPlayerSpeed ) * 0.5f )

//
// Kurim : some functions/defines that can be useful to work on the horizontal movement of player :
//
#define VectorScale2D( in, scale, out ) ( ( out )[0] = ( in )[0] * ( scale ), ( out )[1] = ( in )[1] * ( scale ) )
#define DotProduct2D( x, y )           ( ( x )[0] * ( y )[0] + ( x )[1] * ( y )[1] )

static float VectorNormalize2D( vec3_t v ) { // ByMiK : normalize horizontally (don't affect Z value)
	float length, ilength;
	length = v[0] * v[0] + v[1] * v[1];
	if( length ) {
		length = sqrtf( length ); // FIXME
		ilength = 1.0f / length;
		v[0] *= ilength;
		v[1] *= ilength;
	}
	return length;
}

// Walljump wall availability check
// nbTestDir is the number of directions to test around the player
// maxZnormal is the max Z value of the normal of a poly to consider it a wall
// normal becomes a pointer to the normal of the most appropriate wall
static void PlayerTouchWall( int nbTestDir, float maxZnormal, vec3_t *normal ) {
	vec3_t min, max, dir;
	int i, j;
	trace_t trace;
	float dist = 1.0;
	SyncEntityState *state;

	for( i = 0; i < nbTestDir; i++ ) {
		dir[0] = pml.origin[0] + ( pm->maxs[0]*cosf( ( PI*2.0f /nbTestDir )*i ) + pml.velocity[0] * 0.015f );
		dir[1] = pml.origin[1] + ( pm->maxs[1]*sinf( ( PI*2.0f /nbTestDir )*i ) + pml.velocity[1] * 0.015f );
		dir[2] = pml.origin[2];

		for( j = 0; j < 2; j++ ) {
			min[j] = pm->mins[j];
			max[j] = pm->maxs[j];
		}
		min[2] = max[2] = 0;

		pmove_gs->api.Trace( &trace, pml.origin, min, max, dir, pm->playerState->POVnum, pm->contentmask, 0 );

		if( trace.allsolid ) return;

		if( trace.fraction == 1 )
			continue; // no wall in this direction

		if( trace.surfFlags & (SURF_SKY|SURF_NOWALLJUMP) )
			continue;

		if( trace.ent > 0 ) {
			state = pmove_gs->api.GetEntityState( trace.ent, 0 );
			if( state->type == ET_PLAYER )
				continue;
		}

		if( trace.fraction > 0 ) {
			if( dist > trace.fraction && Abs( trace.plane.normal[2] ) < maxZnormal ) {
				dist = trace.fraction;
				VectorCopy( trace.plane.normal, *normal );
			}
		}
	}
}

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
	int i;

	if( pm->numtouch >= MAXTOUCH || entNum < 0 ) {
		return;
	}

	// see if it is already added
	for( i = 0; i < pm->numtouch; i++ ) {
		if( pm->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entNum;
	pm->numtouch++;
}


static int PM_SlideMove( void ) {
	vec3_t end, dir;
	vec3_t last_valid_origin;
	float value;
	vec3_t planes[MAX_CLIP_PLANES];
	int numplanes = 0;
	trace_t trace;
	int moves, i, j, k;
	int maxmoves = 4;
	float remainingTime = pml.frametime;
	int blockedmask = 0;

	if( pm->groundentity != -1 ) { // clip velocity to ground, no need to wait
		// if the ground is not horizontal (a ramp) clipping will slow the player down
		if( pml.groundplane.normal[2] == 1.0f && pml.velocity[2] < 0.0f ) {
			pml.velocity[2] = 0.0f;
		}
	}

	VectorCopy( pml.origin, last_valid_origin );

	numplanes = 0; // clean up planes count for checking

	for( moves = 0; moves < maxmoves; moves++ ) {
		VectorMA( pml.origin, remainingTime, pml.velocity, end );
		pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );
		if( trace.allsolid ) { // trapped into a solid
			VectorCopy( last_valid_origin, pml.origin );
			return SLIDEMOVEFLAG_TRAPPED;
		}

		if( trace.fraction > 0 ) { // actually covered some distance
			VectorCopy( trace.endpos, pml.origin );
			VectorCopy( trace.endpos, last_valid_origin );
		}

		if( trace.fraction == 1 ) {
			break; // move done

		}
		// save touched entity for return output
		PM_AddTouchEnt( trace.ent );

		// at this point we are blocked but not trapped.

		blockedmask |= SLIDEMOVEFLAG_BLOCKED;
		if( trace.plane.normal[2] < SLIDEMOVE_PLANEINTERACT_EPSILON ) { // is it a vertical wall?
			blockedmask |= SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		remainingTime -= ( trace.fraction * remainingTime );

		// we got blocked, add the plane for sliding along it

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for( i = 0; i < numplanes; i++ ) {
			if( DotProduct( trace.plane.normal, planes[i] ) > ( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) ) {
				VectorAdd( trace.plane.normal, pml.velocity, pml.velocity );
				break;
			}
		}
		if( i < numplanes ) { // found a repeated plane, so don't add it, just repeat the trace
			continue;
		}

		// security check: we can't store more planes
		if( numplanes >= MAX_CLIP_PLANES ) {
			VectorClear( pml.velocity );
			return SLIDEMOVEFLAG_TRAPPED;
		}

		// put the actual plane in the list
		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//

		for( i = 0; i < numplanes; i++ ) {
			if( DotProduct( pml.velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) { // would not touch it
				continue;
			}

			GS_ClipVelocity( pml.velocity, planes[i], pml.velocity, PM_OVERBOUNCE );
			// see if we enter a second plane
			for( j = 0; j < numplanes; j++ ) {
				if( j == i ) { // it's the same plane
					continue;
				}
				if( DotProduct( pml.velocity, planes[j] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue; // not with this one

				}
				//there was a second one. Try to slide along it too
				GS_ClipVelocity( pml.velocity, planes[j], pml.velocity, PM_OVERBOUNCE );

				// check if the slide sent it back to the first plane
				if( DotProduct( pml.velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue;
				}

				// bad luck: slide the original velocity along the crease
				CrossProduct( planes[i], planes[j], dir );
				VectorNormalize( dir );
				value = DotProduct( dir, pml.velocity );
				VectorScale( dir, value, pml.velocity );

				// check if there is a third plane, in that case we're trapped
				for( k = 0; k < numplanes; k++ ) {
					if( j == k || i == k ) { // it's the same plane
						continue;
					}
					if( DotProduct( pml.velocity, planes[k] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
						continue; // not with this one
					}
					VectorClear( pml.velocity );
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
static void PM_StepSlideMove( void ) {
	vec3_t start_o, start_v;
	vec3_t down_o, down_v;
	trace_t trace;
	float down_dist, up_dist;
	float hspeed;
	vec3_t up, down;
	int blocked;

	VectorCopy( pml.origin, start_o );
	VectorCopy( pml.velocity, start_v );

	blocked = PM_SlideMove();

	VectorCopy( pml.origin, down_o );
	VectorCopy( pml.velocity, down_v );

	VectorCopy( start_o, up );
	up[2] += STEPSIZE;

	pmove_gs->api.Trace( &trace, up, pm->mins, pm->maxs, up, pm->playerState->POVnum, pm->contentmask, 0 );
	if( trace.allsolid ) {
		return; // can't step up

	}
	// try sliding above
	VectorCopy( up, pml.origin );
	VectorCopy( start_v, pml.velocity );

	PM_SlideMove();

	// push down the final amount
	VectorCopy( pml.origin, down );
	down[2] -= STEPSIZE;
	pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, down, pm->playerState->POVnum, pm->contentmask, 0 );
	if( !trace.allsolid ) {
		VectorCopy( trace.endpos, pml.origin );
	}

	VectorCopy( pml.origin, up );

	// decide which one went farther
	down_dist = ( down_o[0] - start_o[0] ) * ( down_o[0] - start_o[0] )
				+ ( down_o[1] - start_o[1] ) * ( down_o[1] - start_o[1] );
	up_dist = ( up[0] - start_o[0] ) * ( up[0] - start_o[0] )
			  + ( up[1] - start_o[1] ) * ( up[1] - start_o[1] );

	if( down_dist >= up_dist || trace.allsolid || ( trace.fraction != 1.0 && !ISWALKABLEPLANE( &trace.plane ) ) ) {
		VectorCopy( down_o, pml.origin );
		VectorCopy( down_v, pml.velocity );
		return;
	}

	// only add the stepping output when it was a vertical step (second case is at the exit of a ramp)
	if( ( blocked & SLIDEMOVEFLAG_WALL_BLOCKED ) || trace.plane.normal[2] == 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
		pm->step = ( pml.origin[2] - pml.previous_origin[2] );
	}

	// Preserve speed when sliding up ramps
	hspeed = sqrtf( start_v[0] * start_v[0] + start_v[1] * start_v[1] );
	if( hspeed && ISWALKABLEPLANE( &trace.plane ) ) {
		if( trace.plane.normal[2] >= 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
			VectorCopy( start_v, pml.velocity );
		} else {
			VectorNormalize2D( pml.velocity );
			VectorScale2D( pml.velocity, hspeed, pml.velocity );
		}
	}

	// wsw : jal : The following line is what produces the ramp sliding.

	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity[2] = down_v[2];
}

/*
* PM_Friction -- Modified for wsw
*
* Handles both ground friction and water friction
*/
static void PM_Friction( void ) {
	float *vel;
	float speed, newspeed, control;
	float friction;
	float drop;

	vel = pml.velocity;

	speed = vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2];
	if( speed < 1 ) {
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	speed = sqrtf( speed );
	drop = 0;

	// apply ground friction
	if( ( ( pm->groundentity != -1 ) && !( pml.groundsurfFlags & SURF_SLICK ) ) || pml.ladder ) {
		if( pm->playerState->pmove.knockback_time <= 0 ) {
			friction = pm_friction;
			control = speed < pm_decelerate ? pm_decelerate : speed;
			drop += control * friction * pml.frametime;
		}
	}

	// scale the velocity
	newspeed = speed - drop;
	if( newspeed <= 0 ) {
		newspeed = 0;
		VectorClear( vel );
	} else {
		newspeed /= speed;
		VectorScale( vel, newspeed, vel );
	}
}

/*
* PM_Accelerate
*
* Handles user intended acceleration
*/
static void PM_Accelerate( vec3_t wishdir, float wishspeed, float accel ) {
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if( addspeed <= 0 ) {
		return;
	}

	accelspeed = accel * pml.frametime * wishspeed;
	if( accelspeed > addspeed ) {
		accelspeed = addspeed;
	}

	VectorMA( pml.velocity, accelspeed, wishdir, pml.velocity );
}

// when using +strafe convert the inertia to forward speed.
static void PM_Aircontrol( vec3_t wishdir, float wishspeed ) {
	int i;
	float zspeed, speed, dot, k;
	float smove;

	if( !pm_aircontrol ) {
		return;
	}

	// accelerate
	smove = pml.sidePush;

	if( ( smove > 0 || smove < 0 ) || ( wishspeed == 0.0 ) ) {
		return; // can't control movement if not moving forward or backward

	}
	zspeed = pml.velocity[2];
	pml.velocity[2] = 0;
	speed = VectorNormalize( pml.velocity );


	dot = DotProduct( pml.velocity, wishdir );
	k = 32.0f * pm_aircontrol * dot * dot * pml.frametime;

	if( dot > 0 ) {
		// we can't change direction while slowing down
		for( i = 0; i < 2; i++ )
			pml.velocity[i] = pml.velocity[i] * speed + wishdir[i] * k;

		VectorNormalize( pml.velocity );
	}

	for( i = 0; i < 2; i++ )
		pml.velocity[i] *= speed;

	pml.velocity[2] = zspeed;
}

/*
* PM_AddCurrents
*/
static void PM_AddCurrents( vec3_t wishvel ) {
	//
	// account for ladders
	//

	if( pml.ladder && Abs( pml.velocity[2] ) <= DEFAULT_LADDERSPEED ) {
		if( ( pm->playerState->viewangles[PITCH] <= -15 ) && ( pml.forwardPush > 0 ) ) {
			wishvel[2] = DEFAULT_LADDERSPEED;
		} else if( ( pm->playerState->viewangles[PITCH] >= 15 ) && ( pml.forwardPush > 0 ) ) {
			wishvel[2] = -DEFAULT_LADDERSPEED;
		} else if( pml.upPush > 0 ) {
			wishvel[2] = DEFAULT_LADDERSPEED;
		} else if( pml.upPush < 0 ) {
			wishvel[2] = -DEFAULT_LADDERSPEED;
		} else {
			wishvel[2] = 0;
		}

		// limit horizontal speed when on a ladder
		wishvel[0] = Clamp( -25.0f, wishvel[0], 25.0f );
		wishvel[1] = Clamp( -25.0f, wishvel[1], 25.0f );
	}
}

/*
* PM_WaterMove
*
*/
static void PM_WaterMove( void ) {
	int i;
	vec3_t wishvel;
	float wishspeed;
	vec3_t wishdir;

	// user intentions
	for( i = 0; i < 3; i++ )
		wishvel[i] = pml.forward[i] * pml.forwardPush + pml.right[i] * pml.sidePush;

	wishvel[2] -= pm_waterfriction;

	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	if( wishspeed > pml.maxPlayerSpeed ) {
		wishspeed = pml.maxPlayerSpeed / wishspeed;
		VectorScale( wishvel, wishspeed, wishvel );
		wishspeed = pml.maxPlayerSpeed;
	}
	wishspeed *= 0.5;

	PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );
	PM_StepSlideMove();
}

/*
* PM_Move -- Kurim
*
*/
static void PM_Move( void ) {
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	float maxspeed;
	float accel;
	float wishspeed2;

	fmove = pml.forwardPush;
	smove = pml.sidePush;

	for( i = 0; i < 2; i++ )
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] = 0;

	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	// clamp to server defined max speed

	if( pm->playerState->pmove.crouch_time ) {
		maxspeed = pml.maxCrouchedSpeed;
	} else if( ( pm->cmd.buttons & BUTTON_WALK ) && ( pm->playerState->pmove.features & PMFEAT_WALK ) ) {
		maxspeed = pml.maxWalkSpeed;
	} else {
		maxspeed = pml.maxPlayerSpeed;
	}

	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		VectorScale( wishvel, wishspeed, wishvel );
		wishspeed = maxspeed;
	}

	if( pml.ladder ) {
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		if( !wishvel[2] ) {
			if( pml.velocity[2] > 0 ) {
				pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
				if( pml.velocity[2] < 0 ) {
					pml.velocity[2]  = 0;
				}
			} else {
				pml.velocity[2] += pm->playerState->pmove.gravity * pml.frametime;
				if( pml.velocity[2] > 0 ) {
					pml.velocity[2]  = 0;
				}
			}
		}

		PM_StepSlideMove();
	} else if( pm->groundentity != -1 ) {
		// walking on ground
		if( pml.velocity[2] > 0 ) {
			pml.velocity[2] = 0; //!!! this is before the accel

		}
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		// fix for negative trigger_gravity fields
		if( pm->playerState->pmove.gravity > 0 ) {
			if( pml.velocity[2] > 0 ) {
				pml.velocity[2] = 0;
			}
		} else {
			pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		}

		if( !pml.velocity[0] && !pml.velocity[1] ) {
			return;
		}

		PM_StepSlideMove();
	} else {
		// Air Control
		wishspeed2 = wishspeed;
		if( DotProduct( pml.velocity, wishdir ) < 0
			&& !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING )
			&& ( pm->playerState->pmove.knockback_time <= 0 ) ) {
			accel = pm_airdecelerate;
		} else {
			accel = pm_airaccelerate;
		}

		if( ( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) ) {
			accel = 0; // no stopmove while walljumping
		}
		if( ( smove > 0 || smove < 0 ) && !fmove && ( pm->playerState->pmove.knockback_time <= 0 ) ) {
			if( wishspeed > pm_wishspeed ) {
				wishspeed = pm_wishspeed;
			}
			accel = pm_strafebunnyaccel;
		}

		// Air control
		PM_Accelerate( wishdir, wishspeed, accel );
		if( pm_aircontrol && !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) && ( pm->playerState->pmove.knockback_time <= 0 ) ) { // no air ctrl while wjing
			PM_Aircontrol( wishdir, wishspeed2 );
		}

		// add gravity
		pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		PM_StepSlideMove();
	}
}

/*
* PM_GroundTrace
*
* If the player hull point one-quarter unit down is solid, the player is on ground
*/
static void PM_GroundTrace( trace_t *trace ) {
	vec3_t point;

	// see if standing on something solid
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.25;

	pmove_gs->api.Trace( trace, pml.origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );
}

/*
* PM_GoodPosition
*/
static bool PM_GoodPosition( vec3_t origin, trace_t *trace ) {
	if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		return true;
	}

	pmove_gs->api.Trace( trace, origin, pm->mins, pm->maxs, origin, pm->playerState->POVnum, pm->contentmask, 0 );

	return !trace->allsolid;
}

/*
* PM_UnstickPosition
*/
static void PM_UnstickPosition( trace_t *trace ) {
	int j;
	vec3_t origin;

	VectorCopy( pml.origin, origin );

	// try all combinations
	for( j = 0; j < 8; j++ ) {
		VectorCopy( pml.origin, origin );

		origin[0] += ( ( j & 1 ) ? -1 : 1 );
		origin[1] += ( ( j & 2 ) ? -1 : 1 );
		origin[2] += ( ( j & 4 ) ? -1 : 1 );

		if( PM_GoodPosition( origin, trace ) ) {
			VectorCopy( origin, pml.origin );
			PM_GroundTrace( trace );
			return;
		}
	}

	// go back to the last position
	VectorCopy( pml.previous_origin, pml.origin );
}

/*
* PM_CategorizePosition
*/
static void PM_CategorizePosition( void ) {
	vec3_t point;
	int cont;
	int sample1;
	int sample2;

	if( pml.velocity[2] > 180 ) { // !!ZOID changed from 100 to 180 (ramp accel)
		pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = -1;
	} else {
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

		if( ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			pm->groundentity = -1;
			pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		} else {
			pm->groundentity = trace.ent;
			pm->groundplane = trace.plane;
			pm->groundsurfFlags = trace.surfFlags;
			pm->groundcontents = trace.contents;

			// hitting solid ground will end a waterjump
			if( pm->playerState->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
				pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
				pm->playerState->pmove.pm_time = 0;
			}

			if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) ) { // just hit the ground
				pm->playerState->pmove.pm_flags |= PMF_ON_GROUND;
			}
		}

		if( ( pm->numtouch < MAXTOUCH ) && ( trace.fraction < 1.0 ) ) {
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

	//
	// get waterlevel, accounting for ducking
	//
	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->playerState->viewheight - pm->mins[2];
	sample1 = sample2 / 2;

	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] + pm->mins[2] + 1;
	cont = pmove_gs->api.PointContents( point, 0 );

	if( cont & MASK_WATER ) {
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = pmove_gs->api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = pmove_gs->api.PointContents( point, 0 );
			if( cont & MASK_WATER ) {
				pm->waterlevel = 3;
			}
		}
	}
}

static void PM_ClearDash( void ) {
	pm->playerState->pmove.pm_flags &= ~PMF_DASHING;
	pm->playerState->pmove.dash_time = 0;
}

static void PM_ClearWallJump( void ) {
	pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	pm->playerState->pmove.walljump_time = 0;
}

/*
* PM_CheckJump
*/
static void PM_CheckJump( void ) {
	if( pml.upPush < 10 ) {
		return;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( pm->groundentity == -1 ) {
		return;
	}

	if( !( pm->playerState->pmove.features & PMFEAT_JUMP ) ) {
		return;
	}

	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml.groundplane.normal[2] > 0 && pml.velocity[2] < 0 && DotProduct2D( pml.groundplane.normal, pml.velocity ) > 0 ) {
		GS_ClipVelocity( pml.velocity, pml.groundplane.normal, pml.velocity, PM_OVERBOUNCE );
	}

	float jumpSpeed = ( pm->waterlevel >= 2 ? pml.jumpPlayerSpeedWater : pml.jumpPlayerSpeed );

	if( pml.velocity[2] > 100 ) {
		pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DOUBLEJUMP, 0 );
		pml.velocity[2] += jumpSpeed;
	} else if( pml.velocity[2] > 0 ) {
		pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_JUMP, 0 );
		pml.velocity[2] += jumpSpeed;
	} else {
		pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_JUMP, 0 );
		pml.velocity[2] = jumpSpeed;
	}

	// remove wj count
	pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;
	PM_ClearDash();
	PM_ClearWallJump();
}

/*
* PM_CheckDash -- by Kurim
*/
static void PM_CheckDash( void ) {
	float actual_velocity;
	float upspeed;
	vec3_t dashdir;

	if( !( pm->cmd.buttons & BUTTON_SPECIAL ) ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( pm->playerState->pmove.dash_time > 0 ) {
		return;
	}

	if( pm->playerState->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( ( pm->cmd.buttons & BUTTON_SPECIAL ) && pm->groundentity != -1
		&& ( pm->playerState->pmove.features & PMFEAT_SPECIAL ) ) {
		if( pm->playerState->pmove.pm_flags & PMF_SPECIAL_HELD ) {
			return;
		}

		pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;
		PM_ClearWallJump();

		pm->playerState->pmove.pm_flags |= PMF_DASHING;
		pm->playerState->pmove.pm_flags |= PMF_SPECIAL_HELD;
		pm->groundentity = -1;

		// clip against the ground when jumping if moving that direction
		if( pml.groundplane.normal[2] > 0 && pml.velocity[2] < 0 && DotProduct2D( pml.groundplane.normal, pml.velocity ) > 0 ) {
			GS_ClipVelocity( pml.velocity, pml.groundplane.normal, pml.velocity, PM_OVERBOUNCE );
		}

		if( pml.velocity[2] <= 0.0f ) {
			upspeed = pm_dashupspeed;
		} else {
			upspeed = pm_dashupspeed + pml.velocity[2];
		}

		// ch : we should do explicit forwardPush here, and ignore sidePush ?
		VectorMA( vec3_origin, pml.forwardPush, pml.flatforward, dashdir );
		VectorMA( dashdir, pml.sidePush, pml.right, dashdir );
		dashdir[2] = 0.0;

		if( VectorLength( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
			VectorCopy( pml.flatforward, dashdir );
		}

		VectorNormalize( dashdir );

		actual_velocity = VectorNormalize2D( pml.velocity );
		if( actual_velocity <= pml.dashPlayerSpeed ) {
			VectorScale( dashdir, pml.dashPlayerSpeed, dashdir );
		} else {
			VectorScale( dashdir, actual_velocity, dashdir );
		}

		VectorCopy( dashdir, pml.velocity );
		pml.velocity[2] = upspeed;

		pm->playerState->pmove.dash_time = PM_DASHJUMP_TIMEDELAY;

		// return sound events
		if( Abs( pml.sidePush ) > 10 && Abs( pml.sidePush ) >= Abs( pml.forwardPush ) ) {
			if( pml.sidePush > 0 ) {
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 2 );
			} else {
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 1 );
			}
		} else if( pml.forwardPush < -10 ) {
			pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 3 );
		} else {
			pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 0 );
		}
	} else if( pm->groundentity == -1 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_DASHING;
	}
}

/*
* PM_CheckWallJump -- By Kurim
*/
static void PM_CheckWallJump( void ) {
	vec3_t normal;
	float hspeed;

	if( !( pm->cmd.buttons & BUTTON_SPECIAL ) ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( pm->groundentity != -1 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	}

	if( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING && pml.velocity[2] < 0.0 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}

	if( pm->playerState->pmove.walljump_time <= 0 ) { // reset the wj count after wj delay
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	// don't walljump in the first 100 milliseconds of a dash jump
	if( pm->playerState->pmove.pm_flags & PMF_DASHING && pm->playerState->pmove.dash_time > PM_DASHJUMP_TIMEDELAY - 100 ) {
		return;
	}


	// markthis

	if( pm->groundentity == -1 && ( pm->cmd.buttons & BUTTON_SPECIAL )
		&& ( pm->playerState->pmove.features & PMFEAT_SPECIAL ) &&
		( !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPCOUNT ) )
		&& pm->playerState->pmove.walljump_time <= 0
		) {
		trace_t trace;
		vec3_t point;

		point[0] = pml.origin[0];
		point[1] = pml.origin[1];
		point[2] = pml.origin[2] - STEPSIZE;

		// don't walljump if our height is smaller than a step
		// unless jump is pressed or the player is moving faster than dash speed and upwards
		hspeed = VectorLength( tv( pml.velocity[0], pml.velocity[1], 0 ) );
		pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );

		if( pml.upPush >= 10
			|| ( hspeed > pm->playerState->pmove.dash_speed && pml.velocity[2] > 8 )
			|| ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			VectorClear( normal );
			PlayerTouchWall( 12, 0.3f, &normal );
			if( !VectorLength( normal ) ) {
				return;
			}

			if( !( pm->playerState->pmove.pm_flags & PMF_SPECIAL_HELD )
				&& !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) ) {
				float oldupvelocity = pml.velocity[2];
				pml.velocity[2] = 0.0;

				hspeed = VectorNormalize2D( pml.velocity );

				GS_ClipVelocity( pml.velocity, normal, pml.velocity, 1.0005f );
				VectorMA( pml.velocity, pm_wjbouncefactor, normal, pml.velocity );

				if( hspeed < pm_wjminspeed ) {
					hspeed = pm_wjminspeed;
				}

				VectorNormalize( pml.velocity );

				VectorScale( pml.velocity, hspeed, pml.velocity );
				pml.velocity[2] = ( oldupvelocity > pm_wjupspeed ) ? oldupvelocity : pm_wjupspeed; // jal: if we had a faster upwards speed, keep it

				// set the walljumping state
				PM_ClearDash();
				pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;

				pm->playerState->pmove.pm_flags |= PMF_WALLJUMPING;
				pm->playerState->pmove.pm_flags |= PMF_SPECIAL_HELD;

				pm->playerState->pmove.pm_flags |= PMF_WALLJUMPCOUNT;

				pm->playerState->pmove.walljump_time = PM_WALLJUMP_TIMEDELAY;

				// Create the event
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_WALLJUMP, DirToByte( normal ) );
			}
		}
	} else {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}
}

/*
* PM_CheckSpecialMovement
*/
static void PM_CheckSpecialMovement( void ) {
	vec3_t spot;
	int cont;
	trace_t trace;

	pm->ladder = false;

	if( pm->playerState->pmove.pm_time ) {
		return;
	}

	pml.ladder = false;

	// check for ladder
	VectorMA( pml.origin, 1, pml.flatforward, spot );
	pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->contentmask, 0 );
	if( ( trace.fraction < 1 ) && ( trace.surfFlags & SURF_LADDER ) ) {
		pml.ladder = true;
		pm->ladder = true;
	}

	// check for water jump
	if( pm->waterlevel != 2 ) {
		return;
	}

	VectorMA( pml.origin, 30, pml.flatforward, spot );
	spot[2] += 4;
	cont = pmove_gs->api.PointContents( spot, 0 );
	if( !( cont & CONTENTS_SOLID ) ) {
		return;
	}

	spot[2] += 16;
	cont = pmove_gs->api.PointContents( spot, 0 );
	if( cont ) {
		return;
	}
	// jump out of water
	VectorScale( pml.flatforward, 50, pml.velocity );
	pml.velocity[2] = 350;

	pm->playerState->pmove.pm_flags |= PMF_TIME_WATERJUMP;
	pm->playerState->pmove.pm_time = 255;
}

/*
* PM_FlyMove
*/
static void PM_FlyMove( bool doclip ) {
	float speed, drop, friction, control, newspeed;
	float currentspeed, addspeed, accelspeed, maxspeed;
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	vec3_t end;
	trace_t trace;

	maxspeed = pml.maxPlayerSpeed * 1.5;

	if( pm->cmd.buttons & BUTTON_SPECIAL ) {
		maxspeed *= 2;
	}

	// friction
	speed = VectorLength( pml.velocity );
	if( speed < 1 ) {
		VectorClear( pml.velocity );
	} else {
		drop = 0;

		friction = pm_friction * 1.5; // extra friction
		control = speed < pm_decelerate ? pm_decelerate : speed;
		drop += control * friction * pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if( newspeed < 0 ) {
			newspeed = 0;
		}
		newspeed /= speed;

		VectorScale( pml.velocity, newspeed, pml.velocity );
	}

	// accelerate
	fmove = pml.forwardPush;
	smove = pml.sidePush;

	if( pm->cmd.buttons & BUTTON_SPECIAL ) {
		fmove *= 2;
		smove *= 2;
	}

	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] += pml.upPush;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );


	// clamp to server defined max speed
	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		VectorScale( wishvel, wishspeed, wishvel );
		wishspeed = maxspeed;
	}

	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if( addspeed > 0 ) {
		accelspeed = pm_accelerate * pml.frametime * wishspeed;
		if( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}

		for( i = 0; i < 3; i++ )
			pml.velocity[i] += accelspeed * wishdir[i];
	}

	if( doclip ) {
		for( i = 0; i < 3; i++ )
			end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];

		pmove_gs->api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );

		VectorCopy( trace.endpos, pml.origin );
	} else {
		// move
		VectorMA( pml.origin, pml.frametime, pml.velocity, pml.origin );
	}
}

/*
* PM_AdjustBBox
*
* Sets mins, maxs, and pm->viewheight
*/
static void PM_AdjustBBox( void ) {
	float crouchFrac;
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

	if( pml.upPush < 0 && ( pm->playerState->pmove.features & PMFEAT_CROUCH ) &&
		pm->playerState->pmove.walljump_time < ( PM_WALLJUMP_TIMEDELAY - PM_SPECIAL_CROUCH_INHIBIT ) &&
		pm->playerState->pmove.dash_time < ( PM_DASHJUMP_TIMEDELAY - PM_SPECIAL_CROUCH_INHIBIT ) &&
		( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) ) {

		if( pm->playerState->pmove.crouch_time == 0 ) {
			pm->playerState->pmove.tbag_time = Min2( pm->playerState->pmove.tbag_time + TBAG_AMOUNT_PER_CROUCH, int( MAX_TBAG_TIME ) );

			if( pm->playerState->pmove.tbag_time >= TBAG_THRESHOLD ) {
				float frac = Unlerp( TBAG_THRESHOLD, pm->playerState->pmove.tbag_time, MAX_TBAG_TIME );
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_TBAG, frac * 255 );
			}
		}

		pm->playerState->pmove.crouch_time = bound( 0, pm->playerState->pmove.crouch_time + pm->cmd.msec, CROUCHTIME );

		crouchFrac = (float)pm->playerState->pmove.crouch_time / (float)CROUCHTIME;
		VectorLerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins, pm->mins );
		VectorLerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs, pm->maxs );
		pm->playerState->viewheight = playerbox_stand_viewheight - ( crouchFrac * ( playerbox_stand_viewheight - playerbox_crouch_viewheight ) );

		// it's going down, so, no need of checking for head-chomping
		return;
	}

	// it's crouched, but not pressing the crouch button anymore, try to stand up
	if( pm->playerState->pmove.crouch_time != 0 ) {
		vec3_t curmins, curmaxs, wishmins, wishmaxs;
		float curviewheight, wishviewheight;
		int newcrouchtime;

		// find the current size
		crouchFrac = (float)pm->playerState->pmove.crouch_time / (float)CROUCHTIME;
		VectorLerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins, curmins );
		VectorLerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs, curmaxs );
		curviewheight = playerbox_stand_viewheight - ( crouchFrac * ( playerbox_stand_viewheight - playerbox_crouch_viewheight ) );

		if( !pm->cmd.msec ) { // no need to continue
			VectorCopy( curmins, pm->mins );
			VectorCopy( curmaxs, pm->maxs );
			pm->playerState->viewheight = curviewheight;
			return;
		}

		// find the desired size
		newcrouchtime = Clamp( 0, pm->playerState->pmove.crouch_time - pm->cmd.msec, CROUCHTIME );
		crouchFrac = (float)newcrouchtime / (float)CROUCHTIME;
		VectorLerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins, wishmins );
		VectorLerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs, wishmaxs );
		wishviewheight = playerbox_stand_viewheight - ( crouchFrac * ( playerbox_stand_viewheight - playerbox_crouch_viewheight ) );

		// check that the head is not blocked
		pmove_gs->api.Trace( &trace, pml.origin, wishmins, wishmaxs, pml.origin, pm->playerState->POVnum, pm->contentmask, 0 );
		if( trace.allsolid || trace.startsolid ) {
			// can't do the uncrouching, let the time alone and use old position
			VectorCopy( curmins, pm->mins );
			VectorCopy( curmaxs, pm->maxs );
			pm->playerState->viewheight = curviewheight;
			return;
		}

		// can do the uncrouching, use new position and update the time
		pm->playerState->pmove.crouch_time = newcrouchtime;
		VectorCopy( wishmins, pm->mins );
		VectorCopy( wishmaxs, pm->maxs );
		pm->playerState->viewheight = wishviewheight;
		return;
	}

	// the player is not crouching at all
	VectorCopy( playerbox_stand_mins, pm->mins );
	VectorCopy( playerbox_stand_maxs, pm->maxs );
	pm->playerState->viewheight = playerbox_stand_viewheight;
}

static void PM_UpdateDeltaAngles( void ) {
	int i;

	if( pmove_gs->module != GS_MODULE_GAME ) {
		return;
	}

	for( i = 0; i < 3; i++ )
		pm->playerState->pmove.delta_angles[i] = ANGLE2SHORT( pm->playerState->viewangles[i] ) - pm->cmd.angles[i];
}

/*
* PM_ApplyMouseAnglesClamp
*
*/
static void PM_ApplyMouseAnglesClamp( void ) {
	int i;
	short temp;

	for( i = 0; i < 3; i++ ) {
		temp = pm->cmd.angles[i] + pm->playerState->pmove.delta_angles[i];
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

	AngleVectors( pm->playerState->viewangles, pml.forward, pml.right, pml.up );

	VectorCopy( pml.forward, pml.flatforward );
	pml.flatforward[2] = 0.0f;
	VectorNormalize( pml.flatforward );
}

/*
* PM_BeginMove
*/
static void PM_BeginMove( void ) {
	// clear results
	pm->numtouch = 0;
	pm->groundentity = -1;
	pm->watertype = 0;
	pm->waterlevel = 0;
	pm->step = 0;

	// clear all pmove local vars
	memset( &pml, 0, sizeof( pml ) );

	VectorCopy( pm->playerState->pmove.origin, pml.origin );
	VectorCopy( pm->playerState->pmove.velocity, pml.velocity );

	// save old org in case we get stuck
	VectorCopy( pm->playerState->pmove.origin, pml.previous_origin );
}

/*
* PM_EndMove
*/
static void PM_EndMove( void ) {
	VectorCopy( pml.origin, pm->playerState->pmove.origin );
	VectorCopy( pml.velocity, pm->playerState->pmove.velocity );
}

/*
* Pmove
*
* Can be called by either the server or the client
*/
void Pmove( const gs_state_t * gs, pmove_t *pmove ) {
	if( !pmove->playerState ) {
		return;
	}

	pm = pmove;
	pmove_gs = gs;

	// clear all pmove local vars
	PM_BeginMove();

	float fallvelocity = Max2( 0.0f, -pml.velocity[ 2 ] );

	pml.frametime = pm->cmd.msec * 0.001;

	pml.maxPlayerSpeed = pm->playerState->pmove.max_speed;
	if( pml.maxPlayerSpeed < 0 ) {
		pml.maxPlayerSpeed = DEFAULT_PLAYERSPEED;
	}

	pml.jumpPlayerSpeed = (float)pm->playerState->pmove.jump_speed * GRAVITY_COMPENSATE;
	pml.jumpPlayerSpeedWater = pml.jumpPlayerSpeed * 2;

	if( pml.jumpPlayerSpeed < 0 ) {
		pml.jumpPlayerSpeed = DEFAULT_JUMPSPEED * GRAVITY_COMPENSATE;
	}

	pml.dashPlayerSpeed = pm->playerState->pmove.dash_speed;
	if( pml.dashPlayerSpeed < 0 ) {
		pml.dashPlayerSpeed = DEFAULT_DASHSPEED;
	}

	pml.maxWalkSpeed = DEFAULT_WALKSPEED;
	if( pml.maxWalkSpeed > pml.maxPlayerSpeed * 0.66f ) {
		pml.maxWalkSpeed = pml.maxPlayerSpeed * 0.66f;
	}

	pml.maxCrouchedSpeed = DEFAULT_CROUCHEDSPEED;
	if( pml.maxCrouchedSpeed > pml.maxPlayerSpeed * 0.5f ) {
		pml.maxCrouchedSpeed = pml.maxPlayerSpeed * 0.5f;
	}

	// assign a contentmask for the movement type
	switch( pm->playerState->pmove.pm_type ) {
		case PM_FREEZE:
		case PM_CHASECAM:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags |= PMF_NO_PREDICTION;
			}
			pm->contentmask = 0;
			break;

		case PM_SPECTATOR:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			pm->contentmask = MASK_DEADSOLID;
			break;

		default:
		case PM_NORMAL:
			if( pmove_gs->module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			if( pm->playerState->pmove.features & PMFEAT_GHOSTMOVE ) {
				pm->contentmask = MASK_DEADSOLID;
			} else if( pm->playerState->pmove.features & PMFEAT_TEAMGHOST ) {
				int team = pmove_gs->api.GetEntityState( pm->playerState->POVnum, 0 )->team;
				pm->contentmask = team == TEAM_ALPHA ? MASK_ALPHAPLAYERSOLID : MASK_BETAPLAYERSOLID;
			} else {
				pm->contentmask = MASK_PLAYERSOLID;
			}
			break;
	}

	if( !GS_MatchPaused( pmove_gs ) ) {
		// drop timing counters
		if( pm->playerState->pmove.pm_time ) {
			int msec;

			msec = pm->cmd.msec >> 3;
			if( !msec ) {
				msec = 1;
			}
			if( msec >= pm->playerState->pmove.pm_time ) {
				pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
				pm->playerState->pmove.pm_time = 0;
			} else {
				pm->playerState->pmove.pm_time -= msec;
			}
		}

		pmove_state_t & pmove = pm->playerState->pmove;

		pmove.no_control_time = Max2( 0, pmove.no_control_time - pm->cmd.msec );
		pmove.knockback_time = Max2( 0, pmove.knockback_time - pm->cmd.msec );
		pmove.dash_time = Max2( 0, pmove.dash_time - pm->cmd.msec );
		pmove.walljump_time = Max2( 0, pmove.walljump_time - pm->cmd.msec );
		pmove.tbag_time = Max2( 0, pmove.tbag_time - pm->cmd.msec );
		// crouch_time is handled at PM_AdjustBBox
	}

	pml.forwardPush = pm->cmd.forwardmove * SPEEDKEY / 127.0f;
	pml.sidePush = pm->cmd.sidemove * SPEEDKEY / 127.0f;
	pml.upPush = pm->cmd.upmove * SPEEDKEY / 127.0f;

	if( pm->playerState->pmove.no_control_time > 0 ) {
		pml.forwardPush = 0;
		pml.sidePush = 0;
		pml.upPush = 0;
		pm->cmd.buttons = 0;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( !GS_MatchPaused( pmove_gs ) ) {
			PM_ClearDash();

			PM_ClearWallJump();

			pm->playerState->pmove.knockback_time = 0;
			pm->playerState->pmove.crouch_time = 0;
			pm->playerState->pmove.tbag_time = 0;
			pm->playerState->pmove.pm_flags &= ~( PMF_JUMPPAD_TIME | PMF_DOUBLEJUMPED | PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_SPECIAL_HELD );

			PM_AdjustBBox();
		}

		if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
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

	if( pm->playerState->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		// teleport pause stays exactly in place
	} else if( pm->playerState->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		// waterjump has no control, but falls
		pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		if( pml.velocity[2] < 0 ) {
			// cancel as soon as we are falling down again
			pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
			pm->playerState->pmove.pm_time = 0;
		}

		PM_StepSlideMove();
	} else {
		// Kurim
		// Keep this order !
		PM_CheckJump();

		if( GS_GetWeaponDef( pm->playerState->weapon )->zoom_fov == 0 || ( pm->playerState->pmove.features & PMFEAT_SCOPE ) == 0 ) {
			PM_CheckDash();
			PM_CheckWallJump();
		}

		PM_Friction();

		if( pm->waterlevel >= 2 ) {
			PM_WaterMove();
		} else {
			vec3_t angles;

			VectorCopy( pm->playerState->viewangles, angles );
			if( angles[PITCH] > 180 ) {
				angles[PITCH] = angles[PITCH] - 360;
			}
			angles[PITCH] /= 3;

			AngleVectors( angles, pml.forward, pml.right, pml.up );

			// hack to work when looking straight up and straight down
			if( pml.forward[2] == -1.0f ) {
				VectorCopy( pml.up, pml.flatforward );
			} else if( pml.forward[2] == 1.0f ) {
				VectorCopy( pml.up, pml.flatforward );
				VectorNegate( pml.flatforward, pml.flatforward );
			} else {
				VectorCopy( pml.forward, pml.flatforward );
			}
			pml.flatforward[2] = 0.0f;
			VectorNormalize( pml.flatforward );

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
	if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) && pm->groundentity != -1 ) {
		pm->groundentity = -1;
		pml.velocity[2] = 0;
	}

	if( pm->groundentity != -1 ) { // remove wall-jump and dash bits when touching ground
		// always keep the dash flag 50 msecs at least (to prevent being removed at the start of the dash)
		if( pm->playerState->pmove.dash_time < PM_DASHJUMP_TIMEDELAY - 50 ) {
			pm->playerState->pmove.pm_flags &= ~PMF_DASHING;
		}

		if( pm->playerState->pmove.walljump_time < PM_WALLJUMP_TIMEDELAY - 50 ) {
			PM_ClearWallJump();
		}
	}

	if( oldGroundEntity == -1 ) {
		constexpr float min_fall_velocity = 200;
		constexpr float max_fall_velocity = 800;

		float fall_delta = fallvelocity - Max2( 0.0f, -pml.velocity[ 2 ] );

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
			pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_FALL, frac * 255 );
		}

		pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;
	}
}
