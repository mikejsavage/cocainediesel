#pragma once

#include "qcommon/types.h"

struct gs_state_t;
struct SyncPlayerState;
struct usercmd_t;

enum WeaponCategory {
	WeaponCategory_Primary,
	WeaponCategory_Secondary,
	WeaponCategory_Backup,

	WeaponCategory_Count
};

struct WeaponDef {
	const char * name;
	const char * short_name;

	WeaponCategory category;

	int projectile_count;
	int clip_size;
	u16 reload_time;
	bool staged_reloading;

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

	float damage;
	float selfdamage;
	float wallbangdamage;
	int knockback;
	int splash_radius;
	int mindamage;
	int minknockback;

	int speed;
	float spread;
};

void UpdateWeapons( const gs_state_t * gs, SyncPlayerState * ps, const usercmd_t * cmd, int timeDelta );

const WeaponDef * GS_GetWeaponDef( WeaponType weapon );

WeaponSlot * GS_FindWeapon( SyncPlayerState * player, WeaponType weapon );
const WeaponSlot * GS_FindWeapon( const SyncPlayerState * player, WeaponType weapon );

void GS_TraceBullet( const gs_state_t * gs, trace_t * trace, trace_t * wallbang_trace, Vec3 start, Vec3 dir, Vec3 right, Vec3 up, Vec2 spread, int range, int ignore, int timeDelta );
Vec2 RandomSpreadPattern( u16 entropy, float spread );
float ZoomSpreadness( s16 zoom_time, const WeaponDef * def );
Vec2 FixedSpreadPattern( int i, float spread );
void GS_TraceLaserBeam( const gs_state_t * gs, trace_t * trace, Vec3 origin, Vec3 angles, float range, int ignore, int timeDelta, void ( *impact )( const trace_t * trace, Vec3 dir, void * data ), void * data );

bool GS_CanEquip( const SyncPlayerState * player, WeaponType weapon );
