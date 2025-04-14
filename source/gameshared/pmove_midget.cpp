#include "gameshared/movement.h"

static constexpr float dashupspeed = 160.0f;
static constexpr float dashspeed = 550.0f;

static constexpr float wjupspeed = 371.875f;
static constexpr float wjbouncefactor = 0.4f;



static constexpr float wallclimbspeed = 300.0f;

static constexpr float jumpupspeed = 280.0f;

static constexpr float climbfriction = 5.0f;
static constexpr float climbfallspeed = 30.0f;

static constexpr float stamina_use = 0.15f;
static constexpr float stamina_use_moving = 0.3f;
static constexpr float stamina_recover = 1.0f;

static constexpr float wj_cooldown = 0.05f;

static bool CanClimb( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( !StaminaAvailable( ps, pml, stamina_use ) ) {
		return false;
	}

	Vec3 spots[ 2 ];
	spots[ 0 ] = pml->origin + pml->forward - pml->right * 2;
	spots[ 1 ] = pml->origin + pml->forward + pml->right * 2;

	for( Vec3 spot : spots ) {
		trace_t trace = pmove_gs->api.Trace( pml->origin, pm->bounds, spot, pm->playerState->POVnum, pm->solid_mask, 0 );
		if( trace.HitSomething() && !ISWALKABLEPLANE( trace.normal ) && trace.GotSomewhere() ) {
			return true;
		}
	}

	return false;
}

static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	ps->pmove.jump_buffering = Max2( 0.0f, ps->pmove.jump_buffering - pml->frametime );

	if( pm->groundentity != -1 ) {
		ps->pmove.stamina_state = Stamina_Normal;
	}

	if( !pressed ) {
		ps->pmove.pm_flags &= ~PMF_ABILITY1_HELD;
		return;
	}

	if( pm->groundentity == -1 ) {
		if( !(ps->pmove.pm_flags & PMF_ABILITY1_HELD) && StaminaAvailable( ps, pml, stamina_use ) ) {
			if( Walljump( pm, pml, pmove_gs, ps, jumpupspeed, dashupspeed, dashspeed, wjupspeed, wjbouncefactor ) ) {
				ps->pmove.jump_buffering = wj_cooldown;
				ps->pmove.stamina_state = Stamina_UsingAbility;
				ps->pmove.stamina = 0.0f;
			}

			ps->pmove.pm_flags |= PMF_ABILITY1_HELD;
		}
	} else {
		Jump( pm, pml, pmove_gs, ps, jumpupspeed, true );
		ps->pmove.pm_flags |= PMF_ABILITY1_HELD;
	}
}

static void PM_MidgetSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( ps->pmove.stamina_state == Stamina_Normal && !( ps->pmove.pm_flags & PMF_ABILITY2_HELD ) ) {
		StaminaRecover( ps, pml, stamina_recover );
	}

	if( ps->pmove.no_friction_time > 0 ) {
		return;
	}

	if( pml->ladder ) {
		return;
	}

	if( pressed && CanClimb( pm, pml, pmove_gs, ps ) && ps->pmove.jump_buffering == 0.f ) {
		pml->ladder = Ladder_Fake;
		pml->groundFriction = climbfriction;

		Vec3 wishvel = pml->forward * pml->forwardPush + pml->right * pml->sidePush;
		wishvel.z = 0.0;

		ps->pmove.stamina_state = Stamina_UsingAbility;
		ps->pmove.pm_flags |= PMF_ABILITY2_HELD;

		if( wishvel == Vec3( 0.0f ) ) {
			StaminaUse( ps, pml, stamina_use );
		} else {
			wishvel = Normalize( wishvel );
			StaminaUse( ps, pml, Length( wishvel ) * stamina_use_moving );
		}


		if( pml->forwardPush > 0 ) {
			wishvel.z = Lerp( -1.0f, Unlerp01( 15.0f, ps->viewangles.pitch, -15.0f ), 1.0f );
		}

		pml->velocity = wishvel * wallclimbspeed;
		pml->velocity.z -= climbfallspeed;

		pm->groundentity = -1;
	} else {
		ps->pmove.pm_flags &= ~PMF_ABILITY2_HELD;
	}
}

void PM_MidgetInit( pmove_t * pm, pml_t * pml ) {
	PM_InitPerk( pm, pml, Perk_Midget, PM_MidgetJump, PM_MidgetSpecial );
}
