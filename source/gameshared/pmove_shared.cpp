#include "gameshared/movement.h"


void PM_ClearDash( SyncPlayerState * ps ) {
	ps->pmove.pm_flags &= ~PMF_DASHING;
}

void PM_ClearWallJump( SyncPlayerState * ps ) {
	ps->pmove.pm_flags &= ~PMF_WALLJUMPING;
	ps->pmove.special_count = 0;
	ps->pmove.special_time = 0;
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



void Event_Jump( const gs_state_t * pmove_gs, SyncPlayerState * ps ) {
	pmove_gs->api.PredictedEvent( ps->POVnum, EV_JUMP, ps->perk );
}