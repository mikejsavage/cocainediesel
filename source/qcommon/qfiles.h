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

#define IDBSPHEADER     "IBSP"
#define QFBSPHEADER     "FBSP"

#define MAX_MAP_LEAFS       0x20000

#define SURF_NODAMAGE     0x1         // never give falling damage
#define SURF_SLICK        0x2         // effects game physics
#define SURF_SKY          0x4         // lighting from environment map
#define SURF_LADDER       0x8
#define SURF_NOIMPACT     0x10        // don't make missile explosions
#define SURF_NOMARKS      0x20        // don't leave missile marks
#define SURF_FLESH        0x40        // make flesh sounds and effects
#define SURF_NODRAW       0x80        // don't generate a drawsurface at all
#define SURF_HINT         0x100       // make a primary bsp splitter
#define SURF_SKIP         0x200       // completely ignore, allowing non-closed brushes
#define SURF_NOLIGHTMAP   0x400       // surface doesn't need a lightmap
#define SURF_POINTLIGHT   0x800       // generate lighting info at vertexes
#define SURF_METALSTEPS   0x1000      // clanking footsteps
#define SURF_NOSTEPS      0x2000      // no footstep sounds
#define SURF_NONSOLID     0x4000      // don't collide against curves with this set
#define SURF_LIGHTFILTER  0x8000      // act as a light filter during q3map -light
#define SURF_ALPHASHADOW  0x10000     // do per-pixel light shadow casting in q3map
#define SURF_NODLIGHT     0x20000     // never add dynamic lights
#define SURF_DUST         0x40000     // leave a dust trail when walking on this surface
#define SURF_NOWALLJUMP   0x80000     // can not perform walljumps on this surface

enum BSPFaceType {
	FaceType_Bad,
	FaceType_Planar,
	FaceType_Patch,
	FaceType_Mesh,
	FaceType_Flare,
	FaceType_Foliage,
};

struct dvis_t {
	int numclusters;
	int rowsize;
	unsigned char data[1];
};
