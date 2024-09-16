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
#include "gameshared/q_shared.h"
#include "client/client.h"

static Span< char > keybindings[ Key_Count ];
static bool keydown[ Key_Count ];

static constexpr struct {
	Span< const char > name;
	int key;
} keynames[] = {
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
};

Optional< int > Key_StringToKeynum( Span< const char > str ) {
	if( str.n == 1 ) {
		return ToLowerASCII( str[ 0 ] );
	}

	for( auto key : keynames ) {
		if( StrCaseEqual( str, key.name ) ) {
			return key.key;
		}
	}

	return NONE;
}

Span< const char > Key_KeynumToString( int key ) {
	constexpr const char * uppercase_ascii = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~";

	if( key >= '!' && key <= '~' ) {
		return Span< const char >( uppercase_ascii + key - '!', 1 );
	}

	for( auto k : keynames ) {
		if( key == k.key ) {
			return k.name;
		}
	}

	return Span< const char >();
}

void Key_SetBinding( int key, Span< const char > binding ) {
	Free( sys_allocator, keybindings[ key ].ptr );
	keybindings[ key ] = { };

	if( binding != "" ) {
		keybindings[ key ] = CloneSpan( sys_allocator, binding );
	}
}

static void Key_Unbind_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	Optional< int > key = Key_StringToKeynum( args.tokens[ 1 ] );
	if( !key.exists ) {
		Com_GGPrint( "\"{}\" isn't a valid key", args.tokens[ 1 ] );
		return;
	}

	Key_SetBinding( key.value, "" );
}

void Key_UnbindAll() {
	for( int i = 0; i < int( ARRAY_COUNT( keybindings ) ); i++ ) {
		Key_SetBinding( i, "" );
	}
}

static void Key_Bind_f( const Tokenized & args ) {
	if( args.tokens.n < 2 ) {
		Com_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	Optional< int > key = Key_StringToKeynum( args.tokens[ 1 ] );
	if( !key.exists ) {
		Com_GGPrint( "\"{}\" isn't a valid key", args.tokens[ 1 ] );
		return;
	}

	if( args.tokens.n == 2 ) {
		if( keybindings[ key.value ] != "" ) {
			Com_GGPrint( "\"{}\" = \"{}\"\n", args.tokens[ 1 ], keybindings[ key.value ] );
		}
		else {
			Com_GGPrint( "\"{}\" is not bound\n", args.tokens[ 1 ] );
		}
		return;
	}

	Span< const char > command = Span< const char >( args.tokens[ 2 ].ptr, args.tokens[ args.tokens.n - 1 ].end() - args.tokens[ 2 ].ptr );
	Key_SetBinding( key.value, command );
}

void Key_WriteBindings( DynamicString * config ) {
	config->append( "unbindall\r\n" );

	for( int i = 0; i < int( ARRAY_COUNT( keybindings ) ); i++ ) {
		if( keybindings[ i ] != "" ) {
			config->append( "bind {} \"{}\"\r\n", Key_KeynumToString( i ), keybindings[ i ] );
		}
	}
}

void Key_Init() {
	AddCommand( "bind", Key_Bind_f );
	AddCommand( "unbind", Key_Unbind_f );
	AddCommand( "unbindall", []( const Tokenized & args ) { Key_UnbindAll(); } );

	memset( keybindings, 0, sizeof( keybindings ) );
	memset( keydown, 0, sizeof( keydown ) );
}

void Key_Shutdown() {
	RemoveCommand( "bind" );
	RemoveCommand( "unbind" );
	RemoveCommand( "unbindall" );

	Key_UnbindAll();
}

void Key_Event( int key, bool down ) {
	if( key == K_ESCAPE ) {
		if( !down ) {
			return;
		}

		if( cls.state != CA_ACTIVE ) {
			CL_Disconnect( NULL );
			return;
		}

		CL_GameModule_EscapeKey();

		return;
	}

	if( cls.state == CA_ACTIVE ) {
		TempAllocator temp = cls.frame_arena.temp();
		Span< const char > command = keybindings[ key ];
		if( command != "" ) {
			if( StartsWith( command, "+" ) ) {
				Cmd_Execute( &temp, "{}{} {}", down ? "+" : "-", command + 1, key );
			}
			else if( down ) {
				Cmd_ExecuteLine( &temp, command, true );
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

	memset( keydown, 0, sizeof( keydown ) );
}

Span< const char > Key_GetBindingBuf( int binding ) {
	return keybindings[ binding ];
}
