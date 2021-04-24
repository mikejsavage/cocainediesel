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
	{ "", "", WeaponCategory_Count }, // Weapon_None

	{
		/* name, short name     */ "Knife", "gb",
		/* category             */ WeaponCategory_Count,

		/* projectile count     */ 6,
		/* clip size            */ 0,
		/* reload time          */ 0,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 600,
		/* timeout / range      */ 85,
		/* max recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* min recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* recoil recovery      */ 0.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 25,
		/* self damage          */ 0,
		/* knockback            */ 0,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 45,
	},

	{
		/* name, short name     */ "9mm", "9mm",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 15,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 120,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 125.0f, 20.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -20.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 12,
		/* self damage          */ 0,
		/* knockback            */ 20,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "SMG", "mg",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 25,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 75,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 80.0f, -30.0f ),
		/* min recoil           */ EulerDegrees2( 70.0f, 5.0f ),
		/* recoil recovery      */ 1000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 9,
		/* self damage          */ 0,
		/* knockback            */ 15,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Deagle", "deagle",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 7,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 500,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 525.0f, 30.0f ),
		/* min recoil           */ EulerDegrees2( 500.0f, -30.0f ),
		/* recoil recovery      */ 4000.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 25,
		/* self damage          */ 0,
		/* knockback            */ 30,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Shotgun", "rg",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 25,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reloading     */ true,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 1250,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 325.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 275.0f, -40.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 2,
		/* self damage          */ 0,
		/* knockback            */ 6,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 50,
	},

	{
		/* name, short name     */ "Assault Rifle", "ar",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 30,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 75.0f, 20.0f ),
		/* min recoil           */ EulerDegrees2( 50.0f, -20.0f ),
		/* recoil recovery      */ 1250.0f,
		/* firing mode          */ FiringMode_Clip,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 9,
		/* self damage          */ 0,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Stakes", "stake",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 1,
		/* reload time          */ 1000,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 500,
		/* timeout / range      */ 5000,
		/* max recoil           */ EulerDegrees2( 325.0f, 60.0f ),
		/* min recoil           */ EulerDegrees2( 125.0f, 20.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 50,
		/* self damage          */ 1.0f,
		/* knockback            */ 100,
		/* splash radius        */ 120,
		/* splash min damage    */ 15,
		/* splash min knockback */ 50,

		// projectile def
		/* speed                */ 2000,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Grenades", "gl",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reloading     */ true,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 1000,
		/* timeout / range      */ 2000,
		/* max recoil           */ EulerDegrees2( 325.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 275.0f, -40.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 40,
		/* self damage          */ 1.0f,
		/* knockback            */ 100,
		/* splash radius        */ 120,
		/* splash min damage    */ 10,
		/* splash min knockback */ 50,

		// projectile def
		/* speed                */ 1400,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Rockets", "rl",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reloading     */ true,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 1000,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 325.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 275.0f, -40.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 40,
		/* self damage          */ 1.0f,
		/* knockback            */ 100,
		/* splash radius        */ 120,
		/* splash min damage    */ 10,
		/* splash min knockback */ 50,

		// projectile def
		/* speed                */ 1400,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Plasma", "pg",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 35,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 50,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 75.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 50.0f, -20.0f ),
		/* recoil recovery      */ 1350.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 7,
		/* self damage          */ 0,
		/* knockback            */ 30,
		/* splash radius        */ 45,
		/* splash min damage    */ 7,
		/* splash min knockback */ 5,

		// projectile def
		/* speed                */ 3500,
		/* spread               */ 0.0f,
	},

	{
		/* name, short name     */ "BubbleGun", "bg",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 15,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 175,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 80.0f, 30.0f ),
		/* min recoil           */ EulerDegrees2( 50.0f, -30.0f ),
		/* recoil recovery      */ 1350.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 15,
		/* self damage          */ 1,
		/* knockback            */ 45,
		/* splash radius        */ 80,
		/* splash min damage    */ 14,
		/* splash min knockback */ 25,

		// projectile def
		/* speed                */ 650,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Laser", "lg",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 40,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 50,
		/* timeout / range      */ 900,
		/* max recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* min recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* recoil recovery      */ 0.0f,
		/* firing mode          */ FiringMode_Smooth,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 5,
		/* self damage          */ 0,
		/* knockback            */ 14,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Railgun", "eb",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 8,
		/* reload time          */ 600,
		/* staged reloading     */ true,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 600,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 150.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -40.0f ),
		/* recoil recovery      */ 1000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 25,
		/* self damage          */ 0,
		/* knockback            */ 45,
		/* splash radius        */ 70,
		/* splash min damage    */ 0,
		/* splash min knockback */ 45,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Sniper", "sniper",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 1,
		/* reload time          */ 2000,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_VERY_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 500,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 550.0f, 150.0f ),
		/* min recoil           */ EulerDegrees2( 500.0f, -150.0f ),
		/* recoil recovery      */ 2500.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 25.0f,
		/* zoom inaccuracy      */ 30.0f,

		// damages
		/* damage               */ 50,
		/* self damage          */ 0,
		/* knockback            */ 100,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Rifle", "rifle",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 2000,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 600,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 250.0f, 60.0f ),
		/* min recoil           */ EulerDegrees2( 125.0f, 20.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 38,
		/* self damage          */ 0,
		/* knockback            */ 50,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 5500,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "MasterBlaster", "mb",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 10,
		/* clip size            */ 6,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 500,
		/* timeout / range      */ 5000,
		/* max recoil           */ EulerDegrees2( 325.0f, 30.0f ),
		/* min recoil           */ EulerDegrees2( 125.0f, -10.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 3,
		/* self damage          */ 0,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 3000,
		/* spread               */ 25,
	},

	{
		/* name, short name     */ "Road Gun", "road",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 21,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 50,
		/* timeout / range      */ 5000,
		/* max recoil           */ EulerDegrees2( 50.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 25.0f, -25.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 7,
		/* self damage          */ 0,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 3000,
		/* spread               */ 0,
	},

	{
		/* name, short name     */ "Minigun", "minigun",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 0,
		/* reload time          */ 0,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_VERY_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 75,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* min recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* recoil recovery      */ 1000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 9,
		/* self damage          */ 0,
		/* knockback            */ 45,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 250,
	},
};

STATIC_ASSERT( ARRAY_COUNT( gs_weaponDefs ) == Weapon_Count );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon ) {
	assert( weapon < Weapon_Count );
	return &gs_weaponDefs[ weapon ];
}
