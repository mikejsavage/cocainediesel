#include "gameshared/movement.h"

static constexpr s16 pm_dashtimedelay = 200;


void PM_ClearDash( SyncPlayerState * ps ) {
	ps->pmove.pm_flags &= ~PMF_DASHING;
}


float Normalize2D( Vec3 * v ) {
	float length = Length( v->xy() );
	Vec2 n = SafeNormalize( v->xy() );
	v->x = n.x;
	v->y = n.y;
	return length;
}


// Walljump wall availability check
// nbTestDir is the number of directions to test around the player
// maxZnormal is the max Z value of the normal of a poly to consider it a wall
// normal becomes a pointer to the normal of the most appropriate wall
void PlayerTouchWall( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, int nbTestDir, float maxZnormal, Vec3 * normal ) {
	ZoneScoped;

	float dist = 1.0;

	Vec3 mins = Vec3( pm->mins.xy(), 0.0f );
	Vec3 maxs = Vec3( pm->maxs.xy(), 0.0f );

	for( int i = 0; i < nbTestDir; i++ ) {
		float t = float( i ) / float( nbTestDir );

		Vec3 dir = Vec3(
			pm->maxs.x * cosf( PI * 2.0f * t ) + pml->velocity.x * 0.015f,
			pm->maxs.y * sinf( PI * 2.0f * t ) + pml->velocity.y * 0.015f,
			0.0f
		);
		Vec3 end = pml->origin + dir;

		trace_t trace;
		pmove_gs->api.Trace( &trace, pml->origin, mins, maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );

		if( trace.allsolid )
			return;

		if( trace.fraction == 1 )
			continue; // no wall in this direction

		if( trace.surfFlags & SURF_NOWALLJUMP )
			continue;

		if( trace.ent > 0 ) {
			const SyncEntityState * state = pmove_gs->api.GetEntityState( trace.ent, 0 );
			if( state->type == ET_PLAYER )
				continue;
		}

		if( dist > trace.fraction && Abs( trace.plane.normal.z ) < maxZnormal ) {
			dist = trace.fraction;
			*normal = trace.plane.normal;
		}
	}
}

void StaminaUse( SyncPlayerState * ps, s16 use ) {
	ps->pmove.stamina = Max2( ps->pmove.stamina - use, 0 );
}

void StaminaRecover( SyncPlayerState * ps, s16 recover ) {
	ps->pmove.stamina = Min2( s16( ps->pmove.stamina + recover ), ps->pmove.stamina_max );
}


float JumpVelocity( pmove_t * pm, float vel ) {
	return ( pm->waterlevel >= 2 ? 2 : 1 ) * vel;
}


void PM_Jump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, float jumpspeed ) {
	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml->groundplane.normal.z > 0 && pml->velocity.z > 0 && Dot( pml->groundplane.normal.xy(), pml->velocity.xy() ) > 0 ) {
		pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane.normal, PM_OVERBOUNCE );
	}

	pmove_gs->api.PredictedEvent( ps->POVnum, EV_JUMP, ps->perk );
	pml->velocity.z = Max2( 0.0f, pml->velocity.z ) + JumpVelocity( pm, jumpspeed );
	PM_ClearDash( ps );
}


void PM_Dash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, Vec3 dashdir, float dash_speed, float dash_upspeed ) {
	pm->playerState->pmove.pm_flags |= PMF_DASHING;
	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml->groundplane.normal.z > 0 && pml->velocity.z < 0 && Dot( pml->groundplane.normal.xy(), pml->velocity.xy() ) > 0 ) {
		pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane.normal, PM_OVERBOUNCE );
	}

	dashdir.z = 0.0f;
	dashdir = Normalize( dashdir );

	float upspeed = Max2( 0.0f, pml->velocity.z ) + JumpVelocity( pm, dash_upspeed );
	float actual_velocity = Normalize2D( &pml->velocity );
	if( actual_velocity <= dash_speed ) {
		dashdir *= dash_speed;
	} else {
		dashdir *= actual_velocity;
	}

	pml->velocity = dashdir;
	pml->velocity.z = upspeed;

	// return sound events only when the dashes weren't too close to each other
	if( pm->playerState->pmove.stamina_time == 0 ) {
		pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH,
			Abs( pml->sidePush ) >= Abs( pml->forwardPush ) ?
					( pml->sidePush < 0 ? 1 : 2 ) : //left or right
					( pml->forwardPush < 0 ? 3 : 0 ) ); //back or forward
		pm->playerState->pmove.stamina_time = pm_dashtimedelay;
	}
}