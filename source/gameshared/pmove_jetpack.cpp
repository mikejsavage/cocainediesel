#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;
static constexpr float pm_crouchedspeed = 160.0f;

static constexpr float pm_jetpackspeed = 25.0f;
static constexpr float pm_maxjetpackupspeed = 600.0f;

static constexpr float pm_boostspeed = 15.0f;
static constexpr float pm_boostupspeed = 18.0f;


static constexpr s16 ground_refuel = 8;
static constexpr s16 air_refuel = 1;

static constexpr s16 jetpack_fuel_usage = 4;
static constexpr s16 boost_fuel_usage = 8;

static constexpr s16 maxfuel = 200;
static constexpr s16 jetpackmax = maxfuel - ground_refuel;




static void PM_JetpackRefuel( pmove_t * pm, SyncPlayerState * ps ) {
	if( ps->pmove.special_count > 0 ) {
		if( pm->groundentity != -1 || pm->waterlevel >= 2 ) {
			ps->pmove.special_count -= ground_refuel;
		} else {
			ps->pmove.special_count -= air_refuel;
		}
	}
}



static void PM_JetpackJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pm->playerState->pmove.pm_type == PM_NORMAL &&
		pm->playerState->pmove.features & PMFEAT_JUMP &&
		pml->upPush > 10 &&
		ps->pmove.special_count < jetpackmax )
	{
		pm->groundentity = -1;
		ps->pmove.special_count += jetpack_fuel_usage;
		pml->velocity.z = Min2( pml->velocity.z + pm_jetpackspeed, pm_maxjetpackupspeed );

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 0 );
	}
}



static void PM_JetpackSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	bool pressed = pm->cmd.buttons & BUTTON_SPECIAL;

	if( !pressed ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( ( GS_GetWeaponDef( ps->weapon )->zoom_fov == 0 || ( ps->pmove.features & PMFEAT_SCOPE ) == 0 ) &&
		pm->playerState->pmove.pm_type == PM_NORMAL &&
		ps->pmove.special_count < jetpackmax &&
		pressed && ( pm->playerState->pmove.features & PMFEAT_SPECIAL ) )
	{
		Vec3 dashdir = pml->flatforward;
		pml->forwardPush = pm_boostspeed;

		dashdir = Normalize( dashdir );
		dashdir *= pm_boostspeed;

		pml->velocity.x += dashdir.x;
		pml->velocity.y += dashdir.y;
		pml->velocity.z += pm_boostupspeed;
		pm->groundentity = -1;

		ps->pmove.special_count += boost_fuel_usage;

		pmove_gs->api.PredictedEvent( ps->POVnum, EV_JETPACK, 1 );
	}


	PM_JetpackRefuel( pm, ps );
}


void PM_JetpackInit( pmove_t * pm, pml_t * pml, SyncPlayerState * ps ) {
	pml->maxPlayerSpeed = ps->pmove.max_speed;
	if( pml->maxPlayerSpeed < 0 ) {
		pml->maxPlayerSpeed = pm_defaultspeed;
	}

	pml->maxCrouchedSpeed = pm_crouchedspeed;

	pml->forwardPush = pm->cmd.forwardmove * pm_defaultspeed / 127.0f;
	pml->sidePush = pm->cmd.sidemove * pm_sidewalkspeed / 127.0f;
	pml->upPush = pm->cmd.upmove * pm_defaultspeed / 127.0f;

	pml->jumpCallback = PM_JetpackJump;
	pml->specialCallback = PM_JetpackSpecial;
}