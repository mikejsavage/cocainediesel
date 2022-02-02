#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 400.0f;
static constexpr float pm_sidewalkspeed = 320.0f;

static constexpr float pm_wallclimbspeed = 200.0f;

static constexpr float pm_dashspeed = 550.0f;
static constexpr float pm_dashupspeed = ( 180.0f * GRAVITY_COMPENSATE );

static constexpr float stamina_max = 300.0f / 62.0f;
static constexpr float stamina_use = 1.0f;
static constexpr float stamina_use_moving = 1.5f;
static constexpr float stamina_recover = 10.0f;


static bool CheckWall( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs ) {
	Vec3 spot = pml->origin + pml->forward;
	trace_t trace;
	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->contentmask, 0 );
	return trace.fraction < 1;
}


static void PM_NinjaJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( !pressed ) {
		return;
	}

	if( pm->groundentity == -1 ) {
		return;
	}

	// ch : we should do explicit forwardPush here, and ignore sidePush ?
	Vec3 dashdir = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
	if( Length( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
		dashdir = pml->forward;
		pml->forwardPush = pm_dashspeed;
	}

	Dash( pm, pml, pmove_gs, dashdir, pm_dashspeed, pm_dashupspeed );
}



static void PM_NinjaSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity != -1 ) {
		StaminaRecover( ps, pml, stamina_recover );
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pml->ladder ) {
		return;
	}

	if( pressed && CheckWall( pm, pml, pmove_gs ) && StaminaAvailable( ps, pml, stamina_use ) ) {
		pml->ladder = Ladder_Fake;

		Vec3 wishvel = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
		wishvel.z = 0.0;

		if( !Length( wishvel ) ) {
			StaminaUse( ps, pml, stamina_use );
			return;
		}

		wishvel = Normalize( wishvel );

		if( pml->forwardPush > 0 ) {
			wishvel.z = Lerp( -1.0, Unlerp01( 15.0f, ps->viewangles[ PITCH ], -15.0f ), 1.0 );
		}

		StaminaUse( ps, pml, Length( wishvel ) * stamina_use_moving );
		pml->velocity = wishvel * pm_wallclimbspeed;
	}
}


void PM_NinjaInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, stamina_max, PM_NinjaJump, PM_NinjaSpecial );
}
