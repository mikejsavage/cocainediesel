#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_jumpspeed = 220.0f;
static constexpr float jump_detection = 0.06f; //slight jump buffering

static constexpr float pm_jetpackspeed = 25.0f * 62.0f;
static constexpr float pm_maxjetpackupspeed = 82.0f;
static constexpr float pm_maxjetpackupspeedslowdown = 0.75f;

static constexpr float pm_boostspeed = 7.45f * 62.0f;
static constexpr float pm_boostupspeed = 15.0f * 62.0f;

static constexpr float fuel_use_jetpack = 0.2f;
static constexpr float fuel_use_boost = 0.6f;
static constexpr float fuel_min = 0.01f;

static constexpr float refuel_min = 1.0f; //50%
static constexpr float refuel_ground = 0.75f;
static constexpr float refuel_air = 0.0f;


static void PM_JetpackJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pressed ) {
		ps->pmove.jump_buffering = Max2( 0.0f, ps->pmove.jump_buffering - pml->frametime );

		if( !(ps->pmove.pm_flags & PMF_ABILITY1_HELD) ) {
			ps->pmove.jump_buffering = jump_detection;
		}

		if( pm->groundentity != -1 && ps->pmove.jump_buffering != 0.0f ) {
			Jump( pm, pml, pmove_gs, ps, pm_jumpspeed, true );
		}

		ps->pmove.pm_flags |= PMF_ABILITY1_HELD;

		if( StaminaAvailable( ps, pml, fuel_use_jetpack ) && !pml->ladder && ps->pmove.stamina_state != Stamina_Reloading && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) {
			ps->pmove.stamina_state = Stamina_UsingAbility;
			StaminaUse( ps, pml, fuel_use_jetpack );
			if( pml->velocity.z <= pm_maxjetpackupspeed ) {
				pml->velocity.z = Min2( pml->velocity.z + pm_jetpackspeed * pml->frametime, pm_maxjetpackupspeed );
			} else {
				pml->velocity.z += GRAVITY * pm_maxjetpackupspeedslowdown * pml->frametime;
			}

			pm->groundentity = -1;
			pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 0 );
		}
	} else {
		ps->pmove.pm_flags &= ~PMF_ABILITY1_HELD;
	}

	if( ps->pmove.stamina < fuel_min ) {
		ps->pmove.stamina_state = Stamina_UsedAbility;
	}

	if( ( pm->groundentity != -1 || pm->waterlevel >= 2 || pml->ladder ) ) {
		if( ps->pmove.stamina_state == Stamina_UsedAbility ) {
			ps->pmove.stamina_state = Stamina_Reloading;
		} else if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_Normal;
		}
	}
}



static void PM_JetpackSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( (ps->pmove.stamina_state == Stamina_Normal || ps->pmove.stamina_state == Stamina_Reloading) && !(ps->pmove.pm_flags & PMF_ABILITY2_HELD) ) {
		StaminaRecover( ps, pml, refuel_ground );
		if( ps->pmove.stamina >= refuel_min ) {
			ps->pmove.stamina_state = Stamina_Normal;
		}
	} else if( ps->pmove.stamina_state != Stamina_Reloading && ps->pmove.stamina_state != Stamina_UsedAbility ) {
		StaminaRecover( ps, pml, refuel_air );
	}

	if( pressed && StaminaAvailable( ps, pml, fuel_use_boost ) && ps->pmove.stamina_state != Stamina_Reloading ) {
		Vec3 fwd, right;
		AngleVectors( pm->playerState->viewangles, &fwd, &right, NULL );
		Vec3 dashdir = fwd * pml->forwardPush + right * pml->sidePush;
		if( Length( dashdir ) < 0.01f ) { // no direction keys pressed
			dashdir = fwd;
			pml->forwardPush = pm_boostspeed;
		}

		dashdir = Normalize( dashdir );
		dashdir *= pm_boostspeed * pml->frametime;

		pml->velocity += dashdir;
		pml->velocity.z += pm_boostupspeed * pml->frametime;
		pm->groundentity = -1;

		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;
		ps->pmove.stamina_state = Stamina_UsingAbility;
		StaminaUse( ps, pml, fuel_use_boost );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 1 );
	} else {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
}


void PM_JetpackInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Jetpack, PM_JetpackJump, PM_JetpackSpecial );
}
