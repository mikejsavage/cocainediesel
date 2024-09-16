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

#include "server/server.h"
#include "qcommon/version.h"
#include "qcommon/time.h"

//============================================================================
//
//		CLIENT
//
//============================================================================

void SV_ClientResetCommandBuffers( client_t *client ) {
	// reset the reliable commands buffer
	client->clientCommandExecuted = 0;
	client->reliableAcknowledge = 0;
	client->reliableSequence = 0;
	client->reliableSent = 0;
	memset( client->reliableCommands, 0, sizeof( client->reliableCommands ) );

	// reset the usercommands buffer(clc_move)
	client->UcmdTime = 0;
	client->UcmdExecuted = 0;
	client->UcmdReceived = 0;
	memset( client->ucmds, 0, sizeof( client->ucmds ) );

	// reset snapshots delta-compression
	client->lastframe = -1;
	client->lastSentFrameNum = 0;
}

bool SV_ClientConnect( const NetAddress & address, client_t * client, char * userinfo, u64 session_id, int challenge, bool fakeClient ) {
	int edictnum = ( client - svs.clients ) + 1;
	edict_t * ent = EDICT_NUM( edictnum );

	// get the game a chance to reject this connection or modify the userinfo
	if( !ClientConnect( ent, userinfo, address, fakeClient ) ) {
		return false;
	}

	// the connection is accepted, set up the client slot
	memset( client, 0, sizeof( *client ) );
	client->edict = ent;
	client->challenge = challenge; // save challenge for checksumming

	SV_ClientResetCommandBuffers( client );

	// reset timeouts
	client->lastPacketReceivedTime = svs.monotonic_time;
	client->lastconnect = svs.monotonic_time;

	// init the connection
	client->state = CS_CONNECTING;

	if( fakeClient ) {
		client->netchan.remoteAddress = NULL_ADDRESS;
	} else {
		Netchan_Setup( &client->netchan, address, session_id );
	}

	ClientUserinfoChanged( client->edict, userinfo );

	return true;
}

/*
* SV_DropClient
*
* Called when the player is totally leaving the server, either willingly
* or unwillingly.  This is NOT called if the entire server is quiting
* or crashing.
*/
void SV_DropClient( client_t *drop, const char *format, ... ) {
	va_list argptr;
	char *reason;
	char string[1024];

	if( format ) {
		va_start( argptr, format );
		vsnprintf( string, sizeof( string ), format, argptr );
		va_end( argptr );
		reason = string;
	} else {
		SafeStrCpy( string, "User disconnected", sizeof( string ) );
		reason = NULL;
	}

	// add the disconnect
	if( drop->edict && ( drop->edict->s.svflags & SVF_FAKECLIENT ) ) {
		ClientDisconnect( drop->edict, reason );
		SV_ClientResetCommandBuffers( drop ); // make sure everything is clean
	} else {
		SV_InitClientMessage( drop, &tmpMessage, NULL, 0 );
		SV_SendServerCommand( drop, "disconnect \"%s\"", string );
		SV_AddReliableCommandsToMessage( drop, &tmpMessage );

		SV_SendMessageToClient( drop, &tmpMessage );
		Netchan_PushAllFragments( svs.socket, &drop->netchan );

		if( drop->state >= CS_CONNECTED ) {
			// call the prog function for removing a client
			// this will remove the body, among other things
			ClientDisconnect( drop->edict, reason );
		}
	}

	drop->state = CS_ZOMBIE;    // become free in a few seconds
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/

/*
* SV_New_f
*
* Sends the first message from the server to a connected client.
* This will be sent on the initial connection and upon each server load.
*/
static void SV_New_f( client_t *client, msg_t args ) {
	// if in CS_AWAITING we have sent the response packet the new once already,
	// but client might have not got it so we send it again
	if( client->state >= CS_SPAWNED ) {
		Com_Printf( "New not valid -- already spawned\n" );
		return;
	}

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
	SV_InitClientMessage( client, &tmpMessage, NULL, 0 );

	// set up the entity for the client
	int playernum = client - svs.clients;
	edict_t * ent = EDICT_NUM( playernum + 1 );
	ent->s.number = playernum + 1;
	client->edict = ent;

	// send the serverdata
	MSG_WriteUint8( &tmpMessage, svc_serverdata );
	MSG_WriteUint32( &tmpMessage, APP_PROTOCOL_VERSION );
	MSG_WriteInt32( &tmpMessage, svs.spawncount );
	MSG_WriteInt16( &tmpMessage, (unsigned short)svc.snapFrameTime );
	MSG_WriteUint8( &tmpMessage, sv_maxclients->integer );
	MSG_WriteInt16( &tmpMessage, playernum );
	MSG_WriteString( &tmpMessage, sv_hostname->value );
	MSG_WriteString( &tmpMessage, sv_downloadurl->value );

	SV_ClientResetCommandBuffers( client );

	SV_SendMessageToClient( client, &tmpMessage );
	Netchan_PushAllFragments( svs.socket, &client->netchan );

	// don't let it send reliable commands until we get the first baseline request
	client->state = CS_CONNECTING;
}

static void SV_Baselines_f( client_t *client, msg_t args ) {
	if( client->state == CS_CONNECTING ) {
		client->state = CS_CONNECTED;
	}

	if( client->state != CS_CONNECTED ) {
		Com_Printf( "baselines not valid -- already spawned\n" );
		return;
	}

	// handle the case of a level changing while a client was connecting
	if( MSG_ReadInt32( &args ) != svs.spawncount ) {
		Com_Printf( "SV_Baselines_f from different level\n" );
		SV_New_f( client, args );
		return;
	}

	u32 start = MSG_ReadUint32( &args );

	SyncEntityState nullstate;
	memset( &nullstate, 0, sizeof( nullstate ) );

	// write a packet full of data
	SV_InitClientMessage( client, &tmpMessage, NULL, 0 );

	while( tmpMessage.cursize < FRAGMENT_SIZE * 3 && start < MAX_EDICTS ) {
		const SyncEntityState * base = &sv.baselines[start];
		if( base->number != 0 ) {
			MSG_WriteUint8( &tmpMessage, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &tmpMessage, &nullstate, base, true );
		}
		start++;
	}

	// send next command
	if( start == MAX_EDICTS ) {
		SV_SendServerCommand( client, "precache %i \"%s\"", svs.spawncount, sv.mapname );
	} else {
		SV_SendServerCommand( client, "baselines %i %i", svs.spawncount, start );
	}

	SV_AddReliableCommandsToMessage( client, &tmpMessage );
	SV_SendMessageToClient( client, &tmpMessage );
}

static void SV_Begin_f( client_t *client, msg_t args ) {
	// wsw : r1q2[start] : could be abused to respawn or cause spam/other mod-specific problems
	if( client->state != CS_CONNECTED ) {
		SV_DropClient( client, "Error: Begin while connected" );
		return;
	}
	// wsw : r1q2[end]

	// handle the case of a level changing while a client was connecting
	if( MSG_ReadInt32( &args ) != svs.spawncount ) {
		Com_Printf( "SV_Begin_f from different level\n" );
		SV_SendServerCommand( client, "changing" );
		SV_SendServerCommand( client, "reconnect" );
		return;
	}

	client->state = CS_SPAWNED;

	// call the game begin function
	ClientBegin( client->edict );
}

//=============================================================================

static void SV_Disconnect_f( client_t *client, msg_t args ) {
	SV_DropClient( client, NULL );
}

static void SV_UserinfoCommand_f( client_t * client, msg_t args ) {
	ClientUserinfoChanged( client->edict, MSG_ReadString( &args ) );
}

static void SV_NoDelta_f( client_t *client, msg_t args ) {
	client->nodelta = true;
	client->nodelta_frame = 0;
	client->lastframe = -1; // jal : I'm not sure about this. Seems like it's missing but...
}

static const struct {
	ClientCommandType command;
	void ( *callback )( client_t * client, msg_t args );
} ucmds[] = {
	{ ClientCommand_New, SV_New_f },
	{ ClientCommand_Baselines, SV_Baselines_f },
	{ ClientCommand_Begin, SV_Begin_f },
	{ ClientCommand_Disconnect, SV_Disconnect_f },
	{ ClientCommand_UserInfo, SV_UserinfoCommand_f },
	{ ClientCommand_NoDelta, SV_NoDelta_f },
};

static void SV_ExecuteUserCommand( client_t * client, ClientCommandType command, msg_t args ) {
	for( auto ucmd : ucmds ) {
		if( ucmd.command == command ) {
			ucmd.callback( client, args );
		}
	}

	if( client->state >= CS_SPAWNED && sv.state == ss_game ) {
		ClientCommand( client->edict, command, args );
	}
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

/*
* SV_FindNextUserCommand - Returns the next valid UserCommand in execution list
*/
UserCommand *SV_FindNextUserCommand( client_t *client ) {
	UserCommand *ucmd;
	int64_t higherTime = 0;
	unsigned int i;

	higherTime = svs.gametime; // ucmds can never have a higher timestamp than server time, unless cheating
	ucmd = NULL;
	if( client ) {
		for( i = client->UcmdExecuted + 1; i <= client->UcmdReceived; i++ ) {
			// skip backups if already executed
			if( client->UcmdTime >= client->ucmds[ i % ARRAY_COUNT( client->ucmds ) ].serverTimeStamp ) {
				continue;
			}

			if( ucmd == NULL || client->ucmds[ i % ARRAY_COUNT( client->ucmds ) ].serverTimeStamp < higherTime ) {
				higherTime = client->ucmds[ i % ARRAY_COUNT( client->ucmds ) ].serverTimeStamp;
				ucmd = &client->ucmds[ i % ARRAY_COUNT( client->ucmds ) ];
			}
		}
	}

	return ucmd;
}

/*
* SV_ExecuteClientThinks - Execute all pending UserCommand
*/
void SV_ExecuteClientThinks( int clientNum ) {
	unsigned int msec;
	int64_t minUcmdTime;
	int timeDelta;
	client_t *client;
	UserCommand *ucmd;

	if( clientNum >= sv_maxclients->integer || clientNum < 0 ) {
		return;
	}

	client = svs.clients + clientNum;
	if( client->state < CS_SPAWNED ) {
		return;
	}

	if( client->edict->s.svflags & SVF_FAKECLIENT ) {
		return;
	}

	// don't let client command time delay too far away in the past
	minUcmdTime = ( svs.gametime > 999 ) ? ( svs.gametime - 999 ) : 0;
	if( client->UcmdTime < minUcmdTime ) {
		client->UcmdTime = minUcmdTime;
	}

	while( ( ucmd = SV_FindNextUserCommand( client ) ) != NULL ) {
		msec = Clamp( int64_t( 1 ), ucmd->serverTimeStamp - client->UcmdTime, int64_t( 200 ) );
		ucmd->msec = msec;
		timeDelta = 0;
		if( client->lastframe > 0 ) {
			timeDelta = -(int)( svs.gametime - ucmd->serverTimeStamp );
		}

		ClientThink( client->edict, ucmd, timeDelta );

		client->UcmdTime = ucmd->serverTimeStamp;
	}

	// we did the entire update
	client->UcmdExecuted = client->UcmdReceived;
}

static void SV_ParseMoveCommand( client_t *client, msg_t *msg ) {
	int lastframe = MSG_ReadInt32( msg );

	// read the id of the first ucmd we will receive
	u32 ucmdHead = u32( MSG_ReadInt32( msg ) );
	// read the number of ucmds we will receive
	u32 ucmdCount = u32( MSG_ReadUint8( msg ) );

	if( ucmdCount >= ARRAY_COUNT( client->ucmds ) ) {
		SV_DropClient( client, "%s", "Error: Ucmd overflow" );
		return;
	}

	u32 ucmdFirst = ucmdHead > ucmdCount ? ucmdHead - ucmdCount : 0;
	client->UcmdReceived = ucmdHead < 1 ? 0 : ucmdHead - 1;

	// read the user commands
	for( u32 i = ucmdFirst; i < ucmdHead; i++ ) {
		if( i == ucmdFirst ) { // first one isn't delta compressed
			UserCommand nullcmd;
			memset( &nullcmd, 0, sizeof( nullcmd ) );
			// jalfixme: check for too old overflood
			MSG_ReadDeltaUsercmd( msg, &nullcmd, &client->ucmds[ i % ARRAY_COUNT( client->ucmds ) ] );
		} else {
			MSG_ReadDeltaUsercmd( msg, &client->ucmds[ ( i - 1 ) % ARRAY_COUNT( client->ucmds ) ], &client->ucmds[ i % ARRAY_COUNT( client->ucmds ) ] );
		}
	}

	if( client->state != CS_SPAWNED ) {
		client->lastframe = -1;
		return;
	}

	// calc ping
	if( lastframe != client->lastframe ) {
		client->lastframe = lastframe;
		if( client->lastframe > 0 ) {
			// FIXME: Medar: ping is in gametime, should be in realtime
			//client->frame_latency[client->lastframe%LATENCY_COUNTS] = svs.gametime - (client->frames[client->lastframe & UPDATE_MASK].sentTimeStamp;
			// this is more accurate. A little bit hackish, but more accurate
			client->frame_latency[ client->lastframe % ARRAY_COUNT( client->frame_latency ) ] = svs.gametime - ( client->ucmds[ client->UcmdReceived % ARRAY_COUNT( client->ucmds ) ].serverTimeStamp + svc.snapFrameTime );
		}
	}
}

void SV_ParseClientMessage( client_t *client, msg_t *msg ) {
	if( !msg ) {
		return;
	}

	// only allow one move command
	bool move_issued = false;

	while( msg->readcount < msg->cursize ) {
		int c = MSG_ReadUint8( msg );
		switch( c ) {
			default:
				Com_Printf( "SV_ParseClientMessage: unknown command char\n" );
				SV_DropClient( client, "%s", "Error: Unknown command char" );
				return;

			case clc_move: {
				if( move_issued ) {
					return; // someone is trying to cheat...
				}
				move_issued = true;
				SV_ParseMoveCommand( client, msg );
			} break;

			case clc_svcack: {
				int cmdNum = MSG_ReadIntBase128( msg );
				if( cmdNum < client->reliableAcknowledge || cmdNum > client->reliableSent ) {
					//SV_DropClient( client, "%s", "Error: bad server command acknowledged" );
					return;
				}
				client->reliableAcknowledge = cmdNum;
			} break;

			case clc_clientcommand: {
				int cmdNum = MSG_ReadIntBase128( msg );
				if( cmdNum <= client->clientCommandExecuted ) {
					MSG_ReadUint8( msg );
					MSG_ReadMsg( msg );
					continue;
				}
				client->clientCommandExecuted = cmdNum;
				ClientCommandType command = ClientCommandType( MSG_ReadUint8( msg ) ); // TODO: check in range
				msg_t args = MSG_ReadMsg( msg );
				SV_ExecuteUserCommand( client, command, args );
				if( client->state == CS_ZOMBIE ) {
					return; // disconnect command
				}
			} break;
		}
	}

	if( msg->readcount > msg->cursize ) {
		Com_Printf( "SV_ParseClientMessage: badread\n" );
		SV_DropClient( client, "%s", "Error: Bad message" );
		return;
	}
}
