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
#include "qcommon/cmodel.h"
#include "qcommon/csprng.h"
#include "qcommon/fpe.h"
#include "qcommon/fs.h"
#include "qcommon/maplist.h"
#include "qcommon/threads.h"
#include "qcommon/version.h"

#include <errno.h>
#include <setjmp.h>

static bool com_quit;

static jmp_buf abortframe;     // an ERR_DROP occured, exit the entire frame

Cvar *developer;
Cvar *timescale;

static Cvar *logconsole = NULL;
static Cvar *logconsole_append;
static Cvar *logconsole_flush;
static Cvar *logconsole_timestamp;

static Mutex *com_print_mutex;

static FILE * log_file = NULL;

static server_state_t server_state = ss_dead;
static connstate_t client_state = CA_UNINITIALIZED;
static bool demo_playing = false;

/*
============================================================================

CLIENT / SERVER interactions

============================================================================
*/

static int rd_target;
static char *rd_buffer;
static int rd_buffersize;
static void ( *rd_flush )( int target, const char *buffer, const void *extra );
static const void *rd_extra;

void Com_BeginRedirect( int target, char *buffer, int buffersize,
						void ( *flush )( int, const char*, const void* ), const void *extra ) {
	if( !target || !buffer || !buffersize || !flush ) {
		return;
	}

	Lock( com_print_mutex );

	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;
	rd_extra = extra;

	*rd_buffer = 0;

	Unlock( com_print_mutex );
}

void Com_EndRedirect() {
	Lock( com_print_mutex );

	rd_flush( rd_target, rd_buffer, rd_extra );

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
	rd_extra = NULL;

	Unlock( com_print_mutex );
}

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

	if( logconsole && logconsole->value && logconsole->value[0] ) {
		const char * mode = logconsole_append && logconsole_append->integer ? "a" : "w";
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

	if( rd_target ) {
		if( (int)( strlen( msg ) + strlen( rd_buffer ) ) > ( rd_buffersize - 1 ) ) {
			rd_flush( rd_target, rd_buffer, rd_extra );
			*rd_buffer = 0;
		}
		Q_strncatz( rd_buffer, msg, rd_buffersize );
		return;
	}

	// also echo to debugging console
	Sys_ConsoleOutput( msg );

	Con_Print( msg );

	TracyMessage( msg, strlen( msg ) );

	if( log_file != NULL ) {
		if( logconsole_timestamp && logconsole_timestamp->integer ) {
			char timestamp[MAX_PRINTMSG];
			Sys_FormatCurrentTime( timestamp, sizeof( timestamp ), "%Y-%m-%dT%H:%M:%SZ " );
			WritePartialFile( log_file, timestamp, strlen( timestamp ) );
		}
		WritePartialFile( log_file, msg, strlen( msg ) );
		if( logconsole_flush && logconsole_flush->integer ) {
			fflush( log_file );
		}
	}
}

/*
* Com_DPrintf
*
* A Com_Printf that only shows up if the "developer" cvar is set
*/
void Com_DPrintf( const char *format, ... ) {
	va_list argptr;
	char msg[MAX_PRINTMSG];

	if( !developer || !developer->integer ) {
		return; // don't confuse non-developers with techie stuff...

	}
	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Com_Printf( "%s", msg );
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
	com_quit = true;
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

bool Com_DemoPlaying() {
	return demo_playing;
}

void Com_SetDemoPlaying( bool state ) {
	demo_playing = state;
}

//============================================================================

void Key_Init();
void Key_Shutdown();

void Qcommon_Init( int argc, char ** argv ) {
	ZoneScoped;

	if( !is_public_build ) {
		EnableFPE();
	}

	Sys_Init();

	com_print_mutex = NewMutex();

	InitFS();
	FS_Init();

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Cmd_Init();
	Cvar_PreInit();
	Key_Init(); // need to be able to bind keys before running configs

	developer = NewCvar( "developer", "0", 0 );

	if( !is_dedicated_server ) {
		ExecDefaultCfg();
		Cbuf_ExecuteLine( "exec config.cfg" );
		Cbuf_ExecuteLine( "exec autoexec.cfg" );
	}
	else {
		Cbuf_ExecuteLine( "config dedicated_autoexec.cfg" );
	}

	Cbuf_AddEarlyCommands( argc, argv );

	Cvar_Init();

	AddCommand( "quit", Com_DeferQuit );

	timescale = NewCvar( "timescale", "1.0", CvarFlag_Cheat );
	if( is_dedicated_server ) {
		logconsole = NewCvar( "logconsole", "server.log", CvarFlag_Archive );
	}
	else {
		logconsole = NewCvar( "logconsole", "", CvarFlag_Archive );
	}
	logconsole_append = NewCvar( "logconsole_append", "1", CvarFlag_Archive );
	logconsole_flush =  NewCvar( "logconsole_flush", "0", CvarFlag_Archive );
	logconsole_timestamp =  NewCvar( "logconsole_timestamp", "0", CvarFlag_Archive );

	NewCvar( "gamename", APPLICATION_NOSPACES, CvarFlag_ServerInfo | CvarFlag_ReadOnly );

	InitCSPRNG();

	NET_Init();
	Netchan_Init();

	InitMapList();

	SV_Init();
	CL_Init();

	Cbuf_AddLateCommands( argc, argv );
}

<<<<<<< HEAD
bool Qcommon_Frame( Time dt ) {
	ZoneScoped;

=======
void Qcommon_Frame( u64 real_dt ) {
	ZoneScoped;

	if( com_quit ) {
		Com_Quit();
	}

>>>>>>> origin/ggtime
	if( setjmp( abortframe ) ) {
		return true; // an ERR_DROP was thrown
	}

	if( logconsole && logconsole->modified ) {
		logconsole->modified = false;
		Com_ReopenConsoleLog();
	}

<<<<<<< HEAD
	if( timescale->number < 0 ) {
		Cvar_Set( "timescale", "1.0" );
		Com_Printf( "Timescale < 0, resetting.\n" );
	}

	Time scaled_dt = dt * timescale->number;
=======
	wswcurl_perform();

	FS_Frame();
>>>>>>> origin/ggtime

	if( is_dedicated_server ) {
		while( true ) {
			const char * s = Sys_ConsoleInput();
			if( s == NULL )
				break;
			Cbuf_ExecuteLine( s );
		}
	}

<<<<<<< HEAD
	SV_Frame( scaled_dt, dt );
	CL_Frame( scaled_dt, dt );

	return !com_quit;
}

void Qcommon_Shutdown() {
	ZoneScoped;

	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();

	ShutdownMapList();

=======
	if( timescale->value < 0 ) {
		Cvar_ForceSet( "timescale", "1.0" );
	}

	u64 dt = double( real_dt ) * timescale->value;
	SV_Frame( real_dt, dt );
	CL_Frame( real_dt, dt );
}

void Qcommon_Shutdown() {
	CM_Shutdown();
>>>>>>> origin/ggtime
	Netchan_Shutdown();
	NET_Shutdown();
	Key_Shutdown();

	RemoveCommand( "quit" );

	Com_CloseConsoleLog( true, true );

	FS_Shutdown();
	ShutdownFS();

	ShutdownCSPRNG();

	Cvar_Shutdown();
	Cmd_Shutdown();

	DeleteMutex( com_print_mutex );
}
