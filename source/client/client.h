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
#pragma once

#include "qcommon/qcommon.h"
#include "qcommon/types.h"
#include "qcommon/rng.h"
#include "cgame/cg_public.h"
#include "gameshared/gs_public.h"

#include "client/vid.h"
#include "client/ui.h"
#include "client/keys.h"
#include "client/maps.h"
#include "client/console.h"
#include "client/sound.h"
#include "client/renderer/types.h"

struct ImFont;
struct snapshot_t;

constexpr RGBA8 rgba8_diesel_yellow = RGBA8( 255, 204, 38, 255 );

//=============================================================================

#define MAX_TIMEDELTAS_BACKUP 8
#define MASK_TIMEDELTAS_BACKUP ( MAX_TIMEDELTAS_BACKUP - 1 )

//
// the client_state_t structure is wiped completely at every
// server map change
//
struct client_state_t {
	int timeoutcount;

	int cmdNum;                     // current cmd
	usercmd_t cmds[CMD_BACKUP];     // each mesage will send several old cmds
	int cmd_time[CMD_BACKUP];       // time sent, for calculating pings

	WeaponType weaponSwitch;

	int receivedSnapNum;
	int pendingSnapNum;
	int currentSnapNum;
	int previousSnapNum;
	snapshot_t snapShots[CMD_BACKUP];

	const Map * map;
	CollisionModel * cms;

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	Vec3 prevviewangles;
	Vec3 viewangles;

	int serverTimeDeltas[MAX_TIMEDELTAS_BACKUP];
	int newServerTimeDelta;         // the time difference with the server time, or at least our best guess about it
	int serverTimeDelta;            // the time difference with the server time, or at least our best guess about it
	int64_t serverTime;             // the best match we can guess about current time in the server
	unsigned int snapFrameTime;

	//
	// server state information
	//
	int servercount;        // server identification for prespawns
	int playernum;

	char configstrings[MAX_CONFIGSTRINGS][MAX_CONFIGSTRING_CHARS];
};

extern client_state_t cl;

/*
==================================================================

the client_static_t structure is persistent through an arbitrary number
of server connections

==================================================================
*/

struct cl_demo_t {
	char *name;

	bool recording;
	bool waiting;       // don't record until a non-delta message is received
	bool playing;
	bool paused;        // A boolean to test if demo is paused -- PLX

	int file;
	char *filename;

	time_t localtime;       // time of day of demo recording
	int64_t time;           // milliseconds passed since the start of the demo
	int64_t duration, basetime;

	bool play_jump;
	bool play_jump_latched;
	int64_t play_jump_time;
	bool play_ignore_next_frametime;

	bool pause_on_stop;

	char meta_data[SNAP_MAX_DEMO_META_DATA_SIZE];
	size_t meta_data_realsize;
};

struct client_static_t {
	ArenaAllocator frame_arena;

	RNG rng;

	connstate_t state;          // only set through CL_SetClientState
	keydest_t key_dest;
	keydest_t old_key_dest;

	int64_t monotonicTime; // starts at 0 when the game is launched, increases forever

	int64_t framecount;
	int64_t realtime;               // always increasing, no clamping, etc
	int64_t gametime;               // always increasing, no clamping, etc
	int frametime;                  // milliseconds since last frame
	int realFrameTime;

	socket_t socket_loopback;
	socket_t socket_udp;
	socket_t socket_udp6;

	// screen rendering information
	bool cgameActive;

	// connection information
	char *servername;               // name of server from original connect
	socket_type_t servertype;       // socket type used to connect to the server
	netadr_t serveraddress;         // address of that server
	int64_t connect_time;               // for connection retransmits
	int connect_count;

	socket_t *socket;               // socket used by current connection
	bool reliable;

	netadr_t rconaddress;       // address where we are sending rcon messages, to ignore other print packets

	netadr_t httpaddress;           // address of the builtin HTTP server
	char *httpbaseurl;              // http://<httpaddress>/

	bool rejected;          // these are used when the server rejects our connection
	int rejecttype;
	char rejectmessage[80];

	netchan_t netchan;

	int challenge;              // from the server to use for connecting

	// demo recording info must be here, so it isn't cleared on level change
	cl_demo_t demo;

	const Material * whiteTexture;

	// these are our reliable messages that go to the server
	int64_t reliableSequence;          // the last one we put in the list to be sent
	int64_t reliableSent;              // the last one we sent to the server
	int64_t reliableAcknowledge;       // the last one the server has executed
	char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// reliable messages received from server
	int64_t lastExecutedServerCommand;          // last server command grabbed or executed with CL_GetServerCommand

	// ucmds buffer
	int64_t ucmdAcknowledged;
	int64_t ucmdHead;
	int64_t ucmdSent;

	// times when we got/sent last valid packets from/to server
	int64_t lastPacketSentTime;
	int64_t lastPacketReceivedTime;

	char session[MAX_INFO_VALUE];

	ImFont * huge_font;
	ImFont * large_font;
	ImFont * big_font;
	ImFont * medium_font;
	ImFont * console_font;
};

extern client_static_t cls;
extern gs_state_t client_gs;

//=============================================================================

//
// cvars
//
extern cvar_t *cl_shownet;

extern cvar_t *cl_extrapolationTime;
extern cvar_t *cl_extrapolate;

// wsw : debug netcode
extern cvar_t *cl_debug_serverCmd;
extern cvar_t *cl_debug_timeDelta;

extern cvar_t *cl_devtools;

// delta from this if not from a previous frame
extern SyncEntityState cl_baselines[MAX_EDICTS];

//=============================================================================

//
// cl_main.c
//
void CL_Init( void );
void CL_Quit( void );

void CL_UpdateClientCommandsToServer( msg_t *msg );
void CL_AddReliableCommand( const char *cmd );
void CL_Netchan_Transmit( msg_t *msg );
void CL_SendMessagesToServer( bool sendNow );
void CL_RestartTimeDeltas( int newTimeDelta );
void CL_AdjustServerTime( unsigned int gamemsec );

void CL_SetKeyDest( keydest_t key_dest );
void CL_SetOldKeyDest( keydest_t key_dest );
void CL_ResetServerCount( void );
void CL_SetClientState( connstate_t state );
void CL_ClearState( void );
void CL_ReadPackets( void );
void CL_Disconnect_f( void );

void CL_Reconnect_f( void );
void CL_FinishConnect();
void CL_ServerReconnect_f( void );
void CL_Changing_f( void );
void CL_Precache_f( void );
void CL_ForwardToServer_f( void );
void CL_ServerDisconnect_f( void );

void CL_ForceVsync( bool force );

size_t CL_GetBaseServerURL( char *buffer, size_t buffer_size );

//
// cl_game.c
//
void CL_GameModule_Init( void );
void CL_GameModule_Reset( void );
void CL_GameModule_Shutdown( void );
void CL_GameModule_ConfigString( int number, const char *value );
void CL_GameModule_EscapeKey( void );
bool CL_GameModule_NewSnapshot( int pendingSnapshot );
void CL_GameModule_RenderView();
void CL_GameModule_GetEntitySpatilization( int entnum, Vec3 * origin, Vec3 * velocity );
void CL_GameModule_InputFrame( int frameTime );
unsigned CL_GameModule_GetButtonBits( void );
void CL_GameModule_AddViewAngles( Vec3 * viewAngles );
void CL_GameModule_AddMovement( Vec3 * movement );
void CL_GameModule_MouseMove( int frameTime, Vec2 m );

//
// cl_serverlist.c
//
void CL_ParseGetInfoResponse( const socket_t *socket, const netadr_t *address, msg_t *msg );
void CL_ParseGetStatusResponse( const socket_t *socket, const netadr_t *address, msg_t *msg );
void CL_QueryGetInfoMessage_f( void );
void CL_QueryGetStatusMessage_f( void );
void CL_ParseStatusMessage( const socket_t *socket, const netadr_t *address, msg_t *msg );
void CL_ParseGetServersResponse( const socket_t *socket, const netadr_t *address, msg_t *msg, bool extended );
void CL_GetServers_f( void );
void CL_PingServer_f( void );
void CL_ServerListFrame( void );
void CL_InitServerList( void );
void CL_ShutDownServerList( void );

//
// cl_input.c
//
void CL_InitInput( void );
void CL_UserInputFrame( int realMsec );
void CL_WriteUcmdsToMessage( msg_t *msg );

//
// cl_demo.c
//
void CL_WriteDemoMessage( msg_t *msg );
void CL_DemoCompleted( void );
void CL_PlayDemo_f( void );
void CL_ReadDemoPackets( void );
void CL_LatchedDemoJump( void );
void CL_Stop_f( void );
void CL_Record_f( void );
void CL_PauseDemo_f( void );
void CL_DemoJump_f( void );
size_t CL_ReadDemoMetaData( const char *demopath, char *meta_data, size_t meta_data_size );
const char **CL_DemoComplete( const char *partial );
#define CL_SetDemoMetaKeyValue( k,v ) cls.demo.meta_data_realsize = SNAP_SetDemoMetaKeyValue( cls.demo.meta_data, sizeof( cls.demo.meta_data ), cls.demo.meta_data_realsize, k, v )

//
// cl_parse.c
//
void CL_ParseServerMessage( msg_t *msg );
#define SHOWNET( msg,s ) _SHOWNET( msg,s,cl_shownet->integer );

bool CL_DownloadFile( const char * filename, bool save_to_disk );
bool CL_IsDownloading();
void CL_CancelDownload();
void CL_ReconnectAfterDownload();
void CL_CheckDownloadTimeout();

//
// cl_screen.c
//
void SCR_InitScreen( void );
void SCR_UpdateScreen( void );
void SCR_DebugGraph( float value, float r, float g, float b );
void SCR_RegisterConsoleMedia( void );

void CL_AddNetgraph( void );

//
// cl_imgui
//
void CL_InitImGui();
void CL_ShutdownImGui();
void CL_ImGuiBeginFrame();
void CL_ImGuiEndFrame();

//
// snap_read
//
void SNAP_ParseBaseline( msg_t *msg, SyncEntityState *baselines );
snapshot_t *SNAP_ParseFrame( msg_t *msg, snapshot_t *lastFrame, snapshot_t *backup, SyncEntityState *baselines, int showNet );
