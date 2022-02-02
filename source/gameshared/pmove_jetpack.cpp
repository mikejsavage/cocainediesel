#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;

static constexpr float pm_jetpackspeed = 25.0f * 62.0f;
static constexpr float pm_maxjetpackupspeed = 600.0f;

static constexpr float pm_boostspeed = 15.0f * 62.0f;
static constexpr float pm_boostupspeed = 18.0f * 62.0f;

static constexpr float fuel_max = 200.0f / 62.0f;
static constexpr float fuel_use_jetpack = 4.0f;
static constexpr float fuel_use_boost = 6.0f;

static constexpr float refuel_ground = 4.0f;
static constexpr float refuel_air = 1.0f;



static void PM_JetpackJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush == 1 && StaminaAvailable( ps, pml, fuel_use_jetpack ) && !pml->ladder ) {
		pm->groundentity = -1;
		StaminaUse( ps, pml, fuel_use_jetpack );
		pml->velocity.z = Min2( pml->velocity.z + pm_jetpackspeed * pml->frametime, pm_maxjetpackupspeed );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 0 );
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

		StaminaUse( ps, pml, fuel_use_boost );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 1 );
	}


	if( pm->groundentity != -1 || pm->waterlevel >= 2 ) {
		StaminaRecover( ps, pml, refuel_ground );
	} else {
		StaminaRecover( ps, pml, refuel_air );
	}
}


void PM_JetpackInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, fuel_max, PM_JetpackJump, PM_JetpackSpecial );
}
