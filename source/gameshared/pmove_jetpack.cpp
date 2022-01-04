#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;

static constexpr float pm_sidewalkspeed = 320.0f;
static constexpr float pm_crouchedspeed = 160.0f;

static constexpr float pm_basejumpspeed = 150.0f;
static constexpr float pm_jetpackspeed = 20.0f;
static constexpr s16 pm_jetpackmax = 100;

static constexpr float pm_boostupspeed = 100.0f;
static constexpr float pm_boostspeed = 700.0f;







static void PM_JetpackJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( ( pm->groundentity != -1 || pm->waterlevel >= 2 ) && ps->pmove.special_count > 0 ) {
		ps->pmove.special_count -= 3;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( !( pm->playerState->pmove.features & PMFEAT_JUMP ) ) {
		return;
	}

	if( pml->upPush < 10 ) {
		return;
	}

	if( pm->groundentity != -1 ) {
		float jumpSpeed = ( pm->waterlevel >= 2 ? pml->jumpPlayerSpeedWater : pml->jumpPlayerSpeed );
		pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_JUMP, 0 );
		pml->velocity.z = Max2( 0.0f, pml->velocity.z ) + jumpSpeed;
		pm->groundentity = -1;
	} else if( ps->pmove.special_count < pm_jetpackmax ) {
		ps->pmove.special_count++;
		pml->velocity.z += pm_jetpackspeed;
	}

	ps->pmove.special_time = 0;
}



static void PM_JetpackSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	bool pressed = pm->cmd.buttons & BUTTON_SPECIAL;

	if( !pressed ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( GS_GetWeaponDef( ps->weapon )->zoom_fov != 0 && ( ps->pmove.features & PMFEAT_SCOPE ) != 0 ) {
		return;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( ps->pmove.special_count >= pm_jetpackmax ) {
		return;
	}

	if( pressed && ( pm->playerState->pmove.features & PMFEAT_SPECIAL ) ) {
		Vec3 dashdir = pml->flatforward;
		pml->forwardPush = pml->dashPlayerSpeed;

		dashdir = Normalize( dashdir );
		dashdir *= pml->dashPlayerSpeed;

		pml->velocity.x = dashdir.x;
		pml->velocity.y = dashdir.y;
		if( pm->groundentity != -1 ) {
			pml->velocity.z = Max2( 0.0f, pml->velocity.z ) + pm_boostupspeed;
			pm->groundentity = -1;
		}

		ps->pmove.special_count += 2;
	}
}


void PM_JetpackInit( pmove_t * pm, pml_t * pml, SyncPlayerState * ps ) {
	pml->maxPlayerSpeed = ps->pmove.max_speed;
	if( pml->maxPlayerSpeed < 0 ) {
		pml->maxPlayerSpeed = pm_defaultspeed;
	}

	pml->jumpPlayerSpeed = (float)ps->pmove.jump_speed * GRAVITY_COMPENSATE;
	pml->jumpPlayerSpeedWater = pml->jumpPlayerSpeed * 2;

	if( pml->jumpPlayerSpeed < 0 ) {
		pml->jumpPlayerSpeed = pm_basejumpspeed * GRAVITY_COMPENSATE;
		pml->jumpPlayerSpeedWater = pml->jumpPlayerSpeed * 2;
	}

	pml->dashPlayerSpeed = ps->pmove.dash_speed;
	if( pml->dashPlayerSpeed < 0 ) {
		pml->dashPlayerSpeed = pm_boostspeed;
	}

	pml->maxCrouchedSpeed = pm_crouchedspeed;

	pml->forwardPush = pm->cmd.forwardmove * pm_defaultspeed / 127.0f;
	pml->sidePush = pm->cmd.sidemove * pm_sidewalkspeed / 127.0f;
	pml->upPush = pm->cmd.upmove * pm_defaultspeed / 127.0f;

	pml->jumpCallback = PM_JetpackJump;
	pml->specialCallback = PM_JetpackSpecial;
}