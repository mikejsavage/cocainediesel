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
#include "server/server.h"

#if PLATFORM_WINDOWS
#include <malloc.h> // alloca
#endif

/*
* SNAP_EmitPacketEntities
*
* Writes a delta update of an SyncEntityState list to the message.
*/
static void SNAP_EmitPacketEntities( ginfo_t *gi, client_snapshot_t *from, client_snapshot_t *to, msg_t *msg, const SyncEntityState *baselines, const SyncEntityState *client_entities, int num_client_entities ) {
	MSG_WriteUint8( msg, svc_packetentities );

	int from_num_entities = from == NULL ? 0 : from->num_entities;
	int newindex = 0;
	int oldindex = 0;
	while( newindex < to->num_entities || oldindex < from_num_entities ) {
		const SyncEntityState * oldent;
		const SyncEntityState * newent;
		int oldnum, newnum;
		if( newindex >= to->num_entities ) {
			newent = NULL;
			newnum = 9999;
		} else {
			newent = &client_entities[( to->first_entity + newindex ) % num_client_entities];
			newnum = newent->number;
		}

		if( oldindex >= from_num_entities ) {
			oldent = NULL;
			oldnum = 9999;
		} else {
			oldent = &client_entities[( from->first_entity + oldindex ) % num_client_entities];
			oldnum = oldent->number;
		}

		if( newnum == oldnum ) {
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping ( wsw : jal : I removed it from the players )
			MSG_WriteDeltaEntity( msg, oldent, newent, false );
			oldindex++;
			newindex++;
			continue;
		}

		if( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity( msg, &baselines[newnum], newent, true );
			newindex++;
			continue;
		}

		if( newnum > oldnum ) {
			// the old entity isn't present in the new message
			MSG_WriteEntityNumber( msg, oldnum, true );
			oldindex++;
			continue;
		}
	}

	MSG_WriteEntityNumber( msg, MAX_EDICTS, false ); // end of packetentities
}

static void SNAP_WriteDeltaGameStateToClient( const client_snapshot_t *from, client_snapshot_t *to, msg_t *msg ) {
	MSG_WriteUint8( msg, svc_match );
	MSG_WriteDeltaGameState( msg, from ? &from->gameState : NULL, &to->gameState );
}

static void SNAP_WritePlayerstateToClient( msg_t *msg, const SyncPlayerState *ops, SyncPlayerState *ps ) {
	MSG_WriteUint8( msg, svc_playerinfo );
	MSG_WriteDeltaPlayerState( msg, ops, ps );
}

static void SNAP_WriteMultiPOVCommands( ginfo_t *gi, const client_t *client, msg_t *msg, int64_t frameNum ) {
	int positions[MAX_CLIENTS];

	// find the first command to send from every client
	int maxnumtargets = 0;
	for( int i = 0; i < gi->max_clients; i++ ) {
		const client_t * cl = gi->clients + i;

		if( cl->state < CS_SPAWNED || ( ( !cl->edict || ( cl->edict->s.svflags & SVF_NOCLIENT ) ) && cl != client ) ) {
			continue;
		}

		maxnumtargets++;
		for( positions[i] = cl->gameCommandCurrent - MAX_RELIABLE_COMMANDS + 1; positions[i] <= cl->gameCommandCurrent; positions[i]++ ) {
			int index = positions[i] % ARRAY_COUNT( cl->gameCommands );

			// we need to check for too new commands too, because gamecommands for the next snap are generated
			// all the time, and we might want to create a server demo frame or something in between snaps
			if( cl->gameCommands[index].command[0] && cl->gameCommands[index].framenum + 256 >= frameNum &&
				cl->gameCommands[index].framenum <= frameNum &&
				( client->lastframe >= 0 && cl->gameCommands[index].framenum > client->lastframe ) ) {
				break;
			}
		}
	}

	// send all messages, combining similar messages together to save space
	while( true ) {
		int numtargets = 0, maxtarget = 0;
		int64_t framenum = 0;
		uint8_t targets[MAX_CLIENTS / 8];

		const char * command = NULL;
		memset( targets, 0, sizeof( targets ) );

		// we find the message with the earliest framenum, and collect all recipients for that
		for( int i = 0; i < gi->max_clients; i++ ) {
			const client_t * cl = gi->clients + i;

			if( cl->state < CS_SPAWNED || ( ( !cl->edict || ( cl->edict->s.svflags & SVF_NOCLIENT ) ) && cl != client ) ) {
				continue;
			}

			if( positions[i] > cl->gameCommandCurrent ) {
				continue;
			}

			int index = positions[i] % ARRAY_COUNT( cl->gameCommands );

			if( command && StrEqual( cl->gameCommands[index].command, command ) &&
				framenum == cl->gameCommands[index].framenum ) {
				targets[i >> 3] |= 1 << ( i & 7 );
				maxtarget = i + 1;
				numtargets++;
			} else if( !command || cl->gameCommands[index].framenum < framenum ) {
				command = cl->gameCommands[index].command;
				framenum = cl->gameCommands[index].framenum;
				memset( targets, 0, sizeof( targets ) );
				targets[i >> 3] |= 1 << ( i & 7 );
				maxtarget = i + 1;
				numtargets = 1;
			}

			if( numtargets == maxnumtargets ) {
				break;
			}
		}

		if( command == NULL ) {
			break;
		}

		// never write a command if it's of a higher framenum
		if( frameNum >= framenum ) {
			// do not allow the message buffer to overflow (can happen on flood updates)
			if( msg->cursize + strlen( command ) + 512 > msg->maxsize ) {
				continue;
			}

			MSG_WriteInt16( msg, frameNum - framenum );
			MSG_WriteString( msg, command );

			// 0 means everyone
			if( numtargets == maxnumtargets ) {
				MSG_WriteUint8( msg, 0 );
			} else {
				int bytes = ( maxtarget + 7 ) / 8;
				MSG_WriteUint8( msg, bytes );
				MSG_Write( msg, targets, bytes );
			}
		}

		for( int i = 0; i < maxtarget; i++ ) {
			if( targets[i >> 3] & ( 1 << ( i & 7 ) ) ) {
				positions[i]++;
			}
		}
	}
}

void SNAP_WriteFrameSnapToClient( ginfo_t *gi, client_t *client, msg_t *msg, int64_t frameNum, int64_t gameTime,
								  const SyncEntityState *baselines, const client_entities_t *client_entities ) {
	// this is the frame we are creating
	client_snapshot_t * frame = &client->snapShots[ frameNum % ARRAY_COUNT( client->snapShots ) ];

	// we need to send nodelta frame until the client responds
	if( client->nodelta ) {
		if( !client->nodelta_frame ) {
			client->nodelta_frame = frameNum;
		} else if( client->lastframe >= client->nodelta_frame ) {
			client->nodelta = false;
		}
	}

	client_snapshot_t * oldframe;
	if( client->lastframe <= 0 || client->lastframe > frameNum || client->nodelta ) {
		// client is asking for a not compressed retransmit
		oldframe = NULL;
	}
	else if( frameNum >= client->lastframe + UPDATE_BACKUP - 1 ) {
		// client hasn't gotten a good message through in a long time
		oldframe = NULL;
	} else {
		// we have a valid message to delta from
		oldframe = &client->snapShots[ client->lastframe % ARRAY_COUNT( client->snapShots ) ];
		if( oldframe->multipov != frame->multipov ) {
			oldframe = NULL;        // don't delta compress a frame of different POV type
		}
	}

	MSG_WriteUint8( msg, svc_frame );

	MSG_WriteIntBase128( msg, gameTime ); // serverTimeStamp
	MSG_WriteUintBase128( msg, frameNum );
	MSG_WriteUintBase128( msg, client->lastframe );
	MSG_WriteUintBase128( msg, frame->UcmdExecuted );

	int flags = 0;
	if( oldframe != NULL ) {
		flags |= FRAMESNAP_FLAG_DELTA;
	}
	if( frame->allentities ) {
		flags |= FRAMESNAP_FLAG_ALLENTITIES;
	}
	if( frame->multipov ) {
		flags |= FRAMESNAP_FLAG_MULTIPOV;
	}
	MSG_WriteUint8( msg, flags );

	// add game comands
	MSG_WriteUint8( msg, svc_gamecommands );
	if( frame->multipov ) {
		SNAP_WriteMultiPOVCommands( gi, client, msg, frameNum );
	} else {
		for( int i = client->gameCommandCurrent - MAX_RELIABLE_COMMANDS + 1; i <= client->gameCommandCurrent; i++ ) {
			int index = i % ARRAY_COUNT( client->gameCommands );

			// check that it is valid command and that has not already been sent
			// we can only allow commands from certain amount of old frames, so the short won't overflow
			if( !client->gameCommands[index].command[0] || client->gameCommands[index].framenum + 256 < frameNum ||
				client->gameCommands[index].framenum > frameNum ||
				( client->lastframe >= 0 && client->gameCommands[index].framenum <= (unsigned)client->lastframe ) ) {
				continue;
			}

			// do not allow the message buffer to overflow (can happen on flood updates)
			if( msg->cursize + strlen( client->gameCommands[index].command ) + 512 > msg->maxsize ) {
				continue;
			}

			// send it
			MSG_WriteInt16( msg, frameNum - client->gameCommands[index].framenum );
			MSG_WriteString( msg, client->gameCommands[index].command );
		}
	}
	MSG_WriteInt16( msg, -1 );

	SNAP_WriteDeltaGameStateToClient( oldframe, frame, msg );

	// delta encode the playerstate
	for( int i = 0; i < frame->numplayers; i++ ) {
		if( oldframe && oldframe->numplayers > i ) {
			SNAP_WritePlayerstateToClient( msg, &oldframe->ps[i], &frame->ps[i] );
		} else {
			SNAP_WritePlayerstateToClient( msg, NULL, &frame->ps[i] );
		}
	}
	MSG_WriteUint8( msg, 0 );

	// delta encode the entities
	SNAP_EmitPacketEntities( gi, oldframe, frame, msg, baselines, client_entities->entities, client_entities->num_entities );

	client->lastSentFrameNum = frameNum;
}

/*
=============================================================================

Build a client frame structure

=============================================================================
*/

#define MAX_SNAPSHOT_ENTITIES   1024
typedef struct {
	int numSnapshotEntities;
	int snapshotEntities[MAX_SNAPSHOT_ENTITIES];
	uint8_t entityAddedToSnapList[MAX_EDICTS / 8];
} snapshotEntityNumbers_t;

static bool SNAP_AddEntNumToSnapList( int entNum, snapshotEntityNumbers_t *entList ) {
	if( entNum >= MAX_EDICTS ) {
		return false;
	}
	if( entList->numSnapshotEntities >= MAX_SNAPSHOT_ENTITIES ) { // silent ignore of overflood
		return false;
	}

	// don't double add entities
	if( entList->entityAddedToSnapList[entNum >> 3] & (1 << (entNum & 7)) ) {
		return false;
	}

	entList->snapshotEntities[entList->numSnapshotEntities++] = entNum;
	entList->entityAddedToSnapList[entNum >> 3] |= (1 << (entNum & 7));
	return true;
}

static void SNAP_SortSnapList( snapshotEntityNumbers_t *entsList ) {
	entsList->numSnapshotEntities = 0;

	for( int i = 0; i < MAX_EDICTS; i++ ) {
		if( entsList->entityAddedToSnapList[i >> 3] & (1 << (i & 7)) ) {
			entsList->snapshotEntities[entsList->numSnapshotEntities++] = i;
		}
	}
}

static float SNAP_GainForAttenuation( float dist ) {
	dist = Max2( dist, S_DEFAULT_ATTENUATION_REFDISTANCE );
	dist = Min2( dist, S_DEFAULT_ATTENUATION_MAXDISTANCE );
	return S_DEFAULT_ATTENUATION_REFDISTANCE / ( S_DEFAULT_ATTENUATION_REFDISTANCE + ATTN_DISTANT * ( dist - S_DEFAULT_ATTENUATION_REFDISTANCE ) );
}

static bool SNAP_SnapCullSoundEntity( const edict_t * ent, Vec3 listener_origin ) {
	// extend the influence sphere cause the player could be moving
	float dist = Length( listener_origin - ent->s.origin ) - 128;
	float gain = SNAP_GainForAttenuation( dist < 0 ? 0 : dist );
	return gain <= 0.05f;
}

static bool SNAP_SnapCullEntity( const edict_t * ent, const edict_t * clent, const client_snapshot_t * frame, Vec3 vieworg ) {
	// filters: this entity has been disabled for comunication
	if( ent->s.svflags & SVF_NOCLIENT ) {
		return true;
	}

	// send all entities
	if( frame->allentities ) {
		return false;
	}

	// filters: transmit only to clients in the same team as this entity
	// broadcasting is less important than team specifics
	if( ( ent->s.svflags & SVF_ONLYTEAM ) && ( clent && ent->s.team != clent->s.team ) ) {
		return true;
	}

	// send only to owner
	if( ( ent->s.svflags & SVF_ONLYOWNER ) && ( clent && ent->s.ownerNum != clent->s.number ) ) {
		return true;
	}

	if( ( ent->s.svflags & SVF_OWNERANDCHASERS ) && clent ) {
		bool self = ent->s.ownerNum == clent->s.number;
		bool spec = ent->s.ownerNum == clent->r.client->resp.chase.target;
		if( !self && !spec )
			return true;
	}

	if( ( ent->s.svflags & SVF_NEVEROWNER ) && ( clent && ent->s.ownerNum == clent->s.number ) ) {
		return true;
	}

	if( ent->s.svflags & SVF_BROADCAST ) { // send to everyone
		return false;
	}

	if( ( ent->s.svflags & SVF_FORCETEAM ) && ( clent && ent->s.team == clent->s.team && ent->s.team >= Team_One ) ) {
		return false;
	}

	// sound entities culling
	if( ent->s.svflags & SVF_SOUNDCULL ) {
		return SNAP_SnapCullSoundEntity( ent, vieworg );
	}

	return false;
}

static void SNAP_AddEntitiesVisibleAtOrigin( const ginfo_t * gi, const edict_t * clent, Vec3 vieworg, const client_snapshot_t * frame, snapshotEntityNumbers_t * entList ) {
	// add the entities to the list
	for( int entNum = 0; entNum < gi->num_edicts; entNum++ ) {
		const edict_t * ent = EDICT_NUM( entNum );
		Assert( ent->s.number == entNum );

		// always add the client entity, even if SVF_NOCLIENT
		if( ent != clent && SNAP_SnapCullEntity( ent, clent, frame, vieworg ) ) {
			continue;
		}

		// add it
		if( !SNAP_AddEntNumToSnapList( entNum, entList ) ) {
			continue;
		}

		if( ent->s.svflags & SVF_FORCEOWNER ) {
			Assert( ent->s.ownerNum > 0 && ent->s.ownerNum < gi->num_edicts );
			SNAP_AddEntNumToSnapList( ent->s.ownerNum, entList );
		}
	}
}

static void SNAP_BuildSnapEntitiesList( const ginfo_t * gi, const edict_t * clent, Vec3 vieworg, const client_snapshot_t * frame, snapshotEntityNumbers_t * entList ) {
	entList->numSnapshotEntities = 0;
	memset( entList->entityAddedToSnapList, 0, sizeof( entList->entityAddedToSnapList ) );

	// always add the client entity
	if( clent ) {
		int entNum = NUM_FOR_EDICT( clent );
		Assert( clent->s.number == entNum );

		// FIXME we should send all the entities who's POV we are sending if frame->multipov
		SNAP_AddEntNumToSnapList( entNum, entList );
	}

	SNAP_AddEntitiesVisibleAtOrigin( gi, clent, vieworg, frame, entList );

	SNAP_SortSnapList( entList );
}

/*
* SNAP_BuildClientFrameSnap
*
* Decides which entities are going to be visible to the client, and
* copies off the playerstat and areabits.
*/
void SNAP_BuildClientFrameSnap( const ginfo_t * gi, int64_t frameNum, int64_t timeStamp,
	client_t * client,
	const SyncGameState * gameState, client_entities_t * client_entities
) {
	Assert( gameState );

	const edict_t * clent = client->edict;
	Vec3 org;
	if( clent && !clent->r.client ) {   // allow NULL ent for server record
		return;     // not in game yet
	}
	if( clent ) {
		org = clent->s.origin;
		org.z += clent->r.client->ps.viewheight;
	} else {
		Assert( client->mv );
		org = Vec3( 0.0f );
	}

	// this is the frame we are creating
	client_snapshot_t * frame = &client->snapShots[ frameNum % ARRAY_COUNT( client->snapShots ) ];
	frame->sentTimeStamp = timeStamp;
	frame->UcmdExecuted = client->UcmdExecuted;

	if( client->mv ) {
		frame->multipov = true;
		frame->allentities = true;
	} else {
		frame->multipov = false;
		frame->allentities = false;
	}

	// grab the current SyncPlayerState
	if( frame->multipov ) {
		frame->numplayers = 0;
		for( int i = 0; i < gi->max_clients; i++ ) {
			const edict_t * ent = EDICT_NUM( i + 1 );
			if( ( clent == ent ) || ( ent->r.inuse && ent->r.client && !( ent->s.svflags & SVF_NOCLIENT ) ) ) {
				frame->numplayers++;
			}
		}
	} else {
		frame->numplayers = 1;
	}

	if( frame->multipov ) {
		int numplayers = 0;
		for( int i = 0; i < gi->max_clients; i++ ) {
			const edict_t * ent = EDICT_NUM( i + 1 );
			if( ( clent == ent ) || ( ent->r.inuse && ent->r.client && !( ent->s.svflags & SVF_NOCLIENT ) ) ) {
				frame->ps[numplayers] = ent->r.client->ps;
				frame->ps[numplayers].playerNum = i;
				numplayers++;
			}
		}
	} else {
		frame->ps[0] = clent->r.client->ps;
		frame->ps[0].playerNum = NUM_FOR_EDICT( clent ) - 1;
	}

	// build up the list of visible entities
	snapshotEntityNumbers_t entsList;
	SNAP_BuildSnapEntitiesList( gi, clent, org, frame, &entsList );

	// store current match state information
	frame->gameState = *gameState;

	//=============================

	// dump the entities list
	int ne = client_entities->next_entities;
	frame->num_entities = 0;
	frame->first_entity = ne;

	for( int e = 0; e < entsList.numSnapshotEntities; e++ ) {
		// add it to the circular client_entities array
		const edict_t * ent = EDICT_NUM( entsList.snapshotEntities[e] );
		SyncEntityState * state = &client_entities->entities[ne % client_entities->num_entities];

		*state = ent->s;

		frame->num_entities++;
		ne++;
	}

	client_entities->next_entities = ne;
}
