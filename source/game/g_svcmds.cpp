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

#include "game/g_local.h"

static void Cmd_ConsoleSay_f() {
	G_ChatMsg( NULL, NULL, false, "%s", Cmd_Args() );
}

static void Cmd_ConsoleKick_f() {
	if( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: kick <id or name>\n" );
		return;
	}

	edict_t * ent = G_PlayerForText( Cmd_Argv( 1 ) );
	if( !ent ) {
		Com_Printf( "No such player\n" );
		return;
	}

	PF_DropClient( ent, "Kicked" );
}

void G_AddServerCommands() {
	if( is_dedicated_server ) {
		AddCommand( "say", Cmd_ConsoleSay_f );
	}
	AddCommand( "kick", Cmd_ConsoleKick_f );
}

void G_RemoveCommands() {
	if( is_dedicated_server ) {
		RemoveCommand( "say" );
	}
	RemoveCommand( "kick" );
}
