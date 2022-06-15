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

static void target_laser_think( edict_t *self ) {
	trace_t tr;
	Vec3 point;
	Vec3 last_movedir;

	// our lifetime has expired
	if( self->delay && self->wait < level.time ) {
		if( self->r.owner && self->r.owner->use ) {
			G_CallUse( self->r.owner, self, self->activator );
		}

		G_FreeEdict( self );
		return;
	}

	if( self->enemy ) {
		last_movedir = self->moveinfo.movedir;
		point = self->enemy->r.absmin + self->enemy->r.size * 0.5f;
		self->moveinfo.movedir = point - self->s.origin;
		self->moveinfo.movedir = Normalize( self->moveinfo.movedir );
		if( self->moveinfo.movedir != last_movedir ) {
			self->spawnflags |= 0x80000000;
		}
	}

	edict_t *ignore = self;
	Vec3 start = self->s.origin;
	Vec3 end = start + self->moveinfo.movedir * 2048.0f;
	while( true ) {
		G_Trace( &tr, start, Vec3( 0.0f ), Vec3( 0.0f ), end, ignore, MASK_SHOT );
		if( tr.fraction == 1 ) {
			break;
		}

		// hurt it if we can
		if( game.edicts[tr.ent].takedamage ) {
			if( game.edicts[tr.ent].r.client && self->activator->r.client ) {
				if( game.edicts[tr.ent].s.team != self->activator->s.team ) {
					G_Damage( &game.edicts[tr.ent], self, self->activator, self->moveinfo.movedir, self->moveinfo.movedir, tr.endpos, 5, 0, 0, WorldDamage_Laser );
				}
			} else {
				G_Damage( &game.edicts[tr.ent], self, self->activator, self->moveinfo.movedir, self->moveinfo.movedir, tr.endpos, 5, 0, 0, WorldDamage_Laser );
			}
		}

		// if we hit something that's not a monster or player or is immune to lasers, we're done
		if( !game.edicts[tr.ent].r.client ) {
			if( self->spawnflags & 0x80000000 ) {
				self->spawnflags &= ~0x80000000;
				// G_SpawnEvent( EV_LASER_SPARKS, DirToU64( tr.plane.normal ), &tr.endpos );
			}
			break;
		}

		ignore = &game.edicts[tr.ent];
		start = tr.endpos;
	}

	self->s.origin2 = tr.endpos;
	G_SetBoundsForSpanEntity( self, 8 );

	GClip_LinkEntity( self );

	self->nextThink = level.time + 1;
}

static void target_laser_on( edict_t *self ) {
	if( !self->activator ) {
		self->activator = self;
	}
	self->spawnflags |= 0x80000001;
	self->s.svflags &= ~SVF_NOCLIENT;
	self->wait = level.time + self->delay;
	target_laser_think( self );
}

static void target_laser_off( edict_t *self ) {
	self->spawnflags &= ~1;
	self->s.svflags |= SVF_NOCLIENT;
	self->nextThink = 0;
}

static void target_laser_use( edict_t *self, edict_t *other, edict_t *activator ) {
	self->activator = activator;
	if( self->spawnflags & 1 ) {
		target_laser_off( self );
	} else {
		target_laser_on( self );
	}
}

static void target_laser_start( edict_t *self ) {
	self->movetype = MOVETYPE_NONE;
	self->r.solid = SOLID_NOT;
	self->s.type = ET_LASER;
	self->s.svflags = 0;
	self->s.sound = "sounds/gladiator/laser_hum";

	if( !self->enemy ) {
		if( self->target != EMPTY_HASH ) {
			edict_t * target = G_Find( NULL, &edict_t::name, self->target );
			if( !target ) {
				Com_GGPrint( "{} at {}: {} is a bad target", self->classname, self->s.origin, self->target );
			}
			self->enemy = target;
		} else {
			G_SetMovedir( &self->s.angles, &self->moveinfo.movedir );
		}
	}
	self->use = target_laser_use;
	self->think = target_laser_think;

	if( !self->dmg ) {
		self->dmg = 1;
	}

	if( self->spawnflags & 1 ) {
		target_laser_on( self );
	} else {
		target_laser_off( self );
	}
}

void SP_target_laser( edict_t * ent, const spawn_temp_t * st ) {
	// let everything else get spawned before we start firing
	ent->think = target_laser_start;
	ent->nextThink = level.time + 1;
	ent->count = WorldDamage_Laser;
	ent->s.radius = st->size > 0 ? st->size : 8;
}

void SP_target_position( edict_t * self, const spawn_temp_t * st ) { }
