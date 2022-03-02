#pragma once

#include "qcommon/types.h"
#include "qcommon/net.h"

struct netchan_t {
	int dropped;                // between last packet and previous

	NetAddress remoteAddress;
	u64 session_id;

	// sequencing variables
	int incomingSequence;
	int incoming_acknowledged;
	int outgoingSequence;

	// incoming fragment assembly buffer
	int fragmentSequence;
	size_t fragmentLength;
	uint8_t fragmentBuffer[MAX_MSGLEN];

	// outgoing fragment buffer
	// we need to space out the sending of large fragmented messages
	bool unsentFragments;
	size_t unsentFragmentStart;
	size_t unsentLength;
	uint8_t unsentBuffer[MAX_MSGLEN];
	bool unsentIsCompressed;
};

void Netchan_Init();
void Netchan_Shutdown();
void Netchan_Setup( netchan_t * chan, const NetAddress & address, u64 session_id );
bool Netchan_Process( netchan_t * chan, msg_t * msg );
bool Netchan_Transmit( Socket socket, netchan_t * chan, msg_t * msg );
bool Netchan_PushAllFragments( Socket socket, netchan_t * chan );
bool Netchan_TransmitNextFragment( Socket socket, netchan_t * chan );
int Netchan_CompressMessage( msg_t * msg );
int Netchan_DecompressMessage( msg_t * msg );

#ifndef _MSC_VER
void Netchan_OutOfBandPrint( Socket socket, const NetAddress & address, const char * format, ... ) __attribute__( ( format( printf, 2, 3 ) ) );
#else
void Netchan_OutOfBandPrint( Socket socket, const NetAddress & address, _Printf_format_string_ const char * format, ... );
#endif
