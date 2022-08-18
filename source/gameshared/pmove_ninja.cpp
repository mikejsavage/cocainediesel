#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"

static constexpr float pm_wallclimbspeed = 200.0f;

static constexpr float pm_dashspeed = 550.0f;
static constexpr float pm_dashupspeed = 191.25f;

static constexpr float stamina_use = 0.2f;
static constexpr float stamina_use_moving = 0.3f;
static constexpr float stamina_recover = 1.0f;


static bool CanClimb( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( !StaminaAvailable( ps, pml, stamina_use ) ) {
		return false;
	}

	Vec3 spot = pml->origin + pml->forward;
	trace_t trace;
	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->solid_mask, 0 );
	return trace.fraction < 1;
}


static void PM_NinjaJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity == -1 ) {
		return;
	}

	ps->pmove.stamina_state = Stamina_Normal;

	if( !pressed ) {
		return;
	}

	// ch : we should do explicit forwardPush here, and ignore sidePush ?
	Vec3 dashdir = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
	if( Length( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
		dashdir = pml->forward;
		pml->forwardPush = pm_dashspeed;
	}

	Dash( pm, pml, pmove_gs, dashdir, pm_dashspeed, pm_dashupspeed );
}



static void PM_NinjaSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( ps->pmove.stamina_state == Stamina_Normal && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) {
		StaminaRecover( ps, pml, stamina_recover );
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pml->ladder ) {
		return;
	}

	if( pressed && CanClimb( pm, pml, pmove_gs, ps ) ) {
		pml->ladder = Ladder_Fake;

		Vec3 wishvel = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
		wishvel.z = 0.0;

		if( !Length( wishvel ) ) {
			StaminaUse( ps, pml, stamina_use );
			return;
		}

		wishvel = Normalize( wishvel );

		if( pml->forwardPush > 0 ) {
			wishvel.z = Lerp( -1.0f, Unlerp01( 15.0f, ps->viewangles[ PITCH ], -15.0f ), 1.0f );
		}

		ps->pmove.stamina_state = Stamina_UsingAbility;
		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;

		StaminaUse( ps, pml, Length( wishvel ) * stamina_use_moving );
		pml->velocity = wishvel * pm_wallclimbspeed;
	} else {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
}


void PM_NinjaInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Ninja, PM_NinjaJump, PM_NinjaSpecial );
}
