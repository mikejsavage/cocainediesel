#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/gs_weapons.h"

static constexpr int WEAPONDOWN_TIME = 50;
static constexpr int WEAPONUP_TIME_FAST = 150;
static constexpr int WEAPONUP_TIME_NORMAL = 350;
static constexpr int WEAPONUP_TIME_SLOW = 750;
static constexpr int WEAPONUP_TIME_VERY_SLOW = 1000;
static constexpr int HITSCAN_RANGE = 9001;

static constexpr bool WEAPON_NO_ALTFIRE = false;
static constexpr bool WEAPON_HAS_ALTFIRE = true;


static constexpr WeaponDef weapon_defs[] = {
	// Weapon_None
	WeaponDef (
		{ 
			.name = "",
			.category = WeaponCategory_Count,
		},
		{},
		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "knife",
			.category = WeaponCategory_Melee,


			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
		},

		{
			.projectile_count = 8,

			.refire_time = 200,
			.range = 90,
			.firing_mode = FiringMode_Auto,

			.damage = 10,
			.spread = 30,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "bat",
			.category = WeaponCategory_Melee,

			.reload_time = 1000,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
		},

		{
			.projectile_count = 10,

			.refire_time = 350,
			.range = 70,
			.firing_mode = FiringMode_SemiAuto,

			.damage = 30,
			.knockback = 220,
			.min_damage = 10,

			.spread = 40,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "9mm",
			.category = WeaponCategory_Backup,

			.clip_size = 15,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2000.0f,
		},

		{
			.refire_time = 120,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 125.0f, 20.0f ),
			.recoil_min = EulerDegrees2( 100.0f, -20.0f ),
			.firing_mode = FiringMode_SemiAuto,

			.damage = 12,
			.wallbang_damage_scale = 0.5f,
			.knockback = 20,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{ 
			.name = "pistol",
			.category = WeaponCategory_Backup,

			.clip_size = 20,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2000.0f,
		},

		{
			.refire_time = 120,
			.range = 2000,
			.recoil_max = EulerDegrees2( 125.0f, 20.0f ),
			.recoil_min = EulerDegrees2( 100.0f, -20.0f ),
			.firing_mode = FiringMode_SemiAuto,

			.damage = 15,
			.knockback = 30,

			.speed = 4000,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "smg",
			.category = WeaponCategory_Secondary,

			.clip_size = 25,
			.reload_time = 2000,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1500.0f,
		},

		{
			.refire_time = 75,
			.range = HITSCAN_RANGE,

			.recoil_max = EulerDegrees2( 80.0f, 25.0f ),
			.recoil_min = EulerDegrees2( 50.0f, -25.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 8,
			.wallbang_damage_scale = 0.5f,
			.knockback = 10,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "deagle",
			.category = WeaponCategory_Secondary,

			.clip_size = 7,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 3250.0f,
		},

		{
			.refire_time = 500,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 325.0f, 40.0f ),
			.recoil_min = EulerDegrees2( 300.0f, -40.0f ),
			.firing_mode = FiringMode_SemiAuto,

			.damage = 25,
			.wallbang_damage_scale = 0.8f,
			.knockback = 30,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "shotgun",
			.category = WeaponCategory_Secondary,

			.clip_size = 5,
			.reload_time = 600,
			.staged_reload = true,

			.switch_in_time = WEAPONUP_TIME_SLOW,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1500.0f,
		},

		{
			.projectile_count = 25,

			.refire_time = 1250,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 325.0f, -50.0f ),
			.recoil_min = EulerDegrees2( 275.0f, -40.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 2,
			.wallbang_damage_scale = 0.5f,
			.knockback = 6,

			.spread = 50,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "sawn-off",
			.category = WeaponCategory_Secondary,

			.clip_size = 2,
			.reload_time = 1000,
			.staged_reload = true,

			.switch_in_time = WEAPONUP_TIME_SLOW,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1500.0f,
		},

		{
			.projectile_count = 26,

			.refire_time = 200,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 325.0f, -50.0f ),
			.recoil_min = EulerDegrees2( 275.0f, -40.0f ),
			.firing_mode = FiringMode_SemiAuto,

			.damage = 2,
			.wallbang_damage_scale = 0.5f,
			.knockback = 7,

			.spread = 125,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "burst",
			.category = WeaponCategory_Primary,

			.clip_size = 6,
			.reload_time = 700,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2500.0f,
		},

		{
			.refire_time = 35,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 80.0f, -20.0f ),
			.recoil_min = EulerDegrees2( 70.0f, -10.0f ),
			.firing_mode = FiringMode_Clip,

			.damage = 9,
			.wallbang_damage_scale = 0.75,
			.knockback = 10,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "crossbow",
			.category = WeaponCategory_Backup,

			.clip_size = 1,
			.reload_time = 1000,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2000.0f,
		},

		{
			.refire_time = 500,
			.range = 10000,
			.recoil_max = EulerDegrees2( 250.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 250.0f, -5.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 50,
			.knockback = 100,
			.splash_radius = 120,
			.min_damage = 15,
			.min_knockback = 50,

			.speed = 2000,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "launcher",
			.category = WeaponCategory_Secondary,

			.clip_size = 8,
			.reload_time = 600,
			.staged_reload = true,

			.switch_in_time = WEAPONUP_TIME_SLOW,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2000.0f,
		},

		{
			.refire_time = 1000,
			.range = 2000,
			.recoil_max = EulerDegrees2( 300.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 250.0f, -5.0f ),
			.firing_mode = FiringMode_SemiAuto,

			.damage = 40,
			.self_damage_scale = 1.0f,
			.knockback = 100,
			.splash_radius = 120,
			.min_damage = 10,
			.min_knockback = 50,

			.speed = 1400,
			.restitution = 0.5f,
		},
		
		WEAPON_HAS_ALTFIRE
	),

	WeaponDef (
		{
			.name = "bazooka",
			.category = WeaponCategory_Primary,

			.clip_size = 5,
			.reload_time = 600,
			.staged_reload = true,

			.switch_in_time = WEAPONUP_TIME_SLOW,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2000.0f,
		},

		{
			.refire_time = 1000,
			.range = 10000,
			.recoil_max = EulerDegrees2( 300.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 200.0f, -5.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 40,
			.self_damage_scale = 1.0f,
			.knockback = 100,
			.splash_radius = 120,
			.min_damage = 10,
			.min_knockback = 50,

			.speed = 1400,
		},

		WEAPON_HAS_ALTFIRE
	),

	WeaponDef (
		{
			.name = "assault",
			.category = WeaponCategory_Primary,

			.clip_size = 30,
			.reload_time = 2000,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1250.0f,
		},

		{
			.refire_time = 40,
			.range = 10000,
			.recoil_max = EulerDegrees2( 65.0f, 15.0f ),
			.recoil_min = EulerDegrees2( 45.0f, -15.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 6,
			.self_damage_scale = 1.0f,
			.knockback = 30,

			.speed = 4500,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "bubble",
			.category = WeaponCategory_Backup,

			.clip_size = 15,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1350.0f,
		},

		{
			.refire_time = 175,
			.range = 10000,
			.recoil_max = EulerDegrees2( 80.0f, 25.0f ),
			.recoil_min = EulerDegrees2( 50.0f, -25.0f ),
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

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "laser",
			.category = WeaponCategory_Primary,

			.clip_size = 40,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
		},

		{
			.refire_time = 50,
			.range = 900,
			.firing_mode = FiringMode_Smooth,

			.damage = 5,
			.knockback = 14,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "rail",
			.category = WeaponCategory_Primary,

			.reload_time = 1000, // time to fully charge for rail

			.switch_in_time = WEAPONUP_TIME_SLOW,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1000.0f,
		},

		{
			.refire_time = 1250,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 150.0f, 40.0f ),
			.recoil_min = EulerDegrees2( 100.0f, -40.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 35,
			.wallbang_damage_scale = 1.0f, //not implemented
			.knockback = 50,
		},

		WEAPON_HAS_ALTFIRE
	),

	WeaponDef (
		{
			.name = "sniper",
			.category = WeaponCategory_Primary,

			.clip_size = 1,
			.reload_time = 2000,

			.switch_in_time = WEAPONUP_TIME_VERY_SLOW,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.zoom_fov = 25.0f,
			.zoom_spread = 30.0f,
			
			.recoil_recovery = 1750.0f,
		},

		{
			.refire_time = 500,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 275.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 250.0f, -5.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 50,
			.wallbang_damage_scale = 1.0f,
			.knockback = 100,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "scout",
			.category = WeaponCategory_Backup,

			.clip_size = 8,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
		
			.zoom_fov = 40.0f,
			.zoom_spread = 5.0f,
			
			.recoil_recovery = 900.0f,
		},

		{
			.refire_time = 420,
			.range = HITSCAN_RANGE,
			.recoil_max = EulerDegrees2( 110.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 90.0f, -5.0f ),
			.firing_mode = FiringMode_Auto,


			.damage = 24,
			.wallbang_damage_scale = 1.0f,
			.knockback = 30,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "rifle",
			.category = WeaponCategory_Secondary,

			.clip_size = 5,
			.reload_time = 2000,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1500.0f,
		},

		{
			.refire_time = 600,
			.range = 10000,
			.recoil_max = EulerDegrees2( 200.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 175.0f, -5.0f ),
			.firing_mode = FiringMode_SemiAuto,

			.damage = 40,
			.wallbang_damage_scale = 1.0f, //not implemented
			.knockback = 50,

			.speed = 5500,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "blaster",
			.category = WeaponCategory_Backup,

			.clip_size = 6,
			.reload_time = 1500,

			.switch_in_time = WEAPONUP_TIME_NORMAL,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 2000.0f,
		},

		{
			.projectile_count = 12,

			.refire_time = 500,
			.range = 5000,
			.recoil_max = EulerDegrees2( 300.0f, 25.0f ),
			.recoil_min = EulerDegrees2( 275.0f, -25.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 3,
			.knockback = 10,

			.speed = 3000,
			.restitution = 0.5f,
			.spread = 30,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "roadgun",
			.category = WeaponCategory_Backup,

			.clip_size = 20,
			.reload_time = 1700,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1500.0f,
		},

		{
			.refire_time = 75,
			.range = 5000,
			.recoil_max = EulerDegrees2( 80.0f, 25.0f ),
			.recoil_min = EulerDegrees2( 70.0f, -25.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 8,
			.knockback = 10,

			.speed = 3000,
			.restitution = 0.5f,
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "sticky",
			.category = WeaponCategory_Secondary,

			.clip_size = 15,
			.reload_time = 2000,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.zoom_spread = 0.0f, // actual spread
			
			.recoil_recovery = 1750.0f,
		},

		{
			.refire_time = 75,
			.range = 5000,
			.recoil_max = EulerDegrees2( 125.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 100.0f, -5.0f ),
			.firing_mode = FiringMode_Auto,


			.damage = 12,
			.self_damage_scale = 1.0f,
			.knockback = 20,
			.splash_radius = 120,
			.min_damage = 1,
			.min_knockback = 25,

			.speed = 3000,
			.spread = 1750, // fuse time for sticky
		},

		WEAPON_NO_ALTFIRE
	),

	WeaponDef (
		{
			.name = "sawblade",
			.category = WeaponCategory_Primary,

			.clip_size = 5,
			.reload_time = 2000,

			.switch_in_time = WEAPONUP_TIME_FAST,
			.switch_out_time = WEAPONDOWN_TIME,
			
			.recoil_recovery = 1750.0f,
		},

		{
			.refire_time = 400,
			.range = 4000,
			.recoil_max = EulerDegrees2( 125.0f, 5.0f ),
			.recoil_min = EulerDegrees2( 100.0f, -5.0f ),
			.firing_mode = FiringMode_Auto,

			.damage = 35,
			.knockback = 100,

			.speed = 2000,
			.gravity_scale = 0.0f,
		},

		WEAPON_NO_ALTFIRE
	),

#if 0
	WeaponDef (
		.name = "minigun",
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

const WeaponDef::Properties * GetWeaponDefProperties( WeaponType weapon ) {
	Assert( weapon < Weapon_Count );
	return &weapon_defs[ weapon ].properties;
}

const WeaponDef::Fire * GetWeaponDefFire( WeaponType weapon, bool altfire ) {
	Assert( weapon < Weapon_Count );
	Assert( weapon_defs[ weapon ].has_altfire || !altfire ); // check the alt fire exists if you're looking for it
	return altfire ? &weapon_defs[ weapon ].altfire : &weapon_defs[ weapon ].fire;
}

const GadgetDef gadget_defs[] = {
	GadgetDef { },

	GadgetDef {
		.name = "axe",
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
		.name = "martyr",
		.uses = 1,
	},

	GadgetDef {
		.name = "flash",
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
		.restitution = 0.5f,
	},

	GadgetDef {
		.name = "rocket",
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
		.name = "shuriken",
		.uses = 4,
		.switch_in_time = 0,
		.using_time = 150,
		.switch_out_time = 200 + WEAPONDOWN_TIME,
		.damage = 25,
		.knockback = 60,
		.timeout = 5000,
		.speed = 4000,
		.gravity_scale = 0.0f,
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
		.name = "hooligan",
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
		.name = "midget",
		.health = 70,
		.scale = Vec3( 0.8f, 0.8f, 0.75f ),
		.weight = 0.6f,
		.max_speed = 420.0f,
		.side_speed = 420.0f,
		.ground_accel = 18.0f,
		.air_accel = 0.5f,
		.ground_friction = 16.0f,
	},

	PerkDef {
		.name = "wheel",
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
		.name = "jetpack",
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
		.name = "ninja",
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
		.name = "boomer",
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
