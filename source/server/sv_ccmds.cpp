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

#include "server/server.h"
#include "qcommon/fs.h"
#include "qcommon/maplist.h"


//===============================================================================
//
//OPERATOR CONSOLE ONLY COMMANDS
//
//These commands can only be entered from stdin or by a remote operator datagram
//===============================================================================

/*
* SV_Map_f
*
* User command to change the map
* map: restart game, and start map
* devmap: restart game, enable cheats, and start map
*/
static void SV_Map_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <map>\n", Cmd_Argv( 0 ) );
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();

	const char * map = Cmd_Argv( 1 );
	const char * bsp_path = temp( "{}/base/maps/{}.bsp", RootDirPath(), map );
	const char * zst_path = temp( "{}.zst", bsp_path );

	if( !FileExists( &temp, bsp_path ) && !FileExists( &temp, zst_path ) ) {
		Com_Printf( "Couldn't find map: %s\n", map );
		return;
	}

	if( StrCaseEqual( Cmd_Argv( 0 ), "map" ) || StrCaseEqual( Cmd_Argv( 0 ), "devmap" ) ) {
		sv.state = ss_dead; // don't save current level when changing
	}

	SV_UpdateMaster();

	// start up the next map
	SV_Map( map, StrCaseEqual( Cmd_Argv( 0 ), "devmap" ) );
}

//===============================================================

void SV_Status_f() {
	int i, j, l;
	client_t *cl;
	const char *s;
	int ping;
	if( !svs.clients ) {
		Com_Printf( "No server running.\n" );
		return;
	}
	Com_Printf( "map              : %s\n", sv.mapname );

	Com_Printf( "num score ping name                            lastmsg address               session         \n" );
	Com_Printf( "--- ----- ---- ------------------------------- ------- --------------------- ----------------\n" );
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		if( !cl->state ) {
			continue;
		}
		Com_Printf( "%3i ", i );
		Com_Printf( "%5i ", cl->edict->r.client->r.frags );

		if( cl->state == CS_CONNECTED ) {
			Com_Printf( "CNCT " );
		} else if( cl->state == CS_ZOMBIE ) {
			Com_Printf( "ZMBI " );
		} else if( cl->state == CS_CONNECTING ) {
			Com_Printf( "AWAI " );
		} else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf( "%4i ", ping );
		}

		Com_Printf( "%s", cl->name );
		l = MAX_NAME_CHARS - (int)strlen( cl->name );
		for( j = 0; j < l; j++ )
			Com_Printf( " " );

		Com_Printf( "%7i ", (int)(svs.realtime - cl->lastPacketReceivedTime) );

		s = NET_AddressToString( &cl->netchan.remoteAddress );
		Com_Printf( "%s", s );
		l = 21 - (int)strlen( s );
		for( j = 0; j < l; j++ )
			Com_Printf( " " );
		Com_Printf( " " ); // always add at least one space between the columns because IPv6 addresses are long

		Com_GGPrint( "{16x}", cl->netchan.session_id );
	}
	Com_Printf( "\n" );
}

static void SV_Heartbeat_f() {
	svc.nextHeartbeat = Sys_Milliseconds();
}

/*
* SV_KillServer_f
* Kick everyone off, possibly in preparation for a new game
*/
static void SV_KillServer_f() {
	if( !svs.initialized ) {
		return;
	}

	SV_ShutdownGame( "Server was killed", false );
}

//===========================================================

void SV_InitOperatorCommands() {
	AddCommand( "heartbeat", SV_Heartbeat_f );
	AddCommand( "status", SV_Status_f );

	AddCommand( "map", SV_Map_f );
	AddCommand( "devmap", SV_Map_f );
	AddCommand( "killserver", SV_KillServer_f );

	AddCommand( "serverrecord", SV_Demo_Start_f );
	AddCommand( "serverrecordstop", SV_Demo_Stop_f );
	AddCommand( "serverrecordcancel", SV_Demo_Cancel_f );

	if( is_dedicated_server ) {
		AddCommand( "serverrecordpurge", SV_Demo_Purge_f );
	}

	SetTabCompletionCallback( "map", CompleteMapName );
	SetTabCompletionCallback( "devmap", CompleteMapName );
}

void SV_ShutdownOperatorCommands() {
	RemoveCommand( "heartbeat" );
	RemoveCommand( "status" );

	RemoveCommand( "map" );
	RemoveCommand( "devmap" );
	RemoveCommand( "killserver" );

	RemoveCommand( "serverrecord" );
	RemoveCommand( "serverrecordstop" );
	RemoveCommand( "serverrecordcancel" );

	if( is_dedicated_server ) {
		RemoveCommand( "serverrecordpurge" );
	}
}
