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

void ThrowSmallPileOfGibs( edict_t *self, Vec3 knockback, int damage ) {
	Vec3 origin = self->s.origin;
	self->s.origin.z += 4;

	edict_t * event = G_SpawnEvent( EV_GIB, damage, &origin );
	event->s.team = self->s.team;
	event->s.origin2 = self->velocity + knockback;
}

static void path_corner_touch( edict_t *self, edict_t *other, Plane *plane, int surfFlags ) {
	Vec3 v;
	edict_t *next;

	if( other->movetarget != self ) {
		return;
	}
	if( other->enemy ) {
		return;
	}

	if( self->pathtarget != EMPTY_HASH ) {
		StringHash savetarget;
		savetarget = self->target;
		self->target = self->pathtarget;
		G_UseTargets( self, other );
		self->target = savetarget;
	}

	if( self->target != EMPTY_HASH ) {
		next = G_PickTarget( self->target );
	} else {
		next = NULL;
	}

	if( next && ( next->spawnflags & 1 ) ) {
		v = next->s.origin;
		v.z += next->r.mins.z;
		v.z -= other->r.mins.z;
		other->s.origin = v;
		next = G_PickTarget( next->target );
		other->s.teleported = true;
	}

	other->movetarget = next;

	v = other->movetarget->s.origin - other->s.origin;
}

void SP_path_corner( edict_t * self, const spawn_temp_t * st ) {
	if( self->name == EMPTY_HASH ) {
		Com_GGPrint( "path_corner with no name at {}", self->s.origin );
		G_FreeEdict( self );
		return;
	}

	self->r.solid = SOLID_TRIGGER;
	self->touch = path_corner_touch;
	self->r.mins = Vec3( -8.0f );
	self->r.maxs = Vec3( 8.0f );
	self->s.svflags |= SVF_NOCLIENT;
	GClip_LinkEntity( self );
}

//========================================================
//
//	FUNC_*
//
//========================================================

static void func_wall_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if( self->r.solid == SOLID_NOT ) {
		self->r.solid = SOLID_YES;
		self->s.svflags &= ~SVF_NOCLIENT;
		KillBox( self, WorldDamage_Crush, Vec3( 0.0f ) );
	} else {
		self->r.solid = SOLID_NOT;
		self->s.svflags |= SVF_NOCLIENT;
	}
	GClip_LinkEntity( self );

	if( !( self->spawnflags & 2 ) ) {
		self->use = NULL;
	}
}

void SP_func_wall( edict_t * self, const spawn_temp_t * st ) {
	G_InitMover( self );
	self->r.solid = SOLID_NOT;

	// just a wall
	if( ( self->spawnflags & 7 ) == 0 ) {
		self->r.solid = SOLID_YES;
		GClip_LinkEntity( self );
		return;
	}

	// it must be TRIGGER_SPAWN
	if( !( self->spawnflags & 1 ) ) {
		//		Com_Printf ("func_wall missing TRIGGER_SPAWN\n");
		self->spawnflags |= 1;
	}

	// yell if the spawnflags are odd
	if( self->spawnflags & 4 ) {
		if( !( self->spawnflags & 2 ) ) {
			if( developer->integer ) {
				Com_Printf( "func_wall START_ON without TOGGLE\n" );
			}
			self->spawnflags |= 2;
		}
	}

	self->use = func_wall_use;
	if( self->spawnflags & 4 ) {
		self->r.solid = SOLID_YES;
	} else {
		self->r.solid = SOLID_NOT;
		self->s.svflags |= SVF_NOCLIENT;
	}
	GClip_LinkEntity( self );
}

void SP_func_static( edict_t * ent, const spawn_temp_t * st ) {
	G_InitMover( ent );
	ent->movetype = MOVETYPE_NONE;
	ent->s.svflags = SVF_BROADCAST;
	GClip_LinkEntity( ent );
}

void SP_model( edict_t * ent, const spawn_temp_t * st ) {
	ent->s.svflags &= ~SVF_NOCLIENT;
	GClip_LinkEntity( ent );
}

void SP_decal( edict_t * ent, const spawn_temp_t * st ) {
	ent->s.svflags &= ~SVF_NOCLIENT;
	ent->s.type = ET_DECAL;
	ent->s.radius = st->radius;
	GClip_LinkEntity( ent );
}
