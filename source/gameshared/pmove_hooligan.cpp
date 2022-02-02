#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;

static constexpr float pm_jumpupspeed = 280.0f;
static constexpr float pm_dashupspeed = 160.0f;
static constexpr float pm_dashspeed = 550.0f;
static constexpr s16 pm_dashtimedelay = 200;

static constexpr float pm_wjupspeed = ( 350.0f * GRAVITY_COMPENSATE );
static constexpr float pm_wjbouncefactor = 0.4f;

static constexpr float stamina_usewj = 34.0f / 62.0f;
static constexpr float stamina_usedash = 37.0f / 62.0f;
static constexpr float stamina_recover_ground = 2.5f;
static constexpr float stamina_recover_air = 0.3f;


static void PM_HooliganJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity == -1 ) {
		StaminaRecover( ps, pml, stamina_recover_air );
		return;
	}

	if( !pressed ) {
		StaminaRecover( ps, pml, stamina_recover_ground );
		return;
	}

	Jump( pm, pml, pmove_gs, ps, pm_jumpupspeed, JumpType_Normal );
}


static void PM_HooliganWalljump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( !StaminaAvailableImmediate( ps, stamina_usewj ) ) {
		return;
	}

	trace_t trace;
	Vec3 point = pml->origin;
	point.z -= STEPSIZE;

	// don't walljump if our height is smaller than a step
	// unless jump is pressed or the player is moving faster than dash speed and upwards
	float hspeed = Length( Vec3( pml->velocity.x, pml->velocity.y, 0 ) );
	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, ps->POVnum, pm->contentmask, 0 );

	if( ( hspeed > pm_dashspeed && pml->velocity.z > 8 ) || ( trace.fraction == 1 ) ||
		( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) )
	{
		Vec3 normal( 0.0f );
		PlayerTouchWall( pm, pml, pmove_gs, 12, 0.3f, &normal );
		if( !Length( normal ) )
			return;

		float oldupvelocity = pml->velocity.z;
		pml->velocity.z = 0.0;

		hspeed = Normalize2D( &pml->velocity );

		pml->velocity = GS_ClipVelocity( pml->velocity, normal, 1.0005f );
		pml->velocity = pml->velocity + normal * pm_wjbouncefactor;

		hspeed = Min2( hspeed, pml->maxPlayerSpeed );

		pml->velocity = Normalize( pml->velocity );

		pml->velocity *= hspeed;
		pml->velocity.z = ( oldupvelocity > pm_wjupspeed ) ? oldupvelocity : pm_wjupspeed; // jal: if we had a faster upwards speed, keep it

		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;

		StaminaUseImmediate( ps, stamina_usewj );

		// Create the event
		pmove_gs->api.PredictedEvent( ps->POVnum, EV_WALLJUMP, DirToU64( normal ) );
	}
}



static void PM_HooliganDash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( !StaminaAvailableImmediate( ps, stamina_usedash ) ) {
		return;
	}

	Vec3 dashdir = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
	if( Length( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
		dashdir = pml->forward;
		pml->forwardPush = pm_dashspeed;
	}

	ps->pmove.pm_flags |= PMF_ABILITY2_HELD;

	StaminaUseImmediate( ps, stamina_usedash );
	Dash( pm, pml, pmove_gs, dashdir, pm_dashspeed, pm_dashupspeed );
}





static void PM_HooliganSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( !pressed ) {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pressed && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) {
		if( pm->groundentity == -1 ) {
			PM_HooliganWalljump( pm, pml, pmove_gs, ps );
		} else {
			PM_HooliganDash( pm, pml, pmove_gs, ps );
		}
	}
}


void PM_HooliganInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, PM_HooliganJump, PM_HooliganSpecial );
}
