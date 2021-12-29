#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


constexpr float pm_dashupspeed = ( 174.0f * GRAVITY_COMPENSATE );
constexpr float pm_wjupspeed = ( 350.0f * GRAVITY_COMPENSATE );
constexpr float pm_wjbouncefactor = 0.4f;



static float pm_wjminspeed( pml_t * pml ) {
	return ( pml->maxCrouchedSpeed + pml->maxPlayerSpeed ) * 0.5f;
}



static void PM_WallJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs ) {
	ZoneScoped;

	if( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING && pml->velocity.z < 0.0 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}

	if( pm->playerState->pmove.walljump_time <= 0 ) { // reset the wj count after wj delay
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	}

	// don't walljump in the first 100 milliseconds of a dash jump
	// if( pm->playerState->pmove.pm_flags & PMF_DASHING && pm->playerState->pmove.dash_time > PM_DASHJUMP_TIMEDELAY - 100 ) {
	// 	return;
	// }

	// markthis

	if(	( !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPCOUNT ) ) && pm->playerState->pmove.walljump_time <= 0 ) {
		trace_t trace;
		Vec3 point = pml->origin;
		point.z -= STEPSIZE;

		// don't walljump if our height is smaller than a step
		// unless jump is pressed or the player is moving faster than dash speed and upwards
		float hspeed = Length( Vec3( pml->velocity.x, pml->velocity.y, 0 ) );
		pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );

		if( pml->upPush >= 10
			|| ( hspeed > pm->playerState->pmove.dash_speed && pml->velocity.z > 8 )
			|| ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			Vec3 normal( 0.0f );
			PlayerTouchWall( pm, pml, pmove_gs, 12, 0.3f, &normal );
			if( !Length( normal ) ) {
				return;
			}

			if( !( pm->playerState->pmove.pm_flags & PMF_SPECIAL_HELD )
				&& !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) ) {
				float oldupvelocity = pml->velocity.z;
				pml->velocity.z = 0.0;

				hspeed = Normalize2D( &pml->velocity );

				pml->velocity = GS_ClipVelocity( pml->velocity, normal, 1.0005f );
				pml->velocity = pml->velocity + normal * pm_wjbouncefactor;

				if( hspeed < pm_wjminspeed( pml ) ) {
					hspeed = pm_wjminspeed( pml );
				}

				pml->velocity = Normalize( pml->velocity );

				pml->velocity *= hspeed;
				pml->velocity.z = ( oldupvelocity > pm_wjupspeed ) ? oldupvelocity : pm_wjupspeed; // jal: if we had a faster upwards speed, keep it

				// set the walljumping state
				PM_ClearDash( pm );

				pm->playerState->pmove.pm_flags |= PMF_WALLJUMPING;
				pm->playerState->pmove.pm_flags |= PMF_SPECIAL_HELD;

				pm->playerState->pmove.pm_flags |= PMF_WALLJUMPCOUNT;

				pm->playerState->pmove.walljump_time = PM_WALLJUMP_TIMEDELAY;

				// Create the event
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_WALLJUMP, DirToU64( normal ) );
			}
		}
	} else {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}
}



static void PM_Dash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs ) {
	pm->playerState->pmove.pm_flags |= PMF_DASHING;
	pm->playerState->pmove.pm_flags |= PMF_SPECIAL_HELD;
	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml->groundplane.normal.z > 0 && pml->velocity.z < 0 && Dot( pml->groundplane.normal.xy(), pml->velocity.xy() ) > 0 ) {
		pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane.normal, PM_OVERBOUNCE );
	}

	float upspeed = Max2( 0.0f, pml->velocity.z ) + pm_dashupspeed;

	// ch : we should do explicit forwardPush here, and ignore sidePush ?
	Vec3 dashdir = pml->flatforward * pml->forwardPush + pml->right * pml->sidePush;
	dashdir.z = 0.0f;

	if( Length( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
		dashdir = pml->flatforward;
		pml->forwardPush = pml->dashPlayerSpeed;
	}

	dashdir = Normalize( dashdir );

	float actual_velocity = Normalize2D( &pml->velocity );
	if( actual_velocity <= pml->dashPlayerSpeed ) {
		dashdir *= pml->dashPlayerSpeed;
	} else {
		dashdir *= actual_velocity;
	}

	pml->velocity = dashdir;
	pml->velocity.z = upspeed;

	// return sound events only when the dashes weren't too close to each other
	if( pm->playerState->pmove.dash_time == 0 ) {
		if( Abs( pml->sidePush ) >= Abs( pml->forwardPush ) ) {
			if( pml->sidePush > 0 ) {
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 2 );
			}
			else {
				pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 1 );
			}
		}
		else if( pml->forwardPush < 0 ) {
			pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 3 );
		}
		else {
			pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 0 );
		}
	}

	pm->playerState->pmove.dash_time = PM_DASHJUMP_TIMEDELAY;
}






void PM_DefaultJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush < 10 ) {
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
	if( pml->groundplane.normal.z > 0 && pml->velocity.z < 0 && Dot( pml->groundplane.normal.xy(), pml->velocity.xy() ) > 0 ) {
		pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane.normal, PM_OVERBOUNCE );
	}


	float jumpSpeed = ( pm->waterlevel >= 2 ? pml->jumpPlayerSpeedWater : pml->jumpPlayerSpeed );

	pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_JUMP, 0 );
	pml->velocity.z = Max2( 0.0f, pml->velocity.z ) + jumpSpeed;

	// remove wj count
	PM_ClearDash( pm );
	PM_ClearWallJump( pm );
}



void PM_DefaultSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	bool pressed = pm->cmd.buttons & BUTTON_SPECIAL;

	if( GS_GetWeaponDef( ps->weapon )->zoom_fov != 0 && ( ps->pmove.features & PMFEAT_SCOPE ) != 0 ) {
		return;
	}

	if( !pressed ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( pm->playerState->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pressed && ( pm->playerState->pmove.features & PMFEAT_SPECIAL ) ) {
		if( pm->groundentity != -1 ) {
			PM_Dash( pm, pml, pmove_gs );
			PM_ClearWallJump( pm );
		} else if( pm->groundentity == -1 ) {
			PM_WallJump( pm, pml, pmove_gs );
			PM_ClearDash( pm );
		}
	} else {
		PM_ClearDash( pm );
		PM_ClearWallJump( pm );
	}
}