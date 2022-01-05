#include "gameshared/movement.h"


static constexpr float pm_defaultspeed = 500.0f;
static constexpr float pm_sidewalkspeed = 500.0f;
static constexpr float pm_crouchedspeed = 200.0f;

static constexpr float pm_jumpspeed = 600.0f;
static constexpr s16 pm_jumpboostdelay = 30;



static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( ps->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( !( ps->pmove.features & PMFEAT_JUMP ) ) {
		return;
	}

	if( pml->upPush >= 10 ) {
		pm->playerState->pmove.crouch_time = 1;
		if( ps->pmove.special_count !=  pm_jumpboostdelay ) {
			ps->pmove.special_count++;
		}
	} else if( ps->pmove.special_count != 0 ) {
		if( pm->groundentity == -1 ) {
			ps->pmove.special_count = 0;
			return;
		}

		pm->groundentity = -1;

		// clip against the ground when jumping if moving that direction
		if( pml->groundplane.normal.z > 0 && pml->velocity.z < 0 && Dot( pml->groundplane.normal.xy(), pml->velocity.xy() ) > 0 ) {
			pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane.normal, PM_OVERBOUNCE );
		}

		float jumpSpeed = ( pm->waterlevel >= 2 ? 2 : 1 ) * pm_jumpspeed * (float)ps->pmove.special_count / pm_jumpboostdelay;
		Event_Jump( pmove_gs, ps );
		pml->velocity.z = Max2( 0.0f, pml->velocity.z ) + jumpSpeed;

		ps->pmove.special_count = 0;
	}
}



static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	return;
}


void PM_MidgetInit( pmove_t * pm, pml_t * pml, SyncPlayerState * ps ) {
	pml->maxPlayerSpeed = ps->pmove.max_speed;
	if( pml->maxPlayerSpeed < 0 ) {
		pml->maxPlayerSpeed = pm_defaultspeed;
	}

	pml->maxCrouchedSpeed = pm_crouchedspeed;

	pml->forwardPush = pm->cmd.forwardmove * pm_defaultspeed / 127.0f;
	pml->sidePush = pm->cmd.sidemove * pm_sidewalkspeed / 127.0f;
	pml->upPush = pm->cmd.upmove * pm_defaultspeed / 127.0f;

	pml->jumpCallback = PM_MidgetJump;
	pml->specialCallback = PM_MidgetSpecial;
}