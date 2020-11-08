#include "game/g_local.h"

static void SpikesRearm( edict_t * self ) {
	self->s.linearMovementTimeStamp = 0;
}

static void SpikesDeploy( edict_t * self ) {
	int64_t deployed_for = svs.gametime - self->s.linearMovementTimeStamp;

	if( deployed_for < 1500 ) {
		Vec3 dir;
		AngleVectors( self->s.angles, NULL, NULL, &dir );
		Vec3 knockback = dir * 30.0f;
		KillBox( self, MOD_SPIKES, knockback );
		self->nextThink = level.time + 1;
	}
	else {
		self->think = SpikesRearm;
		self->nextThink = level.time + 500;
	}
}

static void SpikesTouched( edict_t * self, edict_t * other, cplane_t * plane, int surfFlags ) {
	if( other->s.type != ET_PLAYER )
		return;

	if( self->s.radius == 1 ) {
		G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, 10000, 0, 0, MOD_SPIKES );
		return;
	}

	if( self->s.linearMovementTimeStamp == 0 ) {
		self->nextThink = level.time + 1000;
		self->think = SpikesDeploy;
		self->s.linearMovementTimeStamp = Max2( s64( 1 ), svs.gametime );
	}
}

void SP_spikes( edict_t * spikes ) {
	spikes->r.svflags &= ~SVF_NOCLIENT | SVF_PROJECTILE;
	spikes->r.solid = SOLID_TRIGGER;
	spikes->s.radius = spikes->spawnflags & 1;

	spikes->s.angles.x += 90;
	Vec3 forward, right, up;
	AngleVectors( spikes->s.angles, &forward, &right, &up );

	Vec3 mins = -( forward + right ) * 64 + up * 48;
	Vec3 maxs = ( forward + right ) * 64;

	spikes->r.mins.x = Min2( mins.x, maxs.x );
	spikes->r.mins.y = Min2( mins.y, maxs.y );
	spikes->r.mins.z = Min2( mins.z, maxs.z );

	spikes->r.maxs.x = Max2( mins.x, maxs.x );
	spikes->r.maxs.y = Max2( mins.y, maxs.y );
	spikes->r.maxs.z = Max2( mins.z, maxs.z );

	spikes->s.model = "models/objects/spikes/spikes";
	spikes->s.type = ET_SPIKES;

	spikes->touch = SpikesTouched;

	GClip_LinkEntity( spikes );

	edict_t * base = G_Spawn();
	base->r.svflags &= ~SVF_NOCLIENT;
	base->s.origin = spikes->s.origin;
	base->s.angles = spikes->s.angles;
	base->s.model = "models/objects/spikes/base";
	base->s.effects = EF_WORLD_MODEL;
	GClip_LinkEntity( base );
}
