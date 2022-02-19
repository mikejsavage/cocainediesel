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

#include "client/client.h"
#include "cgame/cg_local.h"

Cvar *cl_ucmdMaxResend;

Cvar *cl_ucmdFPS;

static void CL_CreateNewUserCommand( int realMsec );

/*
* CL_UpdateGameInput
*
* Notifies cgame of new frame, refreshes input timings, coordinates and angles
*/
static void CL_UpdateGameInput( Vec2 movement, int frameTime ) {
	if( cls.key_dest == key_game && cls.state == CA_ACTIVE ) {
		CL_GameModule_MouseMove( frameTime, movement );
		cl.viewangles += CG_GetDeltaViewAngles();
	}
}

void CL_UserInputFrame( int realMsec ) {
	TracyZoneScoped;

	// Grab input before possibly resetting it in GlfwInputFrame
	Vec2 movement = GetMouseMovement();

	GlfwInputFrame();

	// refresh mouse angles and movement velocity
	CL_UpdateGameInput( movement, realMsec );

	// create a new UserCommand structure for this frame
	CL_CreateNewUserCommand( realMsec );

	// process console commands
	Cbuf_Execute();
}

void CL_InitInput() {
	cl_ucmdMaxResend = NewCvar( "cl_ucmdMaxResend", "3", CvarFlag_Archive );
	cl_ucmdFPS = NewCvar( "cl_ucmdFPS", "62", CvarFlag_Developer );
}

//===============================================================================
//
//	UCMDS
//
//===============================================================================

/*
* CL_RefreshUcmd
*
* Updates ucmd to use the most recent viewangles.
*/
static void CL_RefreshUcmd( UserCommand *ucmd, int msec, bool ready ) {
	ucmd->msec += msec;

	if( ucmd->msec && cls.key_dest == key_game ) {
		Vec2 movement = CG_GetMovement();

		ucmd->sidemove = movement.x;
		ucmd->forwardmove = movement.y;

		ucmd->buttons |= CL_GameModule_GetButtonBits();
		ucmd->down_edges |= CL_GameModule_GetButtonDownEdges();

		if( cl.weaponSwitch != Weapon_None ) {
			ucmd->weaponSwitch = cl.weaponSwitch;
			cl.weaponSwitch = Weapon_None;
		}
	}

	if( ready ) {
		ucmd->serverTimeStamp = cl.serverTime; // return the time stamp to the server

		if( cl.cmdNum > 0 ) {
			ucmd->msec = ucmd->serverTimeStamp - cl.cmds[( cl.cmdNum - 1 ) & CMD_MASK].serverTimeStamp;
		} else {
			ucmd->msec = 20;
		}
		if( ucmd->msec < 1 ) {
			ucmd->msec = 1;
		}
	}

	ucmd->angles[0] = ANGLE2SHORT( cl.viewangles.x );
	ucmd->angles[1] = ANGLE2SHORT( cl.viewangles.y );
	ucmd->angles[2] = ANGLE2SHORT( cl.viewangles.z );
}

void CL_WriteUcmdsToMessage( msg_t *msg ) {
	unsigned int resendCount;
	unsigned int ucmdFirst;
	unsigned int ucmdHead;

	if( !msg || cls.state < CA_ACTIVE || cls.demo.playing ) {
		return;
	}

	// find out what ucmds we have to send
	ucmdFirst = cls.ucmdAcknowledged + 1;
	ucmdHead = cl.cmdNum + 1;

	if( cl_ucmdMaxResend->integer > CMD_BACKUP / 2 ) {
		Cvar_SetInteger( "cl_ucmdMaxResend", CMD_BACKUP / 2 );
	} else if( cl_ucmdMaxResend->integer < 1 ) {
		Cvar_SetInteger( "cl_ucmdMaxResend", 1 );
	}

	// find what is our resend count (resend doesn't include the newly generated ucmds)
	// and move the start back to the resend start
	if( ucmdFirst <= cls.ucmdSent + 1 ) {
		resendCount = 0;
	} else {
		resendCount = ( cls.ucmdSent + 1 ) - ucmdFirst;
	}
	if( resendCount > (unsigned int)cl_ucmdMaxResend->integer ) {
		resendCount = (unsigned int)cl_ucmdMaxResend->integer;
	}

	if( ucmdFirst > ucmdHead ) {
		ucmdFirst = ucmdHead;
	}

	// if this happens, the player is in a freezing lag. Send him the less possible data
	if( ( ucmdHead - ucmdFirst ) + resendCount > CMD_MASK * 0.5 ) {
		resendCount = 0;
	}

	// move the start backwards to the resend point
	ucmdFirst = ( ucmdFirst > resendCount ) ? ucmdFirst - resendCount : ucmdFirst;

	if( ucmdHead - ucmdFirst > CMD_MASK ) { // ran out of updates, seduce the send to try to recover activity
		ucmdFirst = ucmdHead - 3;
	}

	// begin a client move command
	MSG_WriteUint8( msg, clc_move );

	// (acknowledge server frame snap)
	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if( cl.receivedSnapNum <= 0 ) {
		MSG_WriteInt32( msg, -1 );
	} else {
		MSG_WriteInt32( msg, cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverFrame );
	}

	// Write the actual ucmds

	// write the id number of first ucmd to be sent, and the count
	MSG_WriteInt32( msg, ucmdHead );
	MSG_WriteUint8( msg, (uint8_t)( ucmdHead - ucmdFirst ) );

	UserCommand nullcmd = { };
	const UserCommand * oldcmd = &nullcmd;

	// write the ucmds
	for( unsigned int i = ucmdFirst; i < ucmdHead; i++ ) {
		const UserCommand * cmd = &cl.cmds[i & CMD_MASK];
		MSG_WriteDeltaUsercmd( msg, oldcmd, cmd );
		oldcmd = cmd;
	}

	cls.ucmdSent = ucmdHead;
}

static bool CL_NextUserCommandTimeReached( int realMsec ) {
	static int minMsec = 1, allMsec = 0, extraMsec = 0;
	static float roundingMsec = 0.0f;
	float maxucmds;

	if( cls.state < CA_ACTIVE ) {
		maxucmds = 10; // reduce ratio while connecting
	} else {
		maxucmds = cl_ucmdFPS->integer;
	}

	// the cvar is developer only
	//clamp( maxucmds, 10, 90 ); // don't let people abuse cl_ucmdFPS

	if( cls.demo.playing ) {
		minMsec = 1;
	} else {
		minMsec = ( 1000.0f / maxucmds );
		roundingMsec += ( 1000.0f / maxucmds ) - minMsec;
	}

	if( roundingMsec >= 1.0f ) {
		minMsec += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( minMsec > extraMsec ) { // remove, from min frametime, the extra time we spent in last frame
		minMsec -= extraMsec;
	}

	allMsec += realMsec;
	if( allMsec < minMsec ) {
		//if( !cls.netchan.unsentFragments ) {
		//	NET_Sleep( minMsec - allMsec );
		return false;
	}

	extraMsec = allMsec - minMsec;
	if( extraMsec > minMsec ) {
		extraMsec = minMsec - 1;
	}

	allMsec = 0;

	if( cls.state < CA_ACTIVE ) {
		return false;
	}

	// send a new user command message to the server
	return true;
}

static void CL_CreateNewUserCommand( int realMsec ) {
	UserCommand *ucmd;

	if( !CL_NextUserCommandTimeReached( realMsec ) ) {
		// refresh current command with up to date data for movement prediction
		CL_RefreshUcmd( &cl.cmds[cls.ucmdHead & CMD_MASK], realMsec, false );
		return;
	}

	cl.cmdNum = cls.ucmdHead;
	cl.cmd_time[cl.cmdNum & CMD_MASK] = cls.realtime;

	ucmd = &cl.cmds[cl.cmdNum & CMD_MASK];

	CL_RefreshUcmd( ucmd, realMsec, true );

	// advance head and init the new command
	cls.ucmdHead++;
	ucmd = &cl.cmds[cls.ucmdHead & CMD_MASK];
	memset( ucmd, 0, sizeof( UserCommand ) );
	ucmd->entropy = Random32( &cls.rng );

	// start up with the most recent viewangles
	CL_RefreshUcmd( ucmd, 0, false );
}
