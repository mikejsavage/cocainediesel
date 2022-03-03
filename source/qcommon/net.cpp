#include "qcommon/base.h"
#include "gameshared/q_shared.h"
#include "qcommon/sys_net.h"

#if PLATFORM_WINDOWS
#include "windows/win_net.h"
#else
#include "unix/unix_net.h"
#endif

static NetAddress SockaddrToNetAddress( const sockaddr_storage * sockaddr ) {
	NetAddress address = { };
	if( sockaddr->ss_family == AF_INET ) {
		const sockaddr_in * ipv4 = ( const sockaddr_in * ) sockaddr;
		address.family = SocketFamily_IPv4;
		address.port = ntohs( ipv4->sin_port );
		memcpy( &address.ipv4, &ipv4->sin_addr.s_addr, sizeof( address.ipv4 ) );
	}
	else {
		const sockaddr_in6 * ipv6 = ( const sockaddr_in6 * ) sockaddr;
		address.family = SocketFamily_IPv6;
		address.port = ntohs( ipv6->sin6_port );
		memcpy( &address.ipv6, &ipv6->sin6_addr.s6_addr, sizeof( address.ipv6 ) );
	}
	return address;
}

static sockaddr_storage NetAddressToSockaddr( NetAddress address, socklen_t * size ) {
	sockaddr_storage sockaddr = { };
	if( address.family == SocketFamily_IPv4 ) {
		sockaddr_in & ipv4 = ( sockaddr_in & ) sockaddr;
		sockaddr.ss_family = AF_INET;
		ipv4.sin_port = htons( address.port );
		memcpy( &ipv4.sin_addr.s_addr, &address.ipv4, sizeof( address.ipv4 ) );
		if( size != NULL ) {
			*size = sizeof( ipv4 );
		}
	}
	else {
		sockaddr_in6 & ipv6 = ( sockaddr_in6 & ) sockaddr;
		sockaddr.ss_family = AF_INET6;
		ipv6.sin6_port = htons( address.port );
		memcpy( &ipv6.sin6_addr.s6_addr, &address.ipv6, sizeof( address.ipv6 ) );
		if( size != NULL ) {
			*size = sizeof( ipv6 );
		}
	}
	return sockaddr;
}

static bool operator==( const IPv4 & a, const IPv4 & b ) {
	return memcmp( &a, &b, sizeof( a ) ) == 0;
}

static bool operator==( const IPv6 & a, const IPv6 & b ) {
	return memcmp( &a, &b, sizeof( a ) ) == 0;
}

bool operator==( const NetAddress & a, const NetAddress & b ) {
	if( a.family != b.family || a.port != b.port )
		return false;
	return a.family == SocketFamily_IPv4 ? a.ipv4 == b.ipv4 : a.ipv6 == b.ipv6;
}

bool operator!=( const NetAddress & a, const NetAddress & b ) {
	return !( a == b );
}

bool EqualIgnoringPort( const NetAddress & a, const NetAddress & b ) {
	if( a.family != b.family )
		return false;
	return a.family == SocketFamily_IPv4 ? a.ipv4 == b.ipv4 : a.ipv6 == b.ipv6;
}

void format( FormatBuffer * fb, const NetAddress & address, const FormatOpts & opts ) {
	sockaddr_storage sockaddr = NetAddressToSockaddr( address, NULL );
	char ntop[ INET6_ADDRSTRLEN ];

	const void * src = ( const void * ) &( ( const sockaddr_in * ) &sockaddr )->sin_addr;
	if( address.family == SocketFamily_IPv6 ) {
		src = ( const void * ) &( ( const sockaddr_in6 * ) &sockaddr )->sin6_addr;
	}
	inet_ntop( sockaddr.ss_family, src, ntop, sizeof( ntop ) );

	char buf[ 64 ];
	if( address.family == SocketFamily_IPv4 ) {
		ggformat( buf, sizeof( buf ), "{}:{}", ntop, address.port );
	}
	else {
		ggformat( buf, sizeof( buf ), "[{}]:{}", ntop, address.port );
	}

	format( fb, buf, opts );
}

char * SplitIntoHostnameAndPort( Allocator * a, const char * str, u16 * port ) {
	*port = 0;

	bool is_portless_ipv6 = false;
	if( str[ 0 ] != '[' ) {
		const char * first_colon = strchr( str, ':' );
		if( first_colon != NULL ) {
			const char *  second_colon = strchr( first_colon + 1, ':' );
			is_portless_ipv6 = second_colon != NULL;
		}
	}

	const char * last_colon = strrchr( str, ':' );
	if( last_colon == NULL || is_portless_ipv6 ) {
		return CopyString( a, str );
	}

	*port = SpanToU64( MakeSpan( last_colon + 1 ), 0 );
	return ( *a )( "{}", Span< const char >( str, last_colon - str ) );
}

bool DNS( const char * hostname, NetAddress * address, DNSFamily family ) {
	struct addrinfo hints = { };
	hints.ai_flags = AI_ADDRCONFIG;
	switch( family ) {
		case DNSFamily_Any: hints.ai_family = AF_UNSPEC; break;
		case DNSFamily_IPv4: hints.ai_family = AF_INET; break;
		case DNSFamily_IPv6: hints.ai_family = AF_INET6; break;
	}

	struct addrinfo * addresses;
	int err = getaddrinfo( hostname, "1", &hints, &addresses );
	if( err != 0 ) {
		return false;
	}

	*address = SockaddrToNetAddress( ( const sockaddr_storage * ) addresses->ai_addr );
	freeaddrinfo( addresses );
	return true;
}

static u64 OpenSocket( SocketFamily family, UDPOrTCP type, NonBlockingBool nonblocking, u16 port ) {
	u64 handle = OpenOSSocket( family, type, port );
	if( handle == 0 ) {
		return 0;
	}

	if( nonblocking ) {
		OSSocketMakeNonblocking( handle );
	}

	if( type == UDPOrTCP_UDP ) {
		OSSocketSetSockOptOne( handle, SOL_SOCKET, SO_BROADCAST );
	}
	else {
		OSSocketSetSockOptOne( handle, IPPROTO_TCP, TCP_NODELAY );
	}
	if( family == SocketFamily_IPv6 ) {
		OSSocketSetSockOptOne( handle, IPPROTO_IPV6, IPV6_V6ONLY );
	}

	sockaddr_in address4;
	address4.sin_family = AF_INET;
	address4.sin_port = htons( port );
	address4.sin_addr.s_addr = htonl( INADDR_ANY );

	sockaddr_in6 address6;
	address6.sin6_family = AF_INET6;
	address6.sin6_port = htons( port );
	address6.sin6_addr = in6addr_any;

	const sockaddr * address = family == SocketFamily_IPv4 ? ( const sockaddr * ) &address4 : ( const sockaddr * ) &address6;
	int address_size = family == SocketFamily_IPv4 ? sizeof( address4 ) : sizeof( address6 );

	if( !BindOSSocket( handle, address, address_size ) ) {
		CloseOSSocket( handle );
		return 0;
	}

	return handle;
}

Socket NewUDPClient( NonBlockingBool nonblocking ) {
	Socket socket = { };
	socket.type = SocketType_UDPClient;
	socket.ipv4 = OpenSocket( SocketFamily_IPv4, UDPOrTCP_UDP, nonblocking, 0 );
	socket.ipv6 = OpenSocket( SocketFamily_IPv6, UDPOrTCP_UDP, nonblocking, 0 );
	return socket;
}

Socket NewUDPServer( u16 port, NonBlockingBool nonblocking ) {
	Socket socket = { };
	socket.type = SocketType_UDPServer;
	socket.ipv4 = OpenSocket( SocketFamily_IPv4, UDPOrTCP_UDP, nonblocking, port );
	socket.ipv6 = OpenSocket( SocketFamily_IPv6, UDPOrTCP_UDP, nonblocking, port );
	return socket;
}

Socket NewTCPServer( u16 port, NonBlockingBool nonblocking ) {
	Socket socket = { };
	socket.type = SocketType_TCPServer;
	socket.ipv4 = OpenSocket( SocketFamily_IPv4, UDPOrTCP_TCP, nonblocking, port );
	OSSocketListen( socket.ipv4 );
	socket.ipv6 = OpenSocket( SocketFamily_IPv6, UDPOrTCP_TCP, nonblocking, port );
	OSSocketListen( socket.ipv6 );
	return socket;
}

void CloseSocket( Socket socket ) {
	if( socket.ipv4 != 0 ) {
		CloseOSSocket( socket.ipv4 );
	}
	if( socket.ipv6 != 0 ) {
		CloseOSSocket( socket.ipv6 );
	}
}

size_t UDPSend( Socket socket, NetAddress destination, const void * data, size_t n ) {
	assert( socket.type == SocketType_UDPClient || socket.type == SocketType_UDPServer );

	if( destination == NULL_ADDRESS ) {
		return 0;
	}

	u64 handle = destination.family == SocketFamily_IPv4 ? socket.ipv4 : socket.ipv6;
	if( handle == 0 ) {
		return 0;
	}

	socklen_t sockaddr_size;
	sockaddr_storage sockaddr = NetAddressToSockaddr( destination, &sockaddr_size );

	size_t sent;
	OSSocketSend( handle, data, n, &sockaddr, sockaddr_size, &sent );
	return sent;
}

size_t UDPReceive( Socket socket, NetAddress * source, void * data, size_t n ) {
	assert( socket.type == SocketType_UDPClient || socket.type == SocketType_UDPServer );

	if( socket.ipv4 != 0 ) {
		sockaddr_storage sockaddr;
		size_t received;
		OSSocketReceive( socket.ipv4, data, n, &sockaddr, &received );
		if( received > 0 ) {
			*source = SockaddrToNetAddress( &sockaddr );
			return received;
		}
	}

	if( socket.ipv6 != 0 ) {
		sockaddr_storage sockaddr;
		size_t received;
		OSSocketReceive( socket.ipv6, data, n, &sockaddr, &received );
		if( received > 0 ) {
			*source = SockaddrToNetAddress( &sockaddr );
			return received;
		}
	}

	return 0;
}

bool TCPAccept( Socket server, NonBlockingBool nonblocking, Socket * client, NetAddress * address ) {
	assert( server.type == SocketType_TCPServer );

	if( server.ipv4 != 0 ) {
		sockaddr_storage sockaddr;
		u64 accepted = OSSocketAccept( server.ipv4, &sockaddr );
		if( accepted != 0 ) {
			if( nonblocking ) {
				OSSocketMakeNonblocking( accepted );
			}

			*client = { };
			client->type = SocketType_TCPClient;
			client->ipv4 = accepted;

			*address = SockaddrToNetAddress( &sockaddr );

			return true;
		}
	}

	if( server.ipv6 != 0 ) {
		sockaddr_storage sockaddr;
		u64 accepted = OSSocketAccept( server.ipv6, &sockaddr );
		if( accepted != 0 ) {
			if( nonblocking ) {
				OSSocketMakeNonblocking( accepted );
			}

			*client = { };
			client->type = SocketType_TCPClient;
			client->ipv6 = accepted;

			*address = SockaddrToNetAddress( &sockaddr );

			return true;
		}
	}

	return false;
}

bool TCPSend( Socket socket, const void * data, size_t n, size_t * sent ) {
	assert( socket.type == SocketType_TCPClient );

	u64 handle = socket.ipv4 == 0 ? socket.ipv6 : socket.ipv4;
	assert( handle != 0 );

	return OSSocketSend( handle, data, n, NULL, 0, sent );
}

bool TCPReceive( Socket socket, void * data, size_t n, size_t * received ) {
	assert( socket.type == SocketType_TCPClient );

	u64 handle = socket.ipv4 == 0 ? socket.ipv4 : socket.ipv4;
	assert( handle != 0 );

	return OSSocketReceive( handle, data, n, NULL, received );
}

NetAddress GetBroadcastAddress( u16 port ) {
	NetAddress address = { };
	address.family = SocketFamily_IPv4;
	address.port = port;

	u32 broadcast = htonl( INADDR_BROADCAST );
	memcpy( address.ipv4.ip, &broadcast, sizeof( address.ipv4.ip ) );

	return address;
}

NetAddress GetLoopbackAddress( u16 port ) {
	NetAddress address = { };
	address.family = SocketFamily_IPv4;
	address.port = port;
	address.ipv4.ip[ 0 ] = 127;
	address.ipv4.ip[ 1 ] = 0;
	address.ipv4.ip[ 2 ] = 0;
	address.ipv4.ip[ 3 ] = 1;
	return address;
}
