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
#include "qcommon/glob.h"
#include "qcommon/maplist.h"
#include "qcommon/threads.h"
#include "qcommon/version.h"

#include <errno.h>
#include <setjmp.h>

#define MAX_NUM_ARGVS   50

static bool commands_intialized = false;

static int com_argc;
static char *com_argv[MAX_NUM_ARGVS + 1];

static bool com_quit;

static jmp_buf abortframe;     // an ERR_DROP occured, exit the entire frame

cvar_t *developer;
cvar_t *timescale;
cvar_t *versioncvar;

static cvar_t *logconsole = NULL;
static cvar_t *logconsole_append;
static cvar_t *logconsole_flush;
static cvar_t *logconsole_timestamp;

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

	if( logconsole && logconsole->string && logconsole->string[0] ) {
		const char * mode = logconsole_append && logconsole_append->integer ? "a" : "w";
		log_file = OpenFile( sys_allocator, logconsole->string, mode );
		if( log_file == NULL ) {
			snprintf( errmsg, sizeof( errmsg ), "Couldn't open log file: %s (%s)\n", logconsole->string, strerror( errno ) );
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

void Com_Quit() {
	ZoneScoped;

	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();
	ShutdownMapList();
	Qcommon_Shutdown();
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

int COM_Argc() {
	return com_argc;
}

const char *COM_Argv( int arg ) {
	if( arg < 0 || arg >= com_argc || !com_argv[arg] ) {
		return "";
	}
	return com_argv[arg];
}

void COM_ClearArgv( int arg ) {
	if( arg < 0 || arg >= com_argc || !com_argv[arg] ) {
		return;
	}
	com_argv[arg][0] = '\0';
}

void COM_InitArgv( int argc, char **argv ) {
	int i;

	if( argc > MAX_NUM_ARGVS ) {
		Fatal( "argc > MAX_NUM_ARGVS" );
	}
	com_argc = argc;
	for( i = 0; i < argc; i++ ) {
		if( !argv[i] || strlen( argv[i] ) >= MAX_TOKEN_CHARS ) {
			com_argv[i][0] = '\0';
		} else {
			com_argv[i] = argv[i];
		}
	}
}

int Com_GlobMatch( const char *pattern, const char *text, const bool casecmp ) {
	return glob_match( pattern, text, casecmp );
}

char *_TempCopyString( const char *str, const char *filename, int fileline ) {
	return _Mem_CopyString( tempMemPool, str, filename, fileline );
}

//============================================================================

void Key_Init();
void Key_Shutdown();

void Qcommon_Init( int argc, char **argv ) {
	ZoneScoped;

	if( !is_public_build ) {
		EnableFPE();
	}

	Sys_Init();

	com_print_mutex = NewMutex();

	// initialize memory manager
	Memory_Init();

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	COM_InitArgv( argc, argv );

	Cbuf_Init();

	// initialize cmd/cvar tries
	Cmd_PreInit();
	Cvar_PreInit();

	// create basic commands and cvars
	Cmd_Init();
	Cvar_Init();

	Key_Init();

	// we need to add the early commands twice, because
	// a basepath or cdpath needs to be set before execing
	// config files, but we want other parms to override
	// the settings of the config files
	Cbuf_AddEarlyCommands( false );
	Cbuf_Execute();

	developer = Cvar_Get( "developer", "0", 0 );

	InitFS();
	FS_Init();

	if( !is_dedicated_server ) {
		ExecDefaultCfg();
		Cbuf_AddText( "exec config.cfg\n" );
		Cbuf_AddText( "exec autoexec.cfg\n" );
	}
	else {
		Cbuf_AddText( "config dedicated_autoexec.cfg\n" );
	}

	Cbuf_AddEarlyCommands( true );
	Cbuf_Execute();

	//
	// init commands and vars
	//
	Memory_InitCommands();

	Cmd_AddCommand( "quit", Com_DeferQuit );

	timescale =     Cvar_Get( "timescale", "1.0", CVAR_CHEAT );
	if( is_dedicated_server ) {
		logconsole =        Cvar_Get( "logconsole", "server.log", CVAR_ARCHIVE );
	} else {
		logconsole =        Cvar_Get( "logconsole", "", CVAR_ARCHIVE );
	}
	logconsole_append = Cvar_Get( "logconsole_append", "1", CVAR_ARCHIVE );
	logconsole_flush =  Cvar_Get( "logconsole_flush", "0", CVAR_ARCHIVE );
	logconsole_timestamp =  Cvar_Get( "logconsole_timestamp", "0", CVAR_ARCHIVE );

	Cvar_Get( "gamename", APPLICATION_NOSPACES, CVAR_SERVERINFO | CVAR_READONLY );
	versioncvar = Cvar_Get( "version", APP_VERSION " " ARCH " " OSNAME, CVAR_SERVERINFO | CVAR_READONLY );

	InitCSPRNG();

	NET_Init();
	Netchan_Init();

	InitMapList();

	SV_Init();
	CL_Init();

	Cbuf_AddLateCommands();

	Cbuf_Execute();
}

bool Qcommon_Frame( unsigned int realMsec ) {
	ZoneScoped;

	static unsigned int gameMsec;

	if( setjmp( abortframe ) ) {
		return true; // an ERR_DROP was thrown
	}

	if( logconsole && logconsole->modified ) {
		logconsole->modified = false;
		Com_ReopenConsoleLog();
	}

	if( timescale->value >= 0 ) {
		static float extratime = 0.0f;
		gameMsec = extratime + (float)realMsec * timescale->value;
		extratime = ( extratime + (float)realMsec * timescale->value ) - (float)gameMsec;
	} else {
		gameMsec = realMsec;
	}

	if( is_dedicated_server ) {
		const char * s;
		do {
			s = Sys_ConsoleInput();
			if( s ) {
				Cbuf_AddText( va( "%s\n", s ) );
			}
		} while( s );

		Cbuf_Execute();
	}

	SV_Frame( realMsec, gameMsec );
	CL_Frame( realMsec, gameMsec );

	return !com_quit;
}

void Qcommon_Shutdown() {
	Netchan_Shutdown();
	NET_Shutdown();
	Key_Shutdown();

	Cmd_RemoveCommand( "quit" );

	Memory_ShutdownCommands();

	Com_CloseConsoleLog( true, true );

	FS_Shutdown();
	ShutdownFS();

	ShutdownCSPRNG();

	Cvar_Shutdown();
	Cmd_Shutdown();
	Cbuf_Shutdown();
	Memory_Shutdown();

	DeleteMutex( com_print_mutex );
}
