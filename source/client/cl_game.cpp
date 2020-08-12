/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "client/client.h"
#include "cgame/cg_local.h"
#include "qcommon/cmodel.h"
#include "qcommon/version.h"

static cgame_export_t *cge;

gs_state_t client_gs;

/*
* CL_GameModule_GetConfigString
*/
static void CL_GameModule_GetConfigString( int i, char *str, int size ) {
	if( i < 0 || i >= MAX_CONFIGSTRINGS ) {
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: i > MAX_CONFIGSTRINGS" );
		return;
	}
	if( !str || size <= 0 ) {
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: NULL string" );
		return;
	}

	Q_strncpyz( str, cl.configstrings[i], size );
}

/*
* CL_GameModule_NET_GetUserCmd
*/
static void CL_GameModule_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	if( cmd ) {
		if( frame < 0 ) {
			frame = 0;
		}

		*cmd = cl.cmds[frame & CMD_MASK];
	}
}

/*
* CL_GameModule_NET_GetCurrentUserCmdNum
*/
static int CL_GameModule_NET_GetCurrentUserCmdNum( void ) {
	return cls.ucmdHead;
}

/*
* CL_GameModule_NET_GetCurrentState
*/
static void CL_GameModule_NET_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent ) {
	if( incomingAcknowledged ) {
		*incomingAcknowledged = cls.ucmdAcknowledged;
	}
	if( outgoingSequence ) {
		*outgoingSequence = cls.ucmdHead;
	}
	if( outgoingSent ) {
		*outgoingSent = cls.ucmdSent;
	}
}

//==============================================

/*
* CL_GameModule_Init
*/
void CL_GameModule_Init( void ) {
	cgame_import_t import;

	// stop all playing sounds
	S_StopAllSounds( true );

	CL_GameModule_Shutdown();

	import.GetConfigString = CL_GameModule_GetConfigString;

	import.NET_GetUserCmd = CL_GameModule_NET_GetUserCmd;
	import.NET_GetCurrentUserCmdNum = CL_GameModule_NET_GetCurrentUserCmdNum;
	import.NET_GetCurrentState = CL_GameModule_NET_GetCurrentState;

	import.VID_FlashWindow = FlashWindow;

	cge = GetCGameAPI( &import );

	cge->Init( cls.servername, cl.playernum, cls.demo.playing, cls.demo.playing ? cls.demo.filename : "", cl.snapFrameTime );

	cls.cgameActive = true;
}

/*
* CL_GameModule_Reset
*/
void CL_GameModule_Reset( void ) {
	if( cge ) {
		cge->Reset();
	}
}

/*
* CL_GameModule_Shutdown
*/
void CL_GameModule_Shutdown( void ) {
	if( !cge ) {
		return;
	}

	cls.cgameActive = false;

	cge->Shutdown();
	cge = NULL;
}

/*
* CL_GameModule_EscapeKey
*/
void CL_GameModule_EscapeKey( void ) {
	if( cge ) {
		cge->EscapeKey();
	}
}

/*
* CL_GameModule_GetEntitySoundOrigin
*/
void CL_GameModule_GetEntitySpatilization( int entNum, Vec3 * origin, Vec3 * velocity ) {
	if( cge ) {
		cge->GetEntitySpatilization( entNum, origin, velocity );
	}
}

/*
* CL_GameModule_ConfigString
*/
void CL_GameModule_ConfigString( int number, const char *value ) {
	if( cge ) {
		cge->ConfigString( number, value );
	}
}

/*
* CL_GameModule_NewSnapshot
*/
bool CL_GameModule_NewSnapshot( int pendingSnapshot ) {
	snapshot_t *currentSnap, *newSnap;

	if( cge ) {
		currentSnap = ( cl.currentSnapNum <= 0 ) ? NULL : &cl.snapShots[cl.currentSnapNum & UPDATE_MASK];
		newSnap = &cl.snapShots[pendingSnapshot & UPDATE_MASK];
		return cge->NewFrameSnapshot( newSnap, currentSnap );
	}

	return false;
}

/*
* CL_GameModule_RenderView
*/
void CL_GameModule_RenderView() {
	if( cge && cls.cgameActive ) {
		cge->RenderView( cl_extrapolate->integer && !cls.demo.playing ? cl_extrapolationTime->integer : 0 );
	}
}

/*
* CL_GameModule_GetButtonBits
*/
unsigned CL_GameModule_GetButtonBits( void ) {
	if( cge ) {
		return cge->GetButtonBits();
	}
	return 0;
}

/*
* CL_GameModule_MouseMove
*/
void CL_GameModule_MouseMove( int frameTime, Vec2 d ) {
	if( cge ) {
		cge->MouseMove( frameTime, d );
	}
}
