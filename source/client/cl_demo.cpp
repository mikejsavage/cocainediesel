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
#include "qcommon/fs.h"

static void CL_PauseDemo( bool paused );

/*
* CL_WriteDemoMessage
*
* Dumps the current net message, prefixed by the length
*/
void CL_WriteDemoMessage( msg_t *msg ) {
	if( cls.demo.file <= 0 ) {
		cls.demo.recording = false;
		return;
	}

	// the first eight bytes are just packet sequencing stuff
	SNAP_RecordDemoMessage( cls.demo.file, msg, 8 );
}

/*
* CL_Stop_f
*
* stop recording a demo
*/
void CL_Stop_f() {
	if( !cls.demo.recording ) {
		Com_Printf( "Not recording a demo.\n" );
		return;
	}

	// finish up
	SNAP_StopDemoRecording( cls.demo.file );

	// write some meta information about the match/demo
	CL_SetDemoMetaKeyValue( "hostname", cl.configstrings[CS_HOSTNAME] );
	CL_SetDemoMetaKeyValue( "localtime", va( "%" PRIi64, (int64_t)cls.demo.localtime ) );
	CL_SetDemoMetaKeyValue( "multipov", "0" );
	CL_SetDemoMetaKeyValue( "duration", va( "%u", (int)ceilf( (double)cls.demo.duration / 1000.0 ) ) );
	CL_SetDemoMetaKeyValue( "mapname", cl.map->name );
	CL_SetDemoMetaKeyValue( "matchscore", cl.configstrings[CS_MATCHSCORE] );

	FS_FCloseFile( cls.demo.file );

	SNAP_WriteDemoMetaData( cls.demo.filename, cls.demo.meta_data, cls.demo.meta_data_realsize );

	Com_Printf( "Stopped demo: %s\n", cls.demo.filename );

	cls.demo.file = 0; // file id
	FREE( sys_allocator, cls.demo.filename );
	FREE( sys_allocator, cls.demo.name );
	cls.demo.filename = NULL;
	cls.demo.name = NULL;
	cls.demo.recording = false;
}

/*
* CL_Record_f
*
* record <demoname>
*
* Begins recording a demo from the current position
*/
void CL_Record_f() {
	if( cls.state != CA_ACTIVE ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "record <demoname>\n" );
		return;
	}

	if( cls.demo.playing ) {
		Com_Printf( "You can't record from another demo.\n" );
		return;
	}

	if( cls.demo.recording ) {
		Com_Printf( "Already recording.\n" );
		return;
	}

	if( !COM_ValidateRelativeFilename( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "Invalid filename.\n" );
		return;
	}

	char * filename = ( *sys_allocator )( "{}/demos/{}" APP_DEMO_EXTENSION_STR, HomeDirPath(), Cmd_Argv( 1 ) );
	COM_SanitizeFilePath( filename );

	if( FS_FOpenAbsoluteFile( filename, &cls.demo.file, FS_WRITE | SNAP_DEMO_GZ ) == -1 ) {
		Com_Printf( "Error: Couldn't create the demo file: %s\n", filename );
		FREE( sys_allocator, filename );
		return;
	}

	Com_Printf( "Recording demo: %s\n", filename );

	cls.demo.filename = filename;
	cls.demo.recording = true;
	cls.demo.basetime = cls.demo.duration = cls.demo.time = 0;
	cls.demo.name = CopyString( sys_allocator, Cmd_Argv( 1 ) );

	// don't start saving messages until a non-delta compressed message is received
	CL_AddReliableCommand( "nodelta" ); // request non delta compressed frame from server
	cls.demo.waiting = true;
}


//================================================================
//
//	WARSOW : CLIENT SIDE DEMO PLAYBACK
//
//================================================================

// demo file
static int demofilehandle;
static int demofilelen, demofilelentotal;

/*
* CL_DemoCompleted
*
* Close the demo file and disable demo state. Called from disconnection process
*/
void CL_DemoCompleted() {
	if( demofilehandle ) {
		FS_FCloseFile( demofilehandle );
		demofilehandle = 0;
	}
	demofilelen = demofilelentotal = 0;

	cls.demo.playing = false;
	cls.demo.basetime = cls.demo.duration = cls.demo.time = 0;
	FREE( sys_allocator, cls.demo.filename );
	cls.demo.filename = NULL;
	FREE( sys_allocator, cls.demo.name );
	cls.demo.name = NULL;

	Com_SetDemoPlaying( false );

	CL_PauseDemo( false );

	Com_Printf( "Demo completed\n" );

	memset( &cls.demo, 0, sizeof( cls.demo ) );
}

/*
* CL_ReadDemoMessage
*
* Read a packet from the demo file and send it to the messages parser
*/
static void CL_ReadDemoMessage() {
	static uint8_t msgbuf[MAX_MSGLEN];
	static msg_t demomsg;
	static bool init = true;
	int read;

	if( !demofilehandle ) {
		CL_Disconnect( NULL );
		return;
	}

	if( init ) {
		MSG_Init( &demomsg, msgbuf, sizeof( msgbuf ) );
		init = false;
	}

	read = SNAP_ReadDemoMessage( demofilehandle, &demomsg );
	if( read == -1 ) {
		CL_Disconnect( NULL );
		return;
	}

	CL_ParseServerMessage( &demomsg );
}

/*
* CL_ReadDemoPackets
*
* See if it's time to read a new demo packet
*/
void CL_ReadDemoPackets() {
	if( cls.demo.paused ) {
		return;
	}

	while( cls.demo.playing && ( cl.receivedSnapNum <= 0 || !cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].valid || cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverTime < cl.serverTime ) ) {
		CL_ReadDemoMessage();
		if( cls.demo.paused ) {
			return;
		}
	}

	cls.demo.time = cls.gametime;
	cls.demo.play_jump = false;
}

/*
* CL_LatchedDemoJump
*
* See if it's time to read a new demo packet
*/
void CL_LatchedDemoJump() {
	if( cls.demo.paused || !cls.demo.play_jump_latched ) {
		return;
	}

	cls.gametime = cls.demo.play_jump_time;

	if( cl.serverTime < cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverTime ) {
		cl.pendingSnapNum = 0;
	}

	CL_AdjustServerTime( 1 );

	if( cl.serverTime < cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverTime ) {
		demofilelen = demofilelentotal;
		FS_Seek( demofilehandle, 0, FS_SEEK_SET );
		cl.currentSnapNum = cl.receivedSnapNum = 0;
	}

	cls.demo.play_jump = true;
	cls.demo.play_jump_latched = false;
}

static void CL_StartDemo( const char * demoname, bool yolo ) {
	const char * ext = FileExtension( demoname ) == "" ? APP_DEMO_EXTENSION_STR : "";
	char * filename;
	if( COM_ValidateRelativeFilename( demoname ) ) {
		filename = ( *sys_allocator )( "{}/demos/{}{}", HomeDirPath(), demoname, ext );
	}
	else {
		filename = ( *sys_allocator )( "{}{}", HomeDirPath(), demoname, ext );
	}

	demofilelentotal = FS_FOpenAbsoluteFile( filename, &demofilehandle, FS_READ | SNAP_DEMO_GZ );
	demofilelen = demofilelentotal;

	CL_SetClientState( CA_HANDSHAKE );
	Com_SetDemoPlaying( true );
	cls.demo.playing = true;
	cls.demo.basetime = cls.demo.duration = cls.demo.time = 0;

	cls.demo.play_ignore_next_frametime = false;
	cls.demo.play_jump = false;
	cls.demo.filename = filename;
	cls.demo.name = CopyString( sys_allocator, demoname );
	cls.demo.yolo = yolo;

	CL_PauseDemo( false );
}

void CL_PlayDemo_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	CL_StartDemo( Cmd_Argv( 1 ), false );
}

void CL_YoloDemo_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	CL_StartDemo( Cmd_Argv( 1 ), true );
}

static void CL_PauseDemo( bool paused ) {
	cls.demo.paused = paused;
}

void CL_PauseDemo_f() {
	if( !cls.demo.playing ) {
		Com_Printf( "Can only demopause when playing a demo.\n" );
		return;
	}

	if( Cmd_Argc() > 1 ) {
		if( !Q_stricmp( Cmd_Argv( 1 ), "on" ) ) {
			CL_PauseDemo( true );
		} else if( !Q_stricmp( Cmd_Argv( 1 ), "off" ) ) {
			CL_PauseDemo( false );
		}
		return;
	}

	CL_PauseDemo( !cls.demo.paused );
}

void CL_DemoJump_f() {
	bool relative;
	int time;
	const char *p;

	if( !cls.demo.playing ) {
		Com_Printf( "Can only demojump when playing a demo\n" );
		return;
	}

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: demojump <time>\n" );
		Com_Printf( "Time format is [minutes:]seconds\n" );
		Com_Printf( "Use '+' or '-' in front of the time to specify it in relation to current position\n" );
		return;
	}

	p = Cmd_Argv( 1 );

	if( Cmd_Argv( 1 )[0] == '+' || Cmd_Argv( 1 )[0] == '-' ) {
		relative = true;
		p++;
	} else {
		relative = false;
	}

	if( strchr( p, ':' ) ) {
		time = ( atoi( p ) * 60 + atoi( strchr( p, ':' ) + 1 ) ) * 1000;
	} else {
		time = atoi( p ) * 1000;
	}

	if( Cmd_Argv( 1 )[0] == '-' ) {
		time = -time;
	}

	if( relative ) {
		cls.demo.play_jump_time = cls.gametime + time;
	} else {
		cls.demo.play_jump_time = time; // gametime always starts from 0
	}
	cls.demo.play_jump_latched = true;
}
