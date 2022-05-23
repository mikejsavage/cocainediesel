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

#include <time.h>

#include "client/client.h"
#include "qcommon/fs.h"
#include "qcommon/version.h"
#include "gameshared/demo.h"

static RecordDemoContext record_demo_context = { };
static s64 record_demo_gametime;
static time_t record_demo_utc_time;
static bool record_demo_waiting = false;
static char * record_demo_filename = NULL;

static DemoMetadata playing_demo_metadata;
static msg_t playing_demo_contents = { };
static bool playing_demo_paused;
static bool playing_demo_seek;
static bool playing_demo_seek_latch;
static s64 playing_demo_seek_time;

static bool yolodemo;

bool CL_DemoPlaying() {
	return playing_demo_contents.data != NULL;
}

bool CL_DemoPaused() {
	return playing_demo_paused;
}

bool CL_DemoSeeking() {
	return playing_demo_seek;
}

bool CL_YoloDemo() {
	return yolodemo;
}

static bool CL_DemoRecording() {
	return record_demo_waiting || record_demo_context.file != NULL;
}

void CL_WriteDemoMessage( msg_t msg, size_t offset ) {
	if( record_demo_context.file == NULL )
		return;
	WriteDemoMessage( &record_demo_context, msg, offset );
}

void CL_DemoBaseline( const snapshot_t * snap ) {
	if( !record_demo_waiting || snap->delta )
		return;

	record_demo_gametime = cls.gametime;
	record_demo_utc_time = time( NULL );
	record_demo_waiting = false;

	defer { FREE( sys_allocator, record_demo_filename ); };

	TempAllocator temp = cls.frame_arena.temp();
	StartRecordingDemo( &temp, &record_demo_context, record_demo_filename, cl.servercount, cl.snapFrameTime, client_gs.maxclients, cl.configstrings[ 0 ], cl_baselines );
}

void CL_Record_f() {
	if( cls.state != CA_ACTIVE ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "record <demoname>\n" );
		return;
	}

	if( CL_DemoPlaying() ) {
		Com_Printf( "You can't record from another demo.\n" );
		return;
	}

	if( CL_DemoRecording() ) {
		Com_Printf( "Already recording.\n" );
		return;
	}

	if( !COM_ValidateRelativeFilename( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "Invalid filename.\n" );
		return;
	}

	record_demo_filename = ( *sys_allocator )( "{}/demos/{}" APP_DEMO_EXTENSION_STR, HomeDirPath(), Cmd_Argv( 1 ) );
	COM_SanitizeFilePath( record_demo_filename );

	Com_Printf( "Recording demo: %s\n", record_demo_filename );

	// don't start saving messages until a non-delta compressed message is received
	CL_AddReliableCommand( ClientCommand_NoDelta ); // request non delta compressed frame from server
	record_demo_waiting = true;
}

void CL_StopRecording( bool silent ) {
	if( !CL_DemoRecording() ) {
		if( !silent ) {
			Com_Printf( "Not recording a demo.\n" );
		}
		return;
	}

	if( record_demo_waiting ) {
		FREE( sys_allocator, record_demo_filename );
		record_demo_filename = NULL;
		record_demo_waiting = false;
		return;
	}

	Com_Printf( "Saving demo: %s\n", record_demo_context.filename );

	TempAllocator temp = cls.frame_arena.temp();

	DemoMetadata metadata = { };
	metadata.metadata_version = DEMO_METADATA_VERSION;
	metadata.game_version = MakeSpan( CopyString( &temp, APP_VERSION ) );
	metadata.server = MakeSpan( cls.server_name );
	metadata.map = MakeSpan( CopyString( &temp, cl.map->name ) );
	metadata.utc_time = record_demo_utc_time;
	metadata.duration_seconds = ( cls.gametime - record_demo_gametime ) / 1000;
	metadata.decompressed_size = record_demo_context.decompressed_size;

	StopRecordingDemo( &temp, &record_demo_context, metadata );

	record_demo_context = { };
}

static void FreeDemoMetadata() {
	FREE( sys_allocator, playing_demo_metadata.game_version.ptr );
	FREE( sys_allocator, playing_demo_metadata.server.ptr );
	FREE( sys_allocator, playing_demo_metadata.map.ptr );
}

void CL_DemoCompleted() {
	FreeDemoMetadata();
	FREE( sys_allocator, playing_demo_contents.data );
	playing_demo_contents = { };

	Com_Printf( "Demo completed\n" );
}

void CL_ReadDemoPackets() {
	while( ( cl.receivedSnapNum <= 0 || !cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].valid || cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverTime < cl.serverTime ) ) {
		msg_t msg = MSG_ReadMsg( &playing_demo_contents );
		if( msg.data == NULL ) {
			CL_Disconnect( NULL );
			return;
		}
		CL_ParseServerMessage( &msg );
	}
}

void CL_LatchedDemoJump() {
	if( !playing_demo_seek_latch ) {
		return;
	}

	cls.gametime = playing_demo_seek_time;
	cl.currentSnapNum = cl.pendingSnapNum = cl.receivedSnapNum = 0;

	playing_demo_contents.readcount = 0;

	CL_AdjustServerTime( 1 );

	playing_demo_seek = true;
	playing_demo_seek_latch = false;
}

static void CL_StartDemo( const char * demoname, bool yolo ) {
	CL_Disconnect( NULL );

	const char * ext = FileExtension( demoname ) == "" ? APP_DEMO_EXTENSION_STR : "";
	char * filename;
	if( COM_ValidateRelativeFilename( demoname ) ) {
		filename = ( *sys_allocator )( "{}/demos/{}{}", HomeDirPath(), demoname, ext );
	}
	else {
		filename = ( *sys_allocator )( "{}{}", demoname, ext );
	}
	defer { FREE( sys_allocator, filename ); };

	Span< u8 > demo = ReadFileBinary( sys_allocator, filename );
	if( demo.ptr == NULL ) {
		Com_Printf( S_COLOR_YELLOW "%s doesn't exist\n", filename );
		return;
	}
	defer { FREE( sys_allocator, demo.ptr ); };

	Span< u8 > decompressed;
	bool ok = true;
	ok = ok && ReadDemoMetadata( sys_allocator, &playing_demo_metadata, demo );
	ok = ok && DecompressDemo( sys_allocator, playing_demo_metadata, &decompressed, demo );
	if( !ok ) {
		Com_Printf( S_COLOR_YELLOW "Demo is corrupt\n" );
		FreeDemoMetadata();
		return;
	}

	playing_demo_contents = NewMSGReader( decompressed.ptr, decompressed.n, decompressed.n );
	playing_demo_paused = false;
	playing_demo_seek = false;
	playing_demo_seek_latch = false;
	yolodemo = yolo;

	CL_SetClientState( CA_HANDSHAKE );
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

void CL_PauseDemo_f() {
	if( !CL_DemoPlaying() ) {
		Com_Printf( "Can only demopause when playing a demo.\n" );
		return;
	}

	playing_demo_paused = !playing_demo_paused;
}

void CL_DemoJump_f() {
	if( !CL_DemoPlaying() ) {
		Com_Printf( "Can only demojump when playing a demo\n" );
		return;
	}

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: demojump <time>\n" );
		Com_Printf( "Time format is [minutes:]seconds\n" );
		Com_Printf( "Use '+' or '-' in front of the time to specify it in relation to current position\n" );
		return;
	}

	const char * p = Cmd_Argv( 1 );

	bool relative = false;
	if( Cmd_Argv( 1 )[0] == '+' || Cmd_Argv( 1 )[0] == '-' ) {
		relative = true;
		p++;
	}

	int time;
	if( strchr( p, ':' ) ) {
		time = ( atoi( p ) * 60 + atoi( strchr( p, ':' ) + 1 ) ) * 1000;
	} else {
		time = atoi( p ) * 1000;
	}

	if( Cmd_Argv( 1 )[0] == '-' ) {
		time = -time;
	}

	if( relative ) {
		playing_demo_seek_time = Max2( s64( 0 ), cls.gametime + time );
	} else {
		playing_demo_seek_time = Max2( 0, time ); // gametime always starts from 0
	}
	playing_demo_seek_latch = true;
}
