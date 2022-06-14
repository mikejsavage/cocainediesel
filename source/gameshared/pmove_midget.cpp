#include "gameshared/movement.h"

static constexpr float pm_jumpspeed = 250.0f;
static constexpr float jump_detection = 0.06f; //slight jump buffering

static constexpr float pm_chargedjumpspeed = 975.0f;
static constexpr float bounce_time = 2.0f;

static constexpr float stamina_use = 2.5f;
static constexpr float stamina_use_jumped = 4.0f;
static constexpr float stamina_recover = 1.55f;

static constexpr float floor_distance = STEPSIZE * 0.5f;

static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( (pm->groundentity != -1 || pml->ladder) && (ps->pmove.stamina_state == Stamina_UsedAbility ) ) {
		ps->pmove.stamina_state = Stamina_Normal;
	}

	if( pressed ) {
		ps->pmove.jump_buffering = Max2( 0.0f, ps->pmove.jump_buffering - pml->frametime );
		
		if( !(ps->pmove.pm_flags & PMF_ABILITY1_HELD) ) {
			ps->pmove.jump_buffering = jump_detection;
		}

		ps->pmove.pm_flags |= PMF_ABILITY1_HELD;

		if( pm->groundentity == -1 || pml->ladder || ps->pmove.jump_buffering == 0.0f ) {
			return;
		}

		Jump( pm, pml, pmove_gs, ps, pm_jumpspeed, JumpType_Normal, true );
	} else {
		ps->pmove.pm_flags &= ~PMF_ABILITY1_HELD;
	}

}


//in this one we don't care about pressing special
static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	bool can_start_charge = ps->pmove.stamina_state == Stamina_Normal && (pm_chargedjumpspeed * ps->pmove.stamina) > pm_jumpspeed;

	if( ps->pmove.stamina_state == Stamina_Normal ) {
		StaminaRecover( ps, pml, stamina_recover );
	}

	if( pressed ) {
		if( ps->pmove.stamina_state == Stamina_UsingAbility ||
			( can_start_charge && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) ) //a bit tricky but we don't want midget charge to start if we were already pressing before reaching can_start_charge
		{
			if( can_start_charge ) {
				ps->pmove.stamina_stored = ps->pmove.stamina;
				ps->pmove.stamina_state = Stamina_UsingAbility;
			}
			StaminaUse( ps, pml, stamina_use );
		}

		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;
	}

	if( ps->pmove.stamina_state == Stamina_UsedAbility ) {
		StaminaUse( ps, pml, stamina_use_jumped );
	}

	if( !pressed ) {
		float special_jumpspeed = pm_chargedjumpspeed * ( ps->pmove.stamina_stored - ps->pmove.stamina );
		if( ( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) && ps->pmove.stamina_state == Stamina_UsingAbility && special_jumpspeed > pm_jumpspeed ) {
			Vec3 fwd;
			AngleVectors( pm->playerState->viewangles, &fwd, NULL, NULL );
			Vec3 dashdir = fwd;

			dashdir = Normalize( dashdir );
			dashdir *= pm_chargedjumpspeed;

			pm->groundentity = -1;
			pml->velocity = dashdir;

			ps->pmove.stamina_stored = bounce_time;

			pmove_gs->api.PredictedEvent( ps->POVnum, EV_JUMP, JumpType_MidgetCharge );
		}

		if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_UsedAbility;
		}
		
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
	
	/*if( ps->pmove.stamina_stored == 0.0f || ps->pmove.stamina_state != Stamina_UsedAbility ) {
		return;
	}

	ps->pmove.stamina_stored = Max2( 0.0f, ps->pmove.stamina_stored - pml->frametime );

	trace_t trace;
	Vec3 point = pml->origin;
	point.z -= floor_distance;

	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, ps->POVnum, pm->contentmask, 0 );

	if( trace.fraction == 1 || !trace.startsolid ) {
		Vec3 normal( 0.0f );
		PlayerTouchWall( pm, pml, pmove_gs, 12, 0.3f, &normal, true );
		if( !Length( normal ) )
			return;

		float speed = Length( pml->velocity );
		pml->velocity = GS_ClipVelocity( pml->velocity, normal, 1.0005f );
		pml->velocity = pml->velocity + normal;
		pml->velocity = Normalize( pml->velocity );
		pml->velocity *= speed;

		ps->pmove.stamina_stored = 0.0f;
	}*/
}


void PM_MidgetInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Midget, PM_MidgetJump, PM_MidgetSpecial );
}
