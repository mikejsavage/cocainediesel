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

#pragma once

#include "gameshared/q_arch.h"
#include "gameshared/q_math.h"

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID          1           // an eye is never valid in a solid
#define CONTENTS_LAVA           8
#define CONTENTS_SLIME          16
#define CONTENTS_WATER          32

#define CONTENTS_WALLBANGABLE   0x4000

#define CONTENTS_PLAYERCLIP     0x10000
#define CONTENTS_WEAPONCLIP     0x20000

#define CONTENTS_TEAMALPHA      0x400000
#define CONTENTS_TEAMBETA       0x800000

#define CONTENTS_BODY           0x2000000   // should never be on a brush, only in game
#define CONTENTS_CORPSE         0x4000000
#define CONTENTS_TRIGGER        0x40000000

// content masks
#define MASK_ALL            ( -1 )
#define MASK_SOLID          ( CONTENTS_SOLID | CONTENTS_WEAPONCLIP | CONTENTS_WALLBANGABLE )
#define MASK_PLAYERSOLID    ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_WALLBANGABLE )
#define MASK_DEADSOLID      ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WALLBANGABLE )
#define MASK_WATER          ( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WALLBANGABLE )
#define MASK_OPAQUE         ( CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA | CONTENTS_WALLBANGABLE )
#define MASK_SHOT           ( CONTENTS_SOLID | CONTENTS_WEAPONCLIP | CONTENTS_BODY | CONTENTS_CORPSE | CONTENTS_WALLBANGABLE )
#define MASK_WALLBANG       ( CONTENTS_SOLID | CONTENTS_WEAPONCLIP | CONTENTS_BODY | CONTENTS_CORPSE )

#define MASK_ALPHAPLAYERSOLID  ( MASK_PLAYERSOLID | CONTENTS_TEAMBETA )
#define MASK_BETAPLAYERSOLID   ( MASK_PLAYERSOLID | CONTENTS_TEAMALPHA )

// a trace is returned when a box is swept through the world
typedef struct {
	bool allsolid;          // if true, plane is not valid
	bool startsolid;        // if true, the initial point was in a solid area
	float fraction;             // time completed, 1.0 = didn't hit anything
	Vec3 endpos;              // final position
	Plane plane;             // surface normal at impact
	int surfFlags;              // surface hit
	int contents;               // contents on other side of surface hit
	int ent;                    // not set by CM_*() functions
} trace_t;
