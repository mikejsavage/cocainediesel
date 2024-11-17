#pragma once

#include "qcommon/types.h"
#include "gameshared/gs_synctypes.h"

struct gs_state_t;

struct WeaponDef {
	Span< const char > name;

	WeaponCategory category;

	int projectile_count = 1;
	int clip_size;
	u16 reload_time;
	bool staged_reload;

	u16 switch_in_time;
	u16 switch_out_time;
	u16 refire_time;
	s64 range;

	EulerDegrees2 recoil_max;
	EulerDegrees2 recoil_min;
	float recoil_recovery = 500.0f;

	FiringMode firing_mode;

	float zoom_fov;
	float zoom_spread;

	int damage;
	float self_damage_scale = 1.0f;
	float wallbang_damage_scale = 1.0f;
	int knockback;
	int splash_radius;
	int min_damage;
	int min_knockback;

	int speed;
	float gravity_scale = 1.0f;
	float restitution = 1.0f;
	float spread;
	bool has_altfire;
};

struct GadgetDef {
	Span< const char > name;
	int uses;
	bool drop_on_death;

	u16 switch_in_time;
	u16 using_time;
	u16 cook_time;
	u16 switch_out_time;
	int damage;
	int knockback;
	int min_damage;
	int min_knockback;
	int splash_radius;
	s64 timeout;
	int speed;
	int min_speed;
	float gravity_scale = 1.0f;
	float restitution = 1.0f;
};

struct PerkDef {
	bool disabled;
	Span< const char > name;
	float health;
	Vec3 scale;
	float weight;
	float max_speed;
	float side_speed;
	float ground_accel;
	float air_accel;
	float ground_friction;
};

void UpdateWeapons( const gs_state_t * gs, SyncPlayerState * ps, UserCommand cmd, int timeDelta );
void ClearInventory( SyncPlayerState * ps );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon );
const GadgetDef * GetGadgetDef( GadgetType gadget );
const PerkDef * GetPerkDef( PerkType perk );

WeaponSlot * GS_FindWeapon( SyncPlayerState * player, WeaponType weapon );
const WeaponSlot * GS_FindWeapon( const SyncPlayerState * player, WeaponType weapon );

void GS_TraceBullet( const gs_state_t * gs, trace_t * trace, trace_t * wallbang_trace, Vec3 start, Vec3 dir, Vec3 right, Vec3 up, Vec2 spread, int range, int ignore, int timeDelta );
Vec2 RandomSpreadPattern( u16 entropy, float spread );
float ZoomSpreadness( s16 zoom_time, const WeaponDef * def );
Vec2 FixedSpreadPattern( int i, float spread );
trace_t GS_TraceLaserBeam( const gs_state_t * gs, Vec3 origin, EulerDegrees3 angles, float range, int ignore, int timeDelta );

bool GS_CanEquip( const SyncPlayerState * player, WeaponType weapon );

void format( FormatBuffer * fb, const Loadout & loadout, const FormatOpts & opts );
