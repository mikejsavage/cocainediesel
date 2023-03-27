#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_weapons.h"

static constexpr int HITSCAN = 0;

static constexpr int WEAPONDOWN_TIME = 50;
static constexpr int WEAPONUP_TIME_FAST = 150;
static constexpr int WEAPONUP_TIME_NORMAL = 350;
static constexpr int WEAPONUP_TIME_SLOW = 750;
static constexpr int WEAPONUP_TIME_VERY_SLOW = 1000;
static constexpr int HITSCAN_RANGE = 9001;

const WeaponDef weapon_defs[] = {
	{ "", "", WeaponCategory_Count }, // Weapon_None

	{
		/* name                 */ "KNIFE",
		/* short name           */ "knife",
		/* category             */ WeaponCategory_Melee,

		/* projectile count     */ 8,
		/* clip size            */ 0,
		/* reload time          */ 0,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 200,
		/* timeout / range      */ 90,
		/* max recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* min recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* recoil recovery      */ 0.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 10,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 0,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 30,
	},

	{
		/* name                 */ "BAT",
		/* short name           */ "bat",
		/* category             */ WeaponCategory_Melee,

		/* projectile count     */ 10,
		/* clip size            */ 0,
		/* reload time          */ 1000,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 350,
		/* timeout / range      */ 70,
		/* max recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* min recoil           */ EulerDegrees2( 0.0f, 0.0f ),
		/* recoil recovery      */ 0.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 30,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 220,
		/* splash radius        */ 0,
		/* splash min damage    */ 10,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 40,
	},

	{
		/* name                 */ "9MM",
		/* short name           */ "9mm",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 15,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "PISTOL",
		/* short name           */ "pistol",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 20,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 120,
		/* timeout / range      */ 2000,
		/* max recoil           */ EulerDegrees2( 125.0f, 20.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -20.0f ),
		/* recoil recovery      */ 2000.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 15,
		/* self damage          */ 0.0f,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 30,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 4000,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "SMG",
		/* short name           */ "mg",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 25,
		/* reload time          */ 2000,
		/* staged reload time   */ 0,

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
		/* damage               */ 8,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.5f,
		/* knockback            */ 10,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "DEAGLE",
		/* short name           */ "deagle",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 7,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "SHOTGUN",
		/* short name           */ "rg",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 25,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reload time   */ 600,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 50,
	},

	{
		/* name                 */ "SAWN-OFF",
		/* short name           */ "doublebarrel",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 26,
		/* clip size            */ 2,
		/* reload time          */ 1000,
		/* staged reload time   */ 1000,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 200,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 325.0f, -50.0f ),
		/* min recoil           */ EulerDegrees2( 275.0f, -40.0f ),
		/* recoil recovery      */ 1500.0f,
		/* firing mode          */ FiringMode_SemiAuto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 2,
		/* self damage          */ 0,
		/* wallbang damage      */ 0.5f,
		/* knockback            */ 7,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 125,
	},

	{
		/* name                 */ "BURST",
		/* short name           */ "br",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 6,
		/* reload time          */ 700,
		/* staged reload time   */ 0,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "CROSSBOW",
		/* short name           */ "stake",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 1,
		/* reload time          */ 1000,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 500,
		/* timeout / range      */ 10000,
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
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "MORTAR",
		/* short name           */ "gl",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 8,
		/* reload time          */ 600,
		/* staged reload time   */ 600,

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
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
		/* has_altfire          */ true,
	},

	{
		/* name                 */ "BAZOOKA",
		/* short name           */ "rl",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 600,
		/* staged reload time   */ 600,

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
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
		/* has_altfire          */ true,
	},

	{
		/* name                 */ "ASSAULT",
		/* short name           */ "ar",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 30,
		/* reload time          */ 2000,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 40,
		/* timeout / range      */ 10000,
		/* max recoil           */ EulerDegrees2( 65.0f, 15.0f ),
		/* min recoil           */ EulerDegrees2( 45.0f, -15.0f ),
		/* recoil recovery      */ 1250.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 6,
		/* self damage          */ 1,
		/* wallbang damage      */ 1.0f, //not implemented
		/* knockback            */ 30,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 4500,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0.0f,
	},

	{
		/* name                 */ "BUBBLE",
		/* short name           */ "bg",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 15,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

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
		/* speed                */ 900,
		/* gravity scale        */ 1.0f,
		/* spread               */ 3,
	},

	{
		/* name                 */ "LASER",
		/* short name           */ "lg",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 40,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "RAIL",
		/* short name           */ "eb",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 0,
		/* reload time          */ 1000, // time to fully charge for rail
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_SLOW,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 1250,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 150.0f, 40.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -40.0f ),
		/* recoil recovery      */ 1000.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 35,
		/* self damage          */ 0,
		/* wallbang damage      */ 1.0f, //not implemented
		/* knockback            */ 50,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "SNIPER",
		/* short name           */ "sniper",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 1,
		/* reload time          */ 2000,
		/* staged reload time   */ 0,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "SCOUT",
		/* short name           */ "autosniper",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 8,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_NORMAL,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 420,
		/* timeout / range      */ HITSCAN_RANGE,
		/* max recoil           */ EulerDegrees2( 110.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 90.0f, -5.0f ),
		/* recoil recovery      */ 900.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 40.0f,
		/* zoom inaccuracy      */ 5.0f,

		// damages
		/* damage               */ 24,
		/* self damage          */ 0,
		/* wallbang damage      */ 1.0f,
		/* knockback            */ 30,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 0, // fuse time
	},

	{
		/* name                 */ "RIFLE",
		/* short name           */ "rifle",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 2000,
		/* staged reload time   */ 0,

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
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "BLASTER",
		/* short name           */ "mb",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 12,
		/* clip size            */ 6,
		/* reload time          */ 1500,
		/* staged reload time   */ 0,

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
		/* gravity scale        */ 1.0f,
		/* spread               */ 30,
	},

	{
		/* name                 */ "ROADGUN",
		/* short name           */ "road",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 20,
		/* reload time          */ 1700,
		/* staged reload time   */ 0,

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
		/* gravity scale        */ 1.0f,
		/* spread               */ 0,
	},

	{
		/* name                 */ "STICKY",
		/* short name           */ "sticky",
		/* category             */ WeaponCategory_Secondary,

		/* projectile count     */ 1,
		/* clip size            */ 15,
		/* reload time          */ 2000,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 75,
		/* timeout / range      */ 5000,
		/* max recoil           */ EulerDegrees2( 125.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -5.0f ),
		/* recoil recovery      */ 1750.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f, // actual spread

		// damages
		/* damage               */ 12,
		/* self damage          */ 1,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 20,
		/* splash radius        */ 120,
		/* splash min damage    */ 1,
		/* splash min knockback */ 25,

		// projectile def
		/* speed                */ 3000,
		/* gravity scale        */ 1.0f,
		/* spread               */ 1750, // fuse time for sticky
	},

	{
		/* name                 */ "SAWBLADE",
		/* short name           */ "sawblade",
		/* category             */ WeaponCategory_Primary,

		/* projectile count     */ 1,
		/* clip size            */ 5,
		/* reload time          */ 2000,
		/* staged reload time   */ 0,

		// timings (in msecs)
		/* weapon up time       */ WEAPONUP_TIME_FAST,
		/* weapon down time     */ WEAPONDOWN_TIME,
		/* refire time          */ 400,
		/* timeout / range      */ 4000,
		/* max recoil           */ EulerDegrees2( 125.0f, 5.0f ),
		/* min recoil           */ EulerDegrees2( 100.0f, -5.0f ),
		/* recoil recovery      */ 1750.0f,
		/* firing mode          */ FiringMode_Auto,

		/* zoom fov             */ 0.0f,
		/* zoom inaccuracy      */ 0.0f,

		// damages
		/* damage               */ 35,
		/* self damage          */ 1,
		/* wallbang damage      */ 0.0f,
		/* knockback            */ 100,
		/* splash radius        */ 0,
		/* splash min damage    */ 0,
		/* splash min knockback */ 0,

		// projectile def
		/* speed                */ 2000,
		/* spread               */ 0,
	},

#if 0
	{
		/* name                 */ "MINIGUN",
		/* short name           */ "minigun",
		/* category             */ WeaponCategory_Backup,

		/* projectile count     */ 1,
		/* clip size            */ 0,
		/* reload time          */ 0,
		/* staged reload time   */ 0,

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
		/* speed                */ HITSCAN,
		/* gravity scale        */ 1.0f,
		/* spread               */ 250,
	},
#endif
};

STATIC_ASSERT( ARRAY_COUNT( weapon_defs ) == Weapon_Count );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon ) {
	Assert( weapon < Weapon_Count );
	return &weapon_defs[ weapon ];
}

const GadgetDef gadget_defs[] = {
	{ },

	{
		/* name             */ "HATCHET",
		/* short name       */ "hatchet",
		/* uses             */ 2,
		/* drop on death    */ false,
		/* switch in time   */ WEAPONUP_TIME_FAST,
		/* using time       */ 50,
		/* cook time        */ 700,
		/* switch out time  */ 400 + WEAPONDOWN_TIME,
		/* damage           */ 60,
		/* knockback        */ 100,
		/* min damage       */ 30,
		/* min knockback    */ 0,
		/* splash radius    */ 0,
		/* timeout          */ 5000,
		/* speed            */ 2500,
		/* min speed        */ 2000,
		/* gravity scale    */ 1.0f,
	},

	{
		/* name             */ "AMERICA",
		/* short name       */ "suicidevest",
		/* uses             */ 1,
		/* drop on death    */ false,
	},

	{
		/* name             */ "FLASH",
		/* short name       */ "flash",
		/* uses             */ 2,
		/* drop on death    */ true,
		/* switch in time   */ WEAPONUP_TIME_FAST,
		/* using time       */ 50,
		/* cook time        */ 1200,
		/* switch out time  */ 400 + WEAPONDOWN_TIME,
		/* damage           */ 10,
		/* knockback        */ 80,
		/* min damage       */ 120,
		/* min knockback    */ 0,
		/* splash_radius    */ 300,
		/* timeout          */ 1500,
		/* speed            */ 1500,
		/* min speed        */ 1000,
		/* gravity scale    */ 1.0f,
	},

	{
		/* name             */ "ROCKET",
		/* short name       */ "rocket",
		/* uses             */ 2,
		/* drop on death    */ false,
		/* switch in time   */ 0,
		/* using time       */ 200,
		/* cook time        */ 0,
		/* switch out time  */ 50 + WEAPONDOWN_TIME,
		/* damage           */ 10,
		/* knockback        */ 100,
		/* min damage       */ 10,
		/* min knockback    */ 50,
		/* splash_radius    */ 120,
		/* timeout          */ 10000,
		/* speed            */ 1400,
		/* min speed        */ 1400,
		/* gravity scale    */ 0.25f,
	},

	{
		/* name             */ "SHURIKEN",
		/* short name       */ "shuriken",
		/* uses             */ 4,
		/* drop on death    */ false,
		/* switch in time   */ 0,
		/* using time       */ 150,
		/* cook time        */ 0,
		/* switch out time  */ 200 + WEAPONDOWN_TIME,
		/* damage           */ 25,
		/* knockback        */ 60,
		/* min damage       */ 0,
		/* min knockback    */ 0,
		/* splash radius    */ 0,
		/* timeout          */ 5000,
		/* speed            */ 4000,
	},
};

STATIC_ASSERT( ARRAY_COUNT( gadget_defs ) == Gadget_Count );

const GadgetDef * GetGadgetDef( GadgetType gadget ) {
	Assert( gadget < Gadget_Count );
	return &gadget_defs[ gadget ];
}

const PerkDef perk_defs[] = {
	{ },

	{
		/* enabled          */ true,
		/* name             */ "HOOLIGAN",
		/* short name       */ "hooligan",
		/* health           */ 100,
		/* scale            */ Vec3( 1 ),
		/* weight           */ 1.0f,
		/* max speed        */ 320.0f,
		/* side speed       */ 320.0f,
		/* ground accel     */ 16.0f,
		/* air accel        */ 0.5f,
		/* ground friction  */ 16.0f,
	},

	{
		/* enabled          */ true,
		/* name             */ "MIDGET",
		/* short name       */ "midget",
		/* health           */ 70,
		/* scale            */ Vec3( 0.8f, 0.8f, 0.6f ),
		/* weight           */ 0.7f,
		/* max speed        */ 400.0f,
		/* side speed       */ 500.0f,
		/* ground accel     */ 18.0f,
		/* air accel        */ 0.5f,
		/* ground friction  */ 16.0f,
	},

	{
		/* enabled          */ true,
		/* name             */ "WHEEL",
		/* short name       */ "wheel",
		/* health           */ 90,
		/* scale            */ Vec3( 0.9f, 0.9f, 0.9f ),
		/* weight           */ 0.9f,
		/* max speed        */ 1000.0f,
		/* side speed       */ 1000.0f,
		/* ground accel     */ 1.9f,
		/* air accel        */ 0.1f,
		/* ground friction  */ 2.5f,
	},

	{
		/* enabled          */ true,
		/* name             */ "JETPACK",
		/* short name       */ "jetpack",
		/* health           */ 80,
		/* scale            */ Vec3( 1 ),
		/* weight           */ 1.0f,
		/* max speed        */ 320.0f,
		/* side speed       */ 320.0f,
		/* ground accel     */ 6.0f,
		/* air accel        */ 0.5f,
		/* ground friction  */ 6.0f,
	},

	{
		/* enabled          */ false,
		/* name             */ "NINJA",
		/* short name       */ "ninja",
		/* health           */ 100,
		/* scale            */ Vec3( 1 ),
		/* weight           */ 1.0f,
		/* max speed        */ 400.0f,
		/* side speed       */ 320.0f,
		/* ground accel     */ 16.0f,
		/* air accel        */ 0.5f,
		/* ground friction  */ 16.0f,
	},

	{
		/* enabled          */ false,
		/* name             */ "BOOMER",
		/* short name       */ "boomer",
		/* health           */ 150,
		/* scale            */ Vec3( 1.5f, 1.5f, 1.0f ),
		/* weight           */ 1.5f,
		/* max speed        */ 300.0f,
		/* side speed       */ 300.0f,
		/* ground accel     */ 16.0f,
		/* air accel        */ 1.0f,
		/* ground friction  */ 16.0f,
	}
};

const PerkDef * GetPerkDef( PerkType perk ) {
	Assert( perk < Perk_Count );
	return &perk_defs[ perk ];
}
