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
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static bool CamIsFree;

static Vec3 cam_origin, cam_velocity;
static EulerDegrees2 cam_angles;

void CG_DemoCamGetViewDef( cg_viewdef_t *view ) {
	view->POVent = cgs.demoPlaying && CamIsFree ? 0 : cg.frame.playerState.POVnum;
	view->thirdperson = false;
	view->playerPrediction = false;
	view->drawWeapon = false;
	view->draw2D = false;
}

void CG_DemoCamGetOrientation( Vec3 * origin, EulerDegrees2 * angles, Vec3 * velocity ) {
	*angles = cam_angles;
	*origin = cam_origin;
	*velocity = cam_velocity;
}

static void CG_DemoCamFreeFly() {
	constexpr float SPEED = 500;

	float maxspeed = 250;

	// run frame
	UserCommand cmd;
	CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );
	cmd.msec = cls.realFrameTime;

	Vec3 forward, right, up;
	AngleVectors( EulerDegrees3( cmd.angles ), &forward, &right, &up );
	cam_angles = cmd.angles;

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

ViewType CG_DemoCamUpdate() {
	if( !cgs.demoPlaying ) {
		return ViewType_Player;
	}

	if( CamIsFree ) {
		CG_DemoCamFreeFly();
	}

	return CamIsFree ? ViewType_Player : ViewType_Demo;
}

static void CG_DemoFreeFly_Cmd_f( const Tokenized & args ) {
	if( args.tokens.n > 1 ) {
		if( StrCaseEqual( args.tokens[ 1 ], "on" ) ) {
			CamIsFree = true;
		} else if( StrCaseEqual( args.tokens[ 1 ], "off" ) ) {
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
