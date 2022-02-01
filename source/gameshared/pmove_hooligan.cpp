#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;

static constexpr float pm_jumpupspeed = 280.0f;
static constexpr float pm_dashspeed = 480.0f;
static constexpr s16 pm_dashtimedelay = 200;

static constexpr float pm_wjupspeed = ( 350.0f * GRAVITY_COMPENSATE );
static constexpr float pm_wjbouncefactor = 0.4f;

static constexpr s16 stamina_max = 200;
static constexpr s16 stamina_use = 110;
static constexpr s16 stamina_recover_ground = 10;
static constexpr s16 stamina_recover_air = 1;


static float pm_wjminspeed( pml_t * pml ) {
	return ( pml->maxCrouchedSpeed + pml->maxPlayerSpeed ) * 0.5f;
}


static void PM_HooliganJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush < 1 ) {
		return;
	}

	if( pm->groundentity == -1 ) {
		return;
	}

	float oldupvelocity = pml->velocity.z;
	pml->velocity.z = 0.0;
	float hspeed = Length( pml->velocity );
	pml->velocity.z = oldupvelocity;

	if( hspeed < pm_defaultspeed ) {
		PM_Dash( pm, pml, pmove_gs, pml->forward * pml->forwardPush + pml->right * pml->sidePush, pm_dashspeed, pm_jumpupspeed );
	} else if( pml->forwardPush > 0 && pml->sidePush == 0.0f ) { //sidePush is for people strafing
		PM_Dash( pm, pml, pmove_gs, pml->forward * pml->forwardPush, pm_dashspeed, pm_jumpupspeed );
	} else {
		PM_Jump( pm, pml, pmove_gs, ps, pm_jumpupspeed );
	}
}



static void PM_HooliganSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity != -1 ) {
		StaminaRecover( ps, stamina_recover_ground );
	} else {
		StaminaRecover( ps, stamina_recover_air );
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pressed && ( ps->pmove.features & PMFEAT_SPECIAL ) && pm->groundentity == -1 && ps->pmove.stamina >= stamina_use ) {
		trace_t trace;
		Vec3 point = pml->origin;
		point.z -= STEPSIZE;

		// don't walljump if our height is smaller than a step
		// unless jump is pressed or the player is moving faster than dash speed and upwards
		float hspeed = Length( Vec3( pml->velocity.x, pml->velocity.y, 0 ) );
		pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, ps->POVnum, pm->contentmask, 0 );

		if( pml->upPush == 1
			|| ( hspeed > pm_dashspeed && pml->velocity.z > 8 )
			|| ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			Vec3 normal( 0.0f );
			PlayerTouchWall( pm, pml, pmove_gs, 12, 0.3f, &normal );
			if( !Length( normal ) )
				return;

			if( !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) {
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
				PM_ClearDash( ps );

				ps->pmove.pm_flags |= PMF_ABILITY2_HELD;

				StaminaUse( ps, stamina_use );

				// Create the event
				pmove_gs->api.PredictedEvent( ps->POVnum, EV_WALLJUMP, DirToU64( normal ) );
			}
		}
	}
}


void PM_HooliganInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, stamina_max, PM_HooliganJump, PM_HooliganSpecial );
}