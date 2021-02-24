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

#include "qcommon/types.h"
#include "client/renderer/types.h"
#include "gameshared/q_math.h"

constexpr u32 TILE_SIZE = 32; // forward+ tile size

struct orientation_t {
	mat3_t axis;
	Vec3 origin;
};

struct entity_t {
	const Model * model;

	mat3_t axis;
	Vec3 origin, origin2;

	RGBA8 color;

	float scale;
};

enum XAlignment {
	XAlignment_Left,
	XAlignment_Center,
	XAlignment_Right,
};

enum YAlignment {
	YAlignment_Top,
	YAlignment_Middle,
	YAlignment_Bottom,
};

struct Alignment {
	XAlignment x;
	YAlignment y;
};

constexpr Alignment Alignment_LeftTop = { XAlignment_Left, YAlignment_Top };
constexpr Alignment Alignment_CenterTop = { XAlignment_Center, YAlignment_Top };
constexpr Alignment Alignment_RightTop = { XAlignment_Right, YAlignment_Top };
constexpr Alignment Alignment_LeftMiddle = { XAlignment_Left, YAlignment_Middle };
constexpr Alignment Alignment_CenterMiddle = { XAlignment_Center, YAlignment_Middle };
constexpr Alignment Alignment_RightMiddle = { XAlignment_Right, YAlignment_Middle };
constexpr Alignment Alignment_LeftBottom = { XAlignment_Left, YAlignment_Bottom };
constexpr Alignment Alignment_CenterBottom = { XAlignment_Center, YAlignment_Bottom };
constexpr Alignment Alignment_RightBottom = { XAlignment_Right, YAlignment_Bottom };

constexpr Vec4 vec4_white = Vec4( 1, 1, 1, 1 );
constexpr Vec4 vec4_black = Vec4( 0, 0, 0, 1 );
constexpr Vec4 vec4_red = Vec4( 1, 0, 0, 1 );
constexpr Vec4 vec4_green = Vec4( 0, 1, 0, 1 );
constexpr Vec4 vec4_yellow = Vec4( 1, 1, 0, 1 );

constexpr RGBA8 rgba8_white = RGBA8( 255, 255, 255, 255 );
constexpr RGBA8 rgba8_black = RGBA8( 0, 0, 0, 255 );
