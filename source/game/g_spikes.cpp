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
		KillBox( self, WorldDamage_Spike, knockback );
		self->nextThink = level.time + 1;
	}
	else {
		self->think = SpikesRearm;
		self->nextThink = level.time + 500;
	}
}

static void SpikesTouched( edict_t * self, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( other->s.type != ET_PLAYER )
		return;

	if( self->s.radius == 1 ) {
		G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, 10000, 0, 0, WorldDamage_Spike );
		return;
	}

	if( self->s.linearMovementTimeStamp == 0 ) {
		self->nextThink = level.time + 1000;
		self->think = SpikesDeploy;
		self->s.linearMovementTimeStamp = Max2( s64( 1 ), svs.gametime );
	}
}

void SP_spike( edict_t * spike, const spawn_temp_t * st ) {
	spike->s.svflags &= ~SVF_NOCLIENT | SVF_PROJECTILE;
	spike->r.solid = SOLID_TRIGGER;
	spike->s.radius = spike->spawnflags & 1;

	Vec3 forward, right, up;
	AngleVectors( spike->s.angles, &forward, &right, &up );

	MinMax3 bounds = MinMax3::Empty();
	bounds = Union( bounds, -( forward + right ) * 8.0f + up * 48.0f );
	bounds = Union( bounds, ( forward + right ) * 8.0f );
	spike->r.mins = bounds.mins;
	spike->r.maxs = bounds.maxs;

	spike->s.model = "models/spikes/spike";
	spike->s.type = ET_SPIKES;

	spike->touch = SpikesTouched;

	GClip_LinkEntity( spike );

	edict_t * base = G_Spawn();
	base->s.svflags &= ~SVF_NOCLIENT;
	base->r.mins = bounds.mins;
	base->r.maxs = bounds.maxs;
	base->s.origin = spike->s.origin;
	base->s.angles = spike->s.angles;
	base->s.model = "models/spikes/spike_base";
	GClip_LinkEntity( base );
}

void SP_spikes( edict_t * spikes, const spawn_temp_t * st ) {
	spikes->s.svflags &= ~SVF_NOCLIENT | SVF_PROJECTILE;
	spikes->r.solid = SOLID_TRIGGER;
	spikes->s.radius = spikes->spawnflags & 1;

	Vec3 forward, right, up;
	AngleVectors( spikes->s.angles, &forward, &right, &up );

	MinMax3 bounds = MinMax3::Empty();
	bounds = Union( bounds, -( forward + right ) * 64.0f + up * 48.0f );
	bounds = Union( bounds, ( forward + right ) * 64.0f );
	spikes->r.mins = bounds.mins;
	spikes->r.maxs = bounds.maxs;

	spikes->s.model = "models/spikes/spikes";
	spikes->s.type = ET_SPIKES;

	spikes->touch = SpikesTouched;

	GClip_LinkEntity( spikes );

	edict_t * base = G_Spawn();
	base->s.svflags &= ~SVF_NOCLIENT;
	base->r.mins = bounds.mins;
	base->r.maxs = bounds.maxs;
	base->s.origin = spikes->s.origin;
	base->s.angles = spikes->s.angles;
	base->s.model = "models/spikes/spikes_base";
	GClip_LinkEntity( base );
}
