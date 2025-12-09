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

static float EaseInQuadratic( float t ) {
	return Square( t );
}

static float EaseOutQuadratic( float t ) {
	return /*1.0f -*/ Square( 1.0f - t );
}

static void AddViewWeaponAnimations( Vec3 * origin, EulerDegrees3 * angles, cg_viewweapon_t * viewweapon ) {
	const SyncPlayerState * ps = &cg.predictedPlayerState;

	if( ps->using_gadget ) {
		const GadgetDef * def = GetGadgetDef( ps->gadget );

		if( ps->weapon_state == WeaponState_SwitchingIn ) {
			float frac = EaseOutQuadratic( float( ps->weapon_state_time ) / float( def->switch_in_time ) );
			*origin -= FromQFAxis( cg.view.axis, AXIS_UP ) * frac * 10.0f;
			angles->pitch -= Lerp( 0.0f, frac, 60.0f );
		}
		else if( ps->weapon_state == WeaponState_Cooking ) {
			float charge = float( ps->weapon_state_time ) / float( def->cook_time );
			float pull_back = ( 1.0f - Square( 1.0f - charge ) ) * 9.0f;
			*origin += FromQFAxis( cg.view.axis, AXIS_UP ) * pull_back;
			angles->pitch += Lerp( 0.0f, pull_back, 4.0f );
		}
		else if( ps->weapon_state == WeaponState_Throwing ) {
			float frac = float( ps->weapon_state_time ) / float( def->using_time );
			*origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * frac * 16.0f;
		}
		else if( ps->weapon_state == WeaponState_SwitchingOut ) {
			origin->z -= 1000.0f; // TODO: lol
		}
	}
	else {
		if( ps->weapon == Weapon_None )
			return;

		const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );

		if( ps->weapon_state == WeaponState_Firing || ps->weapon_state == WeaponState_FiringSemiAuto ) {
			float refire_time = GetWeaponDefFire( ps->weapon, ps->weapon_alt_fire )->refire_time;
			float frac = float( ps->weapon_state_time ) / refire_time;
			if( ps->weapon == Weapon_Knife ) {
				*origin += FromQFAxis( cg.view.axis, AXIS_FORWARD ) * 30.0f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
				angles->roll += refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
				angles->yaw -= refire_time * 0.025f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
				angles->pitch += refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			}
			else {
				angles->pitch += refire_time * 0.05f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
			}
		}
		else if( ps->weapon_state == WeaponState_Reloading || ps->weapon_state == WeaponState_StagedReloading ) {
			// TODO: temporary for non-animated models
			const GLTFRenderData * model = FindGLTFRenderData( GetWeaponModelMetadata( ps->weapon )->model );
			if( model != NULL ) {
				u8 animation;
				if( !FindAnimationByName( model, viewweapon->eventAnim, &animation ) ) {
					float t = float( ps->weapon_state_time ) / def->reload_time;
					angles->roll -= Lerp( 0.0f, SmoothStep( t ), 360.0f );
				}
			}
		}
		else if( ps->weapon_state == WeaponState_SwitchingIn || ps->weapon_state == WeaponState_SwitchingOut ) {
			float frac;
			if( ps->weapon_state == WeaponState_SwitchingIn ) {
				frac = EaseOutQuadratic( float( ps->weapon_state_time ) / def->switch_in_time );
			}
			else {
				frac = EaseInQuadratic( float( ps->weapon_state_time ) / def->switch_out_time );
			}
			angles->pitch -= Lerp( 0.0f, frac, 60.0f );
		}
		else if( ps->weapon == Weapon_Bat && ps->weapon_state == WeaponState_Cooking ) {
			float charge = float( ps->weapon_state_time ) / def->reload_time;
			float pull_back = ( 1.0f - Square( 1.0f - charge ) ) * 4.0f;
			*origin -= FromQFAxis( cg.view.axis, AXIS_FORWARD ) * pull_back;
			angles->pitch += pull_back * 10.0f;
			angles->yaw -= pull_back * 2.0f;
		}
	}

	// gun angles from bobbing
	if( cg.bobCycle & 1 ) {
		angles->roll += cg.xyspeed * cg.bobFracSin * 0.012f;
		angles->yaw += cg.xyspeed * cg.bobFracSin * 0.006f;
	} else {
		angles->roll -= cg.xyspeed * cg.bobFracSin * 0.012f;
		angles->yaw -= cg.xyspeed * cg.bobFracSin * 0.006f;
	}
	angles->pitch -= cg.xyspeed * cg.bobFracSin * 0.012f;
}

void CG_ViewWeapon_AddAnimation( int ent_num, StringHash anim ) {
	if( ISVIEWERENTITY( ent_num ) && !cg.view.thirdperson && cg.view.drawWeapon ) {
		cg.weapon.eventAnim = anim;
		cg.weapon.eventAnimStartTime = cl.serverTime;
	}
}

void CG_CalcViewWeapon( cg_viewweapon_t * viewweapon ) {
	const SyncPlayerState * ps = &cg.predictedPlayerState;
	const GLTFRenderData * model = GetEquippedItemRenderData( ps );
	if( model == NULL ) {
		viewweapon->transform = Mat3x4::Identity();
		viewweapon->muzzle_transform = Mat3x4::Identity();
		return;
	}

	Vec3 origin = Vec3( 0.0f );
	EulerDegrees3 angles = { };

	if( !model->camera_node.exists ) {
		if( ps->using_gadget ) {
			Com_Printf( S_COLOR_YELLOW "Gadget models must have a camera!\n" );
			return;
		}

		const WeaponModelMetadata * weaponInfo = GetWeaponModelMetadata( ps->weapon );
		// calculate the entity position
		// weapon config offsets
		angles = weaponInfo->handpositionAngles;

		constexpr Vec3 old_gunpos_cvars = Vec3( 3, 10, -12 ); // depth, horizontal, vertical
		origin = weaponInfo->handpositionOrigin + old_gunpos_cvars;
		origin = Vec3( origin.y, origin.z, -origin.x );
	}
	else {
		constexpr Mat4 y_up_to_camera_space = Mat4(
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

		// TODO: just use this transform directly
		origin = ( y_up_to_camera_space * Vec4( -model->nodes[ model->camera_node.value ].global_transform.col3, 1.0f ) ).xyz();
	}

	// scale forward gun offset depending on fov and aspect ratio
	constexpr float ZOOM_REDUCE_FALLKICK = 0.7f;
	float zoom_ads = float( GetWeaponDefProperties( cg.predictedPlayerState.weapon )->zoom_type == Zoom_ADS );
	float frac = zoom_ads * cg.predictedPlayerState.zoom_time / float( ZOOMTIME );
	origin.x *= ( 1.f - frac ) * frame_static.viewport_width / ( frame_static.viewport_height * cg.view.fracDistFOV );
	origin.y += CG_ViewSmoothFallKick() * ( 1.f - ZOOM_REDUCE_FALLKICK * frac );

	AddViewWeaponAnimations( &origin, &angles, viewweapon );

	// TODO: maybe it would be easier to give viewmodels their own render pass with identity view transform. might be required for the depth hack anyway later
	// TODO: shouldn't this have a model->transform on the end? why does the muzzle transform have it?
	viewweapon->transform = frame_static.inverse_V * Mat4Translation( origin ) * AnglesToMat3x4( angles );

	u8 muzzle;
	if( FindNodeByName( model, "muzzle", &muzzle ) ) {
		viewweapon->muzzle_transform = viewweapon->transform * model->transform * model->nodes[ muzzle ].global_transform;
	}
	else {
		constexpr Mat3x4 hardcoded_offset = Mat4Translation( Vec3( 16, 0, 8 ) );
		viewweapon->muzzle_transform = viewweapon->transform * hardcoded_offset;
	}
}

void CG_AddViewWeapon( cg_viewweapon_t * viewweapon ) {
	if( !cg.view.drawWeapon )
		return;

	const SyncPlayerState * ps = &cg.predictedPlayerState;
	const GLTFRenderData * model = GetEquippedItemRenderData( ps );

	if( model == NULL ) {
		return;
	}

	DrawModelConfig config = { };
	config.draw_model.enabled = true;
	config.draw_model.view_weapon = true;

	u8 animation;
	if( FindAnimationByName( model, viewweapon->eventAnim, &animation ) ) {
		float t = float( cl.serverTime - viewweapon->eventAnimStartTime ) * 0.001f;
		TempAllocator temp = cls.frame_arena.temp();
		Span< Transform > pose = SampleAnimation( &temp, model, t, animation );
		MatrixPalettes palettes = ComputeMatrixPalettes( &temp, model, pose );
		DrawGLTFModel( config, model, viewweapon->transform, CG_TeamColorVec4( ps->team ), palettes );
	}
	else {
		DrawGLTFModel( config, model, viewweapon->transform, CG_TeamColorVec4( ps->team ) );
	}
}

void CG_AddRecoil( WeaponType weapon, bool altfire ) {
	if( !cg.recoiling ) {
		cg.recoil_initial_angles = cl.viewangles;
		cg.recoiling = true;
	}

	EulerDegrees2 min = GetWeaponDefFire( weapon, altfire )->recoil_min;
	EulerDegrees2 max = GetWeaponDefFire( weapon, altfire )->recoil_max;

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
	EulerDegrees2 viewangles = cl.viewangles;
	EulerDegrees2 recovery_delta = AngleDelta( cg.recoil_initial_angles, viewangles );

	cg.recoil_initial_angles.pitch += Min2( 0.0f, cl.viewangles.pitch - cl.prevviewangles.pitch );
	cg.recoil_initial_angles.yaw += AngleDelta( cl.viewangles.yaw, cl.prevviewangles.yaw );

	recovery_delta = AngleDelta( cg.recoil_initial_angles, viewangles );

	constexpr float recenter_speed_scale = 1.0f / 16.0f;
	float recenter_accel = GetWeaponDefProperties( weapon )->recoil_recovery;

	// pitch
	{
		bool recovering = cg.recoil_velocity.pitch >= 0.0f;
		float accel = recenter_accel * ( recovering ? recenter_speed_scale : 1.0f );
		cg.recoil_velocity.pitch += accel * dt;

		if( recovering && recovery_delta.pitch <= cg.recoil_velocity.pitch * dt ) {
			cg.recoiling = false;
			cl.viewangles.pitch = Max2( viewangles.pitch, cg.recoil_initial_angles.pitch );
		}
		else {
			cl.viewangles.pitch += cg.recoil_velocity.pitch * dt;
		}
	}

	// yaw
	{
		bool recovering = SameSign( cg.recoil_velocity.yaw, recovery_delta.yaw );
		float accel = recenter_accel * 0.2f * SignedOne( recovery_delta.yaw ) * ( recovering ? recenter_speed_scale : 1.0f );
		cg.recoil_velocity.yaw += accel * dt;

		cl.viewangles.yaw += cg.recoil_velocity.yaw * dt;
	}
}
