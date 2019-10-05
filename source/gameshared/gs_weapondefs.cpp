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

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

#define INSTANT 0

#define WEAPONDOWN_FRAMETIME 50
#define WEAPONUP_FRAMETIME 50

gs_weapon_definition_t gs_weaponDefs[] =
{
	{
		"no weapon",
		WEAP_NONE,
		{
			AMMO_NONE,                      // ammo tag
			0,                              // ammo usage per shot
			0,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			0,                              // reload frametime
			0,                              // projectile timeout
			false,                          // smooth refire

			//damages
			0,                              // damage
			0,                              // selfdamage ratio
			0,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			0,                              // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Gunblade",
		WEAP_GUNBLADE,
		{
			AMMO_NONE,
			0,                              // ammo usage per shot
			0,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			750,                            // reload frametime
			68,                             // projectile timeout  / projectile range for instant weapons
			false,                          // smooth refire

			//damages
			35,                             // damage
			0,                              // selfdamage ratio
			80,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Machinegun",
		WEAP_MACHINEGUN,
		{
			AMMO_BULLETS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			200,                            // reload frametime
			6000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			10,                             // damage
			0,                              // selfdamage ratio
			40,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Riotgun",
		WEAP_RIOTGUN,
		{
			AMMO_SHELLS,
			1,                              // ammo usage per shot
			20,                             // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1000,                           // reload frametime
			8192,                           // projectile timeout / projectile range for instant weapons
			false,                          // smooth refire

			//damages
			2,                              // damage
			0,                              // selfdamage ratio (rg cant selfdamage)
			7,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			80,                             // spread
			80,                             // v_spread
		},
	},

	{
		"Grenade Launcher",
		WEAP_GRENADELAUNCHER,
		{
			AMMO_GRENADES,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			800,                            // reload frametime
			1250,                           // projectile timeout
			false,                          // smooth refire

			//damages
			40,                             // damage
			0.75,                           // selfdamage ratio
			100,                            // knockback
			125,                            // splash radius
			7,                              // splash minimum damage
			35,                             // splash minimum knockback

			//projectile def
			1000,                           // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Rocket Launcher",
		WEAP_ROCKETLAUNCHER,
		{
			AMMO_ROCKETS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1000,                           // reload frametime
			10000,                          // projectile timeout
			false,                          // smooth refire

			//damages
			40,                             // damage
			0.75,                           // selfdamage ratio
			100,                            // knockback
			120,                            // splash radius
			7,                              // splash minimum damage
			45,                             // splash minimum knockback

			//projectile def
			1250,                           // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Plasmagun",
		WEAP_PLASMAGUN,
		{
			AMMO_PLASMA,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			100,                            // reload frametime
			5000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			8,                              // damage
			0.2,                            // selfdamage ratio
			22,                             // knockback
			45,                             // splash radius
			4,                              // splash minimum damage
			1,                              // splash minimum knockback

			//projectile def
			2500,                           // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Lasergun",
		WEAP_LASERGUN,
		{
			AMMO_LASERS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			50,                             // reload frametime
			700,                            // projectile timeout / projectile range for instant weapons
			true,                           // smooth refire

			//damages
			4,                              // damage
			0,                              // selfdamage ratio (lg cant damage)
			14,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
			0,                              // v_spread
		},
	},

	{
		"Electrobolt",
		WEAP_ELECTROBOLT,
		{
			AMMO_BOLTS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1250,                           // reload frametime
			900,                            // min damage range
			false,                          // smooth refire

			//damages
			35,                             // damage
			0,                              // selfdamage ratio
			80,                             // knockback
			0,                              // splash radius
			70,                             // minimum damage
			35,                             // minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
			0,                              // v_spread
		},
	},
};

STATIC_ASSERT( ARRAY_COUNT( gs_weaponDefs ) == WEAP_TOTAL );

gs_weapon_definition_t * GS_GetWeaponDef( int weapon ) {
	assert( weapon >= 0 && weapon < WEAP_TOTAL );
	return &gs_weaponDefs[ weapon ];
}
