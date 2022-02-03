#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;

static constexpr float pm_jetpackspeed = 25.0f * 62.0f;
static constexpr float pm_maxjetpackupspeed = 600.0f;

static constexpr float pm_boostspeed = 15.0f * 62.0f;
static constexpr float pm_boostupspeed = 18.0f * 62.0f;

static constexpr float fuel_use_jetpack = 1.25f;
static constexpr float fuel_use_boost = 2.0f;
static constexpr float fuel_min = 0.01f;

static constexpr float refuel_ground = 1.0f;
static constexpr float refuel_air = 0.3f;

static constexpr float STATE_NOFUEL = -2.0f;
static constexpr float STATE_AIR = -1.0f;
static constexpr float STATE_NORMAL = 0.0f;



static void PM_JetpackJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pressed && StaminaAvailable( ps, pml, fuel_use_jetpack ) && !pml->ladder ) {
		pm->groundentity = -1;

		ps->pmove.stamina_state = STATE_AIR;
		StaminaUse( ps, pml, fuel_use_jetpack );
		pml->velocity.z = Min2( pml->velocity.z + pm_jetpackspeed * pml->frametime, pm_maxjetpackupspeed );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 0 );
	}

	if( ps->pmove.stamina < fuel_min ) {
		ps->pmove.stamina_state = STATE_NOFUEL;
	}

	if( pm->groundentity != -1 || pm->waterlevel >= 2 ) {
		ps->pmove.stamina_state = STATE_NORMAL;
	}

	if( ps->pmove.stamina_state == STATE_NORMAL ) {
		StaminaRecover( ps, pml, refuel_ground );
	} else if( ps->pmove.stamina_state != STATE_NOFUEL ) {
		StaminaRecover( ps, pml, refuel_air );
	}
}



static void PM_JetpackSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pressed && StaminaAvailable( ps, pml, fuel_use_boost ) ) {
		Vec3 dashdir = pml->forward;
		pml->forwardPush = pm_boostspeed;

		dashdir = Normalize( dashdir );
		dashdir *= pm_boostspeed * pml->frametime;

		pml->velocity.x += dashdir.x;
		pml->velocity.y += dashdir.y;
		pml->velocity.z += pm_boostupspeed * pml->frametime;
		pm->groundentity = -1;

		ps->pmove.stamina_state = STATE_AIR;
		StaminaUse( ps, pml, fuel_use_boost );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 1 );
	}
}


void PM_JetpackInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, PM_JetpackJump, PM_JetpackSpecial );
}
