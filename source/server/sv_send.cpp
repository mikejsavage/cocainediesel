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
#include "qcommon/time.h"

// shared message buffer to be used for occasional messages
msg_t tmpMessage;
uint8_t tmpMessageData[MAX_MSGLEN];

void SV_AddGameCommand( client_t *client, const char *cmd ) {
	int index;

	if( !client ) {
		return;
	}

	if( client->state < CS_SPAWNED ) {
		return;
	}

	Assert( strlen( cmd ) < MAX_STRING_CHARS );

	client->gameCommandCurrent++;
	index = client->gameCommandCurrent % ARRAY_COUNT( client->gameCommands );
	SafeStrCpy( client->gameCommands[index].command, cmd, sizeof( client->gameCommands[index].command ) );
	if( client->lastSentFrameNum ) {
		client->gameCommands[index].framenum = client->lastSentFrameNum + 1;
	} else {
		client->gameCommands[index].framenum = sv.framenum;
	}
}

/*
* SV_AddServerCommand
*
* The given command will be transmitted to the client, and is guaranteed to
* not have future snapshot_t executed before it is executed
*/
void SV_AddServerCommand( client_t *client, const char *cmd ) {
	if( !client ) {
		return;
	}

	if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
		return;
	}

	if( !cmd || !cmd[0] || !strlen( cmd ) ) {
		return;
	}

	client->reliableSequence++;
	// if we would be losing an old command that hasn't been acknowledged, we must drop the connection
	// we check == instead of >= so a broadcast print added by SV_DropClient() doesn't cause a recursive drop client
	if( client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1 ) {
		SV_DropClient( client, "%s", "Error: Server command overflow" );
		return;
	}
	int index = client->reliableSequence % ARRAY_COUNT( client->reliableCommands );
	SafeStrCpy( client->reliableCommands[index], cmd, sizeof( client->reliableCommands[index] ) );
}

/*
* SV_SendServerCommand
*
* Sends a reliable command string to be interpreted by
* the client: "changing", "disconnect", etc
* A NULL client will broadcast to all clients
*/
void SV_SendServerCommand( client_t *cl, const char *format, ... ) {
	va_list argptr;
	char message[MAX_MSGLEN];
	client_t *client;
	int i;

	va_start( argptr, format );
	vsnprintf( message, sizeof( message ), format, argptr );
	va_end( argptr );

	if( cl != NULL ) {
		if( cl->state < CS_CONNECTING ) {
			return;
		}
		SV_AddServerCommand( cl, message );
		return;
	}

	// send the data to all relevant clients
	for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
		if( client->state < CS_CONNECTING ) {
			continue;
		}
		SV_AddServerCommand( client, message );
	}

	SV_Demo_AddServerCommand( message );
}

/*
* SV_AddReliableCommandsToMessage
*
* (re)send all server commands the client hasn't acknowledged yet
*/
void SV_AddReliableCommandsToMessage( client_t *client, msg_t *msg ) {
	unsigned int i;

	if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
		return;
	}

	if( sv_debug_serverCmd->integer ) {
		Com_GGPrint( "sv_cl->reliableAcknowledge: {} sv_cl->reliableSequence:{}", client->reliableAcknowledge,
					client->reliableSequence );
	}

	// write any unacknowledged serverCommands
	for( i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++ ) {
		if( !strlen( client->reliableCommands[i % ARRAY_COUNT( client->reliableCommands )] ) ) {
			continue;
		}
		MSG_WriteUint8( msg, svc_servercmd );
		MSG_WriteInt32( msg, i );
		MSG_WriteString( msg, client->reliableCommands[i % ARRAY_COUNT( client->reliableCommands )] );
		if( sv_debug_serverCmd->integer ) {
			Com_Printf( "SV_AddServerCommandsToMessage(%i):%s\n", i,
						client->reliableCommands[i % ARRAY_COUNT( client->reliableCommands )] );
		}
	}
	client->reliableSent = client->reliableSequence;
}

//=============================================================================
//
//EVENT MESSAGES
//
//=============================================================================

/*
* SV_BroadcastCommand
*
* Sends a command to all connected clients. Ignores client->state < CS_SPAWNED check
*/
void SV_BroadcastCommand( const char *format, ... ) {
	client_t *client;
	int i;
	va_list argptr;
	char string[1024];

	if( !sv.state ) {
		return;
	}

	va_start( argptr, format );
	vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
		if( client->state < CS_CONNECTING ) {
			continue;
		}
		SV_SendServerCommand( client, string );
	}
}

//===============================================================================
//
//FRAME UPDATES
//
//===============================================================================

bool SV_SendClientsFragments() {
	client_t *client;
	int i;
	bool sent = false;

	// send a message to each connected client
	for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
		if( client->state == CS_FREE || client->state == CS_ZOMBIE ) {
			continue;
		}
		if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}
		if( !client->netchan.unsentFragments ) {
			continue;
		}

		Netchan_TransmitNextFragment( svs.socket, &client->netchan );

		sent = true;
	}

	return sent;
}

bool SV_Netchan_Transmit( netchan_t *netchan, msg_t *msg ) {
	// if we got here with unsent fragments, fire them all now
	if( !Netchan_PushAllFragments( svs.socket, netchan ) ) {
		return false;
	}

	Netchan_CompressMessage( msg );
	return Netchan_Transmit( svs.socket, netchan, msg );
}

void SV_InitClientMessage( client_t *client, msg_t *msg, uint8_t *data, size_t size ) {
	if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
		return;
	}

	if( data && size ) {
		*msg = NewMSGWriter( data, size );
	}
	MSG_Clear( msg );

	// write the last client-command we received so it's acknowledged
	MSG_WriteUint8( msg, svc_clcack );
	MSG_WriteUintBase128( msg, client->clientCommandExecuted );
	MSG_WriteUintBase128( msg, client->UcmdReceived ); // acknowledge the last ucmd
}

bool SV_SendMessageToClient( client_t *client, msg_t *msg ) {
	Assert( client );

	if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
		return true;
	}

	// transmit the message data
	client->lastPacketSentTime = svs.monotonic_time;
	return SV_Netchan_Transmit( &client->netchan, msg );
}

/*
* SV_ResetClientFrameCounters
* This is used for a temporary sanity check I'm doing.
*/
void SV_ResetClientFrameCounters() {
	int i;
	client_t *client;
	for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
		if( !client->state ) {
			continue;
		}
		if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}

		client->lastSentFrameNum = 0;
	}
}

void SV_WriteFrameSnapToClient( client_t *client, msg_t *msg ) {
	SNAP_WriteFrameSnapToClient( &sv.gi, client, msg, sv.framenum, svs.gametime, sv.baselines, &svs.client_entities );
}

void SV_BuildClientFrameSnap( client_t *client ) {
	SNAP_BuildClientFrameSnap( &sv.gi, sv.framenum, svs.gametime,
		client, &server_gs.gameState, &svs.client_entities );
}

static void SV_SendClientDatagram( client_t *client ) {
	if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
		return;
	}

	SV_InitClientMessage( client, &tmpMessage, NULL, 0 );

	SV_AddReliableCommandsToMessage( client, &tmpMessage );

	// send over all the relevant SyncEntityState
	// and the SyncPlayerState
	SV_BuildClientFrameSnap( client );

	SV_WriteFrameSnapToClient( client, &tmpMessage );

	SV_SendMessageToClient( client, &tmpMessage );
}

void SV_SendClientMessages() {
	TracyZoneScoped;

	int i;
	client_t *client;

	// send a message to each connected client
	for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
		if( client->state == CS_FREE || client->state == CS_ZOMBIE ) {
			continue;
		}

		if( client->edict && ( client->edict->s.svflags & SVF_FAKECLIENT ) ) {
			client->lastSentFrameNum = sv.framenum;
			continue;
		}

		if( client->state == CS_SPAWNED ) {
			SV_SendClientDatagram( client );
		} else {
			// send pending reliable commands, or send heartbeats for not timing out
			if( client->reliableSequence > client->reliableAcknowledge ||
				svs.monotonic_time - client->lastPacketSentTime > Seconds( 1 ) ) {
				SV_InitClientMessage( client, &tmpMessage, NULL, 0 );
				SV_AddReliableCommandsToMessage( client, &tmpMessage );
				SV_SendMessageToClient( client, &tmpMessage );
			}
		}
	}
}
