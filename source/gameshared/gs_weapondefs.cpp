#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_public.h"

#define INSTANT 0

#define WEAPONDOWN_TIME 50
#define WEAPONUP_TIME_FAST 150
#define WEAPONUP_TIME_NORMAL 350
#define WEAPONUP_TIME_SLOW 750
#define WEAPONUP_TIME_VERY_SLOW 1000

const WeaponDef gs_weaponDefs[] = {
	{ "", "", "" },

	{
		"Knife", "gb",
		"Knife people in the face",
		0,

		0,                              // projectiles fired each shot
		0,                              // clip size
		0,                              // reload time
		false,                          // staged reloading

		//timings (in msecs)->
		WEAPONUP_TIME_FAST,             // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		500,                            // refire time
		70,                             // projectile timeout / projectile range for instant weapons
		0,                              // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		25,                             // damage
		0,                              // selfdamage ratio
		0,                              // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"9mm", "9mm",
		"Pew pew pew",
		100,

		1,                              // projectiles fired each shot
		15, // clip size
		1000, // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_FAST,             // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		100,                            // refire time
		HITSCAN_RANGE,                  // projectile timeout
		2.5f,                           // recoil
		FiringMode_SemiAuto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		12,                             // damage
		0,                              // selfdamage ratio
		20,                             // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"SMG", "mg",
		"Shoots fast direct bullets touching enemies at any range",
		100,

		1,                              // projectiles fired each shot
		25, // clip size
		1500, // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_NORMAL,           // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		75,                             // refire time
		HITSCAN_RANGE,                  // projectile timeout
		2.75f,                          // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		9,                              // damage
		0,                              // selfdamage ratio
		15,                             // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"Deagle", "deagle",
		"Bang bang",
		200,

		1,                              // projectiles fired each shot
		8, // clip size
		2000, // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_NORMAL,           // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		400,                            // refire time
		HITSCAN_RANGE,                  // projectile timeout
		25.0f,                          // recoil
		FiringMode_SemiAuto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		25,                             // damage
		0,                              // selfdamage ratio
		30,                             // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"Shotgun", "rg",
		"Basically a shotgun",
		100,

		25,                             // projectiles fired each shot
		5, // clip size
		1500, // reload time
		true,                           // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_SLOW,             // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		1400,                           // refire time
		HITSCAN_RANGE,                  // projectile timeout / projectile range for instant weapons
		25.0f,                          // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		2,                              // damage
		0,                              // selfdamage ratio (rg cant selfdamage)
		6,                              // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		50,                             // spread
	},

	{
		"Assault Rifle", "ar",
		"Dakka dakka",
		200,

		1,                              // projectiles fired each shot
		20, // clip size
		2000, // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_NORMAL,           // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		150,                            // refire time
		HITSCAN_RANGE,                  // projectile timeout
		3.0f,                           // recoil
		FiringMode_Auto,

		60.0f,                          // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		12,                             // damage
		0,                              // selfdamage ratio
		15,                             // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"Grenades", "gl",
		"Deprecated gun, enjoy it while it lasts nerds",
		100,

		1,                              // projectiles fired each shot
		3,                              // clip size
		2000,                           // reload time
		true,                           // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_NORMAL,           // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		1000,                           // refire time
		2000,                           // projectile timeout
		25.0f,                          // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		40,                             // damage
		0.75f,                          // selfdamage ratio
		100,                            // knockback
		120,                            // splash radius
		10,                             // splash minimum damage
		50,                             // splash minimum knockback

		//projectile def
		1500,                           // speed
		0,                              // spread
	},

	{
		"Rockets", "rl",
		"Shoots slow moving rockets that deal damage in an area and push bodies away",
		200,

		1,                              // projectiles fired each shot
		4,                              // clip size
		600,                           // reload time
		true,                           // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_SLOW,             // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		1000,                           // refire time
		10000,                          // projectile timeout
		10.0f,                          // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		45,                             // damage
		0.75f,                          // selfdamage ratio
		100,                            // knockback
		120,                            // splash radius
		10,                             // splash minimum damage
		50,                             // splash minimum knockback

		//projectile def
		1400,                           // speed
		0,                              // spread
	},

	{
		"Plasma", "pg",
		"Shoots fast projectiles that deal damage in an area",
		100,

		1,                              // projectiles fired each shot
		30, // clip size
		1500, // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_NORMAL,           // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		100,                            // refire time
		10000,                          // projectile timeout
		1.5f,                           // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		8,                              // damage
		0,                              // selfdamage ratio
		22,                             // knockback
		45,                             // splash radius
		4,                              // splash minimum damage
		1,                              // splash minimum knockback

		//projectile def
		2500,                           // speed
		0,                              // spread
	},

	{
		"Laser", "lg",
		"Shoots a continuous trail doing quick but low damage at a certain range",
		200,

		1,                              // projectiles fired each shot
		50,                             // clip size
		1000,                           // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_FAST,             // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		50,                             // refire time
		800,                            // projectile timeout / projectile range for instant weapons
		0,                              // recoil
		FiringMode_Smooth,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		5,                              // damage
		0,                              // selfdamage ratio
		14,                             // knockback
		0,                              // splash radius
		0,                              // splash minimum damage
		0,                              // splash minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"Railgun", "eb",
		"Shoots a direct laser hit doing pretty high damage",
		200,

		1,                              // projectiles fired each shot
		3, // clip size
		2250, // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_SLOW,             // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		1000,                           // refire time
		HITSCAN_RANGE,                  // range
		15.0f,                          // recoil
		FiringMode_Auto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		40,                             // damage
		0,                              // selfdamage ratio
		100,                            // knockback
		0,                              // splash radius
		0,                              // minimum damage
		0,                              // minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"Sniper", "sniper",
		"Boom boom",
		200,

		1,                              // projectiles fired each shot
		1,                              // clip size
		2000,                           // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_VERY_SLOW,        // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		50,                             // refire time
		HITSCAN_RANGE,                  // range
		40.0f,                          // recoil
		FiringMode_Auto,

		40.0f,                          // zoom fov
		30.0f,                          // !zoom inaccuracy

		//damages
		50,                             // damage
		0,                              // selfdamage ratio
		100,                            // knockback
		0,                              // splash radius
		0,                              // minimum damage
		0,                              // minimum knockback

		//projectile def
		INSTANT,                        // speed
		0,                              // spread
	},

	{
		"Rifle", "rifle",
		"Goochie gun",
		200,

		1,                              // projectiles fired each shot
		5,                              // clip size
		2000,                           // reload time
		false,                          // staged reloading

		//timings (in msecs)
		WEAPONUP_TIME_VERY_SLOW,        // weapon up time
		WEAPONDOWN_TIME,                // weapon down time
		750,                            // refire time
		10000,                          // range
		10.0f,                          // recoil
		FiringMode_SemiAuto,

		0.0f,                           // zoom fov
		0.0f,                           // !zoom inaccuracy

		//damages
		40,                             // damage
		0,                              // selfdamage ratio
		50,                             // knockback
		0,                              // splash radius
		0,                              // minimum damage
		0,                              // minimum knockback

		//projectile def
		5000,                           // speed
		0,                              // spread
	},
};

STATIC_ASSERT( ARRAY_COUNT( gs_weaponDefs ) == Weapon_Count );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon ) {
	assert( weapon < Weapon_Count );
	return &gs_weaponDefs[ weapon ];
}
