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
	const WeaponDef * def = GS_GetWeaponDef( cg.predictedPlayerState.weapon );

	if( cg.predictedPlayerState.weapon_state == WeaponState_Firing || cg.predictedPlayerState.weapon_state == WeaponState_FiringSemiAuto ) {
		float frac = 1.0f - float( cg.predictedPlayerState.weapon_time ) / float( def->refire_time );
		if( cg.predictedPlayerState.weapon == Weapon_Knife ) {
			viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * 30.0f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			angles->z -= def->refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			angles->y += def->refire_time * 0.025f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			angles->x -= def->refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
		}
		else {
			angles->x -= def->refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
		}
	}
	else if( cg.predictedPlayerState.weapon_state == WeaponState_SwitchingIn || cg.predictedPlayerState.weapon_state == WeaponState_SwitchingOut ) {
		float frac;
		if( cg.predictedPlayerState.weapon_state == WeaponState_SwitchingIn ) {
			frac = float( cg.predictedPlayerState.weapon_time ) / float( def->weaponup_time );
		}
		else {
			frac = 1.0f - float( cg.predictedPlayerState.weapon_time ) / float( def->weapondown_time );
		}
		frac *= frac; //smoother curve
		angles->x += Lerp( 0.0f, frac, 60.0f );
	}
	else if( cg.predictedPlayerState.weapon_state == WeaponState_Reloading ) {
		float frac = 1.0f - float( cg.predictedPlayerState.weapon_time ) / float( def->reload_time );
		angles->z += Lerp( 0.0f, SmoothStep( frac ), 360.0f );
	}

	if( cg_gunbob->integer ) {
		// gun angles from bobbing
		if( cg.bobCycle & 1 ) {
			angles->z -= cg.xyspeed * cg.bobFracSin * 0.012f;
			angles->y -= cg.xyspeed * cg.bobFracSin * 0.006f;
		} else {
			angles->z += cg.xyspeed * cg.bobFracSin * 0.012f;
			angles->y += cg.xyspeed * cg.bobFracSin * 0.006f;
		}
		angles->x += cg.xyspeed * cg.bobFracSin * 0.012f;

		// gun angles from delta movement
		for( int i = 0; i < 3; i++ ) {
			float delta = AngleNormalize180( ( cg.oldFrame.playerState.viewangles[i] - cg.frame.playerState.viewangles[i] ) * cg.lerpfrac );
			delta = Clamp( -45.0f, delta, 45.0f );

			if( i == YAW ) {
				angles->z += 0.001f * delta;
			}
			angles->ptr()[i] += 0.002f * delta;
		}

		// gun angles from kicks
		*angles += CG_GetKickAngles();
	}
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
	Vec3 gunOffset = weaponInfo->handpositionOrigin + Vec3( cg_gunz->value, cg_gunx->value, cg_guny->value );

	// scale forward gun offset depending on fov and aspect ratio
	gunOffset.x *= frame_static.viewport_width / ( frame_static.viewport_height * cg.view.fracDistFOV ) ;

	// hand cvar offset
	float handOffset = 0.0f;
	if( cgs.demoPlaying ) {
		if( cg_hand->integer == 0 ) {
			handOffset = cg_handOffset->value;
		} else if( cg_hand->integer == 1 ) {
			handOffset = -cg_handOffset->value;
		}
	} else {
		if( cgs.clientInfo[cg.view.POVent - 1].hand == 0 ) {
			handOffset = cg_handOffset->value;
		} else if( cgs.clientInfo[cg.view.POVent - 1].hand == 1 ) {
			handOffset = -cg_handOffset->value;
		}
	}

	gunOffset.y += handOffset;
	if( cg_gunbob->integer ) {
		gunOffset.z += CG_ViewSmoothFallKick();
	}

	// apply the offsets
	viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * gunOffset.x;
	viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_RIGHT ) * gunOffset.y;
	viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_UP ) * gunOffset.z;

	// add angles effects
	CG_ViewWeapon_AddAngleEffects( &gunAngles, viewweapon );

	// finish
	AnglesToAxis( gunAngles, viewweapon->axis );

	if( cg_gun_fov->integer && cg.predictedPlayerState.zoom_time == 0 ) {
		float gun_fov_y = WidescreenFov( Clamp( 20.0f, cg_gun_fov->value, 160.0f ) );
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
		cg.recoil_initial = cl.viewangles;
		cg.recoiling = true;
	}
	constexpr float recenter_mult = 1.0f / 16.0f;
	Vec2 recoil = GS_GetWeaponDef( weapon )->recoil;
	Vec2 viewKickMin = Vec2( -recoil.x, -recoil.y );
	Vec2 viewKickMax = Vec2( 0.0f, recoil.y );
	Vec2 viewKickMinMag = GS_GetWeaponDef( weapon )->recoil_min;

	Vec3 kick = Vec3( 0.0f );
	Vec3 dir_to_center = AngleDelta( cg.recoil_initial, cl.viewangles );
	for( size_t i = 0; i < 2; i++ ) {
		float amount = random_uniform_float( &cls.rng, viewKickMin[ i ] + viewKickMinMag[ i ], viewKickMax[ i ] - viewKickMinMag[ i ] );
		if( amount < 0.0f ) {
			amount -= viewKickMinMag[ i ];
		}
		else {
			amount += viewKickMinMag[ i ];
		}

		bool recentering = ( dir_to_center[ i ] * amount ) > 0.0f;
		float mult = recentering ? recenter_mult : 1.0f;

		kick[ i ] = amount * mult;
	}
	cg.recoil = kick;
}

void CG_Recoil( WeaponType weapon ) {
	if( cg.recoiling ) {
		constexpr float recenter_mult = 1.0f / 16.0f;
		constexpr float stop_epsilon = 0.01f;
		float viewKickCenterSpeed = GS_GetWeaponDef( weapon )->recoil_recover;

		float dt = cls.frametime * 0.001f;
		Vec3 dir_to_center = AngleDelta( cg.recoil_initial, cl.viewangles );
		for( size_t i = 0; i < 2; i++ ) {
			bool recentering = ( dir_to_center[ i ] * cg.recoil[ i ] ) > 0.0f;
			float direction = ( dir_to_center[ i ] > 0.0f ) - ( dir_to_center[ i ] < 0.0f );
			float mult = recentering ? recenter_mult : 1.0f;
			float accel = viewKickCenterSpeed * direction * mult;

			float correction = cl.viewangles[ i ] - cl.prevviewangles[ i ];
			if( correction * dir_to_center[ i ] <= 0.0f ) {
				cg.recoil_initial[ i ] += correction;
				dir_to_center[ i ] += correction;
			}

			if( recentering && Abs( cg.recoil[ i ] * dt ) > Abs( dir_to_center[ i ] ) ) {
				cg.recoil[ i ] = 0.0f;
				cl.viewangles[ i ] = cg.recoil_initial[ i ];
			} else {
				cl.viewangles[ i ] += cg.recoil[ i ] * dt;
			}
			cg.recoil[ i ] += accel * dt;
		}

		if( Length( AngleDelta( cg.recoil_initial, cl.viewangles ) ) < stop_epsilon ) {
			cg.recoiling = false;
		}
	}
}
