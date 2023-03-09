#include "gameshared/movement.h"
#include "qcommon/qfiles.h"

static constexpr float charge_jump_speed = 700.0f;
static constexpr float charge_min_speed = 400.0f;
static constexpr float charge_slide_time = 1.0f;

static constexpr float min_bounce_speed = 250.0f;
static constexpr float bounce_factor = 0.5f;

static constexpr float brake_friction = 16.0f;
static constexpr float normal_friction = 2.0f;

static constexpr float stamina_use = 5.0f;
static constexpr float stamina_recover = 10.0f;

static constexpr float floor_distance = STEPSIZE;

static void PM_WheelBrake( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( (pm->groundentity != -1 || pml->ladder) && (ps->pmove.stamina == 0.0f) ) {
		ps->pmove.stamina_state = Stamina_Normal;
	}

	pml->friction = pressed && !pml->ladder ? 
					brake_friction :
					normal_friction;
}


//in this one we don't care about pressing special
static void PM_WheelJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	bool can_start_charge = ps->pmove.stamina_state == Stamina_Normal;

	ps->pmove.stamina_stored = Max2( 0.0f, ps->pmove.stamina_stored - pml->frametime );

	if( pressed ) {
		if( ps->pmove.stamina_state == Stamina_UsingAbility ||
			( can_start_charge && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) ) //a bit tricky but we don't want midget charge to start if we were already pressing before reaching can_start_charge
		{
			if( can_start_charge ) {
				ps->pmove.stamina_state = Stamina_UsingAbility;
			}
			StaminaRecover( ps, pml, stamina_use );

			pmove_gs->api.PredictedEvent( ps->POVnum, EV_CHARGEJUMP, 0 );
		}

		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;
	}

	if( ps->pmove.stamina_state == Stamina_UsedAbility ) {
		StaminaUse( ps, pml, stamina_recover );
	}

	if( !pressed ) {
		float special_jumpspeed = charge_jump_speed * ps->pmove.stamina;
		if( ( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) && ps->pmove.stamina_state == Stamina_UsingAbility && special_jumpspeed > charge_min_speed ) {
			Vec3 fwd;
			AngleVectors( pm->playerState->viewangles, &fwd, NULL, NULL );
			Vec3 dashdir = fwd;

			dashdir = Normalize( dashdir );
			dashdir *= special_jumpspeed;

			pm->groundentity = -1;
			pml->velocity = dashdir;

			ps->pmove.stamina_stored = charge_slide_time;

			pmove_gs->api.PredictedEvent( ps->POVnum, EV_JUMP, JumpType_MidgetCharge );
		} else if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_UsedAbility;
		}

		if( ps->pmove.stamina_state == Stamina_UsingAbility ) {
			ps->pmove.stamina_state = Stamina_UsedAbility;
		}

		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
	

	//bounce
	trace_t trace;
	Vec3 point = pml->origin;
	point.z -= floor_distance;

	pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, ps->POVnum, pm->contentmask, 0 );

	if( pm->groundentity == -1 && ps->pmove.stamina_state == Stamina_UsedAbility &&
		!ISWALKABLEPLANE( &trace.plane ) && !pml->ladder &&
		(trace.fraction == 1 || !trace.startsolid) )
	{
		Vec3 normal( 0.0f );
		PlayerTouchWall( pm, pml, pmove_gs, 12, 0.3f, &normal, true, SURF_NOWALLJUMP | SURF_LADDER );
		if( !Length( normal ) )
			return;

		//don't want to bounce everywhere while falling imo
		Vec3 velocity2 = pml->velocity;
		if( velocity2.z < 0 ) {
			velocity2.z = 0;
		}

		float speed = Length( velocity2 );
		pml->velocity = GS_ClipVelocity( pml->velocity, normal, 1.0005f );
		if( speed > min_bounce_speed ) {
			pml->velocity = pml->velocity + normal * speed * bounce_factor;
		}
	}

	if( ps->pmove.stamina_stored == 0.0f || ps->pmove.stamina_state != Stamina_UsedAbility ) {
		return;
	}

	ps->pmove.stamina_stored = Max2( 0.0f, ps->pmove.stamina_stored - pml->frametime );

}


void PM_WheelInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Wheel, PM_WheelJump, PM_WheelBrake );
}
