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

#include "server/server.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/version.h"
#include "gameshared/demo.h"

#include "nanosort/nanosort.hpp"

static RecordDemoContext record_demo_context = { };
static client_t demo_client;
static s64 demo_gametime;
static time_t demo_utc_time;

static const char * GetDemoDir( TempAllocator * temp ) {
	return StrEqual( sv_demodir->value, "" ) ? "demos" : ( *temp )( "demos/{}", sv_demodir->value );
}

void SV_Demo_WriteSnap() {
	TracyZoneScoped;

	if( record_demo_context.file == NULL ) {
		return;
	}

	int i;
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_SPAWNED && svs.clients[i].edict &&
			!( svs.clients[i].edict->s.svflags & SVF_NOCLIENT ) ) {
			break;
		}
	}
	if( i == sv_maxclients->integer ) { // FIXME
		Com_Printf( "No players left, stopping server side demo recording\n" );
		SV_Demo_Stop( true );
		return;
	}

	uint8_t msg_buffer[MAX_MSGLEN];
	msg_t msg = NewMSGWriter( msg_buffer, sizeof( msg_buffer ) );

	SV_BuildClientFrameSnap( &demo_client );

	SV_WriteFrameSnapToClient( &demo_client, &msg );

	SV_AddReliableCommandsToMessage( &demo_client, &msg );

	WriteDemoMessage( &record_demo_context, msg );

	demo_client.lastframe = sv.framenum; // FIXME: is this needed?
}

void SV_Demo_AddServerCommand( const char * command ) {
	if( record_demo_context.file == NULL ) {
		return;
	}

	SV_AddServerCommand( &demo_client, command );
}

static void SV_Demo_InitClient() {
	memset( &demo_client, 0, sizeof( demo_client ) );

	demo_client.mv = true;

	demo_client.reliableAcknowledge = 0;
	demo_client.reliableSequence = 0;
	demo_client.reliableSent = 0;
	memset( demo_client.reliableCommands, 0, sizeof( demo_client.reliableCommands ) );

	demo_client.lastframe = sv.framenum - 1;
	demo_client.nodelta = false;
}

void SV_Demo_Record( Span< const char > name ) {
	if( record_demo_context.temp_file != NULL ) {
		Com_Printf( "Already recording\n" );
		return;
	}

	if( sv.state != ss_game ) {
		Com_Printf( "Must be in a level to record\n" );
		return;
	}

	bool any_players = false;
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_SPAWNED && svs.clients[i].edict && !( svs.clients[i].edict->s.svflags & SVF_NOCLIENT ) ) {
			any_players = true;
			break;
		}
	}
	if( !any_players ) {
		Com_Printf( "No players in game, can't record a demo\n" );
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();
	char * filename = temp( "{}/{}.cddemo", GetDemoDir( &temp ), name );
	COM_SanitizeFilePath( filename );

	Com_Printf( "Recording server demo: %s\n", filename );

	bool recording = StartRecordingDemo( &temp, &record_demo_context, filename, svs.spawncount, svc.snapFrameTime, server_gs.maxclients, sv.baselines );
	if( !recording )
		return;

	SV_Demo_InitClient();

	demo_gametime = svs.gametime;
	demo_utc_time = checked_cast< s64 >( time( NULL ) );

	// write one nodelta frame
	demo_client.nodelta = true;
	SV_Demo_WriteSnap();
	demo_client.nodelta = false;
}

void SV_Demo_Start_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "Usage: serverrecord <demoname>\n" );
		return;
	}

	SV_Demo_Record( args.tokens[ 1 ] );
}

void SV_Demo_Stop( bool silent ) {
	if( record_demo_context.file == NULL ) {
		if( !silent ) {
			Com_Printf( "Not recording a demo.\n" );
		}
		return;
	}

	Com_Printf( "Saving demo: %s\n", record_demo_context.filename );

	TempAllocator temp = svs.frame_arena.temp();

	DemoMetadata metadata = { };
	metadata.metadata_version = DEMO_METADATA_VERSION;
	metadata.game_version = MakeSpan( CopyString( &temp, APP_VERSION ) );
	metadata.server = MakeSpan( sv_hostname->value );
	metadata.map = MakeSpan( sv.mapname );
	metadata.utc_time = demo_utc_time;
	metadata.duration_seconds = ( svs.gametime - demo_gametime ) / 1000;
	metadata.decompressed_size = record_demo_context.decompressed_size;

	StopRecordingDemo( &temp, &record_demo_context, metadata );
	record_demo_context = { };
}

static Span< char * > GetServerDemos( TempAllocator * temp ) {
	NonRAIIDynamicArray< char * > demos( temp );

	ListDirHandle scan = BeginListDir( sys_allocator, GetDemoDir( temp ) );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		if( dir || FileExtension( name ) != APP_DEMO_EXTENSION_STR )
			continue;

		demos.add( CopyString( sys_allocator, name ) );
	}

	nanosort( demos.begin(), demos.end(), SortCStringsComparator );

	return demos.span();
}

static bool IsDigit( char c ) {
	return c >= '0' && c <= '9';
}

void SV_DeleteOldDemos() {
	if( !is_dedicated_server ) {
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();
	Span< char * > demos = GetServerDemos( &temp );
	defer {
		for( char * demo : demos ) {
			Free( sys_allocator, demo );
		}
	};

	DynamicArray< const char * > auto_demos( &temp );
	for( const char * demo : demos ) {
		// terrible, but isdigit( '\0' ) is false so this is safe
		const char * _auto = strstr( demo, "_auto" );
		if( _auto != NULL && IsDigit( _auto[ 5 ] ) && IsDigit( _auto[ 6 ] ) && IsDigit( _auto[ 7 ] ) && IsDigit( _auto[ 8 ] ) ) {
			auto_demos.add( demo );
		}
	}

	size_t keep = g_autorecord_maxdemos->integer;
	if( keep >= auto_demos.size() ) {
		return;
	}

	size_t to_remove = auto_demos.size() - keep;
	for( size_t i = 0; i < to_remove; i++ ) {
		const char * path = temp( "{}/{}", GetDemoDir( &temp ), auto_demos[ i ] );
		if( RemoveFile( &temp, path ) ) {
			Com_GGPrint( "Removed old autorecord demo: {}", path );
		}
		else {
			Com_GGPrint( "Couldn't remove old autorecord demo: {}", path );
		}
	}
}

void SV_DemoList_f( edict_t * ent, msg_t args ) {
	TempAllocator temp = svs.frame_arena.temp();
	Span< char * > demos = GetServerDemos( &temp );
	defer {
		for( char * demo : demos ) {
			Free( sys_allocator, demo );
		}
	};

	DynamicString output( &temp, "pr \"Available demos:\n" );

	size_t start = demos.n - Min2( demos.n, size_t( 10 ) );

	for( size_t i = start; i < demos.n; i++ ) {
		output.append( "{}: {}\n", i + 1, demos[ i ] );
	}

	output += "\"";

	PF_GameCmd( ent, output.c_str() );
}

void SV_DemoGetUrl_f( edict_t * ent, msg_t msg ) {
	TempAllocator temp = svs.frame_arena.temp();

	Tokenized args = Tokenize( &temp, MakeSpan( MSG_ReadString( &msg ) ) );
	if( args.tokens.n != 2 ) {
		return;
	}

	Span< char * > demos = GetServerDemos( &temp );
	defer {
		for( char * demo : demos ) {
			Free( sys_allocator, demo );
		}
	};

	u64 id;
	if( !TrySpanToU64( args.tokens[ 1 ], &id ) || id > demos.n ) {
		PF_GameCmd( ent, "pr \"demoget <id from demolist>\"\n" );
		return;
	}

	PF_GameCmd( ent, temp( "downloaddemo \"{}/{}\"", GetDemoDir( &temp ), demos[ id - 1 ] ) );
}
