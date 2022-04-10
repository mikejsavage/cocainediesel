#include "windows/miniwindows.h"
#include <io.h>

#include "windows/win_net.h"

#include "qcommon/base.h"
#include "qcommon/sys_net.h"

static void FatalWSA( const char * name ) {
	int err = WSAGetLastError();

	char buf[ 1024 ];
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), buf, sizeof( buf ), NULL );

	while( strlen( buf ) > 0 ) {
		char last = buf[ strlen( buf ) - 1 ];
		if( last != '\r' && last != '\n' )
			break;
		buf[ strlen( buf ) - 1 ] = '\0';
	}

	Fatal( "%s: %s (%d)", name, buf, err );
}

static u64 OSSocketToHandle( SOCKET socket ) {
	return socket == -1 ? 0 : socket + 1;
}

static SOCKET HandleToOSSocket( u64 handle ) {
	assert( handle != 0 );
	return checked_cast< SOCKET >( handle - 1 );
}

void InitNetworking() {
	WSADATA wsa_data;
	if( WSAStartup( MAKEWORD( 2, 2 ), &wsa_data ) != 0 ) {
		FatalWSA( "WSAStartup" );
	}
}

void ShutdownNetworking() {
	if( WSACleanup() != 0 ) {
		FatalWSA( "WSACleanup" );
	}
}

u64 OpenOSSocket( AddressFamily family, UDPOrTCP type, u16 port ) {
	int first_arg = family == AddressFamily_IPv4 ? AF_INET : AF_INET6;
	int second_arg = type == UDPOrTCP_UDP ? SOCK_DGRAM : SOCK_STREAM;
	int third_arg = type == UDPOrTCP_UDP ? IPPROTO_UDP : IPPROTO_TCP;

	SOCKET s = socket( first_arg, second_arg, third_arg );
	if( s == INVALID_SOCKET ) {
		FatalWSA( "socket" );
	}

	return OSSocketToHandle( s );
}

bool BindOSSocket( u64 handle, const sockaddr * address, int address_size ) {
	if( bind( HandleToOSSocket( handle ), address, address_size ) == SOCKET_ERROR ) {
		if( WSAGetLastError() == WSAEADDRNOTAVAIL ) {
			return false;
		}
		FatalWSA( "bind" );
	}

	return true;
}

void CloseOSSocket( u64 handle ) {
	if( closesocket( HandleToOSSocket( handle ) ) == SOCKET_ERROR ) {
		FatalWSA( "closesocket" );
	}
}

void OSSocketMakeNonblocking( u64 handle ) {
	u_long one = 1;
	if( ioctlsocket( HandleToOSSocket( handle ), FIONBIO, &one ) == SOCKET_ERROR ) {
		FatalWSA( "ioctlsocket" );
	}
}

void OSSocketSetSockOptOne( u64 handle, int level, int opt ) {
	int one = 1;
	if( setsockopt( HandleToOSSocket( handle ), level, opt, ( char * ) &one, sizeof( one ) ) == SOCKET_ERROR ) {
		FatalWSA( "setsockopt" );
	}
}

bool OSSocketSend( u64 handle, const void * data, size_t n, const sockaddr_storage * destination, size_t destination_size, size_t * sent ) {
	SOCKET socket = HandleToOSSocket( handle );

	int ret = sendto( socket, ( const char * ) data, checked_cast< int >( n ), 0,
		( const sockaddr * ) destination, checked_cast< int >( destination_size ) );
	if( ret == SOCKET_ERROR ) {
		int error = WSAGetLastError();
		if( error == WSAEWOULDBLOCK ) {
			*sent = 0;
			return true;
		}
		if( error == WSAECONNABORTED || error == WSAECONNRESET ) {
			return false;
		}
		FatalWSA( "sendto" );
	}

	*sent = checked_cast< size_t >( ret );
	return true;
}

bool OSSocketReceive( u64 handle, void * data, size_t n, sockaddr_storage * source, size_t * received ) {
	SOCKET socket = HandleToOSSocket( handle );

	int source_size = sizeof( sockaddr_in6 );
	int ret = recvfrom( socket, ( char * ) data, n, 0, ( sockaddr * ) source, &source_size );
	if( ret == SOCKET_ERROR ) {
		int error = WSAGetLastError();
		if( error == WSAEWOULDBLOCK ) {
			*received = 0;
			return true;
		}
		if( error == WSAECONNABORTED || error == WSAECONNRESET ) {
			return false;
		}
		FatalWSA( "recvfrom" );
	}

	*received = checked_cast< size_t >( ret );
	return true;
}

void OSSocketListen( u64 handle ) {
	if( handle == 0 ) {
		return;
	}

	SOCKET socket = HandleToOSSocket( handle );
	if( listen( socket, 8 ) == SOCKET_ERROR ) {
		FatalWSA( "listen" );
	}
}

u64 OSSocketAccept( u64 handle, sockaddr_storage * address ) {
	SOCKET socket = HandleToOSSocket( handle );

	int address_size = sizeof( sockaddr_in6 );
	SOCKET client = accept( socket, ( sockaddr * ) address, &address_size );
	if( client == INVALID_SOCKET ) {
		int error = WSAGetLastError();
		if( error == WSAEWOULDBLOCK ) {
			return 0;
		}
		FatalWSA( "accept" );
	}

	return OSSocketToHandle( client );
}

// TODO: use the proper windows api instead of select
void WaitForSockets( TempAllocator * temp, const Socket * sockets, size_t num_sockets, s64 timeout_ms, WaitForSocketWriteableBool wait_for_writeable, WaitForSocketResult * results ) {
	fd_set read_fds, write_fds;
	FD_ZERO( &read_fds );
	FD_ZERO( &write_fds );
	for( size_t i = 0; i < num_sockets; i++ ) {
		if( sockets[ i ].ipv4 != 0 ) {
			FD_SET( HandleToOSSocket( sockets[ i ].ipv4 ), &read_fds );
			FD_SET( HandleToOSSocket( sockets[ i ].ipv4 ), &write_fds );
		}
		if( sockets[ i ].ipv6 != 0 ) {
			FD_SET( HandleToOSSocket( sockets[ i ].ipv6 ), &read_fds );
			FD_SET( HandleToOSSocket( sockets[ i ].ipv6 ), &write_fds );
		}
	}

	timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = ( timeout_ms % 1000 ) * 1000;

	fd_set * write_fds_ptr = wait_for_writeable ? &write_fds : NULL;
	int ret = select( 0, &read_fds, write_fds_ptr, NULL, &timeout );
	if( ret == SOCKET_ERROR ) {
		FatalWSA( "select" );
	}

	if( ret == 0 || results == NULL )
		return;

	for( size_t i = 0; i < num_sockets; i++ ) {
		results[ i ] = { };

		if( sockets[ i ].ipv4 != 0 ) {
			if( FD_ISSET( HandleToOSSocket( sockets[ i ].ipv4 ), &read_fds ) ) {
				results[ i ].readable = true;
			}
			if( FD_ISSET( HandleToOSSocket( sockets[ i ].ipv4 ), &write_fds ) ) {
				results[ i ].writeable = true;
			}
		}

		if( sockets[ i ].ipv6 != 0 ) {
			if( FD_ISSET( HandleToOSSocket( sockets[ i ].ipv6 ), &read_fds ) ) {
				results[ i ].readable = true;
			}
			if( FD_ISSET( HandleToOSSocket( sockets[ i ].ipv6 ), &write_fds ) ) {
				results[ i ].writeable = true;
			}
		}
	}
}
