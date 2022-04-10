#pragma once

#include "qcommon/types.h"
#include "qcommon/net.h"

enum UDPOrTCP {
	UDPOrTCP_UDP,
	UDPOrTCP_TCP,
};

struct sockaddr;
struct sockaddr_storage;

void InitNetworking();
void ShutdownNetworking();

u64 OpenOSSocket( AddressFamily family, UDPOrTCP type, u16 port );
bool BindOSSocket( u64 handle, const sockaddr * address, int address_size );
void CloseOSSocket( u64 handle );

void OSSocketMakeNonblocking( u64 handle );
void OSSocketSetSockOptOne( u64 handle, int level, int opt );

// these return false if the tcp connection was closed
bool OSSocketSend( u64 handle, const void * data, size_t n, const sockaddr_storage * destination, size_t destination_size, size_t * sent );
bool OSSocketReceive( u64 handle, void * data, size_t n, sockaddr_storage * source, size_t * received );

void OSSocketListen( u64 handle );
u64 OSSocketAccept( u64 handle, sockaddr_storage * address );
