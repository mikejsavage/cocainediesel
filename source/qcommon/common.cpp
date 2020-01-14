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
// common.c -- misc functions used in client and server
#include "qcommon.h"
#include "qcommon/glob.h"
#include "qcommon/csprng.h"
#include <setjmp.h>
#include "version.h"
#include "wswcurl.h"

#define MAX_NUM_ARGVS   50

static bool commands_intialized = false;

static int com_argc;
static char *com_argv[MAX_NUM_ARGVS + 1];
static char com_errormsg[MAX_PRINTMSG];

static bool com_quit;

static jmp_buf abortframe;     // an ERR_DROP occured, exit the entire frame

cvar_t *developer;
cvar_t *timescale;
cvar_t *versioncvar;

static cvar_t *logconsole = NULL;
static cvar_t *logconsole_append;
static cvar_t *logconsole_flush;
static cvar_t *logconsole_timestamp;
static cvar_t *com_showtrace;

static qmutex_t *com_print_mutex;

static int log_file = 0;

static int server_state = CA_UNINITIALIZED;
static int client_state = CA_UNINITIALIZED;
static bool demo_playing = false;

struct cmodel_state_s *server_cms = NULL;
unsigned server_map_checksum = 0;

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

	QMutex_Lock( com_print_mutex );

	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;
	rd_extra = extra;

	*rd_buffer = 0;
}

void Com_EndRedirect( void ) {
	rd_flush( rd_target, rd_buffer, rd_extra );

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
	rd_extra = NULL;

	QMutex_Unlock( com_print_mutex );
}

void Com_DeferConsoleLogReopen( void ) {
	if( logconsole != NULL ) {
		logconsole->modified = true;
	}
}

static void Com_CloseConsoleLog( bool lock, bool shutdown ) {
	if( shutdown ) {
		lock = true;
	}

	if( lock ) {
		QMutex_Lock( com_print_mutex );
	}

	if( log_file ) {
		FS_FCloseFile( log_file );
		log_file = 0;
	}

	if( shutdown ) {
		logconsole = NULL;
	}

	if( lock ) {
		QMutex_Unlock( com_print_mutex );
	}
}

static void Com_ReopenConsoleLog( void ) {
	char errmsg[MAX_PRINTMSG] = { 0 };

	QMutex_Lock( com_print_mutex );

	Com_CloseConsoleLog( false, false );

	if( logconsole && logconsole->string && logconsole->string[0] ) {
		size_t name_size;
		char *name;

		name_size = strlen( logconsole->string ) + strlen( ".log" ) + 1;
		name = ( char* )Mem_TempMalloc( name_size );
		Q_strncpyz( name, logconsole->string, name_size );
		COM_DefaultExtension( name, ".log", name_size );

		if( FS_FOpenFile( name, &log_file, ( logconsole_append && logconsole_append->integer ? FS_APPEND : FS_WRITE ) ) == -1 ) {
			log_file = 0;
			snprintf( errmsg, MAX_PRINTMSG, "Couldn't open: %s\n", name );
		}

		Mem_TempFree( name );
	}

	QMutex_Unlock( com_print_mutex );

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

	QMutex_Lock( com_print_mutex );

	if( rd_target ) {
		if( (int)( strlen( msg ) + strlen( rd_buffer ) ) > ( rd_buffersize - 1 ) ) {
			rd_flush( rd_target, rd_buffer, rd_extra );
			*rd_buffer = 0;
		}
		Q_strncatz( rd_buffer, msg, rd_buffersize );

		QMutex_Unlock( com_print_mutex );
		return;
	}

	// also echo to debugging console
	Sys_ConsoleOutput( msg );

	Con_Print( msg );

	TracyMessage( msg, strlen( msg ) );

	if( log_file ) {
		if( logconsole_timestamp && logconsole_timestamp->integer ) {
			char timestamp[MAX_PRINTMSG];
			Sys_FormatTime( timestamp, sizeof( timestamp ), "%Y-%m-%dT%H:%M:%SZ " );
			FS_Printf( log_file, "%s", timestamp );
		}
		FS_Printf( log_file, "%s", msg );
		if( logconsole_flush && logconsole_flush->integer ) {
			FS_Flush( log_file ); // force it to save every time
		}
	}

	QMutex_Unlock( com_print_mutex );
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
* Com_Error
*
* Both client and server can use this, and it will
* do the apropriate things.
*/
void Com_Error( com_error_code_t code, const char *format, ... ) {
	va_list argptr;
	char *msg = com_errormsg;
	const size_t sizeof_msg = sizeof( com_errormsg );
	static bool recursive = false;

	if( recursive ) {
		Com_Printf( "recursive error after: %s", msg ); // wsw : jal : log it
		Sys_Error( "recursive error after: %s", msg );
	}
	recursive = true;

	va_start( argptr, format );
	vsnprintf( msg, sizeof_msg, format, argptr );
	va_end( argptr );

	if( code == ERR_DROP ) {
		Com_Printf( "********************\nERROR: %s\n********************\n", msg );
		SV_ShutdownGame( va( "Server crashed: %s\n", msg ), false );
		CL_Disconnect( msg );
		recursive = false;
		longjmp( abortframe, -1 );
	} else {
		Com_Printf( "********************\nERROR: %s\n********************\n", msg );
		SV_Shutdown( va( "Server fatal crashed: %s\n", msg ) );
		CL_Shutdown();
	}

	if( log_file ) {
		FS_FCloseFile( log_file );
		log_file = 0;
	}

	Sys_Error( "%s", msg );
}

/*
* Com_DeferQuit
*/
void Com_DeferQuit( void ) {
	com_quit = true;
}

/*
* Com_Quit
*
* Both client and server can use this, and it will
* do the apropriate things.
*/
void Com_Quit( void ) {
	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();
	ShutdownMapList();

	Sys_Quit();
}


/*
* Com_ServerState
*/
int Com_ServerState( void ) {
	return server_state;
}

/*
* Com_SetServerState
*/
void Com_SetServerState( int state ) {
	server_state = state;
}

/*
* Com_ServerCM
*/
struct cmodel_state_s *Com_ServerCM( unsigned *checksum ) {
	*checksum = server_map_checksum;
	CM_AddReference( server_cms );
	return server_cms;
}

/*
* Com_SetServerCM
*/
void Com_SetServerCM( struct cmodel_state_s *cms, unsigned checksum ) {
	server_cms = cms;
	server_map_checksum = checksum;
}

int Com_ClientState( void ) {
	return client_state;
}

void Com_SetClientState( int state ) {
	client_state = state;
}

bool Com_DemoPlaying( void ) {
	return demo_playing;
}

void Com_SetDemoPlaying( bool state ) {
	demo_playing = state;
}

//============================================================================

/*
* COM_CheckParm
*
* Returns the position (1 to argc-1) in the program's argument list
* where the given parameter apears, or 0 if not present
*/
int COM_CheckParm( char *parm ) {
	int i;

	for( i = 1; i < com_argc; i++ ) {
		if( !strcmp( parm, com_argv[i] ) ) {
			return i;
		}
	}

	return 0;
}

int COM_Argc( void ) {
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


/*
* COM_InitArgv
*/
void COM_InitArgv( int argc, char **argv ) {
	int i;

	if( argc > MAX_NUM_ARGVS ) {
		Com_Error( ERR_FATAL, "argc > MAX_NUM_ARGVS" );
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

/*
* COM_AddParm
*
* Adds the given string at the end of the current argument list
*/
void COM_AddParm( char *parm ) {
	if( com_argc == MAX_NUM_ARGVS ) {
		Com_Error( ERR_FATAL, "COM_AddParm: MAX_NUM_ARGVS" );
	}
	com_argv[com_argc++] = parm;
}

int Com_GlobMatch( const char *pattern, const char *text, const bool casecmp ) {
	return glob_match( pattern, text, casecmp );
}

char *_ZoneCopyString( const char *str, const char *filename, int fileline ) {
	return _Mem_CopyString( zoneMemPool, str, filename, fileline );
}

char *_TempCopyString( const char *str, const char *filename, int fileline ) {
	return _Mem_CopyString( tempMemPool, str, filename, fileline );
}

void Info_Print( char *s ) {
	char key[512];
	char value[512];
	char *o;
	int l;

	if( *s == '\\' ) {
		s++;
	}
	while( *s ) {
		o = key;
		while( *s && *s != '\\' )
			*o++ = *s++;

		l = o - key;
		if( l < 20 ) {
			memset( o, ' ', 20 - l );
			key[20] = 0;
		} else {
			*o = 0;
		}
		Com_Printf( "%s", key );

		if( !*s ) {
			Com_Printf( "MISSING VALUE\n" );
			return;
		}

		o = value;
		s++;
		while( *s && *s != '\\' )
			*o++ = *s++;
		*o = 0;

		if( *s ) {
			s++;
		}
		Com_Printf( "%s\n", value );
	}
}

//============================================================================

void Key_Init( void );
void Key_Shutdown( void );

/*
* Q_malloc
*
* Just like malloc(), but die if allocation fails
*/
void *Q_malloc( size_t size ) {
	void *buf = malloc( size );

	if( !buf ) {
		Sys_Error( "Q_malloc: failed on allocation of %" PRIuPTR " bytes.\n", (uintptr_t)size );
	}

	return buf;
}

/*
* Q_realloc
*
* Just like realloc(), but die if reallocation fails
*/
void *Q_realloc( void *buf, size_t newsize ) {
	void *newbuf = realloc( buf, newsize );

	if( !newbuf && newsize ) {
		Sys_Error( "Q_realloc: failed on allocation of %" PRIuPTR " bytes.\n", (uintptr_t)newsize );
	}

	return newbuf;
}

/*
* Q_free
*/
void Q_free( void *buf ) {
	free( buf );
}

/*
* Qcommon_InitCommands
*/
void Qcommon_InitCommands( void ) {
	assert( !commands_intialized );

	if( is_dedicated_server ) {
		Cmd_AddCommand( "quit", Com_Quit );
	}

	commands_intialized = true;
}

/*
* Qcommon_ShutdownCommands
*/
void Qcommon_ShutdownCommands( void ) {
	if( !commands_intialized ) {
		return;
	}

	if( is_dedicated_server ) {
		Cmd_RemoveCommand( "quit" );
	}

	commands_intialized = false;
}

/*
* Qcommon_Init
*/
void Qcommon_Init( int argc, char **argv ) {
	ZoneScoped;

	Sys_Init();

	if( setjmp( abortframe ) ) {
		Sys_Error( "Error during initialization: %s", com_errormsg );
	}

	com_print_mutex = QMutex_Create();

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

	wswcurl_init();

	Key_Init();

	// we need to add the early commands twice, because
	// a basepath or cdpath needs to be set before execing
	// config files, but we want other parms to override
	// the settings of the config files
	Cbuf_AddEarlyCommands( false );
	Cbuf_Execute();

	developer =     Cvar_Get( "developer", "0", 0 );

	FS_Init();

	if( !is_dedicated_server ) {
		Cbuf_AddText( "exec default.cfg\n" );
		Cbuf_AddText( "exec config.cfg\n" );
		Cbuf_AddText( "exec autoexec.cfg\n" );
	}
	else {
		Cbuf_AddText( "exec dedicated_autoexec.cfg\n" );
	}

	Cbuf_AddEarlyCommands( true );
	Cbuf_Execute();

	//
	// init commands and vars
	//
	Memory_InitCommands();

	Qcommon_InitCommands();

	timescale =     Cvar_Get( "timescale", "1.0", CVAR_CHEAT );
	if( is_dedicated_server ) {
		logconsole =        Cvar_Get( "logconsole", "server.log", CVAR_ARCHIVE );
	} else {
		logconsole =        Cvar_Get( "logconsole", "", CVAR_ARCHIVE );
	}
	logconsole_append = Cvar_Get( "logconsole_append", "1", CVAR_ARCHIVE );
	logconsole_flush =  Cvar_Get( "logconsole_flush", "0", CVAR_ARCHIVE );
	logconsole_timestamp =  Cvar_Get( "logconsole_timestamp", "0", CVAR_ARCHIVE );

	com_showtrace =     Cvar_Get( "com_showtrace", "0", 0 );

	Cvar_Get( "gamename", APPLICATION_NOSPACES, CVAR_SERVERINFO | CVAR_READONLY );
	versioncvar = Cvar_Get( "version", APP_VERSION " " ARCH " " OSNAME, CVAR_SERVERINFO | CVAR_READONLY );

	CSPRNG_Init();

	NET_Init();
	Netchan_Init();

	CM_Init();

	InitMapList();

	SV_Init();
	CL_Init();

	Cbuf_AddLateCommands();

	Cbuf_Execute();
}

/*
* Qcommon_Frame
*/
void Qcommon_Frame( unsigned int realMsec ) {
	ZoneScoped;

	static unsigned int gameMsec;

	if( com_quit ) {
		Com_Quit();
	}

	if( setjmp( abortframe ) ) {
		return; // an ERR_DROP was thrown
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

	if( com_showtrace->integer ) {
		Com_Printf( "%4i traces %4i brush traces %4i points\n",
					c_traces, c_brush_traces, c_pointcontents );
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

	wswcurl_perform();

	FS_Frame();

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
}

/*
* Qcommon_Shutdown
*/
void Qcommon_Shutdown( void ) {
	CM_Shutdown();
	Netchan_Shutdown();
	NET_Shutdown();
	Key_Shutdown();

	Qcommon_ShutdownCommands();
	Memory_ShutdownCommands();

	Com_CloseConsoleLog( true, true );

	FS_Shutdown();

	CSPRNG_Shutdown();

	wswcurl_cleanup();

	Cvar_Shutdown();
	Cmd_Shutdown();
	Cbuf_Shutdown();
	Memory_Shutdown();

	QMutex_Destroy( &com_print_mutex );
}
