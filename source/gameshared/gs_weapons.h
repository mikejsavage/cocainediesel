#pragma once

#include "qcommon/types.h"
#include "gameshared/gs_synctypes.h"

struct gs_state_t;

struct WeaponDef {
	struct Properties {
		Span< const char > name;

		WeaponCategory category;

		int clip_size;
		u16 reload_time;
		bool staged_reload;

		u16 switch_in_time;
		u16 switch_out_time;

		float zoom_fov = 0.f;
		float zoom_spread = 0.f;

		float recoil_recovery = 500.0f;
	};

	struct Fire {
		int ammo_use = 1;
		int projectile_count = 1;
		
		u16 refire_time = 0;
		s64 range = 0;

		EulerDegrees2 recoil_max;
		EulerDegrees2 recoil_min;

		FiringMode firing_mode = FiringMode_Auto;

		int damage = 0;
		float self_damage_scale = 1.0f;
		float wallbang_damage_scale = 1.0f;
		int knockback = 0;
		int splash_radius = 0;
		int min_damage = 0;
		int min_knockback = 0;

		int speed = 0;
		float gravity_scale = 1.0f;
		float restitution = 1.0f;
		float spread = 0.f;
	};

	Properties properties;	
	Fire fire;
	Fire altfire;
	bool has_altfire;

	constexpr WeaponDef(const Properties& prop, const Fire& f, bool has_alt_fire):
		properties{prop}, fire{f}, altfire{f}, has_altfire{has_alt_fire}
	{ }
	
	constexpr WeaponDef(const Properties& prop, const Fire& f1, const Fire& f2):
		properties{prop}, fire{f1}, altfire{f2}, has_altfire{true}
	{ }
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
const WeaponDef::Properties * GetWeaponDefProperties( WeaponType weapon );
const WeaponDef::Fire * GetWeaponDefFire( WeaponType weapon, bool altfire );
const GadgetDef * GetGadgetDef( GadgetType gadget );
const PerkDef * GetPerkDef( PerkType perk );

WeaponSlot * GS_FindWeapon( SyncPlayerState * player, WeaponType weapon );
const WeaponSlot * GS_FindWeapon( const SyncPlayerState * player, WeaponType weapon );

void GS_TraceBullet( const gs_state_t * gs, trace_t * trace, trace_t * wallbang_trace, Vec3 start, Vec3 dir, Vec3 right, Vec3 up, Vec2 spread, int range, int ignore, int timeDelta );
Vec2 RandomSpreadPattern( u16 entropy, float spread );
float ZoomSpreadness( s16 zoom_time, WeaponType w, bool alt );
Vec2 FixedSpreadPattern( int i, float spread );
trace_t GS_TraceLaserBeam( const gs_state_t * gs, Vec3 origin, EulerDegrees3 angles, float range, int ignore, int timeDelta );

bool GS_CanEquip( const SyncPlayerState * player, WeaponType weapon );

Optional< Loadout > ParseLoadout( Span< const char > loadout_string );

void format( FormatBuffer * fb, const Loadout & loadout, const FormatOpts & opts );
