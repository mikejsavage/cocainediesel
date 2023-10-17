#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"

static constexpr float jumpupspeed = 260.0f;
static constexpr float dashupspeed = 150.0f;
static constexpr float dashspeed = 625.0f;

static constexpr float wjupspeed = 371.875f;
static constexpr float wjbouncefactor = 0.4f;

static constexpr float stamina_usewj = 0.5f; //50%
static constexpr float stamina_recover = 1.5f;


static void PM_HooliganJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( ps->pmove.stamina_state == Stamina_Normal ) {
		StaminaRecover( ps, pml, stamina_recover );
	}

	if( pm->groundentity == -1 && !pml->ladder ) {
		return;
	}

	ps->pmove.stamina_state = Stamina_Normal;

	if( !pressed ) {
		return;
	}

	Jump( pm, pml, pmove_gs, ps, jumpupspeed, true );
}


static void PM_HooliganWalljump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( !StaminaAvailableImmediate( ps, stamina_usewj ) || (ps->pmove.pm_flags & PMF_ABILITY2_HELD) )
		return;

	if( Walljump( pm, pml, pmove_gs, ps, jumpupspeed, dashupspeed, dashspeed, wjupspeed, wjbouncefactor ) ) {
		StaminaUseImmediate( ps, stamina_usewj );

		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;
		ps->pmove.stamina_state = Stamina_UsedAbility;
	}
}


static void PM_HooliganDash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	Vec3 dashdir = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
	if( Length( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
		dashdir = pml->forward;
		pml->forwardPush = dashspeed;
	}

	Dash( pm, pml, pmove_gs, dashdir, dashspeed, dashupspeed );
}





static void PM_HooliganSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( !pressed ) {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	ps->pmove.stamina_stored = Max2( 0.0f, ps->pmove.stamina_stored - pml->frametime );

	if( pressed ) {
		if( pm->groundentity == -1 ) {
			PM_HooliganWalljump( pm, pml, pmove_gs, ps );
		} else {
			PM_HooliganDash( pm, pml, pmove_gs, ps );
		}
	}
}


void PM_HooliganInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Hooligan, PM_HooliganJump, PM_HooliganSpecial );
}
