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

#include "qcommon/qcommon.h"
#include "qcommon/csprng.h"
#include "qcommon/fpe.h"
#include "qcommon/fs.h"
#include "qcommon/maplist.h"
#include "qcommon/threads.h"
#include "qcommon/time.h"
#include "client/keys.h"

#include <errno.h>
#include <setjmp.h>

static bool quitting;
static Time disable_tracy_start_time;

static jmp_buf abortframe;     // an ERR_DROP occured, exit the entire frame

Cvar *timescale;

static Cvar *logconsole = NULL;
static Cvar *logconsole_append;
static Cvar *logconsole_flush;
static Cvar *logconsole_timestamp;

static Mutex *com_print_mutex;

static FILE * log_file = NULL;

static server_state_t server_state = ss_dead;
static connstate_t client_state = CA_UNINITIALIZED;

static constexpr size_t MAX_PRINTMSG = 3072;

static void Com_CloseConsoleLog( bool lock, bool shutdown ) {
	if( shutdown ) {
		lock = true;
	}

	if( lock ) {
		Lock( com_print_mutex );
	}

	if( log_file != NULL ) {
		fclose( log_file );
		log_file = NULL;
	}

	if( shutdown ) {
		logconsole = NULL;
	}

	if( lock ) {
		Unlock( com_print_mutex );
	}
}

static void Com_ReopenConsoleLog() {
	char errmsg[MAX_PRINTMSG] = { 0 };

	Lock( com_print_mutex );

	Com_CloseConsoleLog( false, false );

	if( !StrEqual( logconsole->value, "" ) ) {
		OpenFileMode mode = logconsole_append && logconsole_append->integer ? OpenFile_AppendExisting : OpenFile_WriteOverwrite;
		log_file = OpenFile( sys_allocator, logconsole->value, mode );
		if( log_file == NULL ) {
			snprintf( errmsg, sizeof( errmsg ), "Couldn't open log file: %s (%s)\n", logconsole->value, strerror( errno ) );
		}
	}

	Unlock( com_print_mutex );

	if( errmsg[0] ) {
		Com_Printf( "%s", errmsg );
	}
}

static RGB8 Get256Color( u8 c ) {
	if( c >= 232 ) {
		u8 greyscale = 8 + 10 * ( c - 6 * 6 * 6 - 16 );
		return RGB8( greyscale, greyscale, greyscale );
	}

	u8 ri = ( ( c - 16 ) / 36 ) % 6;
	u8 gi = ( ( c - 16 ) /  6 ) % 6;
	u8 bi = ( ( c - 16 )      ) % 6;

	return RGB8( 55 + ri * 40, 55 + gi * 40, 55 + bi * 40 );
}

static float ColorDistance( RGB8 a, RGB8 b ) {
	float dr = float( a.r ) - float( b.r );
	float dg = float( a.g ) - float( b.g );
	float db = float( a.b ) - float( b.b );
	return dr * dr + dg * dg + db * db;
}

static int Nearest256Color( RGB8 c ) {
	u8 cube = 16 +
		36 * ( ( u32( c.r ) + 25 ) / 51 ) +
		6 * ( ( u32( c.g ) + 25 ) / 51 ) +
		( ( u32( c.b ) + 25 ) / 51 );

	float average = ( float( c.r ) + float( c.g ) + float( c.b ) ) / 3.0f;
	u8 greyscale = 232 + 24 * Unlerp01( 8.0f, average, 238.0f );

	if( ColorDistance( Get256Color( cube ), c ) < ColorDistance( Get256Color( greyscale ), c ) )
		return cube;
	return greyscale;
}

static void PrintStdout( const char * str ) {
	const char * end = str + strlen( str );

	const char * p = str;
	const char * print_from = str;
	while( p < end ) {
		if( p[ 0 ] == '\033' && p[ 1 ] && p[ 2 ] && p[ 3 ] && p[ 4 ] ) {
			const u8 * u = ( const u8 * ) p;
			printf( "\033[38;5;%dm", Nearest256Color( RGB8( u[ 1 ], u[ 2 ], u[ 3 ] ) ) );
			fwrite( print_from, 1, p - print_from, stdout );
			p += 5;
			print_from = p;
			continue;
		}
		p++;
	}

	printf( "%s\033[0m", print_from );
}

/*
* Com_Printf
*
* Both client and server can use this, and it will output
* to the apropriate place.
*/
void Com_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[MAX_PRINTMSG];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Lock( com_print_mutex );
	defer { Unlock( com_print_mutex ); };

	PrintStdout( msg );

	Con_Print( MakeSpan( msg ) );

	TracyCMessage( msg, strlen( msg ) );

	if( log_file != NULL ) {
		if( logconsole_timestamp && logconsole_timestamp->integer ) {
			char timestamp[MAX_PRINTMSG];
			FormatCurrentTime( timestamp, sizeof( timestamp ), "%Y-%m-%dT%H:%M:%SZ " );
			WritePartialFile( log_file, timestamp, strlen( timestamp ) );
		}
		WritePartialFile( log_file, msg, strlen( msg ) );
		if( logconsole_flush && logconsole_flush->integer ) {
			fflush( log_file );
		}
	}
}

/*
 * Quit when run on the server, disconnect when run on the client
 */
void Com_Error( const char *format, ... ) {
	va_list argptr;
	char msg[ MAX_PRINTMSG ];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Com_Printf( "********************\nERROR: %s\n********************\n", msg );
	SV_ShutdownGame( "Server crashed", false );
	CL_Disconnect( msg );

	longjmp( abortframe, -1 );
}

void Com_DeferQuit() {
	quitting = true;
}

server_state_t Com_ServerState() {
	return server_state;
}

void Com_SetServerState( server_state_t state ) {
	server_state = state;
}

connstate_t Com_ClientState() {
	return client_state;
}

void Com_SetClientState( connstate_t state ) {
	client_state = state;
}

//============================================================================

void Qcommon_Init( int argc, char ** argv ) {
	TracyZoneScoped;

	quitting = false;

	if( !is_public_build ) {
		EnableFPE();
	}

	Sys_Init();

	com_print_mutex = NewMutex();

	InitTime();

	// Now() doesn't start from zero in debug builds so make our own zero
	disable_tracy_start_time = Now();

	InitFS();
	Cmd_Init();
	Cvar_Init();
	InitKeys(); // need to be able to bind keys before running configs

	if( !is_dedicated_server ) {
		ExecDefaultCfg();
		Cmd_Execute( sys_allocator, "exec config.cfg" );
	}
	else {
		Cmd_Execute( sys_allocator, "config dedicated_autoexec.cfg" );
	}

	Span< const char * > args = Span< const char * >( ( const char ** ) argv, checked_cast< size_t >( argc ) );
	Cmd_ExecuteEarlyCommands( args );

	AddCommand( "quit", []( const Tokenized & args ) { Com_DeferQuit(); } );

	timescale = NewCvar( "timescale", "1.0", CvarFlag_Cheat );
	logconsole = NewCvar( "logconsole", is_dedicated_server ? "server.log"_sp : ""_sp, CvarFlag_Archive );
	logconsole_append = NewCvar( "logconsole_append", "1", CvarFlag_Archive );
	logconsole_flush = NewCvar( "logconsole_flush", "0", CvarFlag_Archive );
	logconsole_timestamp = NewCvar( "logconsole_timestamp", "0", CvarFlag_Archive );

	NewCvar( "gamename", APPLICATION_NOSPACES, CvarFlag_ServerInfo | CvarFlag_ReadOnly );

	InitCSPRNG();

	InitNetworking();
	Netchan_Init();

	InitMapList();

	SV_Init();
	CL_Init();

	Cmd_ExecuteLateCommands( args );
}

bool Qcommon_Frame( unsigned int realMsec ) {
	if( IFDEF( TRACY_ENABLE ) && tracy_is_active ) {
		if( Now() - disable_tracy_start_time > Minutes( 10 ) ) {
			Com_Printf( "Disabled Tracy to conserve memory\n" );
			tracy_is_active = false;
		}
	}

	TracyZoneScoped;

	static unsigned int gameMsec;

	if( setjmp( abortframe ) ) {
		return true; // an ERR_DROP was thrown
	}

	if( logconsole && logconsole->modified ) {
		logconsole->modified = false;
		Com_ReopenConsoleLog();
	}

	if( timescale->number >= 0 ) {
		static float extratime = 0.0f;
		gameMsec = extratime + (float)realMsec * timescale->number;
		extratime = ( extratime + (float)realMsec * timescale->number ) - (float)gameMsec;
	} else {
		gameMsec = realMsec;
	}

	if( is_dedicated_server ) {
		// execute commands from stdin
		while( true ) {
			const char * s = Sys_ConsoleInput();
			if( s == NULL )
				break;
			Cmd_ExecuteLine( sys_allocator, MakeSpan( s ), true );
		}
	}

	SV_Frame( realMsec, gameMsec );
	CL_Frame( realMsec, gameMsec );

	return !quitting;
}

void Qcommon_Shutdown() {
	TracyZoneScoped;

	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();

	ShutdownMapList();

	Netchan_Shutdown();
	ShutdownNetworking();
	ShutdownKeys();

	RemoveCommand( "quit" );

	Com_CloseConsoleLog( true, true );

	ShutdownFS();

	ShutdownCSPRNG();

	Cvar_Shutdown();
	Cmd_Shutdown();

	DeleteMutex( com_print_mutex );
}
