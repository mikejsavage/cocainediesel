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
#include "client/audio/types.h"
#include "client/renderer/types.h"

struct ImFont;
struct snapshot_t;

constexpr RGBA8 rgba8_diesel_yellow = RGBA8( 255, 204, 38, 255 );
constexpr RGBA8 rgba8_diesel_green = RGBA8( 44, 209, 89, 255 ); //yolo

//=============================================================================

#define MAX_TIMEDELTAS_BACKUP 8

//
// the client_state_t structure is wiped completely at every
// server map change
//
struct client_state_t {
	int timeoutcount;

	int cmdNum;                     // current cmd
	UserCommand cmds[CMD_BACKUP];     // each mesage will send several old cmds
	int cmd_time[CMD_BACKUP];       // time sent, for calculating pings

	WeaponType weaponSwitch;

	int receivedSnapNum;
	int pendingSnapNum;
	int currentSnapNum;
	int previousSnapNum;
	snapshot_t snapShots[CMD_BACKUP];

	const Map * map;

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	EulerDegrees2 prevviewangles;
	EulerDegrees2 viewangles;

	int serverTimeDeltas[MAX_TIMEDELTAS_BACKUP];
	int newServerTimeDelta;         // the time difference with the server time, or at least our best guess about it
	int serverTimeDelta;            // the time difference with the server time, or at least our best guess about it
	int64_t serverTime;             // the best match we can guess about current time in the server
	unsigned int snapFrameTime;

	//
	// server state information
	//
	int servercount;        // server identification for prespawns
	int max_clients;
	int playernum;
};

extern client_state_t cl;

/*
==================================================================

the client_static_t structure is persistent through an arbitrary number
of server connections

==================================================================
*/

struct client_static_t {
	ArenaAllocator frame_arena;

	RNG rng;

	u64 session_id;
	connstate_t state;          // only set through CL_SetClientState

	Time monotonicTime; // starts at 0 when the game is launched, increases forever

	Time shadertoy_time; // starts at 0 when the game is launched, periodically gets reset

	int64_t framecount;
	int64_t realtime;               // always increasing, no clamping, etc
	int64_t gametime;               // always increasing, no clamping, etc
	int frametime;                  // milliseconds since last frame
	int realFrameTime;

	Socket socket;

	// screen rendering information
	bool cgameActive;

	// connection information
	NetAddress serveraddress; // address of that server
	Optional< Time > connect_time; // for connection retransmits
	int connect_count;

	char * server_name;
	char * download_url;              // http://<httpaddress>/

	bool rejected;          // these are used when the server rejects our connection
	char rejectmessage[80];

	netchan_t netchan;

	int challenge;              // from the server to use for connecting

	const Material * white_material;

	// these are our reliable messages that go to the server
	size_t reliableSequence;          // the last one we put in the list to be sent
	size_t reliableSent;              // the last one we sent to the server
	size_t reliableAcknowledge;       // the last one the server has executed

	struct ReliableCommand {
		ClientCommandType command;
		u8 args_buf[ MAX_STRING_CHARS ];
		msg_t args;
	};

	ReliableCommand reliableCommands[ MAX_RELIABLE_COMMANDS ];

	// reliable messages received from server
	int64_t lastExecutedServerCommand;          // last server command grabbed or executed with CL_GetServerCommand

	// ucmds buffer
	int64_t ucmdAcknowledged;
	int64_t ucmdHead;
	int64_t ucmdSent;

	// times when we got/sent last valid packets from/to server
	int64_t lastPacketSentTime;
	int64_t lastPacketReceivedTime;

	ImFont * huge_font;
	ImFont * large_font;
	ImFont * big_font;
	ImFont * medium_font;
	ImFont * medium_italic_font;
	ImFont * large_italic_font;
	ImFont * big_italic_font;
	ImFont * console_font;
	ImFont * idi_nahui_font;
};

extern client_static_t cls;
extern gs_state_t client_gs;

//=============================================================================

//
// cvars
//
extern Cvar *cl_shownet;

extern Cvar *cl_extrapolationTime;
extern Cvar *cl_extrapolate;

// wsw : debug netcode
extern Cvar *cl_debug_serverCmd;
extern Cvar *cl_debug_timeDelta;

extern Cvar *cl_devtools;

// delta from this if not from a previous frame
extern SyncEntityState cl_baselines[MAX_EDICTS];

//=============================================================================

//
// cl_main.c
//
void CL_Init();

msg_t * CL_AddReliableCommand( ClientCommandType command );
void CL_Netchan_Transmit( msg_t * msg );
void CL_SendMessagesToServer( bool sendNow );
void CL_RestartTimeDeltas( int newTimeDelta );
void CL_AdjustServerTime( unsigned int gamemsec );

void CL_SetClientState( connstate_t state );
void CL_ClearState();
void CL_ReadPackets();

void CL_Connect( const NetAddress & address );
void CL_Reconnect_f();
void CL_FinishConnect();
void CL_ServerReconnect_f();
void CL_Changing_f();
void CL_Precache_f( const Tokenized & args );
void CL_ServerDisconnect_f( const Tokenized & args );

void CL_ForceVsync( bool force );

//
// cl_game.c
//
void CL_GetUserCmd( int frame, UserCommand *cmd );
int CL_GetCurrentUserCmdNum();
void CL_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent );

void CL_GameModule_Init();
void CL_GameModule_Reset();
void CL_GameModule_Shutdown();
void CL_GameModule_EscapeKey();
bool CL_GameModule_NewSnapshot( int pendingSnapshot );
void CL_GameModule_RenderView();
void CL_GameModule_InputFrame( int frameTime );
UserCommandButton CL_GameModule_GetButtonBits();
UserCommandButton CL_GameModule_GetButtonDownEdges();
void CL_GameModule_AddViewAngles( Vec3 * viewAngles );
void CL_GameModule_AddMovement( Vec3 * movement );
void CL_GameModule_MouseMove( Vec2 m );

//
// cl_input.c
//
void CL_InitInput();
void CL_UserInputFrame( int realMsec );
void CL_WriteUcmdsToMessage( msg_t *msg );

//
// cl_demo.c
//
void CL_WriteDemoMessage( msg_t msg, size_t offset );
void CL_DemoBaseline( const snapshot_t * snap );
void CL_DemoCompleted();
void CL_PlayDemo_f( const Tokenized & args );
void CL_YoloDemo_f( const Tokenized & args );
void CL_ReadDemoPackets();
void CL_LatchedDemoJump();
void CL_Record_f( const Tokenized & args );
bool CL_DemoPaused();
bool CL_DemoSeeking();
bool CL_YoloDemo();
void CL_StopRecording( bool silent );
void CL_PauseDemo_f();
void CL_DemoJump_f( const Tokenized & args );

//
// cl_parse.c
//
void CL_ParseServerMessage( msg_t *msg );
#define SHOWNET( msg,s ) _SHOWNET( msg,s,cl_shownet->integer );

using DownloadCompleteCallback = void ( * )( const char * filename, Span< const u8 > data );

bool CL_DownloadFile( Span< const char > filename, DownloadCompleteCallback cb );
bool CL_IsDownloading();
void CL_CancelDownload();

//
// cl_screen.c
//
void SCR_InitScreen();
void SCR_UpdateScreen();

void CL_AddNetgraph();

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
snapshot_t *SNAP_ParseFrame( msg_t *msg, const snapshot_t *lastFrame, snapshot_t *backup, SyncEntityState *baselines, int showNet );
