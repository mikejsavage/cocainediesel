/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static void CG_ViewWeapon_UpdateProjectionSource( Vec3 hand_origin, const mat3_t hand_axis, Vec3 weap_origin, const mat3_t weap_axis ) {
	orientation_t *tag_result = &cg.weapon.projectionSource;
	orientation_t tag_weapon;

	tag_weapon.origin = Vec3( 0.0f );
	Matrix3_Copy( axis_identity, tag_weapon.axis );

	// move to tag_weapon
	CG_MoveToTag( &tag_weapon.origin, tag_weapon.axis,
				  hand_origin, hand_axis,
				  weap_origin, weap_axis );

	const WeaponModelMetadata * weaponInfo = GetWeaponModelMetadata( cg.predictedPlayerState.weapon );

	// move to projectionSource tag
	if( weaponInfo ) {
		tag_result->origin = Vec3( 0.0f );
		Matrix3_Copy( axis_identity, tag_result->axis );
		CG_MoveToTag( &tag_result->origin, tag_result->axis,
					  tag_weapon.origin, tag_weapon.axis,
					  weaponInfo->tag_projectionsource.origin, weaponInfo->tag_projectionsource.axis );
		return;
	}

	// fall back: copy gun origin and move it front by 16 units and 8 up
	tag_result->origin = tag_weapon.origin;
	Matrix3_Copy( tag_weapon.axis, tag_result->axis );
	tag_result->origin += FromQFAxis( tag_result->axis, AXIS_FORWARD ) * 16.0f;
	tag_result->origin += FromQFAxis( tag_result->axis, AXIS_UP ) * 8.0f;
}

static float SmoothStep( float t ) {
	return t * t * ( 3.0f - 2.0f * t );
}

static void CG_ViewWeapon_AddAngleEffects( Vec3 * angles, cg_viewweapon_t * viewweapon ) {
	const SyncPlayerState * ps = &cg.predictedPlayerState;
	if( ps->weapon == Weapon_None )
		return;

	const WeaponDef * def = GS_GetWeaponDef( ps->weapon );

	if( ps->weapon_state == WeaponState_Firing || ps->weapon_state == WeaponState_FiringSemiAuto ) {
		float frac = float( ps->weapon_state_time ) / float( def->refire_time );
		if( ps->weapon == Weapon_Knife ) {
			viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * 30.0f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			angles->z -= def->refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			angles->y += def->refire_time * 0.025f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			angles->x -= def->refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
		}
		else {
			angles->x -= def->refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
		}
	}
	else if( ps->weapon_state == WeaponState_SwitchingIn || ps->weapon_state == WeaponState_SwitchingIn || ps->weapon_state == WeaponState_SwitchingOut ) {
		float frac;
		if( ps->weapon_state == WeaponState_SwitchingIn ) {
			frac = 1.0f - float( ps->weapon_state_time ) / float( def->switch_in_time );
		}
		else {
			frac = float( ps->weapon_state_time ) / float( def->switch_out_time );
		}
		frac *= frac; //smoother curve
		angles->x += Lerp( 0.0f, frac, 60.0f );
	}
	else if( ps->weapon_state == WeaponState_Reloading ) {
		float frac = float( ps->weapon_state_time ) / float( def->reload_time );
		angles->z += Lerp( 0.0f, SmoothStep( frac ), 360.0f );
	}
	else if( ps->weapon == Weapon_Railgun && ps->weapon_state == WeaponState_Cooking ) {
		float charge = float( ps->weapon_state_time ) / float( def->reload_time );
		float pull_back = ( 1.0f - Square( 1.0f - charge ) ) * 4.0f;
		viewweapon->origin -= FromQFAxis( cg.view.axis, AXIS_FORWARD ) * pull_back;
	}

	// gun angles from bobbing
	if( cg.bobCycle & 1 ) {
		angles->z -= cg.xyspeed * cg.bobFracSin * 0.012f;
		angles->y -= cg.xyspeed * cg.bobFracSin * 0.006f;
	} else {
		angles->z += cg.xyspeed * cg.bobFracSin * 0.012f;
		angles->y += cg.xyspeed * cg.bobFracSin * 0.006f;
	}
	angles->x += cg.xyspeed * cg.bobFracSin * 0.012f;

	// gun angles from kicks
	*angles += CG_GetKickAngles();
}

void CG_ViewWeapon_StartAnimationEvent( int newAnim ) {
	if( !cg.view.drawWeapon ) {
		return;
	}

	cg.weapon.eventAnim = newAnim;
	cg.weapon.eventAnimStartTime = cl.serverTime;
}

void CG_CalcViewWeapon( cg_viewweapon_t *viewweapon ) {
	const WeaponModelMetadata * weaponInfo = GetWeaponModelMetadata( cg.predictedPlayerState.weapon );

	// calculate the entity position
	viewweapon->origin = cg.view.origin;

	// weapon config offsets
	Vec3 gunAngles = weaponInfo->handpositionAngles + cg.predictedPlayerState.viewangles;

	constexpr Vec3 old_gunpos_cvars = Vec3( 3, 10, -12 ); // depth, horizontal, vertical
	Vec3 gunOffset = weaponInfo->handpositionOrigin + old_gunpos_cvars;

	// scale forward gun offset depending on fov and aspect ratio
	gunOffset.x *= frame_static.viewport_width / ( frame_static.viewport_height * cg.view.fracDistFOV ) ;
	gunOffset.z += CG_ViewSmoothFallKick();

	// apply the offsets
	viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * gunOffset.x;
	viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_RIGHT ) * gunOffset.y;
	viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_UP ) * gunOffset.z;

	// add angles effects
	CG_ViewWeapon_AddAngleEffects( &gunAngles, viewweapon );

	// finish
	AnglesToAxis( gunAngles, viewweapon->axis );

	if( cg.predictedPlayerState.zoom_time == 0 ) {
		constexpr float gun_fov = 90.0f;
		float gun_fov_y = WidescreenFov( gun_fov );
		float gun_fov_x = CalcHorizontalFov( gun_fov_y, frame_static.viewport_width, frame_static.viewport_height );

		float fracWeapFOV = tanf( Radians( gun_fov_x ) * 0.5f ) / cg.view.fracDistFOV;

		viewweapon->axis[AXIS_FORWARD] *= fracWeapFOV;
		viewweapon->axis[AXIS_FORWARD + 1] *= fracWeapFOV;
		viewweapon->axis[AXIS_FORWARD + 2] *= fracWeapFOV;
	}

	CG_ViewWeapon_UpdateProjectionSource( viewweapon->origin, viewweapon->axis, Vec3( 0.0f ), axis_identity );
}

void CG_AddViewWeapon( cg_viewweapon_t *viewweapon ) {
	if( !cg.view.drawWeapon || cg.predictedPlayerState.weapon == Weapon_None )
		return;

	const Model * model = GetWeaponModelMetadata( cg.predictedPlayerState.weapon )->model;
	Mat4 transform = FromAxisAndOrigin( viewweapon->axis, viewweapon->origin );
	DrawViewWeapon( model, transform );
}

void CG_AddRecoil( WeaponType weapon ) {
	if( !cg.recoiling ) {
		cg.recoil_initial_angles = EulerDegrees2( cl.viewangles[ PITCH ], cl.viewangles[ YAW ] );
		cg.recoiling = true;
	}

	EulerDegrees2 min = GS_GetWeaponDef( weapon )->recoil_min;
	EulerDegrees2 max = GS_GetWeaponDef( weapon )->recoil_max;

	cg.recoil_velocity.pitch = -RandomUniformFloat( &cls.rng, min.pitch, max.pitch );
	cg.recoil_velocity.yaw = RandomUniformFloat( &cls.rng, min.yaw, max.yaw );
}

static bool SameSign( float x, float y ) {
	return x * y >= 0.0f;
}

void CG_Recoil( WeaponType weapon ) {
	if( !cg.recoiling )
		return;

	float dt = cls.frametime * 0.001f;
	EulerDegrees2 viewangles = EulerDegrees2( cl.viewangles[ PITCH ], cl.viewangles[ YAW ] );
	EulerDegrees2 recovery_delta = AngleDelta( cg.recoil_initial_angles, viewangles );

	cg.recoil_initial_angles.pitch += Min2( 0.0f, cl.viewangles[ PITCH ] - cl.prevviewangles[ PITCH ] );
	cg.recoil_initial_angles.yaw += AngleDelta( cl.viewangles[ YAW ], cl.prevviewangles[ YAW ] );

	recovery_delta = AngleDelta( cg.recoil_initial_angles, viewangles );

	constexpr float recenter_speed_scale = 1.0f / 16.0f;
	float recenter_accel = GS_GetWeaponDef( weapon )->recoil_recover;

	// pitch
	{
		bool recovering = cg.recoil_velocity.pitch >= 0.0f;
		float accel = recenter_accel * ( recovering ? recenter_speed_scale : 1.0f );
		cg.recoil_velocity.pitch += accel * dt;

		if( recovering && recovery_delta.pitch <= cg.recoil_velocity.pitch * dt ) {
			cg.recoiling = false;
			cl.viewangles[ PITCH ] = Max2( viewangles.pitch, cg.recoil_initial_angles.pitch );
		}
		else {
			cl.viewangles[ PITCH ] += cg.recoil_velocity.pitch * dt;
		}
	}

	// yaw
	{
		bool recovering = SameSign( cg.recoil_velocity.yaw, recovery_delta.yaw );
		float accel = recenter_accel * 0.2f * SignedOne( recovery_delta.yaw ) * ( recovering ? recenter_speed_scale : 1.0f );
		cg.recoil_velocity.yaw += accel * dt;

		cl.viewangles[ YAW ] += cg.recoil_velocity.yaw * dt;
	}
}
