#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_weapons.h"

static constexpr int WEAPONDOWN_TIME = 50;
static constexpr int WEAPONUP_TIME_FAST = 150;
static constexpr int WEAPONUP_TIME_NORMAL = 350;
static constexpr int WEAPONUP_TIME_SLOW = 750;
static constexpr int WEAPONUP_TIME_VERY_SLOW = 1000;
static constexpr int HITSCAN_RANGE = 9001;

static constexpr WeaponDef weapon_defs[] = {
	// Weapon_None
	WeaponDef {
		.name = "",
		.short_name = "",
		.category = WeaponCategory_Count,
	},

	WeaponDef {
		.name = "KNIFE",
		.short_name = "knife",
		.category = WeaponCategory_Melee,

		.projectile_count = 8,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 200,
		.range = 90,
		.firing_mode = FiringMode_Auto,

		.damage = 10,
		.spread = 30,
	},

	WeaponDef {
		.name = "BAT",
		.short_name = "bat",
		.category = WeaponCategory_Melee,

		.projectile_count = 10,
		.reload_time = 1000,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 350,
		.range = 70,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 30,
		.knockback = 220,
		.min_damage = 10,

		.spread = 40,
	},

	WeaponDef {
		.name = "9MM",
		.short_name = "9mm",
		.category = WeaponCategory_Backup,

		.clip_size = 15,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 120,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 125.0f, 20.0f ),
		.recoil_min = EulerDegrees2( 100.0f, -20.0f ),
		.recoil_recovery = 2000.0f,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 12,
		.wallbang_damage_scale = 0.5f,
		.knockback = 20,
	},

	WeaponDef {
		.name = "PISTOL",
		.short_name = "pistol",
		.category = WeaponCategory_Backup,

		.clip_size = 20,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 120,
		.range = 2000,
		.recoil_max = EulerDegrees2( 125.0f, 20.0f ),
		.recoil_min = EulerDegrees2( 100.0f, -20.0f ),
		.recoil_recovery = 2000.0f,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 15,
		.knockback = 30,

		.speed = 4000,
	},

	WeaponDef {
		.name = "SMG",
		.short_name = "mg",
		.category = WeaponCategory_Secondary,

		.clip_size = 25,
		.reload_time = 2000,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 75,
		.range = HITSCAN_RANGE,

		.recoil_max = EulerDegrees2( 80.0f, 25.0f ),
		.recoil_min = EulerDegrees2( 50.0f, -25.0f ),
		.recoil_recovery = 1500.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 8,
		.wallbang_damage_scale = 0.5f,
		.knockback = 10,
	},

	WeaponDef {
		.name = "DEAGLE",
		.short_name = "deagle",
		.category = WeaponCategory_Secondary,

		.clip_size = 7,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 500,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 325.0f, 40.0f ),
		.recoil_min = EulerDegrees2( 300.0f, -40.0f ),
		.recoil_recovery = 3250.0f,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 25,
		.wallbang_damage_scale = 0.8f,
		.knockback = 30,
	},

	WeaponDef {
		.name = "SHOTGUN",
		.short_name = "rg",
		.category = WeaponCategory_Secondary,

		.projectile_count = 25,
		.clip_size = 5,
		.reload_time = 600,
		.staged_reload = true,

		.switch_in_time = WEAPONUP_TIME_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 1250,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 325.0f, -50.0f ),
		.recoil_min = EulerDegrees2( 275.0f, -40.0f ),
		.recoil_recovery = 1500.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 2,
		.wallbang_damage_scale = 0.5f,
		.knockback = 6,

		.spread = 50,
	},

	WeaponDef {
		.name = "SAWN-OFF",
		.short_name = "doublebarrel",
		.category = WeaponCategory_Secondary,

		.projectile_count = 26,
		.clip_size = 2,
		.reload_time = 1000,
		.staged_reload = true,

		.switch_in_time = WEAPONUP_TIME_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 200,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 325.0f, -50.0f ),
		.recoil_min = EulerDegrees2( 275.0f, -40.0f ),
		.recoil_recovery = 1500.0f,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 2,
		.wallbang_damage_scale = 0.5f,
		.knockback = 7,

		.spread = 125,
	},

	WeaponDef {
		.name = "BURST",
		.short_name = "br",
		.category = WeaponCategory_Primary,

		.clip_size = 6,
		.reload_time = 700,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 35,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 80.0f, -20.0f ),
		.recoil_min = EulerDegrees2( 70.0f, -10.0f ),
		.recoil_recovery = 2500.0f,
		.firing_mode = FiringMode_Clip,

		.damage = 9,
		.wallbang_damage_scale = 0.75,
		.knockback = 10,
	},

	WeaponDef {
		.name = "CROSSBOW",
		.short_name = "stake",
		.category = WeaponCategory_Backup,

		.clip_size = 1,
		.reload_time = 1000,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 500,
		.range = 10000,
		.recoil_max = EulerDegrees2( 250.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 250.0f, -5.0f ),
		.recoil_recovery = 2000.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 50,
		.knockback = 100,
		.splash_radius = 120,
		.min_damage = 15,
		.min_knockback = 50,

		.speed = 2000,
	},

	WeaponDef {
		.name = "MORTAR",
		.short_name = "gl",
		.category = WeaponCategory_Secondary,

		.clip_size = 8,
		.reload_time = 600,
		.staged_reload = true,

		.switch_in_time = WEAPONUP_TIME_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 1000,
		.range = 2000,
		.recoil_max = EulerDegrees2( 300.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 250.0f, -5.0f ),
		.recoil_recovery = 2000.0f,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 40,
		.self_damage_scale = 1.0f,
		.knockback = 100,
		.splash_radius = 120,
		.min_damage = 10,
		.min_knockback = 50,

		.speed = 1400,
		.has_altfire = true,
	},

	WeaponDef {
		.name = "BAZOOKA",
		.short_name = "rl",
		.category = WeaponCategory_Primary,

		.clip_size = 5,
		.reload_time = 600,
		.staged_reload = true,

		.switch_in_time = WEAPONUP_TIME_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 1000,
		.range = 10000,
		.recoil_max = EulerDegrees2( 300.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 200.0f, -5.0f ),
		.recoil_recovery = 2000.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 40,
		.self_damage_scale = 1.0f,
		.knockback = 100,
		.splash_radius = 120,
		.min_damage = 10,
		.min_knockback = 50,

		.speed = 1400,
		.has_altfire = true,
	},

	WeaponDef {
		.name = "ASSAULT",
		.short_name = "ar",
		.category = WeaponCategory_Primary,

		.clip_size = 30,
		.reload_time = 2000,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 40,
		.range = 10000,
		.recoil_max = EulerDegrees2( 65.0f, 15.0f ),
		.recoil_min = EulerDegrees2( 45.0f, -15.0f ),
		.recoil_recovery = 1250.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 6,
		.self_damage_scale = 1.0f,
		.knockback = 30,

		.speed = 4500,
	},

	WeaponDef {
		.name = "BUBBLE",
		.short_name = "bg",
		.category = WeaponCategory_Backup,

		.clip_size = 15,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 175,
		.range = 10000,
		.recoil_max = EulerDegrees2( 80.0f, 25.0f ),
		.recoil_min = EulerDegrees2( 50.0f, -25.0f ),
		.recoil_recovery = 1350.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 15,
		.self_damage_scale = 1.0f,
		.knockback = 45,
		.splash_radius = 80,
		.min_damage = 14,
		.min_knockback = 25,

		.speed = 900,
		.spread = 3,
	},

	WeaponDef {
		.name = "LASER",
		.short_name = "lg",
		.category = WeaponCategory_Primary,

		.clip_size = 40,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 50,
		.range = 900,
		.firing_mode = FiringMode_Smooth,

		.damage = 5,
		.knockback = 14,
	},

	WeaponDef {
		.name = "RAIL",
		.short_name = "eb",
		.category = WeaponCategory_Primary,

		.reload_time = 1000, // time to fully charge for rail

		.switch_in_time = WEAPONUP_TIME_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 1250,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 150.0f, 40.0f ),
		.recoil_min = EulerDegrees2( 100.0f, -40.0f ),
		.recoil_recovery = 1000.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 35,
		.wallbang_damage_scale = 1.0f, //not implemented
		.knockback = 50,
	},

	WeaponDef {
		.name = "SNIPER",
		.short_name = "sniper",
		.category = WeaponCategory_Primary,

		.clip_size = 1,
		.reload_time = 2000,

		.switch_in_time = WEAPONUP_TIME_VERY_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 500,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 275.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 250.0f, -5.0f ),
		.recoil_recovery = 1750.0f,
		.firing_mode = FiringMode_Auto,

		.zoom_fov = 25.0f,
		.zoom_spread = 30.0f,

		.damage = 50,
		.wallbang_damage_scale = 1.0f,
		.knockback = 100,
	},

	WeaponDef {
		.name = "SCOUT",
		.short_name = "autosniper",
		.category = WeaponCategory_Backup,

		.clip_size = 8,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 420,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 110.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 90.0f, -5.0f ),
		.recoil_recovery = 900.0f,
		.firing_mode = FiringMode_Auto,

		.zoom_fov = 40.0f,
		.zoom_spread = 5.0f,

		.damage = 24,
		.wallbang_damage_scale = 1.0f,
		.knockback = 30,
	},

	WeaponDef {
		.name = "RIFLE",
		.short_name = "rifle",
		.category = WeaponCategory_Secondary,

		.clip_size = 5,
		.reload_time = 2000,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 600,
		.range = 10000,
		.recoil_max = EulerDegrees2( 200.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 175.0f, -5.0f ),
		.recoil_recovery = 1500.0f,
		.firing_mode = FiringMode_SemiAuto,

		.damage = 40,
		.wallbang_damage_scale = 1.0f, //not implemented
		.knockback = 50,

		.speed = 5500,
	},

	WeaponDef {
		.name = "BLASTER",
		.short_name = "mb",
		.category = WeaponCategory_Backup,

		.projectile_count = 12,
		.clip_size = 6,
		.reload_time = 1500,

		.switch_in_time = WEAPONUP_TIME_NORMAL,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 500,
		.range = 5000,
		.recoil_max = EulerDegrees2( 300.0f, 25.0f ),
		.recoil_min = EulerDegrees2( 275.0f, -25.0f ),
		.recoil_recovery = 2000.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 3,
		.knockback = 10,

		.speed = 3000,
		.spread = 30,
	},

	WeaponDef {
		.name = "ROADGUN",
		.short_name = "road",
		.category = WeaponCategory_Backup,

		.clip_size = 20,
		.reload_time = 1700,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 75,
		.range = 5000,
		.recoil_max = EulerDegrees2( 80.0f, 25.0f ),
		.recoil_min = EulerDegrees2( 70.0f, -25.0f ),
		.recoil_recovery = 1500.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 8,
		.knockback = 10,

		.speed = 3000,
	},

	WeaponDef {
		.name = "STICKY",
		.short_name = "sticky",
		.category = WeaponCategory_Secondary,

		.clip_size = 15,
		.reload_time = 2000,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 75,
		.range = 5000,
		.recoil_max = EulerDegrees2( 125.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 100.0f, -5.0f ),
		.recoil_recovery = 1750.0f,
		.firing_mode = FiringMode_Auto,

		.zoom_spread = 0.0f, // actual spread

		.damage = 12,
		.self_damage_scale = 1.0f,
		.knockback = 20,
		.splash_radius = 120,
		.min_damage = 1,
		.min_knockback = 25,

		.speed = 3000,
		.spread = 1750, // fuse time for sticky
	},

	WeaponDef {
		.name = "SAWBLADE",
		.short_name = "sawblade",
		.category = WeaponCategory_Primary,

		.clip_size = 5,
		.reload_time = 2000,

		.switch_in_time = WEAPONUP_TIME_FAST,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 400,
		.range = 4000,
		.recoil_max = EulerDegrees2( 125.0f, 5.0f ),
		.recoil_min = EulerDegrees2( 100.0f, -5.0f ),
		.recoil_recovery = 1750.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 35,
		.knockback = 100,

		.speed = 2000,
	},

#if 0
	WeaponDef {
		.name = "MINIGUN",
		.short_name = "minigun",
		.category = WeaponCategory_Backup,

		.switch_in_time = WEAPONUP_TIME_VERY_SLOW,
		.switch_out_time = WEAPONDOWN_TIME,
		.refire_time = 75,
		.range = HITSCAN_RANGE,
		.recoil_max = EulerDegrees2( 0.0f, 0.0f ),
		.recoil_min = EulerDegrees2( 0.0f, 0.0f ),
		.recoil_recovery = 1000.0f,
		.firing_mode = FiringMode_Auto,

		.damage = 9,
		.knockback = 30,

		.spread = 250,
	},
#endif
};

STATIC_ASSERT( ARRAY_COUNT( weapon_defs ) == Weapon_Count );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon ) {
	Assert( weapon < Weapon_Count );
	return &weapon_defs[ weapon ];
}

const GadgetDef gadget_defs[] = {
	GadgetDef { },

	GadgetDef {
		.name = "HATCHET",
		.short_name = "hatchet",
		.uses = 2,
		.switch_in_time = WEAPONUP_TIME_FAST,
		.using_time = 50,
		.cook_time = 700,
		.switch_out_time = 400 + WEAPONDOWN_TIME,
		.damage = 60,
		.knockback = 100,
		.min_damage = 30,
		.timeout = 5000,
		.speed = 2500,
		.min_speed = 2000,
	},

	GadgetDef {
		.name = "AMERICA",
		.short_name = "suicidevest",
		.uses = 1,
	},

	GadgetDef {
		.name = "FLASH",
		.short_name = "flash",
		.uses = 2,
		.drop_on_death = true,
		.switch_in_time = WEAPONUP_TIME_FAST,
		.using_time = 50,
		.cook_time = 1200,
		.switch_out_time = 400 + WEAPONDOWN_TIME,
		.damage = 10,
		.knockback = 80,
		.min_damage = 120,
		.splash_radius = 300,
		.timeout = 1500,
		.speed = 1500,
		.min_speed = 1000,
	},

	GadgetDef {
		.name = "ROCKET",
		.short_name = "rocket",
		.uses = 2,
		.switch_in_time = 0,
		.using_time = 200,
		.switch_out_time = 50 + WEAPONDOWN_TIME,
		.damage = 10,
		.knockback = 100,
		.min_damage = 10,
		.min_knockback = 50,
		.splash_radius = 120,
		.timeout = 10000,
		.speed = 1400,
		.min_speed = 1400,
		.gravity_scale = 0.25f,
	},

	GadgetDef {
		.name = "SHURIKEN",
		.short_name = "shuriken",
		.uses = 4,
		.switch_in_time = 0,
		.using_time = 150,
		.switch_out_time = 200 + WEAPONDOWN_TIME,
		.damage = 25,
		.knockback = 60,
		.timeout = 5000,
		.speed = 4000,
	},
};

STATIC_ASSERT( ARRAY_COUNT( gadget_defs ) == Gadget_Count );

const GadgetDef * GetGadgetDef( GadgetType gadget ) {
	Assert( gadget < Gadget_Count );
	return &gadget_defs[ gadget ];
}

const PerkDef perk_defs[] = {
	PerkDef { },

	PerkDef {
		.name = "HOOLIGAN",
		.short_name = "hooligan",
		.health = 100,
		.scale = Vec3( 1 ),
		.weight = 1.0f,
		.max_speed = 400.0f,
		.side_speed = 400.0f,
		.ground_accel = 24.0f,
		.air_accel = 0.5f,
		.ground_friction = 16.0f,
	},

	PerkDef {
		.name = "MIDGET",
		.short_name = "midget",
		.health = 70,
		.scale = Vec3( 0.8f, 0.8f, 0.6f ),
		.weight = 0.7f,
		.max_speed = 400.0f,
		.side_speed = 500.0f,
		.ground_accel = 18.0f,
		.air_accel = 0.5f,
		.ground_friction = 16.0f,
	},

	PerkDef {
		.name = "WHEEL",
		.short_name = "wheel",
		.health = 90,
		.scale = Vec3( 0.9f, 0.9f, 0.9f ),
		.weight = 0.9f,
		.max_speed = 1000.0f,
		.side_speed = 1000.0f,
		.ground_accel = 1.9f,
		.air_accel = 0.1f,
		.ground_friction = 2.5f,
	},

	PerkDef {
		.name = "JETPACK",
		.short_name = "jetpack",
		.health = 80,
		.scale = Vec3( 1 ),
		.weight = 1.0f,
		.max_speed = 300.0f,
		.side_speed = 300.0f,
		.ground_accel = 6.0f,
		.air_accel = 0.5f,
		.ground_friction = 4.5f,
	},

	PerkDef {
		.disabled = true,
		.name = "NINJA",
		.short_name = "ninja",
		.health = 100,
		.scale = Vec3( 1 ),
		.weight = 1.0f,
		.max_speed = 400.0f,
		.side_speed = 320.0f,
		.ground_accel = 16.0f,
		.air_accel = 0.5f,
		.ground_friction = 16.0f,
	},

	PerkDef {
		.disabled = true,
		.name = "BOOMER",
		.short_name = "boomer",
		.health = 150,
		.scale = Vec3( 1.5f, 1.5f, 1.0f ),
		.weight = 1.5f,
		.max_speed = 300.0f,
		.side_speed = 300.0f,
		.ground_accel = 16.0f,
		.air_accel = 1.0f,
		.ground_friction = 16.0f,
	}
};

const PerkDef * GetPerkDef( PerkType perk ) {
	Assert( perk < Perk_Count );
	return &perk_defs[ perk ];
}
