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

#include "windows/miniwindows.h"
#include <io.h>
#include <winsock2.h>
#include <mswsock.h>

#include "qcommon/qcommon.h"
#include "qcommon/sys_net.h"

net_error_t Sys_NET_GetLastError() {
	int error = WSAGetLastError();
	switch( error ) {
		case 0:                 return NET_ERR_NONE;
		case WSAEMSGSIZE:       return NET_ERR_MSGSIZE;
		case WSAECONNRESET:     return NET_ERR_CONNRESET;
		case WSAEWOULDBLOCK:    return NET_ERR_WOULDBLOCK;
		case WSAEAFNOSUPPORT:   return NET_ERR_UNSUPPORTED;
		case ERROR_IO_PENDING:  return NET_ERR_WOULDBLOCK;
		default:                return NET_ERR_UNKNOWN;
	}
}

void Sys_NET_SocketClose( socket_handle_t handle ) {
	closesocket( handle );
}

bool Sys_NET_SocketMakeNonBlocking( socket_handle_t handle ) {
	u_long one = 1;
	return ioctlsocket( handle, FIONBIO, &one ) != SOCKET_ERROR;
}

static void WSAError( const char * name ) {
	int err = WSAGetLastError();

	char buf[ 1024 ];
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), buf, sizeof( buf ), NULL );

	Fatal( "%s: %s (%d)", name, buf, err );
}

void Sys_NET_Init() {
	WSADATA wsa_data;
	if( WSAStartup( MAKEWORD( 2, 2 ), &wsa_data ) != 0 ) {
		WSAError( "WSAStartup" );
	}
}

void Sys_NET_Shutdown() {
	if( WSACleanup() != 0 ) {
		WSAError( "WSACleanup" );
	}
}
