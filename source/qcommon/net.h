#pragma once

#include "qcommon/types.h"

enum AddressFamily {
	AddressFamily_IPv4,
	AddressFamily_IPv6,
};

struct IPv4 {
	u8 ip[ 4 ];
};

struct IPv6 {
	u8 ip[ 16 ];
};

struct NetAddress {
	AddressFamily family;
	union {
		IPv4 ipv4;
		IPv6 ipv6;
	};
	u16 port;
};

bool operator==( const NetAddress & a, const NetAddress & b );
bool operator!=( const NetAddress & a, const NetAddress & b );
bool EqualIgnoringPort( const NetAddress & a, const NetAddress & b );
void format( FormatBuffer * fb, const NetAddress & address, const FormatOpts & opts );

enum SocketType {
	SocketType_UDPClient,
	SocketType_UDPServer,
	SocketType_TCPClient,
	SocketType_TCPServer,
};

struct Socket {
	SocketType type;
	u64 ipv4, ipv6;
};

enum NonBlockingBool : bool {
	NonBlocking_No = false,
	NonBlocking_Yes = true,
};

constexpr NetAddress NULL_ADDRESS = { };

void InitNetworking();
void ShutdownNetworking();

enum DNSFamily {
	DNSFamily_Any,
	DNSFamily_IPv4,
	DNSFamily_IPv6,
};

char * SplitIntoHostnameAndPort( Allocator * a, const char * str, u16 * port );
bool DNS( const char * hostname, NetAddress * address, DNSFamily family = DNSFamily_Any );

Socket NewUDPClient( NonBlockingBool nonblocking );
Socket NewUDPServer( u16 port, NonBlockingBool nonblocking );
Socket NewTCPServer( u16 port, NonBlockingBool nonblocking );
void CloseSocket( Socket socket );

size_t UDPSend( Socket socket, NetAddress destination, const void * data, size_t n );
size_t UDPReceive( Socket socket, NetAddress * source, void * data, size_t n );

bool TCPAccept( Socket server, NonBlockingBool nonblocking, Socket * client, NetAddress * address );
bool TCPSend( Socket socket, const void * data, size_t n, size_t * sent );
bool TCPSendFile( Socket socket, FILE * file, size_t offset, size_t n, size_t * sent );
bool TCPReceive( Socket socket, void * data, size_t n, size_t * received );

NetAddress GetBroadcastAddress( u16 port );
NetAddress GetLoopbackAddress( u16 port );

enum WaitForSocketWriteableBool : bool {
	WaitForSocketWriteable_No = false,
	WaitForSocketWriteable_Yes = true,
};

struct WaitForSocketResult {
	bool readable;
	bool writeable;
};

void WaitForSockets( TempAllocator * temp, const Socket * sockets, size_t num_sockets,
	s64 timeout_ms, WaitForSocketWriteableBool wait_for_writeable,
	WaitForSocketResult * results );
