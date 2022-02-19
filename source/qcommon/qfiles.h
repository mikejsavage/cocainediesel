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

#define SURF_LADDER       0x8
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
