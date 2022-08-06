#include "gameshared/movement.h"

static constexpr float charge_jump_speed = 950.0f;
static constexpr float charge_min_speed = 350.0f;
static constexpr float charge_slide_time = 1.0f;

static constexpr float slide_friction = 0.8f;
static constexpr float slide_speed_fact = 0.5f;

static constexpr float stamina_use = 2.5f;
static constexpr float stamina_recover = 8.0f;

static constexpr float floor_distance = STEPSIZE * 0.5f;

static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( (pm->groundentity != -1 || pml->ladder) && (ps->pmove.stamina == 0.0f ) ) {
		ps->pmove.stamina_state = Stamina_Normal;
	}

	if( pressed && !pml->ladder ) {
		pml->friction = slide_friction;
		pml->maxSpeed *= slide_speed_fact;
	}
}


//in this one we don't care about pressing special
static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	bool can_start_charge = ps->pmove.stamina_state == Stamina_Normal;

	ps->pmove.stamina_stored = Max2( 0.0f, ps->pmove.stamina_stored - pml->frametime );
	pml->friction = slide_friction + (pml->friction - slide_friction) * (charge_slide_time - ps->pmove.stamina_stored)/charge_slide_time;

	if( pressed ) {
		if( ps->pmove.stamina_state == Stamina_UsingAbility ||
			( can_start_charge && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) ) //a bit tricky but we don't want midget charge to start if we were already pressing before reaching can_start_charge
		{
			if( can_start_charge ) {
				ps->pmove.stamina_state = Stamina_UsingAbility;
			}
			StaminaRecover( ps, pml, stamina_use );
		}

		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;
	}

	if( ps->pmove.stamina_state == Stamina_UsedAbility ) {
		StaminaUse( ps, pml, stamina_recover );
	}

	if( !pressed ) {
		float special_jumpspeed = charge_jump_speed * ps->pmove.stamina;
		if( ( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) && ps->pmove.stamina_state == Stamina_UsingAbility && special_jumpspeed > charge_min_speed ) {
			Vec3 fwd;
			AngleVectors( pm->playerState->viewangles, &fwd, NULL, NULL );
			Vec3 dashdir = fwd;

			dashdir = Normalize( dashdir );
			dashdir *= special_jumpspeed;

			pm->groundentity = -1;
			pml->velocity = dashdir;

			ps->pmove.stamina_stored = charge_slide_time;

			pmove_gs->api.PredictedEvent( ps->POVnum, EV_JUMP, JumpType_MidgetCharge );
		} else if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_Normal;
		}

		if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_UsedAbility;
		}

		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
}


void PM_MidgetInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Midget, PM_MidgetJump, PM_MidgetSpecial );
}
