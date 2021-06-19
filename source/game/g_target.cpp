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

static void target_explosion_explode( edict_t *self ) {
	float save;
	int radius;

	G_RadiusDamage( self, self->activator, NULL, NULL, MeanOfDeath_Explosion );

	if( ( self->projectileInfo.radius * 1 / 8 ) > 255 ) {
		radius = ( self->projectileInfo.radius * 1 / 16 ) & 0xFF;
		if( radius < 1 ) {
			radius = 1;
		}
		G_SpawnEvent( EV_EXPLOSION2, radius, &self->s.origin );
	} else {
		radius = ( self->projectileInfo.radius * 1 / 8 ) & 0xFF;
		if( radius < 1 ) {
			radius = 1;
		}
		G_SpawnEvent( EV_EXPLOSION1, radius, &self->s.origin );
	}

	save = self->delay;
	self->delay = 0;
	G_UseTargets( self, self->activator );
	self->delay = save;
}

static void use_target_explosion( edict_t *self, edict_t *other, edict_t *activator ) {
	self->activator = activator;

	if( !self->delay ) {
		target_explosion_explode( self );
		return;
	}

	self->think = target_explosion_explode;
	self->nextThink = level.time + self->delay * 1000;
}

void SP_target_explosion( edict_t *self ) {
	self->use = use_target_explosion;
	self->r.svflags = SVF_NOCLIENT;

	self->projectileInfo.maxDamage = Max2( self->dmg, 1 );
	self->projectileInfo.minDamage = Min2( self->dmg, 1 );
	self->projectileInfo.maxKnockback = self->projectileInfo.maxDamage;
	self->projectileInfo.minKnockback = self->projectileInfo.minDamage;
	self->projectileInfo.radius = st.radius;
	if( !self->projectileInfo.radius ) {
		self->projectileInfo.radius = self->dmg + 100;
	}
}

//==========================================================

static void target_laser_think( edict_t *self ) {
	trace_t tr;
	Vec3 point;
	Vec3 last_movedir;
	int count;

	// our lifetime has expired
	if( self->delay && ( self->wait * 1000 < level.time ) ) {
		if( self->r.owner && self->r.owner->use ) {
			G_CallUse( self->r.owner, self, self->activator );
		}

		G_FreeEdict( self );
		return;
	}

	if( self->spawnflags & 0x80000000 ) {
		count = 8;
	} else {
		count = 4;
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
	while( 1 ) {
		G_Trace( &tr, start, Vec3( 0.0f ), Vec3( 0.0f ), end, ignore, MASK_SHOT );
		if( tr.fraction == 1 ) {
			break;
		}

		// hurt it if we can
		if( game.edicts[tr.ent].takedamage ) {
			if( game.edicts[tr.ent].r.client && self->activator->r.client ) {
				if( !level.gametype.isTeamBased ||
					game.edicts[tr.ent].s.team != self->activator->s.team ) {
					G_Damage( &game.edicts[tr.ent], self, self->activator, self->moveinfo.movedir, self->moveinfo.movedir, tr.endpos, 5, 0, 0, self->count );
				}
			} else {
				G_Damage( &game.edicts[tr.ent], self, self->activator, self->moveinfo.movedir, self->moveinfo.movedir, tr.endpos, 5, 0, 0, self->count );
			}
		}

		// if we hit something that's not a monster or player or is immune to lasers, we're done
		if( !game.edicts[tr.ent].r.client ) {
			if( self->spawnflags & 0x80000000 ) {
				edict_t *event;

				self->spawnflags &= ~0x80000000;

				event = G_SpawnEvent( EV_LASER_SPARKS, DirToU64( tr.plane.normal ), &tr.endpos );
				event->s.counterNum = count;
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
	self->r.svflags &= ~SVF_NOCLIENT;
	self->wait = level.time * 0.001f + self->delay;
	target_laser_think( self );
}

static void target_laser_off( edict_t *self ) {
	self->spawnflags &= ~1;
	self->r.svflags |= SVF_NOCLIENT;
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

void target_laser_start( edict_t *self ) {
	self->movetype = MOVETYPE_NONE;
	self->r.solid = SOLID_NOT;
	self->s.type = ET_LASER;
	self->r.svflags = 0;
	self->s.radius = st.size > 0 ? st.size : 8;
	self->s.sound = "sounds/gladiator/laser_hum";

	if( !self->enemy ) {
		if( self->target ) {
			edict_t * target = G_Find( NULL, FOFS( targetname ), self->target );
			if( !target ) {
				if( developer->integer ) {
					Com_GGPrint( "{} at {}: {} is a bad target", self->classname, self->s.origin, self->target );
				}
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

void SP_target_laser( edict_t *self ) {
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->nextThink = level.time + 1;
	self->count = MeanOfDeath_Laser;
}

void SP_target_position( edict_t *self ) { }

static void SP_target_print_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if( activator->r.client && ( self->spawnflags & 4 ) ) {
		G_CenterPrintMsg( activator, "%s", self->message );
		return;
	}

	// print to team
	if( activator->r.client && self->spawnflags & 3 ) {
		edict_t *e;
		for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
			if( e->r.inuse && e->s.team ) {
				if( self->spawnflags & 1 && e->s.team == activator->s.team ) {
					G_CenterPrintMsg( e, "%s", self->message );
				}
				if( self->spawnflags & 2 && e->s.team != activator->s.team ) {
					G_CenterPrintMsg( e, "%s", self->message );
				}
			}
		}
		return;
	}

	for( int i = 1; i <= server_gs.maxclients; i++ ) {
		edict_t *player = &game.edicts[i];
		if( !player->r.inuse ) {
			continue;
		}

		G_CenterPrintMsg( player, "%s", self->message );
	}
}

void SP_target_print( edict_t *self ) {
	if( !self->message ) {
		G_FreeEdict( self );
		return;
	}

	self->use = SP_target_print_use;
}


//==========================================================

static void target_delay_think( edict_t *ent ) {
	G_UseTargets( ent, ent->activator );
}

static void target_delay_use( edict_t *ent, edict_t *other, edict_t *activator ) {
	ent->nextThink = level.time + 1000 * ( ent->wait + ent->random * random_float11( &svs.rng ) );
	ent->think = target_delay_think;
	ent->activator = activator;
}

void SP_target_delay( edict_t *ent ) {
	// check the "delay" key for backwards compatibility with Q3 maps
	if( ent->delay ) {
		ent->wait = ent->delay;
	}
	if( !ent->wait ) {
		ent->wait = 1.0;
	}

	ent->delay = 0;
	ent->use = target_delay_use;
}
