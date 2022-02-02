#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 300.0f;
static constexpr float pm_sidewalkspeed = 300.0f;

static constexpr float jump_upspeed = 270.0f;
static constexpr s16 jump_detection = 50;


static constexpr float charge_groundAccel = 2.0f;
static constexpr float charge_friction = 0.75f;
static constexpr float charge_speed = 900.0f;
static constexpr float charge_sidespeed = 400.0f;

static constexpr float stamina_max = 300.0f / 62.0f;
static constexpr float stamina_limit = stamina_max * 0.5f;
static constexpr float stamina_use = 2.0f;
static constexpr float stamina_recover = 1.0f;


static void PM_BoomerJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) {
		return;
	}

	if( pml->upPush < 1 ) {
		ps->pmove.pm_flags &= ~PMF_ABILITY1_HELD;
	} else {
		if( !( ps->pmove.pm_flags & PMF_ABILITY1_HELD ) ) {
			ps->pmove.stamina_time = jump_detection;
		}

		ps->pmove.pm_flags |= PMF_ABILITY1_HELD;

		if( pm->groundentity == -1 ) {
			return;
		}

		if( ( ps->pmove.pm_flags & PMF_ABILITY1_HELD ) && ps->pmove.stamina_time == 0 ) {
			return;
		}

		ps->pmove.stamina_time = 0;
		Jump( pm, pml, pmove_gs, ps, jump_upspeed, JumpType_Normal );
	}
}



static void PM_BoomerSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( !pressed ) {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}

	if( ( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) && !pml->ladder ) {
		if( StaminaAvailable( ps, pml, stamina_use ) ) {
			StaminaUse( ps, pml, stamina_use );
			pml->maxPlayerSpeed = charge_speed;
			pml->maxCrouchedSpeed = charge_speed;
			
			pml->groundAccel = charge_groundAccel;
			pml->friction = charge_friction;

			pml->forwardPush = charge_speed;
			pml->sidePush = pm->cmd.sidemove * charge_sidespeed;
		}
	} else {
		StaminaRecover( ps, pml, stamina_recover );

		if( pressed && ps->pmove.stamina >= stamina_limit ) {
			ps->pmove.pm_flags |= PMF_ABILITY2_HELD;
		}
	}
}


void PM_BoomerInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, stamina_max, PM_BoomerJump, PM_BoomerSpecial );
}
