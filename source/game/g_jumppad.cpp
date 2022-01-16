#include "game/g_local.h"

static void TouchJumppad( edict_t * ent, edict_t * other, Plane * plane, int surfFlags ) {
	if( G_TriggerWait( ent ) ) {
		return;
	}

	// add an event
	if( other->r.client ) {
		GS_TouchPushTrigger( &server_gs, &other->r.client->ps, &ent->s );
	}
	else {
		// pushing of non-clients
		if( other->movetype != MOVETYPE_BOUNCEGRENADE ) {
			return;
		}

		other->velocity = GS_EvaluateJumppad( &ent->s, other->velocity );
	}

	G_PositionedSound( ent->s.origin, CHAN_AUTO, "entities/jumppad/trigger" );
}

static void FindJumppadTarget( edict_t * ent ) {
	edict_t * target = G_PickTarget( ent->target );
	ent->target_ent = target;
	if( target == NULL ) {
		return;
	}

	Vec3 velocity = target->s.origin - ent->s.origin;

	float height = target->s.origin.z - ent->s.origin.z;
	float time = sqrtf( height / ( 0.5f * GRAVITY ) );
	if( time != 0 ) {
		velocity.z = 0;
		float dist = Length( velocity );
		velocity = SafeNormalize( velocity );
		velocity = velocity * ( dist / time );
		velocity.z = time * GRAVITY;
		ent->s.type = ET_JUMPPAD;
		ent->s.origin2 = velocity;
	}
	else {
		ent->s.origin2 = velocity;
	}
}

void SP_jumppad( edict_t * ent, const spawn_temp_t * st ) {
	ent->s.svflags &= ~SVF_NOCLIENT;
	ent->r.solid = SOLID_TRIGGER;

	Vec3 forward, right, up;
	AngleVectors( ent->s.angles, &forward, &right, &up );

	MinMax3 bounds = MinMax3::Empty();
	bounds = Union( bounds, -( forward + right ) * 32.0f + up * 24.0f );
	bounds = Union( bounds, ( forward + right ) * 32.0f );
	ent->r.mins = bounds.mins;
	ent->r.maxs = bounds.maxs;

	ent->touch = TouchJumppad;
	ent->think = FindJumppadTarget;

	ent->nextThink = level.time + 1;
	ent->s.svflags &= ~SVF_NOCLIENT;
	ent->s.type = ET_PAINKILLER_JUMPPAD;
	ent->s.origin2 = up * 512.0f;
	ent->s.model = "entities/jumppad/model";

	GClip_LinkEntity( ent );
	ent->timeStamp = level.time;

	if( !ent->wait ) {
		ent->wait = 100;
	}
}
