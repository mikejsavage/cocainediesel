#include "gameshared/movement.h"


static constexpr float pm_defaultspeed = 500.0f;
static constexpr float pm_sidewalkspeed = 500.0f;
static constexpr float pm_crouchedspeed = 200.0f;

static constexpr float pm_jumpspeed = 600.0f;
static constexpr s16 pm_jumpboostdelay = 35;

static constexpr float pm_minbounceupspeed = 100.0f;
static constexpr float pm_wallbouncefactor = 0.25f;




static void PM_MidgetJump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	if( pml->upPush >= 10 ) {
		ps->pmove.stamina = Min2( s16( ps->pmove.stamina + 1 ), pm_jumpboostdelay );
	} else if( ps->pmove.stamina != 0 ) {
		if( pm->groundentity == -1 ) {
			ps->pmove.stamina = 0;
			return;
		}

		PM_Jump( pm, pml, pmove_gs, ps, pm_jumpspeed * (float)ps->pmove.stamina / pm_jumpboostdelay );
		ps->pmove.stamina = 0;
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


void PM_MidgetInit( pmove_t * pm, pml_t * pml, SyncPlayerState * ps ) {
	pml->maxPlayerSpeed = ps->pmove.max_speed;
	if( pml->maxPlayerSpeed < 0 ) {
		pml->maxPlayerSpeed = pm_defaultspeed;
	}

	pml->maxCrouchedSpeed = pm_crouchedspeed;

	pml->forwardPush = pm->cmd.forwardmove * pm_defaultspeed / 127.0f;
	pml->sidePush = pm->cmd.sidemove * pm_sidewalkspeed / 127.0f;
	pml->upPush = pm->cmd.upmove * pm_defaultspeed / 127.0f;

	ps->pmove.stamina_max = pm_jumpboostdelay;

	pml->jumpCallback = PM_MidgetJump;
	pml->specialCallback = PM_MidgetSpecial;
}