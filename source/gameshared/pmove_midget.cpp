#include "gameshared/movement.h"

static constexpr float pm_jumpspeed = 250.0f;
static constexpr float pm_chargedjumpspeed = 700.0f;

static constexpr float pm_minbounceupspeed = 120.0f;
static constexpr float pm_wallbouncefactor = 0.25f;

static constexpr float stamina_use = 2.5f;
static constexpr float stamina_recover = 1.55f;
static constexpr float stamina_jump_limit = 0.5f; //avoids jump spamming


static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( pm->groundentity == -1 ) {
		return;
	}

	if( ps->pmove.stamina_state == Stamina_UsedAbility ) {
		ps->pmove.stamina_state = Stamina_Normal;
	}

	if( !pressed ) {
		return;
	}

	if( pml->ladder ) {
		return;
	}


	Jump( pm, pml, pmove_gs, ps, pm_jumpspeed, JumpType_Normal, true );
}


//in this one we don't care about pressing special
static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	bool can_start_charge = ps->pmove.stamina_state == Stamina_Normal && ps->pmove.stamina > stamina_jump_limit;

	if( ps->pmove.stamina_state == Stamina_Normal ) {
		StaminaRecover( ps, pml, stamina_recover );
	}

	if( pressed && !pml->ladder ) {
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

	if( !pressed ) {
		float factor = ( ps->pmove.stamina_stored - ps->pmove.stamina );
		if( ( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) {
			if( factor > stamina_jump_limit ) {
				Jump( pm, pml, pmove_gs, ps, pm_chargedjumpspeed * factor, JumpType_MidgetCharge, false );
			}
			ps->pmove.stamina_state = Stamina_UsedAbility;
		}
		ps->pmove.stamina_stored = 0.0f;
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}

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

	if( ( trace.fraction == 1 || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) &&
		ps->pmove.stamina_state == Stamina_UsedAbility )
	{
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
	PM_InitPerk( pm, pml, Perk_Midget, PM_MidgetJump, PM_MidgetSpecial );
}
