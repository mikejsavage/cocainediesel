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

static int( WINAPI * pTransmitFile )( SOCKET hSocket,
	HANDLE hFile, DWORD nNumberOfBytesToWrite, DWORD nNumberOfBytesPerSend,
	LPOVERLAPPED lpOverlapped, LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers, DWORD dwReserved
	);

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

int Sys_NET_SocketIoctl( socket_handle_t handle, long request, ioctl_param_t* param ) {
	return ioctlsocket( handle, request, param );
}

int64_t Sys_NET_SendFile( socket_handle_t handle, int fileno, size_t offset, size_t count ) {
	OVERLAPPED ol = { 0 };
	HANDLE fhandle = (HANDLE) _get_osfhandle( fileno );
	DWORD sent;

	if( pTransmitFile == NULL ) {
		return SOCKET_ERROR;
	}
	if( fhandle == INVALID_HANDLE_VALUE ) {
		return SOCKET_ERROR;
	}

	ol.Pointer = (PVOID)( offset );
	pTransmitFile( handle, fhandle, count, 0, &ol, NULL, TF_USE_KERNEL_APC );

	// this blocks
	if( GetOverlappedResult( (HANDLE)handle, &ol, &sent, TRUE ) == FALSE ) {
		return SOCKET_ERROR;
	}

	return sent;
}

static void Sys_NET_InitFunctions() {
	SOCKET sock;
	GUID tf_guid = WSAID_TRANSMITFILE;
	DWORD bytes;

	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( sock == -1 ) {
		return;
	}

	if( WSAIoctl( sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &tf_guid, sizeof( GUID ),
				  &pTransmitFile, sizeof( LPFN_TRANSMITFILE ), &bytes, NULL, NULL ) == -1 ) {
		pTransmitFile = NULL;
	}

	closesocket( sock );
}

static void WSAError( const char * name ) {
	int err = WSAGetLastError();

	char buf[ 1024 ];
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), buf, sizeof( buf ), NULL );

	Com_Error( ERR_FATAL, "%s: %s (%d)", name, buf, err );
}

void Sys_NET_Init() {
	WSADATA wsa_data;
	if( WSAStartup( MAKEWORD( 2, 2 ), &wsa_data ) != 0 ) {
		WSAError( "WSAStartup" );
	}

	Sys_NET_InitFunctions();
}

void Sys_NET_Shutdown() {
	if( WSACleanup() != 0 ) {
		WSAError( "WSACleanup" );
	}
}
