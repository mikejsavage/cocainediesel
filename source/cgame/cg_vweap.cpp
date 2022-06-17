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

static float SmoothStep( float t ) {
	return t * t * ( 3.0f - 2.0f * t );
}

static void CG_ViewWeapon_AddAngleEffects( Vec3 * angles, cg_viewweapon_t * viewweapon ) {
	const SyncPlayerState * ps = &cg.predictedPlayerState;

	if( ps->using_gadget ) {
		const GadgetDef * def = GetGadgetDef( ps->gadget );

		if( ps->weapon_state == WeaponState_SwitchingIn ) {
			float frac = 1.0f - float( ps->weapon_state_time ) / float( def->switch_in_time );
			frac *= frac; //smoother curve
			viewweapon->origin -= FromQFAxis( cg.view.axis, AXIS_UP ) * frac * 10.0f;
			angles->x += Lerp( 0.0f, frac, 60.0f );
		}
		else if( ps->weapon_state == WeaponState_Cooking ) {
			float charge = float( ps->weapon_state_time ) / float( def->cook_time );
			float pull_back = ( 1.0f - Square( 1.0f - charge ) ) * 9.0f;
			viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_UP ) * pull_back;
			angles->x -= Lerp( 0.0f, pull_back, 4.0f );
		}
		else if( ps->weapon_state == WeaponState_Throwing ) {
			float frac = float( ps->weapon_state_time ) / float( def->using_time );
			viewweapon->origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * frac * 16.0f;
		}
		else if( ps->weapon_state == WeaponState_SwitchingOut ) {
			viewweapon->origin.z -= 1000.0f; // TODO: lol
		}
	}
	else {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		if( ps->weapon == Weapon_None )
			return;

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
		else if( ps->weapon_state == WeaponState_Reloading || ps->weapon_state == WeaponState_StagedReloading ) {
			// TODO: temporary for non-animated models
			const Model * model = GetWeaponModelMetadata( ps->weapon )->model;
			bool found = false;
			for( u8 i = 0; i < model->num_animations; i++ ) {
				if( model->animations[ i ].name == viewweapon->eventAnim ) {
					found = true;
					break;
				}
			}

			if( !found ) {
				float t = ps->weapon_state == WeaponState_Reloading ? def->reload_time : def->staged_reload_time;
				float frac = float( ps->weapon_state_time ) / t;
				angles->z += Lerp( 0.0f, SmoothStep( frac ), 360.0f );
			}
		}
		else if( ps->weapon_state == WeaponState_SwitchingIn || ps->weapon_state == WeaponState_SwitchingOut ) {
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
		else if( ps->weapon == Weapon_Bat && ps->weapon_state == WeaponState_Cooking ) {
			float charge = float( ps->weapon_state_time ) / float( def->reload_time );
			float pull_back = ( 1.0f - Square( 1.0f - charge ) ) * 4.0f;
			viewweapon->origin -= FromQFAxis( cg.view.axis, AXIS_FORWARD ) * pull_back;
			angles->x -= pull_back * 12.0f;
			angles->y += pull_back * 2.0f;
		}
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
}

void CG_ViewWeapon_AddAnimation( int ent_num, StringHash anim ) {
	if( ISVIEWERENTITY( ent_num ) && !cg.view.thirdperson && cg.view.drawWeapon ) {
		cg.weapon.eventAnim = anim;
		cg.weapon.eventAnimStartTime = cl.serverTime;
	}
}

void CG_CalcViewWeapon( cg_viewweapon_t * viewweapon ) {
	const SyncPlayerState * ps = &cg.predictedPlayerState;
	const Model * model;
	if( !ps->using_gadget ) {
		model = GetWeaponModelMetadata( ps->weapon )->model;
	}
	else {
		model = GetGadgetModelMetadata( ps->gadget )->model;
	}

	if( model == NULL )
		return;

	Vec3 gunAngles, gunOffset;
	if( model->camera == U8_MAX ) {
		if( ps->using_gadget ) {
			Com_Printf( S_COLOR_YELLOW "Gadget models must have a camera!\n" );
			return;
		}

		const WeaponModelMetadata * weaponInfo = GetWeaponModelMetadata( ps->weapon );
		// calculate the entity position
		// weapon config offsets
		gunAngles = weaponInfo->handpositionAngles;

		constexpr Vec3 old_gunpos_cvars = Vec3( 3, 10, -12 ); // depth, horizontal, vertical
		gunOffset = weaponInfo->handpositionOrigin + old_gunpos_cvars;
		gunOffset = Vec3( gunOffset.y, gunOffset.z, -gunOffset.x );
	}
	else {
		constexpr Mat4 y_up_to_camera_space = Mat4(
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

		gunOffset = ( y_up_to_camera_space * Vec4( -model->nodes[ model->camera ].global_transform.col3.xyz(), 1.0f ) ).xyz();
		gunAngles = Vec3( 0.0f );
	}

	// scale forward gun offset depending on fov and aspect ratio
	gunOffset.x *= frame_static.viewport_width / ( frame_static.viewport_height * cg.view.fracDistFOV );

	viewweapon->origin = ( frame_static.inverse_V * Vec4( gunOffset, 1.0 ) ).xyz();
	viewweapon->origin.z += CG_ViewSmoothFallKick();

	// add angles effects
	gunAngles += cg.predictedPlayerState.viewangles;
	CG_ViewWeapon_AddAngleEffects( &gunAngles, viewweapon );

	// finish
	AnglesToAxis( gunAngles, viewweapon->axis );

	if( cg.predictedPlayerState.zoom_time == 0 ) {
		constexpr float gun_fov = 90.0f;
		float gun_fov_y = WidescreenFov( gun_fov );
		float gun_fov_x = CalcHorizontalFov( "CalcViewWeapon", gun_fov_y, frame_static.viewport_width, frame_static.viewport_height );

		float fracWeapFOV = tanf( Radians( gun_fov_x ) * 0.5f ) / cg.view.fracDistFOV;

		viewweapon->axis[AXIS_FORWARD] *= fracWeapFOV;
		viewweapon->axis[AXIS_FORWARD + 1] *= fracWeapFOV;
		viewweapon->axis[AXIS_FORWARD + 2] *= fracWeapFOV;
	}

	Mat4 gun_transform = FromAxisAndOrigin( viewweapon->axis, viewweapon->origin );
	u8 muzzle;
	if( FindNodeByName( model, Hash32( "muzzle" ), &muzzle ) ) {
		viewweapon->muzzle_transform = gun_transform * model->transform * model->nodes[ muzzle ].global_transform;
	}
	else {
		Mat4 hardcoded_offset = Mat4Translation( Vec3( 16, 0, 8 ) );
		viewweapon->muzzle_transform = gun_transform * hardcoded_offset;
	}
}

void CG_AddViewWeapon( cg_viewweapon_t * viewweapon ) {
	if( !cg.view.drawWeapon )
		return;

	const SyncPlayerState * ps = &cg.predictedPlayerState;
	const Model * model;
	if( !ps->using_gadget ) {
		model = GetWeaponModelMetadata( ps->weapon )->model;
	}
	else {
		model = GetGadgetModelMetadata( ps->gadget )->model;
	}

	if( model == NULL ) {
		return;
	}

	Mat4 transform = FromAxisAndOrigin( viewweapon->axis, viewweapon->origin );

	DrawModelConfig config = { };
	config.draw_model.enabled = true;
	config.draw_model.view_weapon = true;

	bool found = false;
	for( u8 i = 0; i < model->num_animations; i++ ) {
		if( model->animations[ i ].name == viewweapon->eventAnim ) {
			found = true;
			float t = float( cl.serverTime - viewweapon->eventAnimStartTime ) * 0.001f;
			TempAllocator temp = cls.frame_arena.temp();
			Span< TRS > pose = SampleAnimation( &temp, model, t, i );
			MatrixPalettes palettes = ComputeMatrixPalettes( &temp, model, pose );
			DrawModel( config, model, transform, CG_TeamColorVec4( ps->team ), palettes );
			break;
		}
	}
	if( !found ) {
		DrawModel( config, model, transform, CG_TeamColorVec4( ps->team ) );
	}
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
