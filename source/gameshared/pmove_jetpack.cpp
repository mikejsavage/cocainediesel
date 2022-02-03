#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;

static constexpr float pm_jetpackspeed = 25.0f * 62.0f;
static constexpr float pm_maxjetpackupspeed = 150.0f;

static constexpr float pm_boostspeed = 5.0f * 62.0f;
static constexpr float pm_boostupspeed = 18.0f * 62.0f;

static constexpr float fuel_use_jetpack = 0.125f;
static constexpr float fuel_use_boost = 0.5f;
static constexpr float fuel_min = 0.01f;

static constexpr float refuel_min = 0.5f; //50%
static constexpr float refuel_ground = 0.5f;
static constexpr float refuel_air = 0.0f;


static void PM_JetpackJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pressed && StaminaAvailable( ps, pml, fuel_use_jetpack ) && !pml->ladder && ps->pmove.stamina_state != Stamina_Reloading ) {
		if( pm->groundentity != -1 ) {
			Jump( pm, pml, pmove_gs, ps, pm_maxjetpackupspeed, JumpType_Normal );
		}

		ps->pmove.stamina_state = Stamina_UsingAbility;
		StaminaUse( ps, pml, fuel_use_jetpack );
		pml->velocity.z = Min2( pml->velocity.z + pm_jetpackspeed * pml->frametime, pm_maxjetpackupspeed );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 0 );
	}

	if( ps->pmove.stamina < fuel_min ) {
		ps->pmove.stamina_state = Stamina_UsedAbility;
	}

	if( ( pm->groundentity != -1 || pm->waterlevel >= 2 ) ) {
		if( ps->pmove.stamina_state == Stamina_UsedAbility ) {
			ps->pmove.stamina_state = Stamina_Reloading;
		} else if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_Normal;
		}
	}
}



static void PM_JetpackSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( ps->pmove.stamina_state == Stamina_Normal || ps->pmove.stamina_state == Stamina_Reloading ) {
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

		ps->pmove.stamina_state = Stamina_UsingAbility;
		StaminaUse( ps, pml, fuel_use_boost );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 1 );
	}
}


void PM_JetpackInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, PM_JetpackJump, PM_JetpackSpecial );
}
