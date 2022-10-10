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

enum SolidBits : u8 {
	Solid_NotSolid = 0,
	Solid_PlayerClip = ( 1 << 1 ),
	Solid_WeaponClip = ( 1 << 2 ),
	Solid_Wallbangable = ( 1 << 3 ),
	Solid_Ladder = ( 1 << 4 ),
	Solid_Trigger = ( 1 << 5 ),

	Solid_Player = ( 1 << 6 ),
};

constexpr SolidBits Solid_Solid = SolidBits( Solid_PlayerClip | Solid_WeaponClip | Solid_Wallbangable );
constexpr SolidBits Solid_Opaque = SolidBits( Solid_WeaponClip | Solid_Wallbangable );
constexpr SolidBits Solid_Wallbang = SolidBits( Solid_WeaponClip | Solid_Player );
constexpr SolidBits Solid_Shot = SolidBits( Solid_WeaponClip | Solid_Wallbangable | Solid_Player );

constexpr SolidBits Solid_Everything = SolidBits( U8_MAX );

struct trace_t {
	float fraction;
	Vec3 endpos;
	Vec3 contact;
	Vec3 normal;
	SolidBits solidity;
	int ent;

	bool HitSomething() {
		return this->ent > -1;
	}

	bool HitNothing() {
		return this->ent == -1;
	}

	bool GotSomewhere() {
		return this->fraction > 0.0f;
	}

	bool GotNowhere() {
		return this->fraction == 0.0f;
	}
};
