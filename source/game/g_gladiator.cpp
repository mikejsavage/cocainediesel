#include "g_local.h"

static void SpikesRearm( edict_t * self ) {
	self->s.linearMovementTimeStamp = 0;
}

static void SpikesDeploy( edict_t * self ) {
	int64_t deployed_for = svs.gametime - self->s.linearMovementTimeStamp;

	if( deployed_for < 1500 ) {
		vec3_t dir;
		AngleVectors( self->s.angles, NULL, NULL, dir );
		vec3_t knockback;
		VectorScale( dir, 30.0f, knockback );
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
		G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, 10000, 0, 0, MOD_SPIKES );
		return;
	}

	if( self->s.linearMovementTimeStamp == 0 ) {
		self->nextThink = level.time + 1000;
		self->think = SpikesDeploy;
		self->s.linearMovementTimeStamp = max( 1, svs.gametime );
	}
}

void SP_spikes( edict_t * spikes ) {
	spikes->r.svflags &= ~SVF_NOCLIENT | SVF_PROJECTILE;
	spikes->r.solid = SOLID_TRIGGER;
	spikes->s.radius = spikes->spawnflags & 1;

	spikes->s.angles[ PITCH ] += 90;
	vec3_t forward, right, up;
	AngleVectors( spikes->s.angles, forward, right, up );

	vec3_t mins, maxs;
	VectorSet( mins, 0, 0, 0 );
	VectorSet( maxs, 0, 0, 0 );
	VectorMA( mins, -64, forward, mins );
	VectorMA( mins, -64, right, mins );
	VectorMA( mins, 48, up, mins );
	VectorMA( maxs, 64, forward, maxs );
	VectorMA( maxs, 64, right, maxs );

	for( int i = 0; i < 3; i++ ) {
		spikes->r.mins[ i ] = min( mins[ i ], maxs[ i ] );
		spikes->r.maxs[ i ] = max( mins[ i ], maxs[ i ] );
	}

	spikes->s.modelindex = trap_ModelIndex( "models/objects/spikes/spikes" );
	spikes->s.type = ET_SPIKES;

	spikes->touch = SpikesTouched;

	GClip_LinkEntity( spikes );

	edict_t * base = G_Spawn();
	base->r.svflags &= ~SVF_NOCLIENT;
	VectorCopy( spikes->s.origin, base->s.origin );
	VectorCopy( spikes->s.angles, base->s.angles );
	base->s.modelindex = trap_ModelIndex( "models/objects/spikes/base" );
	base->s.effects = EF_WORLD_MODEL;
	GClip_LinkEntity( base );
}
