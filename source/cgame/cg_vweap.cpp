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

/*
* CG_ViewWeapon_UpdateProjectionSource
*/
static void CG_ViewWeapon_UpdateProjectionSource( Vec3 hand_origin, const mat3_t hand_axis, Vec3 weap_origin, const mat3_t weap_axis ) {
	orientation_t *tag_result = &cg.weapon.projectionSource;
	orientation_t tag_weapon;

	tag_weapon.origin = Vec3( 0.0f );
	Matrix3_Copy( axis_identity, tag_weapon.axis );

	// move to tag_weapon
	CG_MoveToTag( &tag_weapon.origin, tag_weapon.axis,
				  hand_origin, hand_axis,
				  weap_origin, weap_axis );

	const WeaponModelMetadata * weaponInfo = cgs.weaponInfos[ cg.weapon.weapon ];

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

/*
* CG_ViewWeapon_AddAngleEffects
*/
static void CG_ViewWeapon_AddAngleEffects( Vec3 * angles ) {
	const WeaponDef * def = GS_GetWeaponDef( cg.predictedPlayerState.weapon );

	if( cg.predictedPlayerState.weapon_state == WeaponState_Firing || cg.predictedPlayerState.weapon_state == WeaponState_FiringSemiAuto ) {
		float frac = 1.0f - float( cg.predictedPlayerState.weapon_time ) / float( def->refire_time );
		angles->x -= def->refire_time * 0.025f * cosf( PI * ( frac * 2.0f - 1.0f ) * 0.5f );
	}
	else if( cg.predictedPlayerState.weapon_state == WeaponState_SwitchingIn || cg.predictedPlayerState.weapon_state == WeaponState_SwitchingOut ) {
		float frac;
		if( cg.predictedPlayerState.weapon_state == WeaponState_SwitchingIn ) {
			frac = float( cg.predictedPlayerState.weapon_time ) / float( def->weaponup_time );
		}
		else {
			frac = 1.0f - float( cg.predictedPlayerState.weapon_time ) / float( def->weapondown_time );
		}
		angles->x += Lerp( 0.0f, frac, 30.0f );
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

/*
* CG_ViewWeapon_baseanimFromWeaponState
*/
static int CG_ViewWeapon_baseanimFromWeaponState( int weapon_state ) {
	if( weapon_state == WeaponState_SwitchingIn )
		return WEAPANIM_WEAPONUP;

	if( weapon_state == WeaponState_SwitchingOut )
		return WEAPANIM_WEAPDOWN;

	return cg_gunbob->integer ? WEAPANIM_STANDBY : WEAPANIM_NOANIM;
}

/*
* CG_FrameForTime
* Returns the frame and interpolation fraction for current time in an animation started at a given time.
* When the animation is finished it will return frame -1. Takes looping into account. Looping animations
* are never finished.
*/
static float CG_FrameForTime( int *frame, int64_t curTime, int64_t startTimeStamp, float frametime, int firstframe, int lastframe, int loopingframes, bool forceLoop ) {
	int64_t runningtime, framecount;
	int curframe;
	float framefrac;

	if( curTime <= startTimeStamp ) {
		*frame = firstframe;
		return 0.0f;
	}

	if( firstframe == lastframe ) {
		*frame = firstframe;
		return 1.0f;
	}

	runningtime = curTime - startTimeStamp;
	framefrac = ( (double)runningtime / (double)frametime );
	framecount = (unsigned int)framefrac;
	framefrac -= framecount;

	curframe = firstframe + framecount;
	if( curframe > lastframe ) {
		if( forceLoop && !loopingframes ) {
			loopingframes = lastframe - firstframe;
		}

		if( loopingframes ) {
			unsigned int numloops;
			unsigned int startcount;

			startcount = ( lastframe - firstframe ) - loopingframes;

			numloops = ( framecount - startcount ) / loopingframes;
			curframe -= loopingframes * numloops;
			if( loopingframes == 1 ) {
				framefrac = 1.0f;
			}
		} else {
			curframe = -1;
		}
	}

	*frame = curframe;

	return framefrac;
}


/*
* CG_ViewWeapon_RefreshAnimation
*/
void CG_ViewWeapon_RefreshAnimation( cg_viewweapon_t *viewweapon ) {
	int baseAnim;
	int curframe = 0;
	float framefrac;

	// if the pov changed, or weapon changed, force restart
	if( viewweapon->POVnum != cg.predictedPlayerState.POVnum || viewweapon->weapon != cg.predictedPlayerState.weapon ) {
		viewweapon->eventAnim = 0;
		viewweapon->eventAnimStartTime = 0;
		viewweapon->baseAnim = 0;
		viewweapon->baseAnimStartTime = 0;
	}

	viewweapon->POVnum = cg.predictedPlayerState.POVnum;
	viewweapon->weapon = cg.predictedPlayerState.weapon;

	// hack cause of missing animation config
	if( viewweapon->weapon == Weapon_None ) {
		viewweapon->eventAnim = 0;
		viewweapon->eventAnimStartTime = 0;
		return;
	}

	baseAnim = CG_ViewWeapon_baseanimFromWeaponState( cg.predictedPlayerState.weapon_state );
	const WeaponModelMetadata * weaponInfo = cgs.weaponInfos[ viewweapon->weapon ];

	// Full restart
	if( !viewweapon->baseAnimStartTime ) {
		viewweapon->baseAnim = baseAnim;
		viewweapon->baseAnimStartTime = cl.serverTime;
	}

	// base animation changed?
	if( baseAnim != viewweapon->baseAnim ) {
		viewweapon->baseAnim = baseAnim;
		viewweapon->baseAnimStartTime = cl.serverTime;
	}

	// if a eventual animation is running override the baseAnim
	if( viewweapon->eventAnim ) {
		if( !viewweapon->eventAnimStartTime ) {
			viewweapon->eventAnimStartTime = cl.serverTime;
		}

		framefrac = CG_FrameForTime( &curframe, cl.serverTime, viewweapon->eventAnimStartTime, weaponInfo->frametime[viewweapon->eventAnim],
									 weaponInfo->firstframe[viewweapon->eventAnim], weaponInfo->lastframe[viewweapon->eventAnim],
									 weaponInfo->loopingframes[viewweapon->eventAnim], false );

		if( curframe >= 0 ) {
			return;
		}

		// disable event anim and fall through
		viewweapon->eventAnim = 0;
		viewweapon->eventAnimStartTime = 0;
	}

	// find new frame for the current animation
	framefrac = CG_FrameForTime( &curframe, cl.serverTime, viewweapon->baseAnimStartTime, weaponInfo->frametime[viewweapon->baseAnim],
								 weaponInfo->firstframe[viewweapon->baseAnim], weaponInfo->lastframe[viewweapon->baseAnim],
								 weaponInfo->loopingframes[viewweapon->baseAnim], true );

	if( curframe < 0 ) {
		Com_Error( ERR_DROP, "CG_ViewWeapon_UpdateAnimation(2): Base Animation without a defined loop.\n" );
	}
}

/*
* CG_ViewWeapon_StartAnimationEvent
*/
void CG_ViewWeapon_StartAnimationEvent( int newAnim ) {
	if( !cg.view.drawWeapon ) {
		return;
	}

	cg.weapon.eventAnim = newAnim;
	cg.weapon.eventAnimStartTime = cl.serverTime;
	CG_ViewWeapon_RefreshAnimation( &cg.weapon );
}

/*
* CG_CalcViewWeapon
*/
void CG_CalcViewWeapon( cg_viewweapon_t *viewweapon ) {
	float handOffset;

	CG_ViewWeapon_RefreshAnimation( viewweapon );

	const WeaponModelMetadata * weaponInfo = cgs.weaponInfos[ viewweapon->weapon ];

	// calculate the entity position
	viewweapon->origin = cg.view.origin;

	// weapon config offsets
	Vec3 gunAngles = weaponInfo->handpositionAngles + cg.predictedPlayerState.viewangles;
	Vec3 gunOffset = weaponInfo->handpositionOrigin + Vec3( cg_gunz->value, cg_gunx->value, cg_guny->value );

	// scale forward gun offset depending on fov and aspect ratio
	gunOffset.x *= frame_static.viewport_width / ( frame_static.viewport_height * cg.view.fracDistFOV ) ;

	// hand cvar offset
	handOffset = 0.0f;
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
	CG_ViewWeapon_AddAngleEffects( &gunAngles );

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
	if( !cg.view.drawWeapon || viewweapon->weapon == Weapon_None ) {
		return;
	}

	const Model * model = cgs.weaponInfos[ viewweapon->weapon ]->model;
	Mat4 transform = FromAxisAndOrigin( viewweapon->axis, viewweapon->origin );
	DrawViewWeapon( model, transform );
	// DrawOutlinedViewWeapon( model, transform, vec4_black, 0.25f );
}
