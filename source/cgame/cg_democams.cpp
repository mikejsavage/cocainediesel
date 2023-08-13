/*
Copyright (C) 2009 German Garcia Fernandez ("Jal")

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

#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "qcommon/cmodel.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static bool CamIsFree;

static Vec3 cam_origin, cam_angles, cam_velocity;

void CG_DemoCamGetViewDef( cg_viewdef_t *view ) {
	view->POVent = cgs.demoPlaying && CamIsFree ? 0 : cg.frame.playerState.POVnum;
	view->thirdperson = false;
	view->playerPrediction = false;
	view->drawWeapon = false;
	view->draw2D = false;
}

void CG_DemoCamGetOrientation( Vec3 * origin, Vec3 * angles, Vec3 * velocity ) {
	*angles = cam_angles;
	*origin = cam_origin;
	*velocity = cam_velocity;
}

static short freecam_delta_angles[3];

static void CG_DemoCamFreeFly() {
	constexpr float SPEED = 500;

	float maxspeed = 250;

	// run frame
	UserCommand cmd;
	CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );
	cmd.msec = cls.realFrameTime;

	Vec3 moveangles = Vec3(
		SHORT2ANGLE( cmd.angles[ 0 ] ) + SHORT2ANGLE( freecam_delta_angles[ 0 ] ),
		SHORT2ANGLE( cmd.angles[ 1 ] ) + SHORT2ANGLE( freecam_delta_angles[ 1 ] ),
		SHORT2ANGLE( cmd.angles[ 2 ] ) + SHORT2ANGLE( freecam_delta_angles[ 2 ] )
	);

	Vec3 forward, right, up;
	AngleVectors( moveangles, &forward, &right, &up );
	cam_angles = moveangles;

	float fmove = cmd.forwardmove * SPEED / 127.0f;
	float smove = cmd.sidemove * SPEED / 127.0f;
	float upmove = int( (cmd.buttons & Button_Ability1) != 0 ) * SPEED;
	if( cmd.buttons & Button_Ability2 ) {
		maxspeed *= 2;
	}

	Vec3 wishvel = forward * fmove + right * smove;
	wishvel.z += upmove;

	float wishspeed = Length( wishvel );
	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		wishvel = wishvel * ( wishspeed );
		wishspeed = maxspeed;
	}

	cam_origin = cam_origin + wishvel * ( (float)cls.realFrameTime * 0.001f );
}

static void CG_DemoCamSetCameraPositionFromView() {
	if( cg.view.type == VIEWDEF_PLAYERVIEW ) {
		cam_origin = cg.view.origin;
		cam_angles = cg.view.angles;
		cam_velocity = cg.view.velocity;
	}

	if( !CamIsFree ) {
		UserCommand cmd;

		CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );

		freecam_delta_angles[ 0 ] = ANGLE2SHORT( cam_angles.x ) - cmd.angles[ 0 ];
		freecam_delta_angles[ 1 ] = ANGLE2SHORT( cam_angles.y ) - cmd.angles[ 1 ];
		freecam_delta_angles[ 2 ] = ANGLE2SHORT( cam_angles.z ) - cmd.angles[ 2 ];
	}
}

int CG_DemoCamUpdate() {
	if( !cgs.demoPlaying ) {
		return VIEWDEF_PLAYERVIEW;
	}

	if( CamIsFree ) {
		CG_DemoCamFreeFly();
	}

	CG_DemoCamSetCameraPositionFromView();

	return CamIsFree ? VIEWDEF_DEMOCAM : VIEWDEF_PLAYERVIEW;
}

static void CG_DemoFreeFly_Cmd_f() {
	if( Cmd_Argc() > 1 ) {
		if( StrCaseEqual( Cmd_Argv( 1 ), "on" ) ) {
			CamIsFree = true;
		} else if( StrCaseEqual( Cmd_Argv( 1 ), "off" ) ) {
			CamIsFree = false;
		}
	} else {
		CamIsFree = !CamIsFree;
	}

	cam_velocity = Vec3( 0.0f );
	Com_Printf( "demo cam mode %s\n", CamIsFree ? "Free Fly" : "Preview" );
}


void CG_DemoCamInit() {
	if( !cgs.demoPlaying ) {
		return;
	}

	AddCommand( "demoFreeFly", CG_DemoFreeFly_Cmd_f );
}

void CG_DemoCamShutdown() {
	if( !cgs.demoPlaying ) {
		return;
	}

	RemoveCommand( "demoFreeFly" );
}
