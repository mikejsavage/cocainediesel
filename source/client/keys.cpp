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

#include "qcommon/string.h"
#include "client/client.h"

static char *keybindings[256];
static bool keydown[256];

struct keyname_t {
	const char *name;
	int keynum;
};

static const keyname_t keynames[] = {
	{ "TAB", K_TAB },
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "CAPSLOCK", K_CAPSLOCK },
	{ "SCROLLLOCK", K_SCROLLLOCK },
	{ "SCROLLOCK", K_SCROLLLOCK },
	{ "NUMLOCK", K_NUMLOCK },
	{ "KP_NUMLOCK", K_NUMLOCK },
	{ "BACKSPACE", K_BACKSPACE },
	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "LALT", K_LALT },
	{ "RALT", K_RALT },
	{ "LCTRL", K_LCTRL },
	{ "RCTRL", K_RCTRL },
	{ "LSHIFT", K_LSHIFT },
	{ "RSHIFT", K_RSHIFT },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },
	{ "F13", K_F13 },
	{ "F14", K_F14 },
	{ "F15", K_F15 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "WINKEY", K_WIN },
	{ "POPUPMENU", K_MENU },

	{ "COMMAND", K_COMMAND },
	{ "OPTION", K_OPTION },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },
	{ "MOUSE6", K_MOUSE6 },
	{ "MOUSE7", K_MOUSE7 },
	{ "MOUSE8", K_MOUSE8 },

	{ "KP_HOME", KP_HOME },
	{ "KP_UPARROW", KP_UPARROW },
	{ "KP_PGUP", KP_PGUP },
	{ "KP_LEFTARROW", KP_LEFTARROW },
	{ "KP_5", KP_5 },
	{ "KP_RIGHTARROW", KP_RIGHTARROW },
	{ "KP_END", KP_END },
	{ "KP_DOWNARROW", KP_DOWNARROW },
	{ "KP_PGDN", KP_PGDN },
	{ "KP_ENTER", KP_ENTER },
	{ "KP_INS", KP_INS },
	{ "KP_DEL", KP_DEL },
	{ "KP_STAR", KP_STAR },
	{ "KP_SLASH", KP_SLASH },
	{ "KP_MINUS", KP_MINUS },
	{ "KP_PLUS", KP_PLUS },

	{ "KP_MULT", KP_MULT },
	{ "KP_EQUAL", KP_EQUAL },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "PAUSE", K_PAUSE },

	{ "SEMICOLON", ';' }, // because a raw semicolon separates commands
};

int Key_StringToKeynum( const char *str ) {
	const keyname_t *kn;

	if( !str || !str[0] ) {
		return -1;
	}
	if( !str[1] ) {
		return (int)(unsigned char)str[0];
	}

	for( kn = keynames; kn->name; kn++ ) {
		if( !Q_stricmp( str, kn->name ) ) {
			return kn->keynum;
		}
	}
	return -1;
}

const char *Key_KeynumToString( int keynum ) {
	static char tinystr[2];

	if( keynum > 32 && keynum < 127 ) { // printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for( keyname_t kn : keynames ) {
		if( keynum == kn.keynum ) {
			return kn.name;
		}
	}

	return NULL;
}

void Key_SetBinding( int keynum, const char *binding ) {
	if( keybindings[keynum] ) {
		Mem_ZoneFree( keybindings[keynum] );
		keybindings[keynum] = NULL;
	}

	if( binding != NULL ) {
		keybindings[keynum] = ZoneCopyString( binding );
	}
}

static void Key_Unbind_f() {
	if( Cmd_Argc() != 2 ) {
		Com_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	int key = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if( key == -1 ) {
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	Key_SetBinding( key, NULL );
}

static void Key_Unbindall() {
	for( int i = 0; i < int( ARRAY_COUNT( keybindings ) ); i++ ) {
		if( keybindings[ i ] ) {
			Key_SetBinding( i, NULL );
		}
	}
}

static void Key_Bind_f() {
	int c = Cmd_Argc();
	if( c < 2 ) {
		Com_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	int b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if( b == -1 ) {
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if( c == 2 ) {
		if( keybindings[b] ) {
			Com_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), keybindings[b] );
		} else {
			Com_Printf( "\"%s\" is not bound\n", Cmd_Argv( 1 ) );
		}
		return;
	}

	// copy the rest of the command line
	String< 1024 > cmd;
	for( int i = 2; i < c; i++ ) {
		if( i != 2 ) {
			cmd += " ";
		}
		cmd += Cmd_Argv( i );
	}

	Key_SetBinding( b, cmd.c_str() );
}

void Key_WriteBindings( int file ) {
	FS_Printf( file, "unbindall\r\n" );

	for( int i = 0; i < int( ARRAY_COUNT( keybindings ) ); i++ ) {
		if( keybindings[i] && keybindings[i][0] ) {
			FS_Printf( file, "bind %s \"%s\"\r\n", Key_KeynumToString( i ), keybindings[i] );
		}
	}
}

static void Key_Bindlist_f() {
	for( int i = 0; i < int( ARRAY_COUNT( keybindings ) ); i++ ) {
		if( keybindings[i] && keybindings[i][0] ) {
			Com_Printf( "%s \"%s\"\n", Key_KeynumToString( i ), keybindings[i] );
		}
	}
}

void Key_Init() {
	Cmd_AddCommand( "bind", Key_Bind_f );
	Cmd_AddCommand( "unbind", Key_Unbind_f );
	Cmd_AddCommand( "unbindall", Key_Unbindall );
	Cmd_AddCommand( "bindlist", Key_Bindlist_f );
}

void Key_Shutdown() {
	Cmd_RemoveCommand( "bind" );
	Cmd_RemoveCommand( "unbind" );
	Cmd_RemoveCommand( "unbindall" );
	Cmd_RemoveCommand( "bindlist" );

	Key_Unbindall();
}

void Key_Event( int key, bool down ) {
	if( key == K_ESCAPE ) {
		if( cls.key_dest != key_game || !down ) {
			return;
		}

		if( cls.state != CA_ACTIVE ) {
			Cbuf_AddText( "disconnect\n" );
			return;
		}

		CL_GameModule_EscapeKey();

		return;
	}

	if( cls.state == CA_ACTIVE && ( cls.key_dest == key_game || ( cls.key_dest != key_menu && key >= K_F1 && key <= K_F15 ) ) ) {
		const char *kb = keybindings[key];

		if( kb ) {
			if( kb[0] == '+' ) {
				char cmd[1024];
				snprintf( cmd, sizeof( cmd ), "%s%s %i\n", down ? "+" : "-", kb + 1, key );
				Cbuf_AddText( cmd );
			}
			else if( down ) {
				Cbuf_AddText( kb );
				Cbuf_AddText( "\n" );
			}
		}
	}

	keydown[ key ] = down;
}

void Key_ClearStates() {
	for( int i = 0; i < int( ARRAY_COUNT( keydown ) ); i++ ) {
		if( keydown[ i ] ) {
			Key_Event( i, false );
		}
	}
}

const char *Key_GetBindingBuf( int binding ) {
	return keybindings[binding];
}
