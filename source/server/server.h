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
#include "qcommon/rng.h"
#include "game/g_local.h"

// some commands are only valid before the server has finished
// initializing (precache commands, static sounds / objects, etc)

struct client_t;
struct ginfo_t {
	edict_t * edicts;
	client_t * clients;

	int num_edicts;         // current number, <= max_edicts
	int max_edicts;
	int max_clients;        // <= sv_maxclients, <= max_edicts
};

struct server_t {
	server_state_t state;       // precache commands are only valid during load

	int64_t nextSnapTime;              // always sv.framenum * svc.snapFrameTime msec
	int64_t framenum;

	char mapname[128];               // map name

	SyncEntityState baselines[MAX_EDICTS];

	//
	// global variables shared between game and server
	//
	ginfo_t gi;
};

#define EDICT_NUM( n ) ( sv.gi.edicts + ( n ) )
#define NUM_FOR_EDICT( e ) ( ( e ) - sv.gi.edicts )

struct client_snapshot_t {
	bool allentities;
	bool multipov;
	int numplayers;
	SyncPlayerState ps[ MAX_CLIENTS ];
	int num_entities;
	int first_entity;                   // into the circular sv.client_entities[]
	int64_t sentTimeStamp;         // time at what this frame snap was sent to the clients
	unsigned int UcmdExecuted;
	SyncGameState gameState;
};

struct game_command_t {
	int64_t framenum;
	char command[MAX_STRING_CHARS];
};

#define LATENCY_COUNTS  16

struct client_t {
	sv_client_state_t state;

	bool mv;                        // send multiview data to the client

	char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	int64_t reliableSequence;      // last added reliable message, not necesarily sent or acknowledged yet
	int64_t reliableAcknowledge;   // last acknowledged reliable message
	int64_t reliableSent;          // last sent reliable message, not necesarily acknowledged yet

	game_command_t gameCommands[MAX_RELIABLE_COMMANDS];
	int64_t gameCommandCurrent;             // position in the gameCommands table

	int64_t clientCommandExecuted; // last client-command we received

	int64_t UcmdTime;
	int64_t UcmdExecuted;          // last client-command we executed
	int64_t UcmdReceived;          // last client-command we received
	UserCommand ucmds[CMD_BACKUP];        // each message will send several old cmds

	int64_t lastPacketSentTime;    // time when we sent the last message to this client
	int64_t lastPacketReceivedTime; // time when we received the last message from this client
	int64_t lastconnect;

	int64_t lastframe;                  // used for delta compression etc.
	bool nodelta;               // send one non delta compressed frame trough
	int64_t nodelta_frame;              // when we get confirmation of this frame, the non-delta frame is through
	int64_t lastSentFrameNum;  // for knowing which was last frame we sent

	int frame_latency[LATENCY_COUNTS];
	int ping;
	edict_t * edict;                 // EDICT_NUM(clientnum+1)

	client_snapshot_t snapShots[UPDATE_BACKUP]; // updates can be delta'd from here

	int challenge;                  // challenge of this user, randomly generated

	netchan_t netchan;
};

// a client can leave the server in one of four ways:
// dropping properly by quiting or disconnecting
// timing out if no valid messages are received for timeout.value seconds
// getting kicked off by the server operator
// a program error, like an overflowed reliable buffer

//=============================================================================

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define MAX_CHALLENGES  1024

// MAX_SNAP_ENTITIES is the guess of what we consider maximum amount of entities
// to be sent to a client into a snap. It's used for finding size of the backup storage
#define MAX_SNAP_ENTITIES 64

struct challenge_t {
	NetAddress adr;
	int challenge;
	int64_t time;
};

struct client_entities_t {
	unsigned num_entities;      // maxclients->integer*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	unsigned next_entities;     // next client_entity to use
	SyncEntityState * entities; // [num_entities]
};

struct server_static_t {
	bool initialized;               // sv_init has completed
	int64_t realtime;               // real world time - always increasing, no clamping, etc
	int64_t gametime;               // game world time - always increasing, no clamping, etc

	ArenaAllocator frame_arena;

	RNG rng;

	Socket socket;

	int spawncount;                     // incremented each server start
	                                    // used to check late spawns

	client_t * clients;
	client_entities_t client_entities;

	challenge_t challenges[MAX_CHALLENGES]; // to prevent invalid IPs from connecting
};

struct server_constant_t {
	Time nextHeartbeat;
	unsigned int snapFrameTime;     // msecs between server packets
	unsigned int gameFrameTime;     // msecs between game code executions
	Time lastMasterResolve;
};

//=============================================================================

// shared message buffer to be used for occasional messages
extern msg_t tmpMessage;
extern uint8_t tmpMessageData[MAX_MSGLEN];

extern server_constant_t svc;              // constant server info (trully persistant since sv_init)
extern server_static_t svs;                // persistant server info
extern server_t sv;                 // local server

extern Cvar * sv_port;

extern Cvar * sv_downloadurl;

extern Cvar * sv_hostname;
extern Cvar * sv_maxclients;

extern Cvar * sv_showChallenge;
extern Cvar * sv_showInfoQueries;

extern Cvar * sv_public;         // should heartbeats be sent

// wsw : debug netcode
extern Cvar * sv_debug_serverCmd;

extern Cvar * sv_demodir;

//===========================================================

//
// sv_main.c
//
void SV_WriteClientdataToMessage( client_t * client, msg_t * msg );

void SV_InitOperatorCommands();
void SV_ShutdownOperatorCommands();

void SV_MasterHeartbeat();

int SVC_FakeConnect( char * userinfo );

//
// sv_oob.c
//
void SV_ConnectionlessPacket( const NetAddress & address, msg_t * msg );
void SV_InitMaster();
void SV_UpdateMaster();

//
// sv_init.c
//
void SV_Map( const char * level, bool devmap );

//
// sv_send.c
//
bool SV_Netchan_Transmit( netchan_t * netchan, msg_t * msg );
void SV_AddServerCommand( client_t * client, const char *cmd );
void SV_SendServerCommand( client_t * cl, const char * format, ... );
void SV_AddGameCommand( client_t * client, const char * cmd );
void SV_AddReliableCommandsToMessage( client_t * client, msg_t * msg );
bool SV_SendClientsFragments();
void SV_InitClientMessage( client_t * client, msg_t * msg, uint8_t *data, size_t size );
bool SV_SendMessageToClient( client_t * client, msg_t * msg );
void SV_ResetClientFrameCounters();

void SV_SendClientMessages();

[[gnu::format( printf, 1, 2 )]] void SV_BroadcastCommand( const char *format, ... );

//
// sv_client.c
//
void SV_ParseClientMessage( client_t * client, msg_t * msg );
bool SV_ClientConnect( const NetAddress & address, client_t * client, char * userinfo,
	u64 session_id, int challenge, bool fakeClient );

[[gnu::format( printf, 2, 3 )]] void SV_DropClient( client_t * drop, const char * format, ... );

void SV_ExecuteClientThinks( int clientNum );
void SV_ClientResetCommandBuffers( client_t * client );
void SV_ClientCloseDownload( client_t * client );

//
// sv_ccmds.c
//
void SV_Status_f();

//
// sv_ents.c
//
void SV_WriteFrameSnapToClient( client_t * client, msg_t * msg );
void SV_BuildClientFrameSnap( client_t * client );

//
// sv_game.c
//
void PF_DropClient( edict_t * ent, const char * message );
int PF_GetClientState( int numClient );
void PF_GameCmd( edict_t * ent, const char * cmd );
void SV_LocateEntities( edict_t * edicts, int num_edicts, int max_edicts );

//
// sv_demos.c
//
void SV_Demo_WriteSnap();
void SV_Demo_AddServerCommand( const char * command );
void SV_Demo_Start_f( const Tokenized & args );
void SV_Demo_Record( Span< const char > name );
void SV_Demo_Stop( bool silent );
void SV_DeleteOldDemos();

void SV_DemoList_f( edict_t * ent, msg_t args );
void SV_DemoGetUrl_f( edict_t * ent, msg_t args );

//
// sv_web.c
//
void InitWebServer();
void ShutdownWebServer();

//
// snap_write
//
void SNAP_WriteFrameSnapToClient( const ginfo_t * gi, client_t * client, msg_t * msg, int64_t frameNum, int64_t gameTime,
	const SyncEntityState * baselines, const client_entities_t * client_entities );

void SNAP_BuildClientFrameSnap( const ginfo_t * gi, int64_t frameNum, int64_t timeStamp,
	client_t * client,
	const SyncGameState * gameState, client_entities_t * client_entities );
