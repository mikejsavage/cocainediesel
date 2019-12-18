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

#include "g_local.h"

void G_AssignMoverSounds( edict_t *ent, const char *start, const char *move, const char *stop ) {
	if( st.noise && Q_stricmp( st.noise, "default" ) ) {
		if( Q_stricmp( st.noise, "silent" ) ) {
			ent->moveinfo.sound_middle = trap_SoundIndex( st.noise );
		}
	} else if( move ) {
		ent->moveinfo.sound_middle = trap_SoundIndex( move );
	}

	if( st.noise_start && Q_stricmp( st.noise_start, "default" ) ) {
		if( Q_stricmp( st.noise_start, "silent" ) ) {
			ent->moveinfo.sound_start = trap_SoundIndex( st.noise_start );
		}
	} else if( start ) {
		ent->moveinfo.sound_start = trap_SoundIndex( start );
	}

	if( st.noise_stop && Q_stricmp( st.noise_stop, "default" ) ) {
		if( Q_stricmp( st.noise_stop, "silent" ) ) {
			ent->moveinfo.sound_end = trap_SoundIndex( st.noise_stop );
		}
	} else if( stop ) {
		ent->moveinfo.sound_end = trap_SoundIndex( stop );
	}
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

	VectorCopy( ent->moveinfo.dest, ent->s.linearMovementEnd );
	VectorCopy( ent->s.origin, ent->s.linearMovementBegin );
	ent->s.linearMovementTimeStamp = svs.gametime - game.frametime;
	ent->s.linearMovementDuration = duration;
}

static void Move_Done( edict_t *ent ) {
	VectorClear( ent->velocity );
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
	vec3_t dir;
	float dist;

	// set up velocity vector
	VectorSubtract( ent->moveinfo.dest, ent->s.origin, dir );
	dist = VectorNormalize( dir );
	VectorScale( dir, ent->moveinfo.speed, ent->velocity );
	ent->nextThink = level.time + 1;
	ent->think = Move_Watch;
	Move_UpdateLinearVelocity( ent, dist, ent->moveinfo.speed );
}

static void Move_Calc( edict_t *ent, vec3_t dest, void ( *func )( edict_t * ) ) {
	VectorClear( ent->velocity );
	VectorCopy( dest, ent->moveinfo.dest );
	ent->moveinfo.endfunc = func;
	Move_UpdateLinearVelocity( ent, 0, 0 );

	if( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
		Move_Begin( ent );
	} else {
		ent->nextThink = level.time + 1;
		ent->think = Move_Begin;
	}
}


//
// Support routines for angular movement (changes in angle using avelocity)
//

static void AngleMove_Done( edict_t *ent ) {
	VectorClear( ent->avelocity );
	ent->moveinfo.endfunc( ent );
}

static bool AngleMove_AdjustFinalStep( edict_t *ent ) {
	float movedist, remainingdist;
	vec3_t destdelta;

	VectorSubtract( ent->moveinfo.destangles, ent->s.angles, destdelta );
	remainingdist = VectorNormalize( destdelta );

	movedist = ent->moveinfo.speed * ( game.frametime * 0.001f );
	if( remainingdist <= movedist ) { // final move: will be reached this frame
		VectorScale( destdelta, 1000.0f / game.frametime, ent->avelocity );
		return true;
	}

	return false;
}

static void AngleMove_Watch( edict_t *ent ) {
	vec3_t destdelta;

	// update remaining distance
	VectorSubtract( ent->moveinfo.destangles, ent->s.angles, destdelta );
	VectorNormalize( destdelta );

	// reached?
	if( VectorCompare( destdelta, vec3_origin ) ) {
		AngleMove_Done( ent );
		return;
	}

	if( AngleMove_AdjustFinalStep( ent ) ) {
		ent->think = AngleMove_Done;
		ent->nextThink = level.time + 1;
		return;
	} else {
		VectorScale( destdelta, ent->moveinfo.speed, ent->avelocity );
	}

	ent->think =  AngleMove_Watch;
	ent->nextThink = level.time + 1;
}

static void AngleMove_Begin( edict_t *ent ) {
	vec3_t destdelta;

	if( AngleMove_AdjustFinalStep( ent ) ) {
		ent->think = AngleMove_Done;
		ent->nextThink = level.time + 1;
		return;
	}

	// set up velocity vector
	VectorSubtract( ent->moveinfo.destangles, ent->s.angles, destdelta );
	VectorNormalize( destdelta );

	VectorScale( destdelta, ent->moveinfo.speed, ent->avelocity );
	ent->nextThink = level.time + 1;
	ent->think = AngleMove_Watch;
}

static void AngleMove_Calc( edict_t *ent, vec3_t destangles, void ( *func )( edict_t * ) ) {
	VectorClear( ent->avelocity );
	VectorCopy( destangles, ent->moveinfo.destangles );
	ent->moveinfo.endfunc = func;

	if( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
		AngleMove_Begin( ent );
	} else {
		ent->nextThink = level.time + 1;
		ent->think = AngleMove_Begin;
	}
}

//=========================================================
//
//  PLATS
//
//=========================================================

#define PLAT_LOW_TRIGGER    1

#define STATE_TOP       0
#define STATE_BOTTOM        1
#define STATE_UP        2
#define STATE_DOWN      3

static void plat_go_down( edict_t *ent );

static void plat_hit_top( edict_t *ent ) {
	if( !( ent->flags & FL_TEAMSLAVE ) ) {
		if( ent->moveinfo.sound_end ) {
			G_AddEvent( ent, EV_PLAT_HIT_TOP, ent->moveinfo.sound_end, true );
		}
		ent->s.sound = 0;
	}
	ent->moveinfo.state = STATE_TOP;

	ent->think = plat_go_down;
	ent->nextThink = level.time + 3000;
}

static void plat_hit_bottom( edict_t *ent ) {
	if( !( ent->flags & FL_TEAMSLAVE ) ) {
		if( ent->moveinfo.sound_end ) {
			G_AddEvent( ent, EV_PLAT_HIT_BOTTOM, ent->moveinfo.sound_end, true );
		}
		ent->s.sound = 0;
	}
	ent->moveinfo.state = STATE_BOTTOM;
}

void plat_go_down( edict_t *ent ) {
	if( !( ent->flags & FL_TEAMSLAVE ) ) {
		if( ent->moveinfo.sound_start ) {
			G_AddEvent( ent, EV_PLAT_START_MOVING, ent->moveinfo.sound_start, true );
		}
		ent->s.sound = ent->moveinfo.sound_middle;
	}

	ent->moveinfo.state = STATE_DOWN;
	Move_Calc( ent, ent->moveinfo.end_origin, plat_hit_bottom );
}

static void plat_go_up( edict_t *ent ) {
	if( !( ent->flags & FL_TEAMSLAVE ) ) {
		if( ent->moveinfo.sound_start ) {
			G_AddEvent( ent, EV_PLAT_START_MOVING, ent->moveinfo.sound_start, true );
		}
		ent->s.sound = ent->moveinfo.sound_middle;
	}
	ent->moveinfo.state = STATE_UP;
	Move_Calc( ent, ent->moveinfo.start_origin, plat_hit_top );
}

static void plat_blocked( edict_t *self, edict_t *other ) {
	if( !other->r.client ) {
		// give it a chance to go away on its own terms (like gibs)
		G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, 100000, 1, 0, MOD_CRUSH );

		// if it's still there, nuke it
		if( other->r.inuse ) {
			BecomeExplosion1( other );
		}
		return;
	}

	G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, self->dmg, 1, 0, MOD_CRUSH );

	if( self->moveinfo.state == STATE_UP ) {
		plat_go_down( self );
	} else if( self->moveinfo.state == STATE_DOWN ) {
		plat_go_up( self );
	}
}


void Use_Plat( edict_t *ent, edict_t *other, edict_t *activator ) {
	if( ent->think ) {
		return; // already down
	}
	plat_go_down( ent );
}


static void Touch_Plat_Center( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( !other->r.client ) {
		return;
	}
	if( G_IsDead( other ) ) {
		return;
	}

	ent = ent->enemy; // now point at the plat, not the trigger
	if( ent->moveinfo.state == STATE_BOTTOM ) {
		plat_go_up( ent );
	} else if( ent->moveinfo.state == STATE_TOP ) {
		ent->nextThink = level.time + 1000; // the player is still on the plat, so delay going down
	}
}

static void plat_spawn_inside_trigger( edict_t *ent ) {
	edict_t *trigger;
	vec3_t tmin, tmax;

	//
	// middle trigger
	//
	trigger = G_Spawn();
	trigger->s.team = ent->s.team;
	trigger->touch = Touch_Plat_Center;
	trigger->movetype = MOVETYPE_NONE;
	trigger->r.solid = SOLID_TRIGGER;
	trigger->enemy = ent;

	tmin[0] = ent->r.mins[0] + 25;
	tmin[1] = ent->r.mins[1] + 25;
	tmin[2] = ent->r.mins[2];

	tmax[0] = ent->r.maxs[0] - 25;
	tmax[1] = ent->r.maxs[1] - 25;
	tmax[2] = ent->r.maxs[2] + 8;

	tmin[2] = tmax[2] - ( ent->moveinfo.start_origin[2] - ent->moveinfo.end_origin[2] + st.lip );

	if( ent->spawnflags & PLAT_LOW_TRIGGER ) {
		tmax[2] = tmin[2] + 8;
	}

	if( tmax[0] - tmin[0] <= 0 ) {
		tmin[0] = ( ent->r.mins[0] + ent->r.maxs[0] ) * 0.5;
		tmax[0] = tmin[0] + 1;
	}
	if( tmax[1] - tmin[1] <= 0 ) {
		tmin[1] = ( ent->r.mins[1] + ent->r.maxs[1] ) * 0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy( tmin, trigger->r.mins );
	VectorCopy( tmax, trigger->r.maxs );

	GClip_LinkEntity( trigger );
}

void SP_func_plat( edict_t *ent ) {
	G_InitMover( ent );

	VectorClear( ent->s.angles );

	ent->moveinfo.blocked = plat_blocked;

	if( !ent->speed ) {
		ent->speed = 300;
	}

	if( !ent->dmg ) {
		ent->dmg = 2;
	}

	if( !st.lip ) {
		st.lip = 8;
	}

	// start is the top position, end is the bottom
	VectorCopy( ent->s.origin, ent->moveinfo.start_origin );
	VectorCopy( ent->s.origin, ent->moveinfo.end_origin );
	if( st.height ) {
		ent->moveinfo.end_origin[2] -= st.height;
	} else {
		ent->moveinfo.end_origin[2] -= ( ent->r.maxs[2] - ent->r.mins[2] ) - st.lip;
	}

	ent->use = Use_Plat;

	plat_spawn_inside_trigger( ent ); // the "start moving" trigger

	if( ent->targetname ) {
		ent->moveinfo.state = STATE_UP;
	} else {
		VectorCopy( ent->moveinfo.end_origin, ent->s.origin );
		ent->moveinfo.state = STATE_BOTTOM;
	}

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.wait = ent->wait;
	VectorCopy( ent->s.angles, ent->moveinfo.start_angles );
	VectorCopy( ent->s.angles, ent->moveinfo.end_angles );

	GClip_LinkEntity( ent );

	G_AssignMoverSounds( ent, S_PLAT_START, S_PLAT_MOVE, S_PLAT_STOP );
}



//=========================================================
//
//DOORS
//
//  spawn a trigger surrounding the entire team unless it is
//  already targeted by another
//
//=========================================================

#define DOOR_START_OPEN     1
#define DOOR_REVERSE        2
#define DOOR_CRUSHER        4
#define DOOR_NOMONSTER      8

#define DOOR_TOGGLE         32
#define DOOR_X_AXIS         64
#define DOOR_Y_AXIS         128

//wsw
#define DOOR_DIE_ONCE       1024 // auto set when health is > 0

static void door_use_areaportals( edict_t *self, bool open ) {
	int iopen = open ? 1 : 0;

	if( self->flags & FL_TEAMSLAVE ) {
		return; // only the team master does this

	}

	// make sure we don't open the same areaportal twice
	if( self->style == iopen ) {
		return;
	}

	self->style = iopen;
	GClip_SetAreaPortalState( self, open );
}

static void door_go_down( edict_t *self );

static void door_hit_top( edict_t *self ) {
	if( !( self->flags & FL_TEAMSLAVE ) ) {
		if( self->moveinfo.sound_end ) {
			G_AddEvent( self, EV_DOOR_HIT_TOP, self->moveinfo.sound_end, true );
		}
		self->s.sound = 0;
	}
	self->moveinfo.state = STATE_TOP;
	if( self->spawnflags & DOOR_TOGGLE ) {
		return;
	}
	if( self->moveinfo.wait >= 0 ) {
		self->think = door_go_down;
		self->nextThink = level.time + ( self->moveinfo.wait * 1000 );
	}
}

static void door_hit_bottom( edict_t *self ) {
	if( !( self->flags & FL_TEAMSLAVE ) ) {
		if( self->moveinfo.sound_end ) {
			G_AddEvent( self, EV_DOOR_HIT_BOTTOM, self->moveinfo.sound_end, true );
		}
		self->s.sound = 0;
	}
	self->moveinfo.state = STATE_BOTTOM;
	door_use_areaportals( self, false );
}

void door_go_down( edict_t *self ) {
	if( !( self->flags & FL_TEAMSLAVE ) ) {
		if( self->moveinfo.sound_start ) {
			G_AddEvent( self, EV_DOOR_START_MOVING, self->moveinfo.sound_start, true );
		}
		self->s.sound = self->moveinfo.sound_middle;
	}
	if( self->max_health ) {
		self->deadflag = DEAD_NO;
		self->takedamage = DAMAGE_YES;
		self->health = self->max_health;
	}

	self->moveinfo.state = STATE_DOWN;
	if( !Q_stricmp( self->classname, "func_door_rotating" ) ) {
		AngleMove_Calc( self, self->moveinfo.start_angles, door_hit_bottom );
	} else {
		Move_Calc( self, self->moveinfo.start_origin, door_hit_bottom );
	}
}

static void door_go_up( edict_t *self, edict_t *activator ) {
	if( self->moveinfo.state == STATE_UP ) {
		return; // already going up

	}
	if( self->moveinfo.state == STATE_TOP ) { // reset top wait time
		if( self->moveinfo.wait >= 0 ) {
			self->nextThink = level.time + ( self->moveinfo.wait * 1000 );
		}
		return;
	}

	if( !( self->flags & FL_TEAMSLAVE ) ) {
		if( self->moveinfo.sound_start ) {
			G_AddEvent( self, EV_DOOR_START_MOVING, self->moveinfo.sound_start, true );
		}
		self->s.sound = self->moveinfo.sound_middle;
	}

	self->moveinfo.state = STATE_UP;
	if( !Q_stricmp( self->classname, "func_door_rotating" ) ) {
		AngleMove_Calc( self, self->moveinfo.end_angles, door_hit_top );
	} else {
		Move_Calc( self, self->moveinfo.end_origin, door_hit_top );
	}

	G_UseTargets( self, activator );
	door_use_areaportals( self, true );
}

static void door_use( edict_t *self, edict_t *other, edict_t *activator ) {
	edict_t *ent;

	if( self->flags & FL_TEAMSLAVE ) {
		return;
	}

	if( self->spawnflags & DOOR_TOGGLE ) {
		if( self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP ) {
			// trigger all paired doors
			for( ent = self; ent; ent = ent->teamchain ) {
				ent->message = NULL;
				ent->touch = NULL;
				door_go_down( ent );
			}
			return;
		}
	}

	// trigger all paired doors
	for( ent = self; ent; ent = ent->teamchain ) {
		ent->message = NULL;
		ent->touch = NULL;
		door_go_up( ent, activator );
	}
}

static void Touch_DoorTrigger( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( G_IsDead( other ) ) {
		return;
	}
	if( self->s.team && other->s.team != self->s.team ) {
		return;
	}
	if( ( !other->r.client ) ) {
		return;
	}
	if( level.time < self->timeStamp + 1000 ) {
		return;
	}

	self->timeStamp = level.time;
	door_use( self->r.owner, other, other );
}

static void Think_CalcMoveSpeed( edict_t *self ) {
	edict_t *ent;
	float min;
	float time;
	float newspeed;
	float dist;

	if( self->flags & FL_TEAMSLAVE ) {
		return; // only the team master does this

	}

	// find the smallest distance any member of the team will be moving
	min = fabs( self->moveinfo.distance );
	for( ent = self->teamchain; ent; ent = ent->teamchain ) {
		dist = fabs( ent->moveinfo.distance );
		if( dist < min ) {
			min = dist;
		}
	}

	time = min / self->moveinfo.speed;

	// adjust speeds so they will all complete at the same time
	for( ent = self; ent; ent = ent->teamchain ) {
		newspeed = fabs( ent->moveinfo.distance ) / time;
		ent->moveinfo.speed = newspeed;
	}
}

static void Think_SpawnDoorTrigger( edict_t *ent ) {
	edict_t *other;
	vec3_t mins, maxs;
	float expand_size = 80;     // was 60

	if( ent->flags & FL_TEAMSLAVE ) {
		return; // only the team leader spawns a trigger

	}
	VectorCopy( ent->r.absmin, mins );
	VectorCopy( ent->r.absmax, maxs );

	for( other = ent->teamchain; other; other = other->teamchain ) {
		AddPointToBounds( other->r.absmin, mins, maxs );
		AddPointToBounds( other->r.absmax, mins, maxs );
	}

	// expand
	mins[0] -= expand_size;
	mins[1] -= expand_size;
	maxs[0] += expand_size;
	maxs[1] += expand_size;

	other = G_Spawn();
	VectorCopy( mins, other->r.mins );
	VectorCopy( maxs, other->r.maxs );
	other->r.owner = ent;
	other->s.team = ent->s.team;
	other->r.solid = SOLID_TRIGGER;
	other->movetype = MOVETYPE_NONE;
	other->touch = Touch_DoorTrigger;
	GClip_LinkEntity( other );

	door_use_areaportals( ent, ( ent->spawnflags & DOOR_START_OPEN ) != 0 );

	Think_CalcMoveSpeed( ent );
}

static void door_blocked( edict_t *self, edict_t *other ) {
	edict_t *ent;

	if( !other->r.client ) {
		// give it a chance to go away on its own terms (like gibs)
		G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, 100000, 1, 0, MOD_CRUSH );

		// if it's still there, nuke it
		if( other->r.inuse ) {
			BecomeExplosion1( other );
		}
		return;
	}

	G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, self->dmg, 1, 0, MOD_CRUSH );

	if( self->spawnflags & DOOR_CRUSHER ) {
		return;
	}


	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if( self->moveinfo.wait >= 0 ) {
		if( self->moveinfo.state == STATE_DOWN ) {
			for( ent = self->teammaster; ent; ent = ent->teamchain )
				door_go_up( ent, ent->activator );
		} else {
			for( ent = self->teammaster; ent; ent = ent->teamchain )
				door_go_down( ent );
		}
	}
}

static void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point ) {
	edict_t *ent;

	for( ent = self->teammaster; ent; ent = ent->teamchain ) {
		ent->health = ent->max_health;
		if( ent->spawnflags & DOOR_DIE_ONCE ) {
			ent->takedamage = DAMAGE_NO;
		}
	}

	if( !self->s.team || self->s.team == attacker->s.team || self->s.team == inflictor->s.team ) {
		door_use( self->teammaster, attacker, attacker );
	}
}

static void door_touch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( !other->r.client ) {
		return;
	}
	if( level.time < self->timeStamp + 5000 ) {
		return;
	}

	self->timeStamp = level.time;

	G_CenterPrintMsg( other, "%s", self->message );
	G_Sound( other, CHAN_AUTO, trap_SoundIndex( S_WORLD_MESSAGE ), ATTN_NORM );
}

void SP_func_door( edict_t *ent ) {
	vec3_t abs_movedir;

	G_InitMover( ent );
	G_SetMovedir( ent->s.angles, ent->moveinfo.movedir );

	G_AssignMoverSounds( ent, S_DOOR_START, S_DOOR_MOVE, S_DOOR_STOP );

	ent->moveinfo.blocked = door_blocked;
	ent->use = door_use;

	if( !ent->speed ) {
		ent->speed = 600;
	}
	if( !ent->wait ) {
		ent->wait = 2;
	}
	if( !st.lip ) {
		st.lip = 8;
	}
	if( !ent->dmg ) {
		ent->dmg = 2;
	}

	if( ent->health < 0 ) {
		ent->health = 0;
	} else if( !ent->health ) {
		ent->health = 1;
	} else {
		ent->spawnflags |= DOOR_DIE_ONCE; // not used by the editor

	}
	if( st.gameteam ) {
		if( st.gameteam >= TEAM_SPECTATOR && st.gameteam < GS_MAX_TEAMS ) {
			ent->s.team = st.gameteam;
		} else {
			ent->s.team = TEAM_SPECTATOR;
		}
	}

	ent->s.effects = EF_WORLD_MODEL;

	// calculate second position
	VectorCopy( ent->s.origin, ent->moveinfo.start_origin );
	abs_movedir[0] = fabs( ent->moveinfo.movedir[0] );
	abs_movedir[1] = fabs( ent->moveinfo.movedir[1] );
	abs_movedir[2] = fabs( ent->moveinfo.movedir[2] );
	ent->moveinfo.distance = abs_movedir[0] * ent->r.size[0] + abs_movedir[1] * ent->r.size[1] + abs_movedir[2] * ent->r.size[2] - st.lip;
	VectorMA( ent->moveinfo.start_origin, ent->moveinfo.distance, ent->moveinfo.movedir, ent->moveinfo.end_origin );

	// if it starts open, switch the positions
	if( ent->spawnflags & DOOR_START_OPEN ) {
		VectorCopy( ent->moveinfo.end_origin, ent->s.origin );
		VectorCopy( ent->moveinfo.start_origin, ent->moveinfo.end_origin );
		VectorCopy( ent->s.origin, ent->moveinfo.start_origin );
		VectorNegate( ent->moveinfo.movedir, ent->moveinfo.movedir );
	}

	ent->moveinfo.state = STATE_BOTTOM;

	if( ent->health ) {
		ent->max_health = ent->health;
		ent->takedamage = DAMAGE_YES;
		ent->die = door_killed;
	} else if( ent->targetname && ent->message ) {
		trap_SoundIndex( S_WORLD_MESSAGE ); // precache
		ent->touch = door_touch;
	}

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.wait = ent->wait;
	VectorCopy( ent->s.angles, ent->moveinfo.start_angles );
	VectorCopy( ent->s.angles, ent->moveinfo.end_angles );

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if( !ent->team ) {
		ent->teammaster = ent;
	}

	GClip_LinkEntity( ent );

	ent->style = -1;
	door_use_areaportals( ent, ( ent->spawnflags & DOOR_START_OPEN ) != 0 );

	ent->nextThink = level.time + 1;
	if( ent->targetname ) {
		ent->think = Think_CalcMoveSpeed;
	} else {
		ent->think = Think_SpawnDoorTrigger;
	}
}

void SP_func_door_rotating( edict_t *ent ) {
	G_InitMover( ent );

	VectorClear( ent->s.angles );

	// set the axis of rotation
	VectorClear( ent->moveinfo.movedir );
	if( ent->spawnflags & DOOR_X_AXIS ) {
		ent->moveinfo.movedir[2] = 1.0;
	} else if( ent->spawnflags & DOOR_Y_AXIS ) {
		ent->moveinfo.movedir[0] = 1.0;
	} else { // Z_AXIS
		ent->moveinfo.movedir[1] = 1.0;
	}

	// check for reverse rotation
	if( ent->spawnflags & DOOR_REVERSE ) {
		VectorNegate( ent->moveinfo.movedir, ent->moveinfo.movedir );
	}

	if( !st.distance ) {
		if( developer->integer ) {
			G_Printf( "%s at %s with no distance set\n", ent->classname, vtos( ent->s.origin ) );
		}
		st.distance = 90;
	}

	VectorCopy( ent->s.angles, ent->moveinfo.start_angles );
	VectorMA( ent->moveinfo.start_angles, st.distance, ent->moveinfo.movedir, ent->moveinfo.end_angles );
	ent->moveinfo.distance = st.distance;

	ent->moveinfo.blocked = door_blocked;
	ent->use = door_use;

	if( !ent->speed ) {
		ent->speed = 100;
	}
	if( !ent->wait ) {
		ent->wait = 3;
	}
	if( !ent->dmg ) {
		ent->dmg = 2;
	}

	G_AssignMoverSounds( ent, S_DOOR_ROTATING_START, S_DOOR_ROTATING_MOVE, S_DOOR_ROTATING_STOP );

	// if it starts open, switch the positions
	if( ent->spawnflags & DOOR_START_OPEN ) {
		VectorCopy( ent->moveinfo.end_angles, ent->s.angles );
		VectorCopy( ent->moveinfo.start_angles, ent->moveinfo.end_angles );
		VectorCopy( ent->s.angles, ent->moveinfo.start_angles );
		VectorNegate( ent->moveinfo.movedir, ent->moveinfo.movedir );
	}

	if( ent->health ) {
		ent->takedamage = DAMAGE_YES;
		ent->die = door_killed;
		ent->max_health = ent->health;
	}

	if( ent->targetname && ent->message ) {
		trap_SoundIndex( S_WORLD_MESSAGE ); // precache
		ent->touch = door_touch;
	}

	ent->moveinfo.state = STATE_BOTTOM;
	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.wait = ent->wait;
	VectorCopy( ent->s.origin, ent->moveinfo.start_origin );
	VectorCopy( ent->s.origin, ent->moveinfo.end_origin );

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if( !ent->team ) {
		ent->teammaster = ent;
	}

	GClip_LinkEntity( ent );

	ent->nextThink = level.time + 1;
	if( ent->health || ent->targetname ) {
		ent->think = Think_CalcMoveSpeed;
	} else {
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
			VectorScale( self->moveinfo.movedir, self->speed, self->avelocity );
			self->moveinfo.current_speed = self->speed;
		}

		self->think = NULL;
		self->moveinfo.state = STATE_FULLSPEED;
		return;
	}

	// if here, some more acceleration needs to be done
	// add acceleration value to current speed to cause accel
	self->moveinfo.current_speed += self->accel;
	VectorScale( self->moveinfo.movedir, self->moveinfo.current_speed, self->avelocity );
	self->nextThink = level.time + 1;
}

static void Think_RotateDecel( edict_t *self ) {
	if( self->moveinfo.current_speed <= 0 ) { // has reached full stop
		// if calculation cause it to go a little under, readjust
		if( self->moveinfo.current_speed != 0 ) {
			VectorClear( self->avelocity );
			self->moveinfo.current_speed = 0;
		}

		self->think = NULL;
		self->moveinfo.state = STATE_STOPPED;
		return;
	}

	// if here, some more deceleration needs to be done
	// subtract deceleration value from current speed to cause decel
	self->moveinfo.current_speed -= self->decel;
	VectorScale( self->moveinfo.movedir, self->moveinfo.current_speed, self->avelocity );
	self->nextThink = level.time + 1;
}

static void rotating_blocked( edict_t *self, edict_t *other ) {
	G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, self->dmg, 1, 0, MOD_CRUSH );
}

static void rotating_touch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( self->avelocity[0] || self->avelocity[1] || self->avelocity[2] ) {
		G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, self->dmg, 1, 0, MOD_CRUSH );
	}
}

static void rotating_use( edict_t *self, edict_t *other, edict_t *activator ) {
	// first, figure out what state we are in
	if( self->moveinfo.state == STATE_ACCEL || self->moveinfo.state == STATE_FULLSPEED ) {
		// if decel is 0 then just stop
		if( self->decel == 0 ) {
			VectorClear( self->avelocity );
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
			VectorScale( self->moveinfo.movedir, self->speed, self->avelocity );
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

void SP_func_rotating( edict_t *ent ) {
	G_InitMover( ent );

	if( ent->spawnflags & 32 ) {
		ent->movetype = MOVETYPE_STOP;
	} else {
		ent->movetype = MOVETYPE_PUSH;
	}

	ent->moveinfo.state = STATE_STOPPED; // rotating thingy starts out idle

	// set the axis of rotation
	VectorClear( ent->moveinfo.movedir );
	if( ent->spawnflags & 4 ) {
		ent->moveinfo.movedir[2] = 1.0;
	} else if( ent->spawnflags & 8 ) {
		ent->moveinfo.movedir[0] = 1.0;
	} else { // Z_AXIS
		ent->moveinfo.movedir[1] = 1.0;
	}

	// check for reverse rotation
	if( ent->spawnflags & 2 ) {
		VectorNegate( ent->moveinfo.movedir, ent->moveinfo.movedir );
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

	G_AssignMoverSounds( ent, S_FUNC_ROTATING_START, S_FUNC_ROTATING_MOVE, S_FUNC_ROTATING_STOP );

	if( !( ent->spawnflags & 1 ) ) {
		G_CallUse( ent, NULL, NULL );
	}

	GClip_LinkEntity( ent );
}


//======================================================================
//
//BUTTONS
//
//======================================================================

static void button_done( edict_t *self ) {
	self->moveinfo.state = STATE_BOTTOM;
}

static void button_return( edict_t *self ) {
	self->moveinfo.state = STATE_DOWN;

	Move_Calc( self, self->moveinfo.start_origin, button_done );

	if( self->health ) {
		self->deadflag = DEAD_NO;
		self->takedamage = DAMAGE_YES;
	}
}

static void button_wait( edict_t *self ) {
	self->moveinfo.state = STATE_TOP;

	G_UseTargets( self, self->activator );
	if( self->moveinfo.wait >= 0 ) {
		self->nextThink = level.time + ( self->moveinfo.wait * 1000 );
		self->think = button_return;
	}
}

static void button_fire( edict_t *self ) {
	if( self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP ) {
		return;
	}

	self->moveinfo.state = STATE_UP;
	if( self->moveinfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
		G_AddEvent( self, EV_BUTTON_FIRE, self->moveinfo.sound_start, true );
	}
	Move_Calc( self, self->moveinfo.end_origin, button_wait );
}

static void button_use( edict_t *self, edict_t *other, edict_t *activator ) {
	self->activator = activator;
	button_fire( self );
}

static void button_touch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( !other->r.client ) {
		return;
	}
	if( G_IsDead( other ) ) {
		return;
	}

	self->activator = other;
	button_fire( self );
}

static void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point ) {
	self->activator = attacker;
	self->health = self->max_health;
	self->takedamage = DAMAGE_NO;
	button_fire( self );
}

void SP_func_button( edict_t *ent ) {
	vec3_t abs_movedir;
	float dist;

	G_InitMover( ent );
	G_SetMovedir( ent->s.angles, ent->moveinfo.movedir );

	if( st.noise && Q_stricmp( st.noise, "default" ) ) {
		if( Q_stricmp( st.noise, "silent" ) != 0 ) {
			ent->moveinfo.sound_start = trap_SoundIndex( st.noise );
		}
	} else {
		ent->moveinfo.sound_start = trap_SoundIndex( S_BUTTON_START );
	}

	if( !ent->speed ) {
		ent->speed = 40;
	}

	if( !ent->wait ) {
		ent->wait = 3;
	}
	if( !st.lip ) {
		st.lip = 4;
	}

	VectorCopy( ent->s.origin, ent->moveinfo.start_origin );
	abs_movedir[0] = fabs( ent->moveinfo.movedir[0] );
	abs_movedir[1] = fabs( ent->moveinfo.movedir[1] );
	abs_movedir[2] = fabs( ent->moveinfo.movedir[2] );
	dist = abs_movedir[0] * ent->r.size[0] + abs_movedir[1] * ent->r.size[1] + abs_movedir[2] * ent->r.size[2] - st.lip;
	VectorMA( ent->moveinfo.start_origin, dist, ent->moveinfo.movedir, ent->moveinfo.end_origin );

	ent->use = button_use;

	if( ent->health ) {
		ent->max_health = ent->health;
		ent->die = button_killed;
		ent->takedamage = DAMAGE_YES;
	} else if( !ent->targetname ) {
		ent->touch = button_touch;
	}

	ent->moveinfo.state = STATE_BOTTOM;

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.wait = ent->wait;
	VectorCopy( ent->s.angles, ent->moveinfo.start_angles );
	VectorCopy( ent->s.angles, ent->moveinfo.end_angles );

	GClip_LinkEntity( ent );
}

#define TRAIN_START_ON      1
#define TRAIN_TOGGLE        2
#define TRAIN_BLOCK_STOPS   4

static void train_next( edict_t *self );

static void train_blocked( edict_t *self, edict_t *other ) {
	if( !other->r.client ) {
		// give it a chance to go away on its own terms (like gibs)
		G_Damage( other, self, self, vec3_origin, vec3_origin, other->s.origin, 100000, 1, 0, MOD_CRUSH );

		// if it's still there, nuke it
		if( other->r.inuse ) {
			BecomeExplosion1( other );
		}
		return;
	}

	if( level.time < self->timeStamp + 500 ) {
		return;
	}

	if( !self->dmg ) {
		return;
	}
	self->timeStamp = level.time;
	G_Damage( other, self, world, vec3_origin, vec3_origin, other->s.origin, self->dmg, 1, 0, MOD_CRUSH );
}

static void train_wait( edict_t *self ) {
	if( self->target_ent->pathtarget ) {
		const char *savetarget;
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
			self->nextThink = level.time + ( self->moveinfo.wait * 1000 );
			self->think = train_next;
		} else if( self->spawnflags & TRAIN_TOGGLE ) {   // && wait < 0
			train_next( self );
			self->spawnflags &= ~TRAIN_START_ON;
			VectorClear( self->velocity );
			self->nextThink = 0;
		}

		if( !( self->flags & FL_TEAMSLAVE ) ) {
			if( self->moveinfo.sound_end ) {
				G_AddEvent( self, EV_TRAIN_STOP, self->moveinfo.sound_end, true );
			}
			self->s.sound = 0;
		}
	} else {
		train_next( self );
	}

}

void train_next( edict_t *self ) {
	edict_t *ent;
	vec3_t dest;
	bool first;

	first = true;
again:
	if( !self->target ) {
		//		G_Printf ("train_next: no next target\n");
		return;
	}

	ent = G_PickTarget( self->target );
	if( !ent ) {
		if( developer->integer ) {
			G_Printf( "train_next: bad target %s\n", self->target );
		}
		return;
	}

	self->target = ent->target;

	// check for a teleport path_corner
	if( ent->spawnflags & 1 ) {
		if( !first ) {
			if( developer->integer ) {
				G_Printf( "connected teleport path_corners, see %s at %s\n", ent->classname, vtos( ent->s.origin ) );
			}

			return;
		}

		first = false;
		VectorSubtract( ent->s.origin, self->r.mins, self->s.origin );
		VectorCopy( self->s.origin, self->olds.origin );
		GClip_LinkEntity( self );
		self->s.teleported = true;
		goto again;
	}

	self->moveinfo.wait = ent->wait;
	self->target_ent = ent;

	if( !( self->flags & FL_TEAMSLAVE ) ) {
		if( self->moveinfo.sound_start ) {
			G_AddEvent( self, EV_TRAIN_START, self->moveinfo.sound_start, true );
		}
		self->s.sound = self->moveinfo.sound_middle;
	}

	VectorSubtract( ent->s.origin, self->r.mins, dest );
	self->moveinfo.state = STATE_TOP;
	VectorCopy( self->s.origin, self->moveinfo.start_origin );
	VectorCopy( dest, self->moveinfo.end_origin );
	Move_Calc( self, dest, train_wait );
	self->spawnflags |= TRAIN_START_ON;
}

static void train_resume( edict_t *self ) {
	edict_t *ent;
	vec3_t dest;

	ent = self->target_ent;

	VectorSubtract( ent->s.origin, self->r.mins, dest );
	self->moveinfo.state = STATE_TOP;
	VectorCopy( self->s.origin, self->moveinfo.start_origin );
	VectorCopy( dest, self->moveinfo.end_origin );
	Move_Calc( self, dest, train_wait );
	self->spawnflags |= TRAIN_START_ON;
}

static void func_train_find( edict_t *self ) {
	edict_t *ent;

	if( !self->target ) {
		if( developer->integer ) {
			G_Printf( "train_find: no target\n" );
		}
		return;
	}

	ent = G_PickTarget( self->target );
	if( !ent ) {
		if( developer->integer ) {
			G_Printf( "train_find: target %s not found\n", self->target );
		}
		return;
	}

	self->target = ent->target;

	VectorSubtract( ent->s.origin, self->r.mins, self->s.origin );
	GClip_LinkEntity( self );

	// if not triggered, start immediately
	if( !self->targetname ) {
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
		VectorClear( self->velocity );
		self->nextThink = 0;
	} else {
		if( self->target_ent ) {
			train_resume( self );
		} else {
			train_next( self );
		}
	}
}

void SP_func_train( edict_t *self ) {
	G_InitMover( self );

	VectorClear( self->s.angles );
	self->moveinfo.blocked = train_blocked;
	if( self->spawnflags & TRAIN_BLOCK_STOPS ) {
		self->dmg = 0;
	} else {
		if( !self->dmg ) {
			self->dmg = 100;
		}
	}

	G_AssignMoverSounds( self, NULL, NULL, NULL );

	if( !self->speed ) {
		self->speed = 100;
	}

	self->moveinfo.speed = self->speed;
	self->use = train_use;
	self->s.effects = EF_WORLD_MODEL;

	GClip_LinkEntity( self );

	if( self->target ) {
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->nextThink = level.time + 1;
		self->think = func_train_find;
	} else {
		if( developer->integer ) {
			G_Printf( "func_train without a target at %s\n", vtos( self->r.absmin ) );
		}
	}
}

static void trigger_elevator_use( edict_t *self, edict_t *other, edict_t *activator ) {
	edict_t *target;

	if( self->movetarget->nextThink ) {
		//		G_Printf( "elevator busy\n" );
		return;
	}

	if( !other->pathtarget ) {
		if( developer->integer ) {
			G_Printf( "elevator used with no pathtarget\n" );
		}
		return;
	}

	target = G_PickTarget( other->pathtarget );
	if( !target ) {
		if( developer->integer ) {
			G_Printf( "elevator used with bad pathtarget: %s\n", other->pathtarget );
		}
		return;
	}

	self->movetarget->target_ent = target;
	train_resume( self->movetarget );
}

static void trigger_elevator_init( edict_t *self ) {
	if( !self->target ) {
		if( developer->integer ) {
			G_Printf( "trigger_elevator has no target\n" );
		}
		return;
	}
	self->movetarget = G_PickTarget( self->target );
	if( !self->movetarget ) {
		if( developer->integer ) {
			G_Printf( "trigger_elevator unable to find target %s\n", self->target );
		}
		return;
	}
	if( Q_stricmp( self->movetarget->classname, "func_train" ) ) {
		if( developer->integer ) {
			G_Printf( "trigger_elevator target %s is not a train\n", self->target );
		}
		return;
	}

	self->use = trigger_elevator_use;
	self->r.svflags = SVF_NOCLIENT;

}

void SP_trigger_elevator( edict_t *self ) {
	self->think = trigger_elevator_init;
	self->nextThink = level.time + 1;
}

void func_timer_think( edict_t *self ) {
	G_UseTargets( self, self->activator );
	self->nextThink = level.time + 1000 * ( self->wait + crandom() * self->random );
}

void func_timer_use( edict_t *self, edict_t *other, edict_t *activator ) {
	self->activator = activator;

	// if on, turn it off
	if( self->nextThink ) {
		self->nextThink = 0;
		return;
	}

	// turn it on
	if( self->delay ) {
		self->nextThink = level.time + self->delay * 1000;
	} else {
		func_timer_think( self );
	}
}

void SP_func_timer( edict_t *self ) {
	if( !self->wait ) {
		self->wait = 1.0;
	}

	self->use = func_timer_use;
	self->think = func_timer_think;

	if( self->random >= self->wait ) {
		self->random = self->wait - 0.1;
		if( developer->integer ) {
			G_Printf( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
		}
	}

	if( self->spawnflags & 1 ) {
		self->nextThink = level.time + 1000 *
						  ( 1.0 + st.pausetime + self->delay + self->wait + crandom() * self->random );
		self->activator = self;
	}
}
