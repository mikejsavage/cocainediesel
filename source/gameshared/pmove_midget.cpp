#include "gameshared/movement.h"


static constexpr float pm_defaultspeed = 500.0f;
static constexpr float pm_sidewalkspeed = 500.0f;

static constexpr float pm_jumpspeed = 700.0f;

static constexpr float pm_minbounceupspeed = 100.0f;
static constexpr float pm_wallbouncefactor = 0.25f;

static constexpr s16 pm_midgetjumpdetection = 50;

static constexpr float stamina_max = 40.0f / 62.0f;
static constexpr float stamina_use = 1.0f;
static constexpr float stamina_recover = 5.0f;
static constexpr float stamina_jump_limit = stamina_max - 15.0f / 62.0f; //avoids jump spamming




static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush == 1 ) {
		if( pml->ladder ) {
			return;
		}
		
		StaminaUse( ps, pml, stamina_use );
		ps->pmove.pm_flags |= PMF_ABILITY1_HELD;
	} else {
		StaminaRecover( ps, pml, stamina_recover );
		ps->pmove.stamina_time = pm_midgetjumpdetection;
		if( pm->groundentity == -1 ) {
			return;
		}

		if( ps->pmove.stamina >= stamina_jump_limit ) {
			return;
		}

		if( ps->pmove.stamina_time == 0 ) {
			return;
		}

		if( !( ps->pmove.pm_flags & PMF_ABILITY1_HELD ) ) { //avoids jumping on spawn
			return;
		}

		ps->pmove.pm_flags &= ~PMF_ABILITY1_HELD;
		ps->pmove.stamina_time = 0;
		PM_Jump( pm, pml, pmove_gs, ps, pm_jumpspeed * ( stamina_max - ps->pmove.stamina ) / stamina_max );
	}
}


//in this one we don't care about pressing special
static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity != -1 ) {
		return;
	}

	if( pml->velocity.z < pm_minbounceupspeed ) {
		return;
	}

	trace_t trace;
	Vec3 point = pml->origin;
	point.z -= STEPSIZE;
	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );

	if( ( trace.fraction == 1 || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) ) {
		Vec3 normal( 0.0f );
		PlayerTouchWall( pm, pml, pmove_gs, 12, 0.1f, &normal );
		if( !Length( normal ) )
			return;

		float oldupvelocity = pml->velocity.z;
		pml->velocity.z = 0.0;

		float hspeed = Normalize2D( &pml->velocity );

		pml->velocity = GS_ClipVelocity( pml->velocity, normal, 1.0005f );
		pml->velocity = pml->velocity + normal * pm_wallbouncefactor;

		pml->velocity = Normalize( pml->velocity );

		pml->velocity *= hspeed;
		pml->velocity.z = oldupvelocity;
	}
}


void PM_MidgetInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, pm_defaultspeed, pm_sidewalkspeed, stamina_max, PM_MidgetJump, PM_MidgetSpecial );
}
