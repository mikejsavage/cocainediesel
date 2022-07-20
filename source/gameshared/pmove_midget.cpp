#include "gameshared/movement.h"

static constexpr float wallclimbspeed = 200.0f;

static constexpr float jumpspeed = 280.0f;

static constexpr float climbfriction = 10.0f;

static constexpr float stamina_use = 0.15f;
static constexpr float stamina_use_moving = 0.3f;
static constexpr float stamina_recover = 1.0f;


static bool CanClimb( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( !StaminaAvailable( ps, pml, stamina_use ) ) {
		return false;
	}

	Vec3 spot = pml->origin + pml->forward;
	trace_t trace;
	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->contentmask, 0 );
	return trace.fraction < 1 && !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid;
}


static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity == -1 ) {
		return;
	}

	ps->pmove.stamina_state = Stamina_Normal;

	if( !pressed ) {
		return;
	}

	Jump( pm, pml, pmove_gs, ps, jumpspeed, true );
}



static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
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
		pml->friction = climbfriction;

		Vec3 wishvel = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
		wishvel.z = 0.0;

		ps->pmove.stamina_state = Stamina_UsingAbility;
		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;

		if( !Length( wishvel ) ) {
			StaminaUse( ps, pml, stamina_use );
			return;
		}

		wishvel = Normalize( wishvel );

		if( pml->forwardPush > 0 ) {
			wishvel.z = Lerp( -1.0f, Unlerp01( 15.0f, ps->viewangles[ PITCH ], -15.0f ), 1.0f );
		}

		StaminaUse( ps, pml, Length( wishvel ) * stamina_use_moving );
		pml->velocity = wishvel * wallclimbspeed;

		pm->groundentity = -1;
	} else {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
}


void PM_MidgetInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Ninja, PM_MidgetJump, PM_MidgetSpecial );
}
