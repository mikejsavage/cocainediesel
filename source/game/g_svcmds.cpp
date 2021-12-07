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
#include "qcommon/fs.h"
#include "qcommon/string.h"

static void Cmd_ConsoleSay_f() {
	G_ChatMsg( NULL, NULL, false, "%s", Cmd_Args() );
}

static void Cmd_ConsoleKick_f() {
	edict_t *ent;

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: kick <id or name>\n" );
		return;
	}

	ent = G_PlayerForText( Cmd_Argv( 1 ) );
	if( !ent ) {
		Com_Printf( "No such player\n" );
		return;
	}

	PF_DropClient( ent, DROP_TYPE_NORECONNECT, "Kicked" );
}

//==============================================================================
//
//PACKET FILTERING
//
//
//You can add or remove addresses from the filter list with:
//
//addip <ip>
//removeip <ip>
//
//The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".
//
//Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.
//
//listip
//Prints the current list of filters.
//
//writeip
//Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.
//
//filterban <0 or 1>
//
//If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.
//
//If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.
//
//==============================================================================

typedef struct
{
	unsigned mask;
	unsigned compare;
	int64_t timeout;
} ipfilter_t;

#define MAX_IPFILTERS   1024

static ipfilter_t ipfilters[MAX_IPFILTERS];
static int numipfilters;

static bool StringToFilter( const char *s, ipfilter_t *f ) {
	char num[128];
	int i, j;
	uint8_t b[4];
	uint8_t m[4];

	for( i = 0; i < 4; i++ ) {
		b[i] = 0;
		m[i] = 0;
	}

	for( i = 0; i < 4; i++ ) {
		if( *s < '0' || *s > '9' ) {
			Com_Printf( "Bad filter address: %s\n", s );
			return false;
		}

		j = 0;
		while( *s >= '0' && *s <= '9' ) {
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi( num );
		if( b[i] != 0 ) {
			m[i] = 255;
		}

		if( !*s || *s == ':' ) {
			break;
		}
		s++;
	}

	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;

	return true;
}

void SV_ResetPacketFiltersTimeouts() {
	int i;

	for( i = 0; i < MAX_IPFILTERS; i++ )
		ipfilters[i].timeout = 0;
}

bool SV_FilterPacket( char *from ) {
	int i;
	unsigned in;
	uint8_t m[4];
	char *p;

	if( !filterban->integer ) {
		return false;
	}

	i = 0;
	p = from;
	while( *p && i < 4 ) {
		m[i] = 0;
		while( *p >= '0' && *p <= '9' ) {
			m[i] = m[i] * 10 + ( *p - '0' );
			p++;
		}
		if( !*p || *p == ':' ) {
			break;
		}
		i++, p++;
	}

	in = *(unsigned *)m;

	for( i = 0; i < numipfilters; i++ )
		if( ( in & ipfilters[i].mask ) == ipfilters[i].compare
			&& ( !ipfilters[i].timeout || ipfilters[i].timeout > svs.gametime ) ) {
			return true;
		}

	return false;
}

void SV_ReadIPList() {
	SV_ResetPacketFiltersTimeouts();

	Cbuf_ExecuteText( EXEC_APPEND, "exec listip.cfg\n" );
}

void SV_WriteIPList() {
	DynamicString output( sys_allocator, "set filterban {}\r\n", filterban->integer );

	for( int i = 0; i < numipfilters; i++ ) {
		if( ipfilters[i].timeout && ipfilters[i].timeout <= svs.gametime ) {
			continue;
		}

		const u8 * ip = ( const u8 * ) &ipfilters[ i ].compare;
		output.append( "addip {}.{}.{}.{}", ip[ 0 ], ip[ 1 ], ip[ 2 ], ip[ 3 ] );
		if( ipfilters[i].timeout != 0 ) {
			output.append( " {.2}", ( ipfilters[i].timeout - svs.gametime ) / ( 1000.0f * 60.0f ) );
		}
		output += "\r\n";
	}

	TempAllocator temp = svs.frame_arena.temp();
	if( !WriteFile( &temp, "listip.cfg", output.c_str(), output.length() ) ) {
		Com_Printf( "Couldn't write listip.cfg\n" );
	}
}

static void Cmd_AddIP_f() {
	int i;

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: addip <ip-mask> [time-mins]\n" );
		return;
	}

	for( i = 0; i < numipfilters; i++ )
		if( ipfilters[i].compare == 0xffffffff || ( ipfilters[i].timeout && ipfilters[i].timeout <= svs.gametime ) ) {
			break;
		}          // free spot
	if( i == numipfilters ) {
		if( numipfilters == MAX_IPFILTERS ) {
			Com_Printf( "IP filter list is full\n" );
			return;
		}
		numipfilters++;
	}

	ipfilters[i].timeout = 0;
	if( !StringToFilter( Cmd_Argv( 1 ), &ipfilters[i] ) ) {
		ipfilters[i].compare = 0xffffffff;
	} else if( Cmd_Argc() == 3 ) {
		ipfilters[i].timeout = svs.gametime + atof( Cmd_Argv( 2 ) ) * 60 * 1000;
	}
}

static void Cmd_RemoveIP_f() {
	ipfilter_t f;
	int i, j;

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: removeip <ip-mask>\n" );
		return;
	}

	if( !StringToFilter( Cmd_Argv( 1 ), &f ) ) {
		return;
	}

	for( i = 0; i < numipfilters; i++ )
		if( ipfilters[i].mask == f.mask
			&& ipfilters[i].compare == f.compare ) {
			for( j = i + 1; j < numipfilters; j++ )
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			Com_Printf( "Removed.\n" );
			return;
		}
	Com_Printf( "Didn't find %s.\n", Cmd_Argv( 1 ) );
}

static void Cmd_ListIP_f() {
	int i;
	uint8_t b[4];

	Com_Printf( "Filter list:\n" );
	for( i = 0; i < numipfilters; i++ ) {
		*(unsigned *)b = ipfilters[i].compare;
		if( ipfilters[i].timeout && ipfilters[i].timeout > svs.gametime ) {
			Com_Printf( "%3i.%3i.%3i.%3i %.2f\n", b[0], b[1], b[2], b[3],
					  (float)( ipfilters[i].timeout - svs.gametime ) / ( 60 * 1000.0f ) );
		} else if( !ipfilters[i].timeout ) {
			Com_Printf( "%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3] );
		}
	}
}

static void Cmd_WriteIP_f() {
	SV_WriteIPList();
}

void G_AddServerCommands() {
	if( is_dedicated_server ) {
		Cmd_AddCommand( "say", Cmd_ConsoleSay_f );
	}
	Cmd_AddCommand( "kick", Cmd_ConsoleKick_f );

	Cmd_AddCommand( "addip", Cmd_AddIP_f );
	Cmd_AddCommand( "removeip", Cmd_RemoveIP_f );
	Cmd_AddCommand( "listip", Cmd_ListIP_f );
	Cmd_AddCommand( "writeip", Cmd_WriteIP_f );
}

void G_RemoveCommands() {
	if( is_dedicated_server ) {
		Cmd_RemoveCommand( "say" );
	}
	Cmd_RemoveCommand( "kick" );

	Cmd_RemoveCommand( "addip" );
	Cmd_RemoveCommand( "removeip" );
	Cmd_RemoveCommand( "listip" );
	Cmd_RemoveCommand( "writeip" );
}
