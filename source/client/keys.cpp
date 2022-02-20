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

#include <ctype.h>

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

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },
	{ "MOUSE6", K_MOUSE6 },
	{ "MOUSE7", K_MOUSE7 },

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

	{ nullptr, 0 }
};

int Key_StringToKeynum( const char *str ) {
	const keyname_t *kn;

	if( !str || !str[0] ) {
		return -1;
	}
	if( !str[1] ) {
		return tolower( (unsigned char)str[0] );
	}

	for( kn = keynames; kn->name; kn++ ) {
		if( StrCaseEqual( str, kn->name ) ) {
			return kn->keynum;
		}
	}
	return -1;
}

Span< const char > Key_KeynumToString( int keynum ) {
	static constexpr const char uppercase_ascii[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~";

	if( keynum > 32 && keynum < 127 ) {
		return Span< const char >( uppercase_ascii + keynum - '!', 1 );
	}

	for( keyname_t kn : keynames ) {
		if( keynum == kn.keynum ) {
			return MakeSpan( kn.name );
		}
	}

	return Span< const char >();
}

void Key_SetBinding( int keynum, const char *binding ) {
	if( keybindings[keynum] ) {
		FREE( sys_allocator, keybindings[keynum] );
		keybindings[keynum] = NULL;
	}

	if( binding != NULL ) {
		keybindings[keynum] = CopyString( sys_allocator, binding );
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

void Key_WriteBindings( DynamicString * config ) {
	config->append( "unbindall\r\n" );

	for( int i = 0; i < int( ARRAY_COUNT( keybindings ) ); i++ ) {
		if( keybindings[i] && keybindings[i][0] ) {
			config->append( "bind {} \"{}\"\r\n", Key_KeynumToString( i ), keybindings[ i ] );
		}
	}
}

void Key_Init() {
	AddCommand( "bind", Key_Bind_f );
	AddCommand( "unbind", Key_Unbind_f );
	AddCommand( "unbindall", Key_Unbindall );
}

void Key_Shutdown() {
	RemoveCommand( "bind" );
	RemoveCommand( "unbind" );
	RemoveCommand( "unbindall" );

	Key_Unbindall();
}

void Key_Event( int key, bool down ) {
	if( key == K_ESCAPE ) {
		if( cls.key_dest != key_game || !down ) {
			return;
		}

		if( cls.state != CA_ACTIVE ) {
			CL_Disconnect_f();
			return;
		}

		CL_GameModule_EscapeKey();

		return;
	}

	if( cls.state == CA_ACTIVE && ( cls.key_dest == key_game || !down || ( key >= K_F1 && key <= K_F12 ) ) ) {
		const char * command = keybindings[ key ];
		if( command != NULL ) {
			if( StartsWith( command, "+" ) ) {
				Cbuf_Add( "{}{} {}", down ? "+" : "-", command + 1, key );
			}
			else if( down ) {
				Cbuf_Add( "{}", command );
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
