#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"


static constexpr float pm_defaultspeed = 320.0f;
static constexpr float pm_sidewalkspeed = 320.0f;
static constexpr float pm_crouchedspeed = 160.0f;

static constexpr float pm_jumpupspeed = 280.0f;
static constexpr float pm_dashspeed = 450.0f;
static constexpr s16 pm_dashtimedelay = 200;

static constexpr float pm_wjupspeed = ( 350.0f * GRAVITY_COMPENSATE );
static constexpr float pm_wjbouncefactor = 0.4f;
static constexpr s16 pm_wjtimedelay = 1300;
static constexpr s16 pm_wjtimedelayaircontrol = pm_wjtimedelay - 100;

static constexpr s16 pm_wjmax = 2;


static float pm_wjminspeed( pml_t * pml ) {
	return ( pml->maxCrouchedSpeed + pml->maxPlayerSpeed ) * 0.5f;
}


static void ClearWallJump( SyncPlayerState * ps ) {
	PM_ClearWallJump( ps );
	ps->pmove.stamina = pm_wjmax;
}


static void PM_HooliganJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush < 10 ) {
		return;
	}

	if( pm->groundentity == -1 ) {
		return;
	}

	Vec3 velocity = pml->velocity;
	velocity.z = 0.0f;
	if( Length( velocity ) <= pm_defaultspeed && pml->forwardPush > 0 ) {
		PM_Dash( pm, pml, pmove_gs, pml->flatforward * pml->forwardPush, pm_dashspeed, pm_jumpupspeed );
	} else {
		PM_Jump( pm, pml, pmove_gs, ps, pm_jumpupspeed );
	}

	ClearWallJump( ps );
}



static void PM_HooliganSpecial( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, bool pressed ) {
	if( ps->pmove.pm_flags & PMF_WALLJUMPING && ps->pmove.stamina_reload < pm_wjtimedelayaircontrol ) {
		ps->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}

	if( pm->groundentity != -1 || ( ps->pmove.stamina_reload <= 0 && ps->pmove.stamina == 0 ) ) {
		ClearWallJump( ps );
	}

	if( ps->pmove.knockback_time > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( pressed && ( pm->playerState->pmove.features & PMFEAT_SPECIAL ) && pm->groundentity == -1 && ps->pmove.stamina != 0 ) {
		if(	ps->pmove.stamina != 0 ) {
			trace_t trace;
			Vec3 point = pml->origin;
			point.z -= STEPSIZE;

			// don't walljump if our height is smaller than a step
			// unless jump is pressed or the player is moving faster than dash speed and upwards
			float hspeed = Length( Vec3( pml->velocity.x, pml->velocity.y, 0 ) );
			pmove_gs->api.Trace( &trace, pml->origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );

			if( pml->upPush >= 10
				|| ( hspeed > pm_dashspeed && pml->velocity.z > 8 )
				|| ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
				Vec3 normal( 0.0f );
				PlayerTouchWall( pm, pml, pmove_gs, 12, 0.3f, &normal );
				if( !Length( normal ) )
					return;

				if( !( ps->pmove.pm_flags & PMF_SPECIAL_HELD ) &&
				    !( ps->pmove.pm_flags & PMF_WALLJUMPING ) ) {
					float oldupvelocity = pml->velocity.z;
					pml->velocity.z = 0.0;

					hspeed = Normalize2D( &pml->velocity );

					pml->velocity = GS_ClipVelocity( pml->velocity, normal, 1.0005f );
					pml->velocity = pml->velocity + normal * pm_wjbouncefactor;

					if( hspeed < pm_wjminspeed( pml ) ) {
						hspeed = pm_wjminspeed( pml );
					}

					pml->velocity = Normalize( pml->velocity );

					pml->velocity *= hspeed;
					pml->velocity.z = ( oldupvelocity > pm_wjupspeed ) ? oldupvelocity : pm_wjupspeed; // jal: if we had a faster upwards speed, keep it

					// set the walljumping state
					PM_ClearDash( ps );

					ps->pmove.pm_flags |= PMF_WALLJUMPING;
					ps->pmove.pm_flags |= PMF_SPECIAL_HELD;

					ps->pmove.stamina--;

					ps->pmove.stamina_reload = pm_wjtimedelay;

					// Create the event
					pmove_gs->api.PredictedEvent( ps->POVnum, EV_WALLJUMP, DirToU64( normal ) );
				}
			}
		}
	}
}


void PM_HooliganInit( pmove_t * pm, pml_t * pml, SyncPlayerState * ps ) {
	pml->maxPlayerSpeed = ps->pmove.max_speed;
	if( pml->maxPlayerSpeed < 0 ) {
		pml->maxPlayerSpeed = pm_defaultspeed;
	}

	pml->maxCrouchedSpeed = pm_crouchedspeed;

	pml->forwardPush = pm->cmd.forwardmove * pm_defaultspeed / 127.0f;
	pml->sidePush = pm->cmd.sidemove * pm_sidewalkspeed / 127.0f;
	pml->upPush = pm->cmd.upmove * pm_defaultspeed / 127.0f;

	ps->pmove.stamina_max = pm_wjmax;

	pml->jumpCallback = PM_HooliganJump;
	pml->specialCallback = PM_HooliganSpecial;
}