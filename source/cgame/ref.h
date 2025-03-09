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
#include "client/renderer/api.h"
#include "gameshared/q_math.h"

struct InterpolatedEntity {
	mat3_t axis;
	Vec3 origin, origin2;

	RGBA8 color;

	Vec3 scale;

	bool animating;
	float animation_time;
};

struct MultiTypeColor {
	const RGBA8 rgba8;
	const Vec4 vec4;

	constexpr MultiTypeColor( u8 r, u8 g, u8 b, u8 a ):
		rgba8( r, g, b, a ),
		vec4( r / 255.f, g / 255.f, b / 255.f, a / 255.f )
	{}
};

constexpr MultiTypeColor diesel_yellow( 255, 204, 38, 255 );
constexpr MultiTypeColor diesel_green( 44, 209, 89, 255 ); //yolo
constexpr MultiTypeColor diesel_red( 255, 0, 57, 255 );

constexpr MultiTypeColor white( 255, 255, 255, 255 );
constexpr MultiTypeColor black( 0, 0, 0, 255 );
constexpr MultiTypeColor dark( 5, 5, 5, 255 );
constexpr MultiTypeColor red( 255, 0, 0, 255 );
constexpr MultiTypeColor green( 0, 255, 0, 255 );
constexpr MultiTypeColor yellow( 255, 255, 0, 255 );
