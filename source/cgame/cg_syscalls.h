/*
Copyright (C) 2002-2003 Victor Luchits

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

extern cgame_import_t CGAME_IMPORT;

static inline void trap_GetConfigString( int i, char *str, int size ) {
	CGAME_IMPORT.GetConfigString( i, str, size );
}

static inline bool trap_DownloadRequest( const char *filename ) {
	return CGAME_IMPORT.DownloadRequest( filename );
}

static inline void trap_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	CGAME_IMPORT.NET_GetUserCmd( frame, cmd );
}

static inline int trap_NET_GetCurrentUserCmdNum( void ) {
	return CGAME_IMPORT.NET_GetCurrentUserCmdNum();
}

static inline void trap_NET_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent ) {
	CGAME_IMPORT.NET_GetCurrentState( incomingAcknowledged, outgoingSequence, outgoingSent );
}

static inline void trap_VID_FlashWindow() {
	CGAME_IMPORT.VID_FlashWindow();
}
