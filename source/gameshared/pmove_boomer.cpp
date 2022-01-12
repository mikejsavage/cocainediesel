#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 300.0f;
static constexpr float pm_sidewalkspeed = 300.0f;
static constexpr float pm_crouchedspeed = 100.0f;

static constexpr float pm_jumpupspeed = 250.0f;

static constexpr float pm_chargespeed = 600.0f;
static constexpr float pm_chargesidespeed = 200.0f;

static constexpr s16 pm_boomerjumpdetection = 50;

static constexpr s16 stamina_max = 300;
static constexpr s16 stamina_use = 3;
static constexpr s16 stamina_recover = 1;


static void PM_BoomerJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( ps->pmove.pm_flags & PMF_SPECIAL_HELD ) {
		return;
	}

	if( pml->upPush < 1 ) {
		ps->pmove.pm_flags &= ~PMF_JUMP_HELD;
	} else {
		if( !( ps->pmove.pm_flags & PMF_JUMP_HELD ) ) {
			ps->pmove.stamina_time = pm_boomerjumpdetection;
		}

		ps->pmove.pm_flags |= PMF_JUMP_HELD;

		if( pm->groundentity == -1 ) {
			return;
		}

		if( ( ps->pmove.pm_flags & PMF_JUMP_HELD ) && ps->pmove.stamina_time == 0 ) {
			return;
		}

		ps->pmove.stamina_time = 0;
		PM_Jump( pm, pml, pmove_gs, ps, pm_jumpupspeed );
	}
}



static void PM_BoomerSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( !pressed ) {
		ps->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( ( ps->pmove.pm_flags & PMF_SPECIAL_HELD ) && !pml->ladder ) {
		if( ps->pmove.stamina >= stamina_use ) {
			StaminaUse( ps, stamina_use );
			pml->maxPlayerSpeed = pm_chargespeed;
			pml->maxCrouchedSpeed = pm_chargespeed;
			pml->forwardPush = pm_chargespeed;
			pml->sidePush = pm->cmd.sidemove * pm_chargesidespeed;
		}
	} else {
		StaminaRecover( ps, stamina_recover );

		if( pressed && ps->pmove.stamina == stamina_max ) {
			ps->pmove.pm_flags |= PMF_SPECIAL_HELD;
		}
	}
}


void PM_BoomerInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_crouchedspeed, pm_sidewalkspeed, stamina_max, PM_BoomerJump, PM_BoomerSpecial );
}