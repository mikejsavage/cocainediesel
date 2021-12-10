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

#include <algorithm>

#include <ctype.h>
#include <time.h>

#include "server/server.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"

#define SV_DEMO_DIR va( "demos/server%s%s", sv_demodir->value[0] ? "/" : "", sv_demodir->value[0] ? sv_demodir->value : "" )

static void SV_Demo_WriteMessage( msg_t *msg ) {
	assert( svs.demo.file );
	if( !svs.demo.file ) {
		return;
	}

	SNAP_RecordDemoMessage( svs.demo.file, msg, 0 );
}

static void SV_Demo_WriteStartMessages() {
	memset( svs.demo.meta_data, 0, sizeof( svs.demo.meta_data ) );
	svs.demo.meta_data_realsize = 0;

	SNAP_BeginDemoRecording( svs.demo.file, svs.spawncount, svc.snapFrameTime, sv.configstrings[0], sv.baselines );
}

void SV_Demo_WriteSnap() {
	ZoneScoped;

	int i;
	msg_t msg;
	uint8_t msg_buffer[MAX_MSGLEN];

	if( !svs.demo.file ) {
		return;
	}

	for( i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_SPAWNED && svs.clients[i].edict &&
			!( svs.clients[i].edict->r.svflags & SVF_NOCLIENT ) ) {
			break;
		}
	}
	if( i == sv_maxclients->integer ) { // FIXME
		Com_Printf( "No players left, stopping server side demo recording\n" );
		SV_Demo_Stop_f();
		return;
	}

	MSG_Init( &msg, msg_buffer, sizeof( msg_buffer ) );

	SV_BuildClientFrameSnap( &svs.demo.client );

	SV_WriteFrameSnapToClient( &svs.demo.client, &msg );

	SV_AddReliableCommandsToMessage( &svs.demo.client, &msg );

	SV_Demo_WriteMessage( &msg );

	svs.demo.duration = svs.gametime - svs.demo.basetime;
	svs.demo.client.lastframe = sv.framenum; // FIXME: is this needed?
}

static void SV_Demo_InitClient() {
	memset( &svs.demo.client, 0, sizeof( svs.demo.client ) );

	svs.demo.client.mv = true;

	svs.demo.client.reliableAcknowledge = 0;
	svs.demo.client.reliableSequence = 0;
	svs.demo.client.reliableSent = 0;
	memset( svs.demo.client.reliableCommands, 0, sizeof( svs.demo.client.reliableCommands ) );

	svs.demo.client.lastframe = sv.framenum - 1;
	svs.demo.client.nodelta = false;
}

void SV_Demo_Start_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: serverrecord <demoname>\n" );
		return;
	}

	if( svs.demo.file ) {
		Com_Printf( "Already recording\n" );
		return;
	}

	if( sv.state != ss_game ) {
		Com_Printf( "Must be in a level to record\n" );
		return;
	}

	bool any_players = false;
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_SPAWNED && svs.clients[i].edict && !( svs.clients[i].edict->r.svflags & SVF_NOCLIENT ) ) {
			any_players = true;
			break;
		}
	}
	if( !any_players ) {
		Com_Printf( "No players in game, can't record a demo\n" );
		return;
	}

	//
	// open the demo file
	//

	if( !COM_ValidateRelativeFilename( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "Invalid filename.\n" );
		return;
	}

	svs.demo.filename = ( *sys_allocator )( "{}/{}" APP_DEMO_EXTENSION_STR, SV_DEMO_DIR, Cmd_Argv( 1 ) );
	COM_SanitizeFilePath( svs.demo.filename );

	svs.demo.tempname = ( *sys_allocator )( "{}.rec", svs.demo.filename );

	// open it
	if( FS_FOpenAbsoluteFile( svs.demo.tempname, &svs.demo.file, FS_WRITE | SNAP_DEMO_GZ ) == -1 ) {
		Com_Printf( "Error: Couldn't open file: %s\n", svs.demo.tempname );
		FREE( sys_allocator, svs.demo.filename );
		svs.demo.filename = NULL;
		FREE( sys_allocator, svs.demo.tempname );
		svs.demo.tempname = NULL;
		return;
	}

	Com_Printf( "Recording server demo: %s\n", svs.demo.filename );

	SV_Demo_InitClient();

	// write serverdata, configstrings and baselines
	svs.demo.duration = 0;
	svs.demo.basetime = svs.gametime;
	svs.demo.localtime = time( NULL );
	SV_Demo_WriteStartMessages();

	// write one nodelta frame
	svs.demo.client.nodelta = true;
	SV_Demo_WriteSnap();
	svs.demo.client.nodelta = false;
}

static void SV_Demo_Stop( bool cancel, bool silent ) {
	if( !svs.demo.file ) {
		if( !silent ) {
			Com_Printf( "No server demo recording in progress\n" );
		}
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();

	if( cancel ) {
		Com_Printf( "Cancelled server demo recording: %s\n", svs.demo.filename );
	}
	else {
		SNAP_StopDemoRecording( svs.demo.file );
		Com_Printf( "Stopped server demo recording: %s\n", svs.demo.filename );
	}

	FS_FCloseFile( svs.demo.file );
	svs.demo.file = 0;

	if( cancel ) {
		if( !RemoveFile( &temp, svs.demo.tempname ) ) {
			Com_Printf( "Error: Failed to delete the temporary server demo file\n" );
		}
	}
	else {
		// write some meta information about the match/demo
		SV_SetDemoMetaKeyValue( "hostname", sv.configstrings[CS_HOSTNAME] );
		SV_SetDemoMetaKeyValue( "localtime", va( "%" PRIi64, (int64_t)svs.demo.localtime ) );
		SV_SetDemoMetaKeyValue( "multipov", "1" );
		SV_SetDemoMetaKeyValue( "duration", va( "%u", (int)ceilf( (double)svs.demo.duration / 1000.0 ) ) );
		SV_SetDemoMetaKeyValue( "mapname", sv.mapname );
		SV_SetDemoMetaKeyValue( "matchscore", sv.configstrings[CS_MATCHSCORE] );

		SNAP_WriteDemoMetaData( svs.demo.tempname, svs.demo.meta_data, svs.demo.meta_data_realsize );

		if( !MoveFile( &temp, svs.demo.tempname, svs.demo.filename, MoveFile_DoReplace ) ) {
			Com_Printf( "Error: Failed to rename the server demo file\n" );
		}
	}

	svs.demo.localtime = 0;
	svs.demo.basetime = svs.demo.duration = 0;

	SNAP_FreeClientFrames( &svs.demo.client );

	FREE( sys_allocator, svs.demo.filename );
	svs.demo.filename = NULL;
	FREE( sys_allocator, svs.demo.tempname );
	svs.demo.tempname = NULL;
}

void SV_Demo_Stop_f() {
	SV_Demo_Stop( false, atoi( Cmd_Argv( 1 ) ) != 0 );
}

void SV_Demo_Cancel_f() {
	SV_Demo_Stop( true, atoi( Cmd_Argv( 1 ) ) != 0 );
}

static void GetServerDemos( DynamicArray< char * > * demos ) {
	ListDirHandle scan = BeginListDir( sys_allocator, SV_DEMO_DIR );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		if( dir || FileExtension( name ) != APP_DEMO_EXTENSION_STR )
			continue;

		demos->add( CopyString( sys_allocator, name ) );
	}

	std::sort( demos->begin(), demos->end(), SortCStringsComparator );
}

void SV_Demo_Purge_f() {
	if( !is_dedicated_server ) {
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();

	DynamicArray< char * > demos( &temp );
	GetServerDemos( &demos );
	defer {
		for( char * demo : demos ) {
			FREE( sys_allocator, demo );
		}
	};

	DynamicArray< const char * > auto_demos( &temp );
	for( const char * demo : demos ) {
		// terrible, but isdigit( '\0' ) is false so this is safe
		const char * _auto = strstr( demo, "_auto" );
		if( _auto != NULL && isdigit( _auto[ 5 ] ) && isdigit( _auto[ 6 ] ) && isdigit( _auto[ 7 ] ) && isdigit( _auto[ 8 ] ) ) {
			auto_demos.add( demo );
		}
	}

	int keep = g_autorecord_maxdemos->integer;
	if( keep >= auto_demos.size() ) {
		return;
	}

	size_t to_remove = auto_demos.size() - keep;
	for( size_t i = 0; i < to_remove; i++ ) {
		DynamicString path( &temp, "{}/{}", SV_DEMO_DIR, auto_demos[ i ] );
		if( RemoveFile( &temp, path.c_str() ) ) {
			Com_GGPrint( "Removed old autorecord demo: {}", path );
		}
		else {
			Com_GGPrint( "Couldn't remove old autorecord demo: {}", path );
		}
	}
}

void SV_DemoList_f( client_t *client ) {
	if( client->state < CS_SPAWNED ) {
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();

	DynamicArray< char * > demos( &temp );
	GetServerDemos( &demos );
	defer {
		for( char * demo : demos ) {
			FREE( sys_allocator, demo );
		}
	};

	DynamicString output( &temp, "pr \"Available demos:\n" );

	size_t start = demos.size() - Min2( demos.size(), size_t( 10 ) );

	for( size_t i = start; i < demos.size(); i++ ) {
		output.append( "{}: {}\n", i + 1, demos[ i ] );
	}

	output += "\"";

	SV_AddGameCommand( client, output.c_str() );
}

void SV_DemoGet_f( client_t *client ) {
	if( client->state < CS_SPAWNED ) {
		return;
	}

	if( Cmd_Argc() != 2 ) {
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();

	DynamicArray< char * > demos( &temp );
	GetServerDemos( &demos );
	defer {
		for( char * demo : demos ) {
			FREE( sys_allocator, demo );
		}
	};

	Span< const char > arg = MakeSpan( Cmd_Argv( 1 ) );
	int id;
	if( !TrySpanToInt( arg, &id ) || id <= 0 || id > demos.size() ) {
		SV_AddGameCommand( client, "demoget" );
		return;
	}

	SV_AddGameCommand( client, temp( "demoget \"{}/{}\"", SV_DEMO_DIR, demos[ id - 1 ] ) );
}

bool SV_IsDemoDownloadRequest( const char * request ) {
	return StartsWith( request, SV_DEMO_DIR ) && FileExtension( request ) == APP_DEMO_EXTENSION_STR;
}
