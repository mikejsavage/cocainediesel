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

#include "client/client.h"

#define SEMICOLON_BINDNAME  "SEMICOLON"

static char *keybindings[256];
static int key_repeats[256];   // if > 1, it is autorepeating
static bool keydown[256];

static bool key_initialized = false;

struct keyname_t {
	const char *name;
	int keynum;
};

static const keyname_t keynames[] =
{
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
	//	{"LWINKEY", K_LWIN},
	//	{"RWINKEY", K_RWIN},
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

	{ NULL, 0 }
};

/*
* Key_StringToKeynum
*
* Returns a key number to be used to index keybindings[] by looking at
* the given string.  Single ascii characters return themselves, while
* the K_* names are matched up.
*/
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

/*
* Key_KeynumToString
*
* Returns a string (either a single ascii char, or a K_* name) for the
* given keynum.
* FIXME: handle quote special (general escape sequence?)
*/
const char *Key_KeynumToString( int keynum ) {
	const keyname_t *kn;
	static char tinystr[2];

	if( keynum == -1 ) {
		return NULL;
	}
	if( keynum > 32 && keynum < 127 ) { // printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for( kn = keynames; kn->name; kn++ )
		if( keynum == kn->keynum ) {
			return kn->name;
		}

	return NULL;
}

/*
* Key_SetBinding
*/
void Key_SetBinding( int keynum, const char *binding ) {
	if( keynum == -1 ) {
		return;
	}

	// free old bindings
	if( keybindings[keynum] ) {
		Mem_ZoneFree( keybindings[keynum] );
		keybindings[keynum] = NULL;
	}

	if( !binding ) {
		return;
	}

	// allocate memory for new binding
	keybindings[keynum] = ZoneCopyString( binding );
}

/*
* Key_Unbind_f
*/
static void Key_Unbind_f( void ) {
	int b;

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if( b == -1 ) {
		Com_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	Key_SetBinding( b, NULL );
}

static void Key_Unbindall( void ) {
	int i;

	for( i = 0; i < 256; i++ ) {
		if( keybindings[i] ) {
			Key_SetBinding( i, NULL );
		}
	}
}

/*
* Key_Bind_f
*/
static void Key_Bind_f( void ) {
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();
	if( c < 2 ) {
		Com_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
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
	cmd[0] = 0; // start out with a null string
	for( i = 2; i < c; i++ ) {
		Q_strncatz( cmd, Cmd_Argv( i ), sizeof( cmd ) );
		if( i != ( c - 1 ) ) {
			Q_strncatz( cmd, " ", sizeof( cmd ) );
		}
	}

	Key_SetBinding( b, cmd );
}

/*
* Key_WriteBindings
*
* Writes lines containing "bind key value"
*/
void Key_WriteBindings( int file ) {
	int i;

	FS_Printf( file, "unbindall\r\n" );

	for( i = 0; i < 256; i++ )
		if( keybindings[i] && keybindings[i][0] ) {
			FS_Printf( file, "bind %s \"%s\"\r\n", ( i == ';' ? SEMICOLON_BINDNAME : Key_KeynumToString( i ) ), keybindings[i] );
		}
}

/*
* Key_Bindlist_f
*/
static void Key_Bindlist_f( void ) {
	int i;

	for( i = 0; i < 256; i++ )
		if( keybindings[i] && keybindings[i][0] ) {
			Com_Printf( "%s \"%s\"\n", Key_KeynumToString( i ), keybindings[i] );
		}
}

/*
* Key_Init
*/
void Key_Init( void ) {
	assert( !key_initialized );

	//
	// register our functions
	//
	Cmd_AddCommand( "bind", Key_Bind_f );
	Cmd_AddCommand( "unbind", Key_Unbind_f );
	Cmd_AddCommand( "unbindall", Key_Unbindall );
	Cmd_AddCommand( "bindlist", Key_Bindlist_f );

	key_initialized = true;
}

void Key_Shutdown( void ) {
	if( !key_initialized ) {
		return;
	}

	Cmd_RemoveCommand( "bind" );
	Cmd_RemoveCommand( "unbind" );
	Cmd_RemoveCommand( "unbindall" );
	Cmd_RemoveCommand( "bindlist" );

	Key_Unbindall();
}

/*
* Key_CharEvent
*
* Called by the system between frames for key down events for standard characters
* Should NOT be called during an interrupt!
*/
void Key_CharEvent( int key, wchar_t charkey ) {
	switch( cls.key_dest ) {
		case key_menu:
		case key_console:
		case key_message:
			UI_CharEvent( true, charkey );
			break;
		case key_game:
			break;
		default:
			Com_Error( ERR_FATAL, "Bad cls.key_dest" );
	}
}

void CloseChat(); // TODO: this shouldn't be here!

/*
* Key_Event
*
* Called by the system between frames for both key up and key down events
* Should NOT be called during an interrupt!
*/
void Key_Event( int key, bool down ) {
	char cmd[1024];

	// update auto-repeat status
	if( down ) {
		key_repeats[key]++;
		if( key_repeats[key] > 1 ) {
			if( ( key != K_BACKSPACE && key != K_DEL
				  && key != K_LEFTARROW && key != K_RIGHTARROW
				  && key != K_UPARROW && key != K_DOWNARROW
				  && key != K_PGUP && key != K_PGDN && ( key < 32 || key > 126 || key == '`' ) )
				|| cls.key_dest == key_game ) {
				return;
			}
		}
	} else {
		key_repeats[key] = 0;
	}

	// switch between fullscreen/windowed when ALT+ENTER is pressed
	if( key == K_ENTER && down && ( keydown[K_LALT] || keydown[K_RALT] ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "toggle vid_fullscreen\n" );
		return;
	}

#if defined ( __MACOSX__ )
	// quit the game when Control + q is pressed
	if( key == 'q' && down && keydown[K_COMMAND] ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
		return;
	}
#endif

	// menu key is hardcoded, so the user can never unbind it
	if( key == K_ESCAPE ) {
		if( !down ) {
			return;
		}

		if( cls.state != CA_ACTIVE ) {
			if( cls.key_dest == key_game || cls.key_dest == key_menu ) {
				if( cls.state != CA_DISCONNECTED ) {
					Cbuf_AddText( "disconnect\n" );
				} else if( cls.key_dest == key_menu ) {
					UI_KeyEvent( true, key, down );
				}
				return;
			}
		}

		switch( cls.key_dest ) {
			case key_message:
				CloseChat();
				break;
			case key_menu:
				UI_KeyEvent( true, key, down );
				break;
			case key_game:
				CL_GameModule_EscapeKey();
				break;
			case key_console:
				Con_ToggleConsole();
				break;
			default:
				Com_Error( ERR_FATAL, "Bad cls.key_dest" );
		}
		return;
	}

	//
	// if not a consolekey, send to the interpreter no matter what mode is
	//
	if( ( cls.key_dest == key_game && cls.state == CA_ACTIVE )
		|| ( cls.key_dest == key_message && ( key >= K_F1 && key <= K_F15 ) ) ) {
		const char *kb = keybindings[key];

		if( kb ) {
			if( kb[0] == '+' ) { // button commands add keynum as a parm
				if( down ) {
					Q_snprintfz( cmd, sizeof( cmd ), "%s %i\n", kb, key );
					Cbuf_AddText( cmd );
				} else if( keydown[key] ) {
					Q_snprintfz( cmd, sizeof( cmd ), "-%s %i\n", kb + 1, key );
					Cbuf_AddText( cmd );
				}
			} else if( down ) {
				Cbuf_AddText( kb );
				Cbuf_AddText( "\n" );
			}
		}
	}

	keydown[key] = down;

	if( cls.key_dest == key_menu || cls.key_dest == key_console || cls.key_dest == key_message ) {
		UI_KeyEvent( cls.key_dest == key_menu, key, down );
	}
}

/*
* Key_ClearStates
*/
void Key_ClearStates( void ) {
	for( int i = 0; i < 256; i++ ) {
		if( keydown[i] || key_repeats[i] ) {
			Key_Event( i, false );
		}
		keydown[i] = 0;
		key_repeats[i] = 0;
	}
}


/*
* Key_GetBindingBuf
*/
const char *Key_GetBindingBuf( int binding ) {
	return keybindings[binding];
}
