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

#include "qcommon/base.h"
#include "game/g_local.h"

static void G_AssignMoverSounds( edict_t * ent, const spawn_temp_t * st, StringHash default_start, StringHash default_move, StringHash default_stop ) {
	ent->moveinfo.sound_start = st->noise_start != EMPTY_HASH ? st->noise_start : default_start;
	ent->moveinfo.sound_middle = st->noise != EMPTY_HASH ? st->noise : default_move;
	ent->moveinfo.sound_end = st->noise_stop != EMPTY_HASH ? st->noise_stop : default_stop;
}

//=========================================================
//  movement options:
//
//  linear
//  smooth start, hard stop
//  smooth start, smooth stop
//
//  start
//  end
//  speed
//  begin sound
//  end sound
//  target fired when reaching end
//  wait at end
//
//  object characteristics that use move segments
//  ---------------------------------------------
//  movetype_push, or movetype_stop
//  action when touched
//  action when blocked
//  action when used
//	disabled?
//  auto trigger spawning
//
//
//=========================================================

//
// Support routines for movement (changes in origin using velocity)
//

static void Move_UpdateLinearVelocity( edict_t *ent, float dist, int speed ) {
	int duration = 0;

	if( speed ) {
		duration = (float)dist * 1000.0f / speed;
		if( !duration ) {
			duration = 1;
		}
	}

	ent->s.linearMovement = speed != 0;
	if( !ent->s.linearMovement ) {
		return;
	}

	ent->s.linearMovementEnd = ent->moveinfo.dest;
	ent->s.linearMovementBegin = ent->s.origin;
	ent->s.linearMovementTimeStamp = svs.gametime - game.frametime;
	ent->s.linearMovementDuration = duration;
}

static void Move_Done( edict_t *ent ) {
	ent->velocity = Vec3( 0.0f );
	ent->moveinfo.endfunc( ent );
	G_CallStop( ent );

	//Move_UpdateLinearVelocity( ent, 0, 0 );
}

static void Move_Watch( edict_t *ent ) {
	int moveTime;

	moveTime = svs.gametime - ent->s.linearMovementTimeStamp;
	if( moveTime >= (int)ent->s.linearMovementDuration ) {
		ent->think = Move_Done;
		ent->nextThink = level.time + 1;
		return;
	}

	ent->think = Move_Watch;
	ent->nextThink = level.time + 1;
}

static void Move_Begin( edict_t *ent ) {
	// set up velocity vector
	Vec3 dir = ent->moveinfo.dest - ent->s.origin;
	float dist = Length( dir );
	dir = SafeNormalize( dir );
	ent->velocity = dir * ent->moveinfo.speed;
	ent->nextThink = level.time + 1;
	ent->think = Move_Watch;
	Move_UpdateLinearVelocity( ent, dist, ent->moveinfo.speed );
}

static void Move_Calc( edict_t *ent, Vec3 dest, void ( *func )( edict_t * ) ) {
	ent->velocity = Vec3( 0.0f );
	ent->moveinfo.dest = dest;
	ent->moveinfo.endfunc = func;
	Move_UpdateLinearVelocity( ent, 0, 0 );

	if( level.current_entity == ent ) {
		Move_Begin( ent );
	} else {
		ent->nextThink = level.time + 1;
		ent->think = Move_Begin;
	}
}

static constexpr int STATE_TOP = 0;
static constexpr int STATE_BOTTOM = 1;
static constexpr int STATE_UP = 2;
static constexpr int STATE_DOWN = 3;

//=========================================================
//
//DOORS
//
//  spawn a trigger surrounding the entire team unless it is
//  already targeted by another
//
//=========================================================

static constexpr int DOOR_START_OPEN = 1;
static constexpr int DOOR_CRUSHER = 4;
static constexpr int DOOR_TOGGLE = 32;

static void door_use_areaportals( edict_t *self, bool open ) {
	int iopen = open ? 1 : 0;

	// make sure we don't open the same areaportal twice
	if( self->style == iopen ) {
		return;
	}

	self->style = iopen;
	GClip_SetAreaPortalState( self, open );
}

static void door_go_down( edict_t *self );

static void door_hit_top( edict_t *self ) {
	self->moveinfo.state = STATE_TOP;
	if( self->spawnflags & DOOR_TOGGLE ) {
		return;
	}
	if( self->moveinfo.wait >= 0 ) {
		self->think = door_go_down;
		self->nextThink = level.time + self->moveinfo.wait;
	}
}

static void door_hit_bottom( edict_t *self ) {
	self->moveinfo.state = STATE_BOTTOM;
	door_use_areaportals( self, false );
}

void door_go_down( edict_t *self ) {
	if( self->moveinfo.sound_end != EMPTY_HASH ) {
		G_AddEvent( self, EV_DOOR_START_MOVING, self->moveinfo.sound_end.hash, true );
	}
	self->s.sound = self->moveinfo.sound_middle;

	if( self->max_health ) {
		self->deadflag = DEAD_NO;
		self->takedamage = DAMAGE_YES;
		self->health = self->max_health;
	}

	self->moveinfo.state = STATE_DOWN;
	Move_Calc( self, self->moveinfo.start_origin, door_hit_bottom );
}

static void door_go_up( edict_t *self, edict_t *activator ) {
	if( self->moveinfo.state == STATE_UP ) {
		return; // already going up
	}

	if( self->moveinfo.state == STATE_TOP ) { // reset top wait time
		if( self->moveinfo.wait >= 0 ) {
			self->nextThink = level.time + self->moveinfo.wait;
		}
		return;
	}

	if( self->moveinfo.sound_start != EMPTY_HASH ) {
		G_AddEvent( self, EV_DOOR_START_MOVING, self->moveinfo.sound_start.hash, true );
	}
	self->s.sound = self->moveinfo.sound_middle;

	self->moveinfo.state = STATE_UP;
	Move_Calc( self, self->moveinfo.end_origin, door_hit_top );

	G_UseTargets( self, activator );
	door_use_areaportals( self, true );
}

static void door_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if( self->spawnflags & DOOR_TOGGLE ) {
		if( self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP ) {
			self->message = NULL;
			self->touch = NULL;
			door_go_down( self );
			return;
		}
	}

	self->message = NULL;
	self->touch = NULL;
	door_go_up( self, activator );
}

static void Touch_DoorTrigger( edict_t *self, edict_t *other, Plane *plane, int surfFlags ) {
	if( G_IsDead( other ) ) {
		return;
	}
	if( self->s.team && other->s.team != self->s.team ) {
		return;
	}
	if( ( !other->r.client ) ) {
		return;
	}

	door_use( self->r.owner, other, other );
}

static void Think_SpawnDoorTrigger( edict_t *ent ) {
	edict_t *other;
	float expand_size = 80;     // was 60

	Vec3 mins = ent->r.absmin;
	Vec3 maxs = ent->r.absmax;

	// expand
	mins.x -= expand_size;
	mins.y -= expand_size;
	maxs.x += expand_size;
	maxs.y += expand_size;

	other = G_Spawn();
	other->r.mins = mins;
	other->r.maxs = maxs;
	other->r.owner = ent;
	other->s.team = ent->s.team;
	other->r.solid = SOLID_TRIGGER;
	other->movetype = MOVETYPE_NONE;
	other->touch = Touch_DoorTrigger;
	GClip_LinkEntity( other );

	door_use_areaportals( ent, ( ent->spawnflags & DOOR_START_OPEN ) != 0 );
}

static void door_blocked( edict_t *self, edict_t *other ) {
	if( !other->r.client ) {
		// give it a chance to go away on its own terms (like gibs)
		G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, 100000, 1, 0, WorldDamage_Crush );
		G_FreeEdict( other );
		return;
	}

	G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, self->dmg, 1, 0, WorldDamage_Crush );

	if( self->spawnflags & DOOR_CRUSHER ) {
		return;
	}

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if( self->moveinfo.wait >= 0 ) {
		if( self->moveinfo.state == STATE_DOWN ) {
			door_go_up( self, self->activator );
		} else {
			door_go_down( self );
		}
	}
}

void SP_func_door( edict_t * ent, const spawn_temp_t * st ) {
	G_InitMover( ent );
	G_SetMovedir( &ent->s.angles, &ent->moveinfo.movedir );

	G_AssignMoverSounds( ent, st, "sounds/movers/door_start", EMPTY_HASH, "sounds/movers/door_close" );

	ent->moveinfo.blocked = door_blocked;
	ent->use = door_use;

	if( !ent->speed ) {
		ent->speed = 1500;
	}
	if( !ent->wait ) {
		ent->wait = 2000;
	}

	int lip = st->lip != 0 ? st->lip : 8;

	if( st->gameteam ) {
		if( st->gameteam >= Team_None && st->gameteam < Team_Count ) {
			ent->s.team = Team( st->gameteam );
		}
	}

	// calculate second position
	ent->moveinfo.start_origin = ent->s.origin;
	Vec3 abs_movedir;
	abs_movedir.x = Abs( ent->moveinfo.movedir.x );
	abs_movedir.y = Abs( ent->moveinfo.movedir.y );
	abs_movedir.z = Abs( ent->moveinfo.movedir.z );
	ent->moveinfo.distance = Dot( abs_movedir, ent->r.size ) - lip;
	ent->moveinfo.end_origin = ent->moveinfo.start_origin + ent->moveinfo.movedir * ent->moveinfo.distance;

	// if it starts open, switch the positions
	if( ent->spawnflags & DOOR_START_OPEN ) {
		ent->s.origin = ent->moveinfo.end_origin;
		ent->moveinfo.end_origin = ent->moveinfo.start_origin;
		ent->moveinfo.start_origin = ent->s.origin;
		ent->moveinfo.movedir = -ent->moveinfo.movedir;
	}

	ent->moveinfo.state = STATE_BOTTOM;

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.wait = ent->wait;
	ent->moveinfo.start_angles = ent->s.angles;
	ent->moveinfo.end_angles = ent->s.angles;

	GClip_LinkEntity( ent );

	ent->style = -1;
	door_use_areaportals( ent, ( ent->spawnflags & DOOR_START_OPEN ) != 0 );

	if( ent->name == EMPTY_HASH ) {
		ent->nextThink = level.time + 1;
		ent->think = Think_SpawnDoorTrigger;
	}
}

//====================================================================

#define STATE_STOPPED       0
#define STATE_ACCEL     1
#define STATE_FULLSPEED     2
#define STATE_DECEL     3

static void Think_RotateAccel( edict_t *self ) {
	if( self->moveinfo.current_speed >= self->speed ) { // has reached full speed
		// if calculation causes it to go a little over, readjust
		if( self->moveinfo.current_speed != self->speed ) {
			self->avelocity = self->moveinfo.movedir * self->speed;
			self->moveinfo.current_speed = self->speed;
		}

		self->think = NULL;
		self->moveinfo.state = STATE_FULLSPEED;
		return;
	}

	// if here, some more acceleration needs to be done
	// add acceleration value to current speed to cause accel
	self->moveinfo.current_speed += self->accel;
	self->avelocity = self->moveinfo.movedir * self->moveinfo.current_speed;
	self->nextThink = level.time + 1;
}

static void Think_RotateDecel( edict_t *self ) {
	if( self->moveinfo.current_speed <= 0 ) { // has reached full stop
		// if calculation cause it to go a little under, readjust
		if( self->moveinfo.current_speed != 0 ) {
			self->avelocity = Vec3( 0.0f );
			self->moveinfo.current_speed = 0;
		}

		self->think = NULL;
		self->moveinfo.state = STATE_STOPPED;
		return;
	}

	// if here, some more deceleration needs to be done
	// subtract deceleration value from current speed to cause decel
	self->moveinfo.current_speed -= self->decel;
	self->avelocity = self->moveinfo.movedir * self->moveinfo.current_speed;
	self->nextThink = level.time + 1;
}

static void rotating_blocked( edict_t *self, edict_t *other ) {
	G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, self->dmg, 1, 0, WorldDamage_Crush );
}

static void rotating_touch( edict_t *self, edict_t *other, Plane *plane, int surfFlags ) {
	if( self->avelocity != Vec3( 0.0f ) ) {
		G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, self->dmg, 1, 0, WorldDamage_Crush );
	}
}

static void rotating_use( edict_t *self, edict_t *other, edict_t *activator ) {
	// first, figure out what state we are in
	if( self->moveinfo.state == STATE_ACCEL || self->moveinfo.state == STATE_FULLSPEED ) {
		// if decel is 0 then just stop
		if( self->decel == 0 ) {
			self->avelocity = Vec3( 0.0f );
			self->moveinfo.current_speed = 0;
			self->touch = NULL;
			self->think = NULL;
			self->moveinfo.state = STATE_STOPPED;
		} else {
			// otherwise decelerate
			self->think = Think_RotateDecel;
			self->nextThink = level.time + 1;
			self->moveinfo.state = STATE_DECEL;
		} // decelerate
	} else {
		self->s.sound = self->moveinfo.sound_middle;

		// check if accel is 0.  If so, just start the rotation
		if( self->accel == 0 ) {
			self->avelocity = self->moveinfo.movedir * self->speed;
			self->moveinfo.state = STATE_FULLSPEED;
		} else {
			// accelerate baybee
			self->think = Think_RotateAccel;
			self->nextThink = level.time + 1;
			self->moveinfo.state = STATE_ACCEL;
		}
	}

	// setup touch function if needed
	if( self->spawnflags & 16 ) {
		self->touch = rotating_touch;
	}
}

void SP_func_rotating( edict_t * ent, const spawn_temp_t * st ) {
	G_InitMover( ent );

	if( ent->spawnflags & 32 ) {
		ent->movetype = MOVETYPE_STOP;
	} else {
		ent->movetype = MOVETYPE_PUSH;
	}

	ent->moveinfo.state = STATE_STOPPED; // rotating thingy starts out idle

	// set the axis of rotation
	ent->moveinfo.movedir = Vec3( 0.0f );
	if( ent->spawnflags & 4 ) {
		ent->moveinfo.movedir.z = 1.0;
	} else if( ent->spawnflags & 8 ) {
		ent->moveinfo.movedir.x = 1.0;
	} else { // Z_AXIS
		ent->moveinfo.movedir.y = 1.0;
	}

	// check for reverse rotation
	if( ent->spawnflags & 2 ) {
		ent->moveinfo.movedir = -ent->moveinfo.movedir;
	}

	if( !ent->speed ) {
		ent->speed = 100;
	}
	if( !ent->dmg ) {
		ent->dmg = 2;
	}

	if( ent->accel < 0 ) { // sanity check
		ent->accel = 0;
	} else {
		ent->accel *= 0.1f;
	}

	if( ent->decel < 0 ) { // sanity check
		ent->decel = 0;
	} else {
		ent->decel *= 0.1f;
	}

	ent->moveinfo.current_speed = 0;

	ent->use = rotating_use;
	if( ent->dmg ) {
		ent->moveinfo.blocked = rotating_blocked;
	}

	G_AssignMoverSounds( ent, st, EMPTY_HASH, EMPTY_HASH, EMPTY_HASH );

	if( !( ent->spawnflags & 1 ) ) {
		G_CallUse( ent, NULL, NULL );
	}

	GClip_LinkEntity( ent );
}

#define TRAIN_START_ON      1
#define TRAIN_TOGGLE        2
#define TRAIN_BLOCK_STOPS   4

static void train_next( edict_t *self );

static void train_blocked( edict_t *self, edict_t *other ) {
	if( !other->r.client ) {
		// give it a chance to go away on its own terms (like gibs)
		G_Damage( other, self, self, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, 100000, 1, 0, WorldDamage_Crush );
		G_FreeEdict( other );
		return;
	}

	if( level.time < self->timeStamp + 500 ) {
		return;
	}

	if( !self->dmg ) {
		return;
	}
	self->timeStamp = level.time;
	G_Damage( other, self, world, Vec3( 0.0f ), Vec3( 0.0f ), other->s.origin, self->dmg, 1, 0, WorldDamage_Crush );
}

static void train_wait( edict_t *self ) {
	if( self->target_ent->pathtarget != EMPTY_HASH ) {
		StringHash savetarget;
		edict_t *ent;

		ent = self->target_ent;
		savetarget = ent->target;
		ent->target = ent->pathtarget;
		G_UseTargets( ent, self->activator );
		ent->target = savetarget;

		// make sure we didn't get killed by a killtarget
		if( !self->r.inuse ) {
			return;
		}
	}

	if( self->moveinfo.wait ) {
		if( self->moveinfo.wait > 0 ) {
			self->nextThink = level.time + self->moveinfo.wait;
			self->think = train_next;
		} else if( self->spawnflags & TRAIN_TOGGLE ) {   // && wait < 0
			train_next( self );
			self->spawnflags &= ~TRAIN_START_ON;
			self->velocity = Vec3( 0.0f );
			self->nextThink = 0;
		}

		if( self->moveinfo.sound_end != EMPTY_HASH ) {
			G_AddEvent( self, EV_TRAIN_STOP, self->moveinfo.sound_end.hash, true );
		}
		self->s.sound = EMPTY_HASH;
	} else {
		train_next( self );
	}
}

void train_next( edict_t *self ) {
	edict_t *ent;
	bool first;

	first = true;
again:
	if( self->target == EMPTY_HASH ) {
		return;
	}

	ent = G_PickTarget( self->target );
	if( !ent ) {
		Com_GGPrint( "train_next: bad target {}\n", self->target );
		return;
	}

	self->target = ent->target;

	// check for a teleport path_corner
	if( ent->spawnflags & 1 ) {
		if( !first ) {
			Com_GGPrint( "connected teleport path_corners, see {} at {}", ent->classname, ent->s.origin );

			return;
		}

		first = false;
		self->s.origin = ent->s.origin - self->r.mins;
		self->olds.origin = self->s.origin;
		GClip_LinkEntity( self );
		self->s.teleported = true;
		goto again;
	}

	self->moveinfo.wait = ent->wait;
	self->target_ent = ent;

	if( self->moveinfo.sound_start != EMPTY_HASH ) {
		G_AddEvent( self, EV_TRAIN_START, self->moveinfo.sound_start.hash, true );
	}
	self->s.sound = self->moveinfo.sound_middle;

	Vec3 dest = ent->s.origin - self->r.mins;
	self->moveinfo.state = STATE_TOP;
	self->moveinfo.start_origin = self->s.origin;
	self->moveinfo.end_origin = dest;
	Move_Calc( self, dest, train_wait );
	self->spawnflags |= TRAIN_START_ON;
}

static void train_resume( edict_t *self ) {
	edict_t * ent = self->target_ent;

	Vec3 dest = ent->s.origin - self->r.mins;
	self->moveinfo.state = STATE_TOP;
	self->moveinfo.start_origin = self->s.origin;
	self->moveinfo.end_origin = dest;
	Move_Calc( self, dest, train_wait );
	self->spawnflags |= TRAIN_START_ON;
}

static void func_train_find( edict_t *self ) {
	edict_t *ent;

	if( self->target == EMPTY_HASH ) {
		Com_Printf( "train_find: no target\n" );
		return;
	}

	ent = G_PickTarget( self->target );
	if( !ent ) {
		Com_GGPrint( "train_find: target {} not found\n", self->target );
		return;
	}

	self->target = ent->target;

	self->s.origin = ent->s.origin - self->r.mins;
	GClip_LinkEntity( self );

	// if not triggered, start immediately
	if( self->name == EMPTY_HASH ) {
		self->spawnflags |= TRAIN_START_ON;
	}

	if( self->spawnflags & TRAIN_START_ON ) {
		self->nextThink = level.time + 1;
		self->think = train_next;
		self->activator = self;
	}
}

static void train_use( edict_t *self, edict_t *other, edict_t *activator ) {
	self->activator = activator;

	if( self->spawnflags & TRAIN_START_ON ) {
		if( !( self->spawnflags & TRAIN_TOGGLE ) ) {
			return;
		}
		self->spawnflags &= ~TRAIN_START_ON;
		self->velocity = Vec3( 0.0f );
		self->nextThink = 0;
	} else {
		if( self->target_ent ) {
			train_resume( self );
		} else {
			train_next( self );
		}
	}
}

void SP_func_train( edict_t * self, const spawn_temp_t * st ) {
	G_InitMover( self );

	self->s.angles = Vec3( 0.0f );
	self->moveinfo.blocked = train_blocked;
	if( self->spawnflags & TRAIN_BLOCK_STOPS ) {
		self->dmg = 0;
	} else {
		if( !self->dmg ) {
			self->dmg = 100;
		}
	}

	G_AssignMoverSounds( self, st, EMPTY_HASH, EMPTY_HASH, EMPTY_HASH );

	if( !self->speed ) {
		self->speed = 100;
	}

	self->moveinfo.speed = self->speed;
	self->use = train_use;

	GClip_LinkEntity( self );

	if( self->target != EMPTY_HASH ) {
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->nextThink = level.time + 1;
		self->think = func_train_find;
	} else {
		Com_GGPrint( "func_train without a target at {}", self->s.origin );
	}
}
