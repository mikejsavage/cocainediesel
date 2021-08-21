#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_weapons.h"

#define INSTANT 0

#define WEAPONDOWN_TIME 50
#define WEAPONUP_TIME_FAST 150
#define WEAPONUP_TIME_NORMAL 350
#define WEAPONUP_TIME_SLOW 750
#define WEAPONUP_TIME_VERY_SLOW 1000
#define HITSCAN_RANGE 9001

const WeaponDef weapon_defs[] = {
	{ "", "", WeaponCategory_Count }, // Weapon_None

	{
		/* name                 */ "Knife",
		/* short name           */ "gb",
		/* category             */ WeaponCategory_Count,

		/* projectile count     */ 8,
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
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 0,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 20,
	},

	{
		/* name                 */ "9mm",
		/* short name           */ "9mm",
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
		/* wallbang damage      */ 0.5f,
		/* knockback            */ 20,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "SMG",
		/* short name           */ "mg",
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

		/* max recoil           */ EulerDegrees2( 80.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 50.0f, -25.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 9,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.5f,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Deagle",
		/* short name           */ "deagle",
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
		/* max recoil           */ EulerDegrees2( 325.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 300.0f, -40.0f ),
		/* recoil recovery      */ 3250.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 25,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.8f,
		/* knockback            */ 30,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Shotgun",
		/* short name           */ "rg",
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
		/* max recoil           */ EulerDegrees2( 325.0f, -50.0f ),
		/* min recoil           */ EulerDegrees2( 275.0f, -40.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 2,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.5f,
		/* knockback            */ 6,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 50,
	},

	{
		/* name                 */ "Burst Rifle",
		/* short name           */ "br",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 35,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 80.0f, -20.0f ),
		/* min recoil           */ EulerDegrees2( 70.0f, -10.0f ),
		/* recoil recovery      */ 2500.0f,
		/* firing mode          */ FiringMode_Clip,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 9,
		/* self damage          */ 0.0f,
		/* wallbang damage      */ 0.75,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Stakes",
		/* short name           */ "stake",
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
		/* max recoil           */ EulerDegrees2( 250.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 250.0f, -5.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 50,
		/* self damage          */ 1.0f,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 100,
		/* splash radius        */ 120,
		/* splash min damage    */ 15,
		/* splash min knockback */ 50,

		// projectile def
		/* speed                */ 2000,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Grenades",
		/* short name           */ "gl",
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
		/* max recoil           */ EulerDegrees2( 300.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 250.0f, -5.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 40,
		/* self damage          */ 1.0f,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 100,
		/* splash radius        */ 120,
		/* splash min damage    */ 10,
		/* splash min knockback */ 50,

		// projectile def
		/* speed                */ 1400,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Rockets",
		/* short name           */ "rl",
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
		/* max recoil           */ EulerDegrees2( 300.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 200.0f, -5.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 40,
		/* self damage          */ 1.0f,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 100,
		/* splash radius        */ 120,
		/* splash min damage    */ 10,
		/* splash min knockback */ 50,

		// projectile def
		/* speed                */ 1400,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Assault Rifle",
		/* short name           */ "ar",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 30,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 50,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 80.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 50.0f, -25.0f ),
		/* recoil recovery      */ 1350.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 8,
		/* self damage          */ 0,
		/* wallbang damage      */ 1.0f, //not implemented
		/* knockback            */ 30,
		/* splash radius        */ 45,
		/* splash min damage    */ 7,
		/* splash min knockback */ 5,

		// projectile def
		/* speed                */ 3500,
		/* spread               */ 0.0f,
	},

	{
		/* name                 */ "BubbleGun",
		/* short name           */ "bg",
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
		/* max recoil           */ EulerDegrees2( 80.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 50.0f, -25.0f ),
		/* recoil recovery      */ 1350.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 15,
		/* self damage          */ 1,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 45,
		/* splash radius        */ 80,
		/* splash min damage    */ 14,
		/* splash min knockback */ 25,

		// projectile def
		/* speed                */ 650,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Laser",
		/* short name           */ "lg",
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
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 14,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Railgun",
		/* short name           */ "eb",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 0,
		/* reload time          */ 500, // time to fully charge for rail
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 1000,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 150.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -40.0f ),
		/* recoil recovery      */ 1000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 38,
		/* self damage          */ 0,
		/* wallbang damage      */ 1.0f, //not implemented
		/* knockback            */ 50,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Sniper",
		/* short name           */ "sniper",
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
		/* max recoil           */ EulerDegrees2( 275.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 250.0f, -5.0f ),
		/* recoil recovery      */ 1750.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 25.0f,
		/* zoom inaccuracy      */ 30.0f,

		// damages
		/* damage               */ 50,
		/* self damage          */ 0,
		/* wallbang damage      */ 1.0f,
		/* knockback            */ 100,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 0,
	},

	{
		/* name                 */ "Rifle",
		/* short name           */ "rifle",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 2000,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 600,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 200.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 175.0f, -5.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 40,
		/* self damage          */ 0,
		/* wallbang damage      */ 1.0f, //not implemented
		/* knockback            */ 50,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 5500,
		/* spread               */ 0,
	},

	{
		/* name                 */ "MasterBlaster",
		/* short name           */ "mb",
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
		/* max recoil           */ EulerDegrees2( 300.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 275.0f, -25.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 3,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 3000,
		/* spread               */ 25,
	},

	{
		/* name                 */ "Road Gun",
		/* short name           */ "road",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 20,
		/* reload time          */ 1500,
		/* staged reloading     */ false,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 75,
		/* timeout / range      */ 5000,
		/* max recoil           */ EulerDegrees2( 80.0f, 25.0f ),
		/* min recoil           */ EulerDegrees2( 70.0f, -25.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 8,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 3000,
		/* spread               */ 0,
	},

#if 0
	{
		/* name                 */ "Minigun",
		/* short name           */ "minigun",
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
		/* knockback            */ 30,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ INSTANT,
		/* spread               */ 250,
	},
#endif
};

STATIC_ASSERT( ARRAY_COUNT( weapon_defs ) == Weapon_Count );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon ) {
	assert( weapon < Weapon_Count );
	return &weapon_defs[ weapon ];
}

const GadgetDef gadget_defs[] = {
	{ },

	{
		/* name             */ "Throwing axe",
		/* short name       */ "axe",
		/* switch_in_time   */ WEAPONUP_TIME_NORMAL,
		/* using_time       */ 500,
		/* cook_time        */ 1000,
		/* switch_out_time  */ WEAPONDOWN_TIME,
		/* damage           */ 50,
		/* knockback        */ 100,
		/* mindamage        */ 25,
		/* minknockback     */ 0,
		/* splash_radius    */ 0,
		/* timeout          */ 5000,
		/* speed            */ 1500,
		/* uses             */ 1,
	},

	{
		"Suicide vest",
		"suicidevest",
	},

	{
		/* name             */ "Flash bang",
		/* short name       */ "flashbang",
		/* switch_in_time   */ WEAPONUP_TIME_NORMAL,
		/* using_time       */ 500,
		/* cook_time        */ 0,
		/* switch_out_time  */ WEAPONDOWN_TIME,
		/* damage           */ 5,
		/* knockback        */ 0,
		/* mindamage        */ 120,
		/* minknockback     */ 0,
		/* splash_radius    */ 300,
		/* timeout          */ 2500,
		/* speed            */ 750,
		/* uses             */ 1,
	},
};

STATIC_ASSERT( ARRAY_COUNT( gadget_defs ) == Gadget_Count );

const GadgetDef * GetGadgetDef( GadgetType gadget ) {
	assert( gadget < Gadget_Count );
	return &gadget_defs[ gadget ];
}
