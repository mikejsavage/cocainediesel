#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "unix/unix_net.h"

#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/sys_net.h"

void InitNetworking() { }
void ShutdownNetworking() { }

static u64 OSSocketToHandle( int socket ) {
	return socket == -1 ? 0 : socket + 1;
}

static int HandleToOSSocket( u64 handle ) {
	assert( handle != 0 );
	return checked_cast< int >( handle - 1 );
}

u64 OpenOSSocket( AddressFamily family, UDPOrTCP type, u16 port ) {
	int first_arg = family == AddressFamily_IPv4 ? AF_INET : AF_INET6;
	int second_arg = type == UDPOrTCP_UDP ? SOCK_DGRAM : SOCK_STREAM;
	int third_arg = type == UDPOrTCP_UDP ? IPPROTO_UDP : IPPROTO_TCP;

	int s = socket( first_arg, second_arg, third_arg );
	if( s == -1 ) {
		FatalErrno( "socket" );
	}

	return OSSocketToHandle( s );
}

bool BindOSSocket( u64 handle, const sockaddr * address, int address_size ) {
	if( bind( HandleToOSSocket( handle ), address, address_size ) == -1 ) {
		if( errno == EADDRNOTAVAIL ) {
			return false;
		}
		FatalErrno( "bind" );
	}

	return true;
}

void CloseOSSocket( u64 handle ) {
	if( close( HandleToOSSocket( handle ) ) == -1 ) {
		FatalErrno( "CloseOSSocket" );
	}
}

void OSSocketMakeNonblocking( u64 handle ) {
	int one = 1;
	if( ioctl( HandleToOSSocket( handle ), FIONBIO, &one ) == -1 ) {
		FatalErrno( "ioctl" );
	}
}

void OSSocketSetSockOptOne( u64 handle, int level, int opt ) {
	int one = 1;
	if( setsockopt( HandleToOSSocket( handle ), level, opt, ( char * ) &one, sizeof( one ) ) == -1 ) {
		FatalErrno( "setsockopt" );
	}
}

bool OSSocketSend( u64 handle, const void * data, size_t n, const sockaddr_storage * destination, size_t destination_size, size_t * sent ) {
	int socket = HandleToOSSocket( handle );

	while( true ) {
		int ret = sendto( socket, ( const char * ) data, checked_cast< int >( n ), MSG_NOSIGNAL,
			( const sockaddr * ) destination, checked_cast< int >( destination_size ) );
		if( ret == -1 ) {
			if( errno == EINTR ) {
				continue;
			}
			if( errno == EAGAIN ) {
				*sent = 0;
				return true;
			}
			if( errno == ECONNRESET || errno == ENETUNREACH ) {
				return false;
			}
			FatalErrno( "sendto" );
		}

		*sent = checked_cast< size_t >( ret );
		return true;
	}
}

bool OSSocketReceive( u64 handle, void * data, size_t n, sockaddr_storage * source, size_t * received ) {
	int socket = HandleToOSSocket( handle );
	socklen_t source_size = sizeof( sockaddr_in6 );

	while( true ) {
		int ret = recvfrom( socket, ( char * ) data, n, 0, ( sockaddr * ) source, &source_size );
		if( ret == -1 ) {
			if( errno == EINTR ) {
				continue;
			}
			if( errno == EAGAIN ) {
				*received = 0;
				return true;
			}
			if( errno == ECONNRESET ) {
				return false;
			}
			FatalErrno( "recvfrom" );
		}

		*received = checked_cast< size_t >( ret );
		return true;
	}
}

void OSSocketListen( u64 handle ) {
	if( handle == 0 ) {
		return;
	}

	int socket = HandleToOSSocket( handle );
	if( listen( socket, 8 ) == -1 ) {
		FatalErrno( "listen" );
	}
}

u64 OSSocketAccept( u64 handle, sockaddr_storage * address ) {
	int socket = HandleToOSSocket( handle );

	socklen_t address_size = sizeof( sockaddr_in6 );
	int client = accept( socket, ( sockaddr * ) address, &address_size );
	if( client == -1 ) {
		if( errno == EINTR || errno == EAGAIN ) {
			return 0;
		}
		FatalErrno( "accept" );
	}

	return OSSocketToHandle( client );
}

void WaitForSockets( TempAllocator * temp, const Socket * sockets, size_t num_sockets, s64 timeout_ms, WaitForSocketWriteableBool wait_for_writeable, WaitForSocketResult * results ) {
	short events = wait_for_writeable ? POLLIN | POLLOUT : POLLIN;

	DynamicArray< pollfd > fds( temp );
	for( size_t i = 0; i < num_sockets; i++ ) {
		if( sockets[ i ].ipv4 != 0 ) {
			pollfd fd = { HandleToOSSocket( sockets[ i ].ipv4 ), events };
			fds.add( fd );
		}
		if( sockets[ i ].ipv6 != 0 ) {
			pollfd fd = { HandleToOSSocket( sockets[ i ].ipv6 ), events };
			fds.add( fd );
		}
	}

	DynamicArray< size_t > fd_to_socket( temp );
	for( size_t i = 0; i < num_sockets; i++ ) {
		if( sockets[ i ].ipv4 != 0 ) {
			fd_to_socket.add( i );
		}
		if( sockets[ i ].ipv6 != 0 ) {
			fd_to_socket.add( i );
		}
	}

	int ret = poll( fds.ptr(), fds.size(), checked_cast< int >( timeout_ms ) );
	if( ret == -1 ) {
		if( errno == EINTR ) {
			return;
		}
		FatalErrno( "poll" );
	}

	if( ret == 0 || results == NULL )
		return;

	for( size_t i = 0; i < fds.size(); i++ ) {
		size_t idx = fd_to_socket[ i ];
		results[ idx ].readable = results[ idx ].readable || ( fds[ i ].revents & POLLIN ) != 0;
		results[ idx ].writeable = results[ idx ].writeable || ( fds[ i ].revents & POLLOUT ) != 0;
	}
}
