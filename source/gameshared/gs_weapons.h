#pragma once

#include "qcommon/types.h"
#include "gameshared/gs_synctypes.h"

struct gs_state_t;
struct SyncPlayerState;
struct UserCommand;

struct WeaponDef {
	const char * name;
	const char * short_name;

	WeaponCategory category;

	int projectile_count;
	int clip_size;
	u16 reload_time;
	u16 staged_reload_time;

	u16 switch_in_time;
	u16 switch_out_time;
	u16 refire_time;
	s64 range;

	EulerDegrees2 recoil_max;
	EulerDegrees2 recoil_min;
	float recoil_recover;

	FiringMode firing_mode;

	float zoom_fov;
	float zoom_spread;

	int damage;
	float selfdamage;
	float wallbangdamage;
	float knockback;
	float splash_radius;
	float min_damage;
	float min_knockback;

	int speed;
	float spread;
};

struct GadgetDef {
	const char * name;
	const char * short_name;
	int uses;

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
};

struct PerkDef {
	bool enabled;
	const char * name;
	const char * short_name;
	float health;
	Vec3 scale;
	float weight;
	float max_speed;
	float side_speed;
	float max_airspeed;
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
void GS_TraceLaserBeam( const gs_state_t * gs, trace_t * trace, Vec3 origin, Vec3 angles, float range, int ignore, int timeDelta, void ( *impact )( const trace_t * trace, Vec3 dir, void * data ), void * data );

bool GS_CanEquip( const SyncPlayerState * player, WeaponType weapon );

void format( FormatBuffer * fb, const Loadout & loadout, const FormatOpts & opts );
