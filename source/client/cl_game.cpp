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
#include "client/audio/api.h"
#include "cgame/cg_local.h"

static cgame_export_t *cge;

gs_state_t client_gs;

void CL_GetUserCmd( int frame, UserCommand * cmd ) {
	*cmd = cl.cmds[ Max2( frame, 0 ) % ARRAY_COUNT( cl.cmds ) ];
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
	if( cge ) {
		snapshot_t * currentSnap = cl.currentSnapNum <= 0 ? NULL : &cl.snapShots[ cl.currentSnapNum % ARRAY_COUNT( cl.snapShots ) ];
		snapshot_t * newSnap = &cl.snapShots[ pendingSnapshot % ARRAY_COUNT( cl.snapShots ) ];
		return cge->NewFrameSnapshot( newSnap, currentSnap );
	}

	return false;
}

void CL_GameModule_RenderView() {
	if( cge && cls.cgameActive ) {
		cge->RenderView( cl_extrapolate->integer && !CL_DemoPlaying() ? cl_extrapolationTime->integer : 0 );
	}
}

UserCommandButton CL_GameModule_GetButtonBits() {
	if( cge ) {
		return cge->GetButtonBits();
	}
	return { };
}

UserCommandButton CL_GameModule_GetButtonDownEdges() {
	if( cge ) {
		return cge->GetButtonDownEdges();
	}
	return { };
}

void CL_GameModule_MouseMove( Vec2 d ) {
	if( cge ) {
		cge->MouseMove( d );
	}
}
