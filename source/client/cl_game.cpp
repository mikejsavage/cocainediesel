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

static cgame_export_t *cge;

gs_state_t client_gs;

void CL_GetUserCmd( int frame, UserCommand *cmd ) {
	if( cmd ) {
		if( frame < 0 ) {
			frame = 0;
		}

		*cmd = cl.cmds[frame & CMD_MASK];
	}
}

int CL_GetCurrentUserCmdNum() {
	return cls.ucmdHead;
}

void CL_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent ) {
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

void CL_GameModule_Init() {
	StopAllSounds( true );

	CL_GameModule_Shutdown();

	cge = GetCGameAPI();
	cge->Init( cl.playernum, cl.max_clients, CL_DemoPlaying(), "", cl.snapFrameTime );
	cls.cgameActive = true;
}

void CL_GameModule_Reset() {
	if( cge ) {
		cge->Reset();
	}
}

void CL_GameModule_Shutdown() {
	if( !cge ) {
		return;
	}

	cls.cgameActive = false;

	cge->Shutdown();
	cge = NULL;
}

void CL_GameModule_EscapeKey() {
	if( cge ) {
		cge->EscapeKey();
	}
}

bool CL_GameModule_NewSnapshot( int pendingSnapshot ) {
	snapshot_t *currentSnap, *newSnap;

	if( cge ) {
		currentSnap = ( cl.currentSnapNum <= 0 ) ? NULL : &cl.snapShots[cl.currentSnapNum & UPDATE_MASK];
		newSnap = &cl.snapShots[pendingSnapshot & UPDATE_MASK];
		return cge->NewFrameSnapshot( newSnap, currentSnap );
	}

	return false;
}

void CL_GameModule_RenderView() {
	if( cge && cls.cgameActive ) {
		cge->RenderView( cl_extrapolate->integer && !CL_DemoPlaying() ? cl_extrapolationTime->integer : 0 );
	}
}

u8 CL_GameModule_GetButtonBits() {
	if( cge ) {
		return cge->GetButtonBits();
	}
	return 0;
}

u8 CL_GameModule_GetButtonDownEdges() {
	if( cge ) {
		return cge->GetButtonDownEdges();
	}
	return 0;
}

void CL_GameModule_MouseMove( Vec2 d ) {
	if( cge ) {
		cge->MouseMove( d );
	}
}
