#include "gameshared/movement.h"
#include "gameshared/gs_weapons.h"

float Normalize2D( Vec3 * v ) {
	float length = Length( v->xy() );
	Vec2 n = SafeNormalize( v->xy() );
	v->x = n.x;
	v->y = n.y;
	return length;
}

Optional< Vec3 > PlayerTouchWall( const pmove_t * pm, const pml_t * pml, const gs_state_t * pmove_gs, bool z, SolidBits ignoreFlags ) {
	TracyZoneScoped;

	constexpr int candidate_dirs = 12;
	constexpr float max_z_normal = 0.3f;

	Optional< Vec3 > best_normal = NONE;
	float best_fraction = 1.0f;

	MinMax3 bounds( Vec3( pm->bounds.mins.xy(), 0.0f ), Vec3( pm->bounds.maxs.xy(), 0.0f ) );

	for( int i = 0; i < candidate_dirs; i++ ) {
		float t = float( i ) / float( candidate_dirs );

		Vec3 dir = Vec3(
			pm->bounds.maxs.x * cosf( PI * 2.0f * t ) + pml->velocity.x * 0.015f,
			pm->bounds.maxs.y * sinf( PI * 2.0f * t ) + pml->velocity.y * 0.015f,
			z ? pm->bounds.maxs.z * cosf( PI * 2.0f * t ) + pml->velocity.z * 0.015f : 0.0f
		);
		Vec3 end = pml->origin + dir;

		trace_t trace = pmove_gs->api.Trace( pml->origin, bounds, end, pm->playerState->POVnum, Solid_PlayerClip, 0 );

		if( trace.HitNothing() )
			continue; // no wall in this direction

		if( trace.normal == Vec3( 0.0f ) )
			break;

		if( trace.solidity & ignoreFlags )
			continue;

		if( trace.fraction < best_fraction && Abs( trace.normal.z ) < max_z_normal ) {
			best_fraction = trace.fraction;
			best_normal = trace.normal;
		}
	}

	return best_normal;
}

bool StaminaAvailable( SyncPlayerState * ps, pml_t * pml, float need ) {
	return ps->pmove.stamina >= need * pml->frametime;
}

bool StaminaAvailableImmediate( SyncPlayerState * ps, float need ) {
	return ps->pmove.stamina >= need;
}

void StaminaUse( SyncPlayerState * ps, pml_t * pml, float use ) {
	ps->pmove.stamina = Max2( ps->pmove.stamina - use * pml->frametime, 0.0f );
}

void StaminaUseImmediate( SyncPlayerState * ps, float use ) {
	ps->pmove.stamina = Max2( ps->pmove.stamina - use, 0.0f );
}

void StaminaRecover( SyncPlayerState * ps, pml_t * pml, float recover ) {
	ps->pmove.stamina = Min2( ps->pmove.stamina + recover * pml->frametime, 1.0f );
}

void PM_InitPerk( pmove_t * pm, pml_t * pml, PerkType perk,
				void (*ability1Callback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool pressed ),
				void (*ability2Callback)( pmove_t *, pml_t *, const gs_state_t *, SyncPlayerState *, bool pressed ) )
{
	const PerkDef * def = GetPerkDef( perk );

	pml->maxSpeed = def->max_speed;
	if( pm->playerState->pmove.max_speed > 0 ) {
		pml->maxSpeed = pm->playerState->pmove.max_speed;
	}

	pml->forwardPush *= def->max_speed;
	pml->sidePush *= def->side_speed;
	pml->groundAccel = def->ground_accel;
	pml->airAccel = def->air_accel;

	pml->groundFriction = def->ground_friction;

	pml->ability1Callback = ability1Callback;
	pml->ability2Callback = ability2Callback;
}

void Jump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, float jumpspeed, bool addvel ) {
	if( pml->ladder ) {
		return;
	}

	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml->groundplane.z > 0 && pml->velocity.z > 0 && Dot( pml->groundplane.xy(), pml->velocity.xy() ) > 0 ) {
		pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane );
	}

	pmove_gs->api.PredictedEvent( ps->POVnum, EV_JUMP, JumpType_Normal );
	pml->velocity.z = (addvel ? Max2( 0.0f, pml->velocity.z ) : 0.0f) + jumpspeed;
}

void Dash( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, Vec3 dashdir, float dash_speed, float dash_upspeed ) {
	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml->groundplane.z > 0 && pml->velocity.z < 0 && Dot( pml->groundplane.xy(), pml->velocity.xy() ) > 0 ) {
		pml->velocity = GS_ClipVelocity( pml->velocity, pml->groundplane );
	}

	dashdir.z = 0.0f;
	if( Length( dashdir ) == 0.0f ) {
		Jump( pm, pml, pmove_gs, pm->playerState, dash_upspeed, true );
		return;
	}

	dashdir = Normalize( dashdir );

	float upspeed = Max2( 0.0f, pml->velocity.z ) + dash_upspeed;
	float actual_velocity = Normalize2D( &pml->velocity );
	if( actual_velocity <= dash_speed ) {
		dashdir *= dash_speed;
	} else {
		dashdir *= actual_velocity;
	}

	pml->velocity = dashdir;
	pml->velocity.z = upspeed;

	pmove_gs->api.PredictedEvent( pm->playerState->POVnum, EV_DASH,
		Abs( pml->sidePush ) >= Abs( pml->forwardPush ) ?
				( pml->sidePush < 0 ? 1 : 2 ) : //left or right
				( pml->forwardPush < 0 ? 3 : 0 ) ); //back or forward
}

bool Walljump( pmove_t * pm, pml_t * pml, const gs_state_t * pmove_gs, SyncPlayerState * ps, float jumpupspeed, float dashupspeed, float dashspeed, float wjupspeed, float wjbouncefactor ) {
	// don't walljump if our height is smaller than a step
	// unless jump is pressed or the player is moving faster than dash speed and upwards
	constexpr float floor_distance = STEPSIZE * 0.45f;
	Vec3 point = pml->origin;
	point.z -= floor_distance;
	trace_t trace = pmove_gs->api.Trace( pml->origin, pm->bounds, point, ps->POVnum, pm->solid_mask, 0 );

	float hspeed = Length( Vec3( pml->velocity.x, pml->velocity.y, 0 ) );
	if( ( hspeed > dashspeed && pml->velocity.z > 8 ) || trace.HitNothing() || !ISWALKABLEPLANE( trace.normal ) ) {
		Optional< Vec3 > normal = PlayerTouchWall( pm, pml, pmove_gs, false, Solid_NotSolid );
		if( !normal.exists )
			return false;

		float oldupvelocity = pml->velocity.z;
		pml->velocity.z = 0.0;

		hspeed = Normalize2D( &pml->velocity );

		pml->velocity = GS_ClipVelocity( pml->velocity, normal.value );
		pml->velocity = pml->velocity + normal.value * wjbouncefactor;

		hspeed = Max2( hspeed, pml->maxSpeed );

		pml->velocity = Normalize( pml->velocity );

		pml->velocity *= hspeed;
		pml->velocity.z = ( oldupvelocity > wjupspeed ) ? oldupvelocity : wjupspeed; // jal: if we had a faster upwards speed, keep it

		// Create the event
		pmove_gs->api.PredictedEvent( ps->POVnum, EV_WALLJUMP, DirToU64( normal.value ) );
		return true;
	}

	return false;
}
