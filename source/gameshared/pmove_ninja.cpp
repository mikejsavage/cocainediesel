#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 400.0f;
static constexpr float pm_sidewalkspeed = 320.0f;
static constexpr float pm_crouchedspeed = 200.0f;

static constexpr float pm_wallclimbspeed = 250.0f;

static constexpr float pm_dashspeed = 550.0f;
static constexpr float pm_dashupspeed = ( 180.0f * GRAVITY_COMPENSATE );
static constexpr s16 pm_dashtimedelay = 200;

static constexpr s16 stamina_max = 300;
static constexpr s16 stamina_use = 1;
static constexpr s16 stamina_recover = 5;


static bool CheckWall( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs ) {
	Vec3 spot = pml->origin + pml->flatforward;
	trace_t trace;
	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->contentmask, 0 );
	return trace.fraction < 1;
}


static void PM_NinjaJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush < 10 ) {
		return;
	}

	if( pm->groundentity == -1 ) {
		return;
	}

	// ch : we should do explicit forwardPush here, and ignore sidePush ?
	Vec3 dashdir = pml->flatforward * pml->forwardPush + pml->right * pml->sidePush;
	if( Length( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
		dashdir = pml->flatforward;
		pml->forwardPush = pm_dashspeed;
	}

	PM_Dash( pm, pml, pmove_gs, dashdir, pm_dashspeed, pm_dashupspeed );

	pm->playerState->pmove.stamina_reload = pm_dashtimedelay;
}



static void PM_NinjaSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity != -1 ) {
		ps->pmove.stamina = Min2( s16( ps->pmove.stamina + stamina_recover ), stamina_max );
	}

	if( GS_GetWeaponDef( ps->weapon )->zoom_fov != 0 && ( ps->pmove.features & PMFEAT_SCOPE ) != 0 ) {
		return;
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pressed && ( ps->pmove.features & PMFEAT_SPECIAL ) && CheckWall( pm, pml, pmove_gs ) && ps->pmove.stamina > 0 ) {
		pml->ladder = Ladder_Fake;

		Vec3 wishvel = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
		wishvel.z = 0.0;

		if( !Length( wishvel ) ) {
			ps->pmove.stamina -= stamina_use;
			return;
		}

		wishvel = Normalize( wishvel );

		if( pml->forwardPush > 0 ) {
			wishvel.z = Lerp( -1.0, Unlerp01( 15.0f, ps->viewangles[PITCH], -15.0f ), 1.0 );
		}

		ps->pmove.stamina -= Length( wishvel ) * stamina_use;
		pml->velocity = wishvel * pm_wallclimbspeed;
	}
}


void PM_NinjaInit( pmove_t * pm, pml_t * pml, SyncPlayerState * ps ) {
	pml->maxPlayerSpeed = ps->pmove.max_speed;
	if( pml->maxPlayerSpeed < 0 ) {
		pml->maxPlayerSpeed = pm_defaultspeed;
	}

	pml->maxCrouchedSpeed = pm_crouchedspeed;

	pml->forwardPush = pm->cmd.forwardmove * pm_defaultspeed / 127.0f;
	pml->sidePush = pm->cmd.sidemove * pm_sidewalkspeed / 127.0f;
	pml->upPush = pm->cmd.upmove * pm_defaultspeed / 127.0f;

	ps->pmove.stamina_max = stamina_max;

	pml->jumpCallback = PM_NinjaJump;
	pml->specialCallback = PM_NinjaSpecial;
}