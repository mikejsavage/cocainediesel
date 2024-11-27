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
#include "qcommon/time.h"
#include "qcommon/version.h"
#include "gameshared/demo.h"

static RecordDemoContext record_demo_context = { };
static Time record_demo_game_time;
static time_t record_demo_utc_time;
static bool record_demo_waiting = false;
static char * record_demo_filename = NULL;

static DemoMetadata playing_demo_metadata;
static msg_t playing_demo_contents = { };
static bool playing_demo_paused;
static bool playing_demo_seek;
static Optional< Time > playing_demo_seek_time;

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

	record_demo_game_time = cls.game_time;
	record_demo_utc_time = time( NULL );
	record_demo_waiting = false;

	defer { Free( sys_allocator, record_demo_filename ); };

	TempAllocator temp = cls.frame_arena.temp();
	StartRecordingDemo( &temp, &record_demo_context, record_demo_filename, cl.servercount, cl.snapFrameTime, client_gs.maxclients, cl_baselines );
}

void CL_Record_f( const Tokenized & args ) {
	if( cls.state != CA_ACTIVE ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if( args.tokens.n != 2 ) {
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

	record_demo_filename = ( *sys_allocator )( "{}/demos/{}" APP_DEMO_EXTENSION_STR, HomeDirPath(), args.tokens[ 1 ] );
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
		Free( sys_allocator, record_demo_filename );
		record_demo_filename = NULL;
		record_demo_waiting = false;
		return;
	}

	Com_Printf( "Saving demo: %s\n", record_demo_context.filename );

	TempAllocator temp = cls.frame_arena.temp();

	DemoMetadata metadata = { };
	metadata.metadata_version = DEMO_METADATA_VERSION;
	metadata.game_version = MakeSpan( CopyString( &temp, APP_VERSION ) );
	metadata.server = cls.server_name;
	metadata.map = CloneSpan( &temp, cl.map->name );
	metadata.utc_time = record_demo_utc_time;
	metadata.duration_seconds = ToSeconds( cls.game_time - record_demo_game_time );
	metadata.decompressed_size = record_demo_context.decompressed_size;

	StopRecordingDemo( &temp, &record_demo_context, metadata );

	record_demo_context = { };
}

static void FreeDemoMetadata() {
	Free( sys_allocator, playing_demo_metadata.game_version.ptr );
	Free( sys_allocator, playing_demo_metadata.server.ptr );
	Free( sys_allocator, playing_demo_metadata.map.ptr );
}

void CL_DemoCompleted() {
	FreeDemoMetadata();
	Free( sys_allocator, playing_demo_contents.data );
	playing_demo_contents = { };

	Com_Printf( "Demo completed\n" );
}

void CL_ReadDemoPackets() {
	while( ( cl.receivedSnapNum <= 0 || !cl.snapShots[ cl.receivedSnapNum % ARRAY_COUNT( cl.snapShots ) ].valid || cl.snapShots[ cl.receivedSnapNum % ARRAY_COUNT( cl.snapShots ) ].serverTime < cl.serverTime ) ) {
		msg_t msg = MSG_ReadMsg( &playing_demo_contents );
		if( msg.data == NULL ) {
			CL_Disconnect( NULL );
			return;
		}
		CL_ParseServerMessage( &msg );
	}
}

void CL_LatchedDemoJump() {
	if( !playing_demo_seek_time.exists ) {
		return;
	}

	cls.game_time = playing_demo_seek_time.value;
	cl.currentSnapNum = cl.pendingSnapNum = cl.receivedSnapNum = 0;

	playing_demo_contents.readcount = 0;

	CL_AdjustServerTime( 1 );

	playing_demo_seek = true;
	playing_demo_seek_time = NONE;
}

static void CL_StartDemo( Span< const char > demoname, bool yolo ) {
	CL_Disconnect( NULL );

	Span< const char > ext = FileExtension( demoname ) == "" ? Span< const char >( APP_DEMO_EXTENSION_STR ) : Span< const char >( "" );
	char * filename;
	if( COM_ValidateRelativeFilename( demoname ) ) {
		filename = ( *sys_allocator )( "{}/demos/{}{}", HomeDirPath(), demoname, ext );
	}
	else {
		filename = ( *sys_allocator )( "{}{}", demoname, ext );
	}
	defer { Free( sys_allocator, filename ); };

	Span< u8 > demo = ReadFileBinary( sys_allocator, filename );
	if( demo.ptr == NULL ) {
		Com_Printf( S_COLOR_YELLOW "%s doesn't exist\n", filename );
		return;
	}
	defer { Free( sys_allocator, demo.ptr ); };

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
	playing_demo_seek_time = NONE;
	yolodemo = yolo;

	CL_SetClientState( CA_HANDSHAKE );
}

void CL_PlayDemo_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	CL_StartDemo( args.tokens[ 1 ], false );
}

void CL_YoloDemo_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	CL_StartDemo( args.tokens[ 1 ], true );
}

void CL_PauseDemo_f() {
	if( !CL_DemoPlaying() ) {
		Com_Printf( "Can only demopause when playing a demo.\n" );
		return;
	}

	playing_demo_paused = !playing_demo_paused;
}

void CL_DemoJump_f( const Tokenized & args ) {
	if( !CL_DemoPlaying() ) {
		Com_Printf( "Can only demojump when playing a demo\n" );
		return;
	}

	auto bad_syntax = []() {
		Com_Printf( "Usage: demojump [+/-]<seconds>\n" );
	};

	if( args.tokens.n != 2 ) {
		return bad_syntax();
	}

	Span< const char > time_str = args.tokens[ 1 ];
	bool relative = false;
	bool negative = false;
	if( time_str[ 0 ] == '+' || time_str[ 0 ] == '-' ) {
		relative = true;
		negative = time_str[ 0 ] == '-';
	}

	s64 seconds;
	if( !TrySpanToS64( time_str, &seconds ) ) {
		return bad_syntax();
	}

	Time time = Seconds( seconds );

	if( !relative ) {
		playing_demo_seek_time = time;
	}
	else if( negative ) {
		playing_demo_seek_time = cls.game_time - Min2( cls.game_time, time );
	}
	else {
		playing_demo_seek_time = cls.game_time + time;
	}
}
