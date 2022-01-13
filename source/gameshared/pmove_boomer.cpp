#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 300.0f;
static constexpr float pm_sidewalkspeed = 300.0f;
static constexpr float pm_crouchedspeed = 100.0f;

static constexpr float jump_upspeed = 250.0f;
static constexpr s16 jump_detection = 50;


static constexpr float charge_groundAccel = 2.0f;
static constexpr float charge_friction = 0.75f;
static constexpr float charge_speed = 900.0f;
static constexpr float charge_sidespeed = 400.0f;

static constexpr s16 stamina_max = 300;
static constexpr s16 stamina_limit = stamina_max * 0.5f;
static constexpr s16 stamina_use = 2;
static constexpr s16 stamina_recover = 1;


static void PM_BoomerJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( ps->pmove.pm_flags & PMF_SPECIAL_HELD ) {
		return;
	}

	if( pml->upPush < 1 ) {
		ps->pmove.pm_flags &= ~PMF_JUMP_HELD;
	} else {
		if( !( ps->pmove.pm_flags & PMF_JUMP_HELD ) ) {
			ps->pmove.stamina_time = jump_detection;
		}

		ps->pmove.pm_flags |= PMF_JUMP_HELD;

		if( pm->groundentity == -1 ) {
			return;
		}

		if( ( ps->pmove.pm_flags & PMF_JUMP_HELD ) && ps->pmove.stamina_time == 0 ) {
			return;
		}

		ps->pmove.stamina_time = 0;
		PM_Jump( pm, pml, pmove_gs, ps, jump_upspeed );
	}
}



static void PM_BoomerSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( !pressed ) {
		ps->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( ( ps->pmove.pm_flags & PMF_SPECIAL_HELD ) && !pml->ladder ) {
		if( ps->pmove.stamina >= stamina_use ) {
			StaminaUse( ps, stamina_use );
			pml->maxPlayerSpeed = charge_speed;
			pml->maxCrouchedSpeed = charge_speed;
			
			pml->groundAccel = charge_groundAccel;
			pml->friction = charge_friction;

			pml->forwardPush = charge_speed;
			pml->sidePush = pm->cmd.sidemove * charge_sidespeed;
		}
	} else {
		StaminaRecover( ps, stamina_recover );

		if( pressed && ps->pmove.stamina >= stamina_limit ) {
			ps->pmove.pm_flags |= PMF_SPECIAL_HELD;
		}
	}
}


void PM_BoomerInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_crouchedspeed, pm_sidewalkspeed, stamina_max, PM_BoomerJump, PM_BoomerSpecial );
}