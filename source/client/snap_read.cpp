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
#include "cgame/cg_public.h"

const char * const svc_strings[256] = {
	"svc_servercmd",
	"svc_serverdata",
	"svc_spawnbaseline",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_gamecommands",
	"svc_match",
	"svc_clcack",
	"svc_unreliable",
	"svc_frame",
};

void _SHOWNET( msg_t *msg, const char *s, int shownet ) {
	if( shownet >= 2 ) {
		Com_Printf( "%3i:%s\n", (int)(msg->readcount - 1), s );
	}
}

static void SNAP_ParseDeltaGameState( msg_t *msg, snapshot_t *oldframe, snapshot_t *newframe ) {
	MSG_ReadDeltaGameState( msg, oldframe ? &oldframe->gameState : NULL, &newframe->gameState );
}

static void SNAP_ParsePlayerstate( msg_t *msg, const SyncPlayerState *oldstate, SyncPlayerState *state ) {
	MSG_ReadDeltaPlayerState( msg, oldstate, state );
}

/*
* SNAP_ParseDeltaEntity
*
* Parses deltas from the given base and adds the resulting entity to the current frame
*/
static void SNAP_ParseDeltaEntity( msg_t *msg, snapshot_t *frame, int newnum, SyncEntityState *old ) {
	SyncEntityState * state = &frame->parsedEntities[ frame->numEntities % ARRAY_COUNT( frame->parsedEntities ) ];
	frame->numEntities++;
	MSG_ReadDeltaEntity( msg, old, state );
	state->number = newnum;
}

void SNAP_ParseBaseline( msg_t *msg, SyncEntityState *baselines ) {
	bool remove;
	int newnum = MSG_ReadEntityNumber( msg, &remove );
	Assert( !remove );

	if( !remove ) {
		SyncEntityState nullstate = { };
		MSG_ReadDeltaEntity( msg, &nullstate, &baselines[newnum] );
		baselines[ newnum ].number = newnum;
	}
}

/*
* SNAP_ParsePacketEntities
*
* An svc_packetentities has just been parsed, deal with the
* rest of the data stream.
*/
static void SNAP_ParsePacketEntities( msg_t *msg, snapshot_t *oldframe, snapshot_t *newframe, SyncEntityState *baselines, int shownet ) {
	SyncEntityState *oldstate = NULL;
	int oldnum;

	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	int oldindex = 0;
	if( !oldframe ) {
		oldnum = 99999;
	} else if( oldindex >= oldframe->numEntities ) {
		oldnum = 99999;
	} else {
		oldstate = &oldframe->parsedEntities[ oldindex % ARRAY_COUNT( oldframe->parsedEntities ) ];
		oldnum = oldstate->number;
	}

	while( true ) {
		bool remove;
		int newnum = MSG_ReadEntityNumber( msg, &remove );
		if( newnum == MAX_EDICTS ) {
			break;
		}
		if( newnum > MAX_EDICTS ) {
			Com_Error( "CL_ParsePacketEntities: bad number:%i", newnum );
		}
		if( msg->readcount > msg->cursize ) {
			Com_Error( "CL_ParsePacketEntities: end of message" );
		}

		while( oldnum < newnum ) {
			// one or more entities from the old packet are unchanged
			if( shownet == 3 ) {
				Com_Printf( "   unchanged: %i\n", oldnum );
			}

			newframe->parsedEntities[ newframe->numEntities % ARRAY_COUNT( newframe->parsedEntities ) ] = *oldstate;
			newframe->numEntities++;

			oldindex++;
			if( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = &oldframe->parsedEntities[ oldindex % ARRAY_COUNT( oldframe->parsedEntities ) ];
				oldnum = oldstate->number;
			}
		}

		// delta from baseline
		if( oldnum > newnum ) {
			if( remove ) {
				Com_Printf( "U_REMOVE: oldnum > newnum (can't remove from baseline!)\n" );
				continue;
			}

			// delta from baseline
			if( shownet == 3 ) {
				Com_Printf( "   baseline: %i\n", newnum );
			}

			SNAP_ParseDeltaEntity( msg, newframe, newnum, &baselines[newnum] );
			continue;
		}

		if( oldnum == newnum ) {
			if( remove ) {
				// the entity present in oldframe is not in the current frame
				if( shownet == 3 ) {
					Com_Printf( "   remove: %i\n", newnum );
				}

				if( oldnum != newnum ) {
					Com_Printf( "U_REMOVE: oldnum != newnum\n" );
				}

				oldindex++;
				if( oldindex >= oldframe->numEntities ) {
					oldnum = 99999;
				} else {
					oldstate = &oldframe->parsedEntities[ oldindex % ARRAY_COUNT( oldframe->parsedEntities ) ];
					oldnum = oldstate->number;
				}
				continue;
			}

			// delta from previous state
			if( shownet == 3 ) {
				Com_Printf( "   delta: %i\n", newnum );
			}

			SNAP_ParseDeltaEntity( msg, newframe, newnum, oldstate );

			oldindex++;
			if( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = &oldframe->parsedEntities[ oldindex % ARRAY_COUNT( oldframe->parsedEntities ) ];
				oldnum = oldstate->number;
			}
			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while( oldnum != 99999 ) {
		// one or more entities from the old packet are unchanged
		if( shownet == 3 ) {
			Com_Printf( "   unchanged: %i\n", oldnum );
		}

		newframe->parsedEntities[ newframe->numEntities % ARRAY_COUNT( newframe->parsedEntities ) ] = *oldstate;
		newframe->numEntities++;

		oldindex++;
		if( oldindex >= oldframe->numEntities ) {
			oldnum = 99999;
		} else {
			oldstate = &oldframe->parsedEntities[ oldindex % ARRAY_COUNT( oldframe->parsedEntities ) ];
			oldnum = oldstate->number;
		}
	}
}

static snapshot_t *SNAP_ParseFrameHeader( msg_t *msg, snapshot_t *newframe, snapshot_t *backup ) {
	// get the snapshot id
	s64 serverTime = MSG_ReadIntBase128( msg );
	int snapNum = MSG_ReadUintBase128( msg );

	if( backup ) {
		newframe = &backup[snapNum % CMD_BACKUP];
	}

	memset( newframe, 0, sizeof( snapshot_t ) );

	newframe->serverTime = serverTime;
	newframe->serverFrame = snapNum;
	newframe->deltaFrameNum = MSG_ReadUintBase128( msg );
	newframe->ucmdExecuted = MSG_ReadUintBase128( msg );

	u8 flags = MSG_ReadUint8( msg );
	newframe->delta = ( flags & FRAMESNAP_FLAG_DELTA ) ? true : false;
	newframe->multipov = ( flags & FRAMESNAP_FLAG_MULTIPOV ) ? true : false;
	newframe->allentities = ( flags & FRAMESNAP_FLAG_ALLENTITIES ) ? true : false;

	// validate the new frame
	newframe->valid = false;

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message
	if( !newframe->delta ) {
		newframe->valid = true; // uncompressed frame
	} else {
		if( newframe->deltaFrameNum <= 0 ) {
			Com_Printf( "Invalid delta frame (not supposed to happen!).\n" );
		} else if( backup ) {
			snapshot_t *deltaframe = &backup[newframe->deltaFrameNum % CMD_BACKUP];
			if( !deltaframe->valid ) {
				// should never happen
				Com_Printf( "Delta from invalid frame (not supposed to happen!).\n" );
			} else if( deltaframe->serverFrame != newframe->deltaFrameNum ) {
				// The frame that the server did the delta from
				// is too old, so we can't reconstruct it properly.
				Com_Printf( "Delta frame too old.\n" );
			} else {
				newframe->valid = true; // valid delta parse
			}
		}
	}

	return newframe;
}

snapshot_t *SNAP_ParseFrame( msg_t *msg, const snapshot_t *lastFrame, snapshot_t *backup, SyncEntityState *baselines, int showNet ) {
	snapshot_t  *deltaframe;
	int numplayers;
	char *text;
	int framediff;
	gcommand_t *gcmd;
	snapshot_t  *newframe;

	// read header
	newframe = SNAP_ParseFrameHeader( msg, NULL, backup );
	deltaframe = NULL;

	if( showNet == 3 ) {
		Com_GGPrint( "   frame:{} old:{}{}", newframe->serverFrame, newframe->deltaFrameNum,
			newframe->delta ? "" : " no delta" );
	}

	if( newframe->delta ) {
		if( newframe->deltaFrameNum > 0 ) {
			deltaframe = &backup[newframe->deltaFrameNum % CMD_BACKUP];
		}
	}

	// read game commands
	int cmd = MSG_ReadUint8( msg );
	if( cmd != svc_gamecommands ) {
		Com_Error( "SNAP_ParseFrame: not gamecommands" );
	}

	size_t numtargets = 0;
	while( ( framediff = MSG_ReadInt16( msg ) ) != -1 ) {
		text = MSG_ReadString( msg );

		// see if it's valid and not yet handled
		if( newframe->valid &&
			( !lastFrame || !lastFrame->valid || newframe->serverFrame > lastFrame->serverFrame + framediff ) ) {
			newframe->numgamecommands++;
			if( newframe->numgamecommands > ARRAY_COUNT( newframe->gamecommands ) ) {
				Com_Error( "SNAP_ParseFrame: too many gamecommands" );
			}
			if( newframe->gamecommandsDataHead + strlen( text ) >= sizeof( newframe->gamecommandsData ) ) {
				Com_Error( "SNAP_ParseFrame: too much gamecommands" );
			}

			gcmd = &newframe->gamecommands[newframe->numgamecommands - 1];
			gcmd->all = true;

			SafeStrCpy( newframe->gamecommandsData + newframe->gamecommandsDataHead, text,
						sizeof( newframe->gamecommandsData ) - newframe->gamecommandsDataHead );
			gcmd->commandOffset = newframe->gamecommandsDataHead;
			newframe->gamecommandsDataHead += strlen( text ) + 1;

			if( newframe->multipov ) {
				numtargets = MSG_ReadUint8( msg );
				if( numtargets ) {
					if( numtargets > sizeof( gcmd->targets ) ) {
						Com_Error( "SNAP_ParseFrame: too many gamecommand targets" );
					}
					gcmd->all = false;
					MSG_ReadData( msg, gcmd->targets, numtargets );
				}
			}
		} else if( newframe->multipov ) {   // otherwise, ignore it
			numtargets = MSG_ReadUint8( msg );
			MSG_SkipData( msg, numtargets );
		}
	}

	// read match info
	cmd = MSG_ReadUint8( msg );
	_SHOWNET( msg, svc_strings[cmd], showNet );
	if( cmd != svc_match ) {
		Com_Error( "SNAP_ParseFrame: not match info" );
	}
	SNAP_ParseDeltaGameState( msg, deltaframe, newframe );

	// read playerinfos
	numplayers = 0;
	while( ( cmd = MSG_ReadUint8( msg ) ) ) {
		_SHOWNET( msg, svc_strings[cmd], showNet );
		if( cmd != svc_playerinfo ) {
			Com_Error( "SNAP_ParseFrame: not playerinfo" );
		}
		if( deltaframe && deltaframe->numplayers >= numplayers ) {
			SNAP_ParsePlayerstate( msg, &deltaframe->playerStates[numplayers], &newframe->playerStates[numplayers] );
		} else {
			SNAP_ParsePlayerstate( msg, NULL, &newframe->playerStates[numplayers] );
		}
		numplayers++;
	}
	newframe->numplayers = numplayers;
	newframe->playerState = newframe->playerStates[0];

	// read packet entities
	cmd = MSG_ReadUint8( msg );
	_SHOWNET( msg, svc_strings[cmd], showNet );
	if( cmd != svc_packetentities ) {
		Com_Error( "SNAP_ParseFrame: not packetentities" );
	}
	SNAP_ParsePacketEntities( msg, deltaframe, newframe, baselines, showNet );

	return newframe;
}
