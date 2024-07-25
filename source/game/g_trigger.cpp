/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "game/g_local.h"

/*
* G_TriggerWait
*
* Called always when using a trigger that supports wait flag
* Returns true if the trigger shouldn't be activated
*/
bool G_TriggerWait( edict_t * ent ) {
	if( ent->timeStamp >= level.time ) {
		return true;
	}

	// the wait time has passed, so set back up for another activation
	ent->timeStamp = level.time + ent->wait;
	return false;
}

void InitTrigger( edict_t * ent ) {
	ent->s.solidity = Solid_Trigger;
	ent->movetype = MOVETYPE_NONE;
	ent->s.svflags = SVF_NOCLIENT;
}

//==============================================================================
//
//trigger_push
//
//==============================================================================

static void G_JumpPadSound( edict_t * ent ) {
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	Vec3 org = ent->s.origin + Center( bounds );

	G_PositionedSound( org, "entities/jumppad/trigger" );
}

#define MIN_TRIGGER_PUSH_REBOUNCE_TIME 100

static void trigger_push_touch( edict_t *self, edict_t *other, Vec3 normal, SolidBits solid_mask ) {
	if( self->s.team && self->s.team != other->s.team ) {
		return;
	}

	if( G_TriggerWait( self ) ) {
		return;
	}

	// add an event
	if( other->r.client ) {
		GS_TouchPushTrigger( &server_gs, &other->r.client->ps, &self->s );
	}
	else {
		// pushing of non-clients
		if( other->movetype != MOVETYPE_BOUNCEGRENADE ) {
			return;
		}

		other->velocity = GS_EvaluateJumppad( &self->s, other->velocity );
	}

	G_JumpPadSound( self ); // play jump pad sound
}

static void trigger_push_setup( edict_t *self ) {
	edict_t * target = G_PickTarget( self->target );
	self->target_ent = target;
	if( target == NULL ) {
		G_FreeEdict( self );
		return;
	}

	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &self->s );
	Vec3 origin = Center( bounds );
	Vec3 velocity = target->s.origin - origin;

	float height = target->s.origin.z - origin.z;
	float time = sqrtf( height / ( 0.5f * GRAVITY ) );
	if( time != 0 ) {
		velocity.z = 0;
		float dist = Length( velocity );
		velocity = SafeNormalize( velocity );
		velocity = velocity * ( dist / time );
		velocity.z = time * GRAVITY;
		self->s.origin2 = velocity;
	}
	else {
		self->s.origin2 = velocity;
	}
}

void SP_trigger_push( edict_t * self, const spawn_temp_t * st ) {
	InitTrigger( self );

	if( st->gameteam >= Team_None && st->gameteam < Team_Count ) {
		self->s.team = Team( st->gameteam );
	}

	self->touch = trigger_push_touch;
	self->think = trigger_push_setup;
	self->nextThink = level.time + 1;
	self->s.svflags &= ~SVF_NOCLIENT;
	self->s.type = ( self->spawnflags & 1 ) ? ET_PAINKILLER_JUMPPAD : ET_JUMPPAD;
	GClip_LinkEntity( self );
	self->timeStamp = level.time;
	if( !self->wait ) {
		self->wait = MIN_TRIGGER_PUSH_REBOUNCE_TIME;
	}
}

//==============================================================================
//
//trigger_hurt
//
//==============================================================================

static void hurt_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if( EntitySolidity( ServerCollisionModelStorage(), &self->s ) == Solid_NotSolid ) {
		self->s.solidity = Solid_Trigger;
	} else {
		self->s.solidity = Solid_NotSolid;
	}
	GClip_LinkEntity( self );

	if( !( self->spawnflags & 2 ) ) {
		self->use = NULL;
	}
}

static void hurt_touch( edict_t *self, edict_t *other, Vec3 normal, SolidBits solid_mask ) {
	if( !other->takedamage || G_IsDead( other ) ) {
		return;
	}

	if( self->s.team && self->s.team != other->s.team ) {
		return;
	}

	if( G_TriggerWait( self ) ) {
		return;
	}

	int damage = self->dmg;
	if( self->spawnflags & ( 32 | 64 ) ) {
		damage = other->health + 1;
	}

	G_Damage( other, self, world, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, damage, damage, 0, WorldDamage_Trigger );
}

void SP_trigger_hurt( edict_t * self, const spawn_temp_t * st ) {
	InitTrigger( self );

	if( self->dmg > 300 ) { // HACK: force KILL spawnflag for big damages
		self->spawnflags |= 32;
	}

	self->sound = st->noise;

	if( st->gameteam >= Team_None && st->gameteam < Team_Count ) {
		self->s.team = Team( st->gameteam );
	}

	self->touch = hurt_touch;

	if( !self->dmg ) {
		self->dmg = 5;
	}

	if( self->spawnflags & 16 || !self->wait ) {
		self->wait = 100;
	}

	if( self->spawnflags & 1 ) {
		self->s.solidity = Solid_NotSolid;
	} else {
		self->s.solidity = Solid_Trigger;
	}

	if( self->spawnflags & 2 ) {
		self->use = hurt_use;
	}
}
