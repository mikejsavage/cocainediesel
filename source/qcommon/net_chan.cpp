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
#include "qcommon/csprng.h"

#include "zstd/zstd.h"

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16	game port

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher than the last reliable sequence, but without the correct even/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted. It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

If the sequence number is -1, the packet should be handled without a netcon.

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection. This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.


The game port field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the game port matches, then the
channel matches even if the IP port differs. The IP port should be updated
to the new value before sending out any replies.


If there is no information that needs to be transfered on a given frame,
such as during the connection stage while waiting for the client to load,
then a packet only needs to be delivered if there is something in the
unacknowledged reliable
*/

static Cvar * showpackets;
static Cvar * showdrop;
static Cvar * net_showfragments;

/*
* Netchan_OutOfBand
*
* Sends an out-of-band datagram
*/
static void Netchan_OutOfBand( Socket socket, const NetAddress & address, const void * data, size_t length ) {
	uint8_t send_buf[MAX_PACKETLEN];
	msg_t send = NewMSGWriter( send_buf, sizeof( send_buf ) );

	MSG_WriteInt32( &send, -1 ); // -1 sequence means out of band
	MSG_Write( &send, data, length );

	UDPSend( socket, address, send.data, send.cursize );
}

/*
* Netchan_OutOfBandPrint
*
* Sends a text message in an out-of-band datagram
*/
void Netchan_OutOfBandPrint( Socket socket, const NetAddress & address, const char * format, ... ) {
	va_list argptr;
	static char string[MAX_PACKETLEN - 4];

	va_start( argptr, format );
	vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	Netchan_OutOfBand( socket, address, string, strlen( string ) );
}

/*
* Netchan_Setup
*
* called to open a channel to a remote system
*/
void Netchan_Setup( netchan_t * chan, const NetAddress & address, u64 session_id ) {
	memset( chan, 0, sizeof( * chan ) );

	chan->remoteAddress = address;
	chan->session_id = session_id;
	chan->incomingSequence = 0;
	chan->outgoingSequence = 1;
}

void Netchan_CompressMessage( msg_t * msg ) {
	static u8 compressed[ MAX_MSGLEN ];
	size_t compressed_size = ZSTD_compress( compressed, sizeof( compressed ), msg->data, msg->cursize, ZSTD_CLEVEL_DEFAULT );
	if( ZSTD_isError( compressed_size ) || compressed_size >= msg->cursize )
		return;

	MSG_Clear( msg );
	MSG_Write( msg, compressed, compressed_size );
	msg->compressed = true;
}

bool Netchan_DecompressMessage( msg_t * msg ) {
	if( !msg->compressed )
		return true;

	static u8 decompressed[ MAX_MSGLEN ];
	size_t decompressed_size = ZSTD_decompress( decompressed, sizeof( decompressed ) - msg->readcount, msg->data + msg->readcount, msg->cursize - msg->readcount );
	if( ZSTD_isError( decompressed_size ) )
		return false;

	msg->cursize = msg->readcount;
	MSG_Write( msg, decompressed, decompressed_size );
	msg->compressed = false;

	return true;
}

/*
* Netchan_DropAllFragments
*
* Send all remaining fragments at once
*/
static void Netchan_DropAllFragments( netchan_t * chan ) {
	if( chan->unsentFragments ) {
		chan->outgoingSequence++;
		chan->unsentFragments = false;
	}
}

/*
* Netchan_TransmitNextFragment
*
* Send one fragment of the current message
*/
bool Netchan_TransmitNextFragment( Socket socket, netchan_t * chan ) {
	uint8_t send_buf[MAX_PACKETLEN];
	msg_t send = NewMSGWriter( send_buf, sizeof( send_buf ) );

	if( net_showfragments->integer ) {
		// Com_Printf( "Transmit fragment (%s) (id:%i)\n", DescribeSocket( chan->socket ), chan->outgoingSequence );
	}

	MSG_WriteInt32( &send, chan->outgoingSequence | FRAGMENT_BIT );
	// wsw : jal : by now our header sends incoming ack too (q3 doesn't)
	// wsw : also add compressed bit if it's compressed
	if( chan->unsentIsCompressed ) {
		MSG_WriteInt32( &send, chan->incomingSequence | FRAGMENT_BIT );
	} else {
		MSG_WriteInt32( &send, chan->incomingSequence );
	}

	MSG_WriteUint64( &send, chan->session_id );

	// copy the reliable message to the packet first
	int fragmentLength;
	bool last;
	if( chan->unsentFragmentStart + FRAGMENT_SIZE > chan->unsentLength ) {
		fragmentLength = chan->unsentLength - chan->unsentFragmentStart;
		last = true;
	} else {
		fragmentLength = ceilf( ( chan->unsentLength - chan->unsentFragmentStart ) * 1.0f / ceilf( ( chan->unsentLength - chan->unsentFragmentStart ) * 1.0f / FRAGMENT_SIZE ) );
		last = false;
	}

	MSG_WriteInt16( &send, chan->unsentFragmentStart );
	MSG_WriteInt16( &send, ( last ? ( fragmentLength | FRAGMENT_LAST ) : fragmentLength ) );
	MSG_Write( &send, chan->unsentBuffer + chan->unsentFragmentStart, fragmentLength );

	// send the datagram
	if( UDPSend( socket, chan->remoteAddress, send.data, send.cursize ) != send.cursize ) {
		Netchan_DropAllFragments( chan );
		return false;
	}

	if( showpackets->integer ) {
		// Com_Printf( "%s send %4li : s=%i fragment=%li,%i\n", DescribeSocket( chan->socket ), send.cursize,
		// 			chan->outgoingSequence, chan->unsentFragmentStart, fragmentLength );
	}

	chan->unsentFragmentStart += fragmentLength;

	// this exit condition is a little tricky, because a packet
	// that is exactly the fragment length still needs to send
	// a second packet of zero length so that the other side
	// can tell there aren't more to follow
	if( chan->unsentFragmentStart == chan->unsentLength && fragmentLength != FRAGMENT_SIZE ) {
		chan->outgoingSequence++;
		chan->unsentFragments = false;
	}

	return true;
}

/*
* Netchan_PushAllFragments
*
* Send all remaining fragments at once
*/
bool Netchan_PushAllFragments( Socket socket, netchan_t * chan ) {
	while( chan->unsentFragments ) {
		if( !Netchan_TransmitNextFragment( socket, chan ) ) {
			return false;
		}
	}

	return true;
}

/*
* Netchan_Transmit
*
* Sends a message to a connection, fragmenting if necessary
* A 0 length will still generate a packet.
*/
bool Netchan_Transmit( Socket socket, netchan_t * chan, msg_t * msg ) {
	assert( msg );

	if( msg->cursize > MAX_MSGLEN ) {
		Com_Error( "Netchan_Transmit: Excessive length = %li", msg->cursize );
		return false;
	}
	chan->unsentFragmentStart = 0;
	chan->unsentIsCompressed = false;

	// fragment large reliable messages
	if( msg->cursize >= FRAGMENT_SIZE ) {
		chan->unsentFragments = true;
		chan->unsentLength = msg->cursize;
		chan->unsentIsCompressed = msg->compressed;
		memcpy( chan->unsentBuffer, msg->data, msg->cursize );

		// only send the first fragment now
		return Netchan_TransmitNextFragment( socket, chan );
	}

	// write the packet header
	uint8_t send_buf[MAX_PACKETLEN];
	msg_t send = NewMSGWriter( send_buf, sizeof( send_buf ) );
	MSG_WriteInt32( &send, chan->outgoingSequence );
	// wsw : jal : by now our header sends incoming ack too (q3 doesn't)
	// wsw : jal : also add compressed information if it's compressed
	if( msg->compressed ) {
		MSG_WriteInt32( &send, chan->incomingSequence | FRAGMENT_BIT );
	} else {
		MSG_WriteInt32( &send, chan->incomingSequence );
	}

	chan->outgoingSequence++;

	MSG_WriteUint64( &send, chan->session_id );

	MSG_Write( &send, msg->data, msg->cursize );

	// send the datagram
	if( UDPSend( socket, chan->remoteAddress, send.data, send.cursize ) != send.cursize ) {
		return false;
	}

	if( showpackets->integer ) {
		// Com_Printf( "%s send %4li : s=%i ack=%i\n", DescribeSocket( chan->socket ), send.cursize,
		// 			chan->outgoingSequence - 1, chan->incomingSequence );
	}

	return true;
}

/*
* Netchan_Process
*
* Returns false if the message should not be processed due to being
* out of order or a fragment.
*
* Msg must be large enough to hold MAX_MSGLEN, because if this is the
* final fragment of a multi-part message, the entire thing will be
* copied out.
*/
bool Netchan_Process( netchan_t * chan, msg_t * msg ) {
	int sequence, sequence_ack;
	int fragmentStart, fragmentLength;
	bool fragmented = false;
	int headerlength;
	bool compressed = false;
	bool lastfragment = false;

	// get sequence numbers
	MSG_BeginReading( msg );
	sequence = MSG_ReadInt32( msg );
	sequence_ack = MSG_ReadInt32( msg ); // wsw : jal : by now our header sends incoming ack too (q3 doesn't)

	// check for fragment information
	if( sequence & FRAGMENT_BIT ) {
		sequence &= ~FRAGMENT_BIT;
		fragmented = true;

		if( net_showfragments->integer ) {
			// Com_Printf( "Process fragmented packet (%s) (id:%i)\n", DescribeSocket( chan->socket ), sequence );
		}
	} else {
		fragmented = false;
	}

	// wsw : jal : check for compressed information
	if( sequence_ack & FRAGMENT_BIT ) {
		sequence_ack &= ~FRAGMENT_BIT;
		compressed = true;
		if( !fragmented ) {
			msg->compressed = true;
		}
	}

	u64 session_id = MSG_ReadUint64( msg );

	// read the fragment information
	if( fragmented ) {
		fragmentStart = MSG_ReadInt16( msg );
		fragmentLength = MSG_ReadInt16( msg );
		if( fragmentLength & FRAGMENT_LAST ) {
			lastfragment = true;
			fragmentLength &= ~FRAGMENT_LAST;
		}
	} else {
		fragmentStart = 0; // stop warning message
		fragmentLength = 0;
	}

	if( showpackets->integer ) {
		if( fragmented ) {
			// Com_Printf( "%s recv %4li : s=%i fragment=%i,%i\n", DescribeSocket( chan->socket ), msg->cursize,
			// 			sequence, fragmentStart, fragmentLength );
		} else {
			// Com_Printf( "%s recv %4li : s=%i\n", DescribeSocket( chan->socket ), msg->cursize, sequence );
		}
	}

	//
	// discard out of order or duplicated packets
	//
	if( sequence <= chan->incomingSequence ) {
		if( showdrop->integer || showpackets->integer ) {
			Com_GGPrint( "{}:Out of order packet {} at {}", chan->remoteAddress, sequence, chan->incomingSequence );
		}
		return false;
	}

	//
	// dropped packets don't keep the message from being used
	//
	chan->dropped = sequence - ( chan->incomingSequence + 1 );
	if( chan->dropped > 0 ) {
		if( showdrop->integer || showpackets->integer ) {
			Com_GGPrint( "{}:Dropped {} packets at {}", chan->remoteAddress, chan->dropped, sequence );
		}
	}

	//
	// if this is the final framgent of a reliable message,
	// bump incoming_reliable_sequence
	//
	if( fragmented ) {
		// TTimo
		// make sure we add the fragments in correct order
		// either a packet was dropped, or we received this one too soon
		// we don't reconstruct the fragments. we will wait till this fragment gets to us again
		// (NOTE: we could probably try to rebuild by out of order chunks if needed)
		if( sequence != chan->fragmentSequence ) {
			chan->fragmentSequence = sequence;
			chan->fragmentLength = 0;
		}

		// if we missed a fragment, dump the message
		if( fragmentStart != (int) chan->fragmentLength ) {
			if( showdrop->integer || showpackets->integer ) {
				Com_GGPrint( "{}:Dropped a message fragment {}", chan->remoteAddress, sequence );
			}
			// we can still keep the part that we have so far,
			// so we don't need to clear chan->fragmentLength
			return false;
		}

		// copy the fragment to the fragment buffer
		if( fragmentLength < 0 || msg->readcount + fragmentLength > msg->cursize ||
			chan->fragmentLength + fragmentLength > sizeof( chan->fragmentBuffer ) ) {
			if( showdrop->integer || showpackets->integer ) {
				Com_GGPrint( "{}:illegal fragment length", chan->remoteAddress );
			}
			return false;
		}

		memcpy( chan->fragmentBuffer + chan->fragmentLength, msg->data + msg->readcount, fragmentLength );

		chan->fragmentLength += fragmentLength;

		// if this wasn't the last fragment, don't process anything
		if( !lastfragment ) {
			return false;
		}

		if( chan->fragmentLength > msg->maxsize ) {
			Com_GGPrint( "{}:fragmentLength {} > msg->maxsize", chan->remoteAddress, chan->fragmentLength );
			return false;
		}

		// wsw : jal : reconstruct the message

		MSG_Clear( msg );
		MSG_WriteInt32( msg, sequence );
		MSG_WriteInt32( msg, sequence_ack );
		MSG_WriteUint64( msg, session_id );

		msg->compressed = compressed;

		headerlength = msg->cursize;
		MSG_Write( msg, chan->fragmentBuffer, chan->fragmentLength );
		msg->readcount = headerlength; // put read pointer after header again
		chan->fragmentLength = 0;

		//let it be finished as standard packets
	}

	// the message can now be read from the current message pointer
	chan->incomingSequence = sequence;

	// wsw : jal[start] :  get the ack from the very first fragment
	chan->incoming_acknowledged = sequence_ack;
	// wsw : jal[end]

	return true;
}

void Netchan_Init() {
	showpackets = NewCvar( "showpackets", "0" );
	showdrop = NewCvar( "showdrop", "0" );
	net_showfragments = NewCvar( "net_showfragments", "0" );
}

void Netchan_Shutdown() {
}
