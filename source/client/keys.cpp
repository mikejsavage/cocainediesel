#include "qcommon/base.h"
#include "qcommon/cmd.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/keys.h"

#include "imgui/imgui.h"

#include "sdl/SDL3/SDL_keycode.h"

static Span< char > binds[ Key_Count ];
static bool pressed[ Key_Count ];

struct KeyInfo {
	Key key;
	ImGuiKey imgui;
	Optional< SDL_Keycode > sdl; // don't use SDLK_UNKNOWN because GLFW would emit GLFW_KEY_UNKNOWN so maybe SDL does too
	Span< const char > name;
	Span< const char > config_name;
};

static constexpr KeyInfo keys[] = {
	{ Key_Tab,            ImGuiKey_Tab,            SDLK_TAB,          "Tab",              "TAB" },
	{ Key_Enter,          ImGuiKey_Enter,          SDLK_RETURN,       "Enter",            "ENTER" },
	{ Key_Escape,         ImGuiKey_Escape,         SDLK_ESCAPE,       "Escape",           "ESCAPE" },
	{ Key_Space,          ImGuiKey_Space,          SDLK_SPACE,        "Space",            "SPACE" },
	{ Key_Backspace,      ImGuiKey_Backspace,      SDLK_BACKSPACE,    "Backspace",        "BACKSPACE" },
	{ Key_CapsLock,       ImGuiKey_CapsLock,       SDLK_CAPSLOCK,     "Caps Lock",        "CAPSLOCK" },
	{ Key_ScrollLock,     ImGuiKey_ScrollLock,     SDLK_SCROLLLOCK,   "Scroll Lock",      "SCROLLLOCK" },
	{ Key_Pause,          ImGuiKey_Pause,          SDLK_PAUSE,        "Pause",            "PAUSE" },

	{ Key_UpArrow,        ImGuiKey_UpArrow,        SDLK_UP,           "Up Arrow",         "UPARROW" },
	{ Key_DownArrow,      ImGuiKey_DownArrow,      SDLK_DOWN,         "Down Arrow",       "DOWNARROW" },
	{ Key_LeftArrow,      ImGuiKey_LeftArrow,      SDLK_LEFT,         "Left Arrow",       "LEFTARROW" },
	{ Key_RightArrow,     ImGuiKey_RightArrow,     SDLK_RIGHT,        "Right Arrow",      "RIGHTARROW" },

	{ Key_Insert,         ImGuiKey_Insert,         SDLK_INSERT,       "Insert",           "INSERT" },
	{ Key_Delete,         ImGuiKey_Delete,         SDLK_DELETE,       "Delete",           "DEL" },
	{ Key_PageUp,         ImGuiKey_PageUp,         SDLK_PAGEUP,       "Page Up",          "PGUP" },
	{ Key_PageDown,       ImGuiKey_PageDown,       SDLK_PAGEDOWN,     "Page Down",        "PGDN" },
	{ Key_Home,           ImGuiKey_Home,           SDLK_HOME,         "Home",             "HOME" },
	{ Key_End,            ImGuiKey_End,            SDLK_END,          "End",              "END" },

	{ Key_LeftAlt,        ImGuiKey_LeftAlt,        SDLK_LALT,         "Left Alt",         "LALT" },
	{ Key_RightAlt,       ImGuiKey_RightAlt,       SDLK_RALT,         "Right Alt",        "RALT" },
	{ Key_LeftCtrl,       ImGuiKey_LeftCtrl,       SDLK_LCTRL,        "Left Ctrl",        "LCTRL" },
	{ Key_RightCtrl,      ImGuiKey_RightCtrl,      SDLK_RCTRL,        "Right Ctrl",       "RCTRL" },
	{ Key_LeftShift,      ImGuiKey_LeftShift,      SDLK_LSHIFT,       "Left Shift",       "LSHIFT" },
	{ Key_RightShift,     ImGuiKey_RightShift,     SDLK_RSHIFT,       "Right Shift",      "RSHIFT" },

	{ Key_F1,             ImGuiKey_F1,             SDLK_F1,           "F1",               "F1" },
	{ Key_F2,             ImGuiKey_F2,             SDLK_F2,           "F2",               "F2" },
	{ Key_F3,             ImGuiKey_F3,             SDLK_F3,           "F3",               "F3" },
	{ Key_F4,             ImGuiKey_F4,             SDLK_F4,           "F4",               "F4" },
	{ Key_F5,             ImGuiKey_F5,             SDLK_F5,           "F5",               "F5" },
	{ Key_F6,             ImGuiKey_F6,             SDLK_F6,           "F6",               "F6" },
	{ Key_F7,             ImGuiKey_F7,             SDLK_F7,           "F7",               "F7" },
	{ Key_F8,             ImGuiKey_F8,             SDLK_F8,           "F8",               "F8" },
	{ Key_F9,             ImGuiKey_F9,             SDLK_F9,           "F9",               "F9" },
	{ Key_F10,            ImGuiKey_F10,            SDLK_F10,          "F10",              "F10" },
	{ Key_F11,            ImGuiKey_F11,            SDLK_F11,          "F11",              "F11" },
	{ Key_F12,            ImGuiKey_F12,            SDLK_F12,          "F12",              "F12" },

	{ Key_0,              ImGuiKey_0,              SDLK_0,            "0",                "0" },
	{ Key_1,              ImGuiKey_1,              SDLK_1,            "1",                "1" },
	{ Key_2,              ImGuiKey_2,              SDLK_2,            "2",                "2" },
	{ Key_3,              ImGuiKey_3,              SDLK_3,            "3",                "3" },
	{ Key_4,              ImGuiKey_4,              SDLK_4,            "4",                "4" },
	{ Key_5,              ImGuiKey_5,              SDLK_5,            "5",                "5" },
	{ Key_6,              ImGuiKey_6,              SDLK_6,            "6",                "6" },
	{ Key_7,              ImGuiKey_7,              SDLK_7,            "7",                "7" },
	{ Key_8,              ImGuiKey_8,              SDLK_8,            "8",                "8" },
	{ Key_9,              ImGuiKey_9,              SDLK_9,            "9",                "9" },

	{ Key_A,              ImGuiKey_A,              SDLK_A,            "A",                "A" },
	{ Key_B,              ImGuiKey_B,              SDLK_B,            "B",                "B" },
	{ Key_C,              ImGuiKey_C,              SDLK_C,            "C",                "C" },
	{ Key_D,              ImGuiKey_D,              SDLK_D,            "D",                "D" },
	{ Key_E,              ImGuiKey_E,              SDLK_E,            "E",                "E" },
	{ Key_F,              ImGuiKey_F,              SDLK_F,            "F",                "F" },
	{ Key_G,              ImGuiKey_G,              SDLK_G,            "G",                "G" },
	{ Key_H,              ImGuiKey_H,              SDLK_H,            "H",                "H" },
	{ Key_I,              ImGuiKey_I,              SDLK_I,            "I",                "I" },
	{ Key_J,              ImGuiKey_J,              SDLK_J,            "J",                "J" },
	{ Key_K,              ImGuiKey_K,              SDLK_K,            "K",                "K" },
	{ Key_L,              ImGuiKey_L,              SDLK_L,            "L",                "L" },
	{ Key_M,              ImGuiKey_M,              SDLK_M,            "M",                "M" },
	{ Key_N,              ImGuiKey_N,              SDLK_N,            "N",                "N" },
	{ Key_O,              ImGuiKey_O,              SDLK_O,            "O",                "O" },
	{ Key_P,              ImGuiKey_P,              SDLK_P,            "P",                "P" },
	{ Key_Q,              ImGuiKey_Q,              SDLK_Q,            "Q",                "Q" },
	{ Key_R,              ImGuiKey_R,              SDLK_R,            "R",                "R" },
	{ Key_S,              ImGuiKey_S,              SDLK_S,            "S",                "S" },
	{ Key_T,              ImGuiKey_T,              SDLK_T,            "T",                "T" },
	{ Key_U,              ImGuiKey_U,              SDLK_U,            "U",                "U" },
	{ Key_V,              ImGuiKey_V,              SDLK_V,            "V",                "V" },
	{ Key_W,              ImGuiKey_W,              SDLK_W,            "W",                "W" },
	{ Key_X,              ImGuiKey_X,              SDLK_X,            "X",                "X" },
	{ Key_Y,              ImGuiKey_Y,              SDLK_Y,            "Y",                "Y" },
	{ Key_Z,              ImGuiKey_Z,              SDLK_Z,            "Z",                "Z" },

	{ Key_Apostrophe,     ImGuiKey_Apostrophe,     SDLK_APOSTROPHE,   "'",                "'" },
	{ Key_Comma,          ImGuiKey_Comma,          SDLK_COMMA,        ",",                "," },
	{ Key_Minus,          ImGuiKey_Minus,          SDLK_MINUS,        "-",                "-" },
	{ Key_Period,         ImGuiKey_Period,         SDLK_PERIOD,       ".",                "." },
	{ Key_Slash,          ImGuiKey_Slash,          SDLK_SLASH,        "/",                "/" },
	{ Key_Semicolon,      ImGuiKey_Semicolon,      SDLK_SEMICOLON,    ";",                ";" },
	{ Key_Equal,          ImGuiKey_Equal,          SDLK_EQUALS,       "=",                "=" },
	{ Key_LeftBracket,    ImGuiKey_LeftBracket,    SDLK_LEFTBRACKET,  "[",                "[" },
	{ Key_Backslash,      ImGuiKey_Backslash,      SDLK_BACKSLASH,    "\\",               "\\" },
	{ Key_RightBracket,   ImGuiKey_RightBracket,   SDLK_RIGHTBRACKET, "]",                "]" },

	{ Key_KeypadDecimal,  ImGuiKey_KeypadDecimal,  SDLK_KP_DECIMAL,   "Keypad Decimal",   "KP_DEL" },
	{ Key_KeypadDivide,   ImGuiKey_KeypadDivide,   SDLK_KP_DIVIDE,    "Keypad Divide",    "KP_SLASH" },
	{ Key_KeypadMultiply, ImGuiKey_KeypadMultiply, SDLK_KP_MULTIPLY,  "Keypad Multiply",  "KP_MULT" },
	{ Key_KeypadSubtract, ImGuiKey_KeypadSubtract, SDLK_KP_MINUS,     "Keypad Subtract",  "KP_MINUS" },
	{ Key_KeypadAdd,      ImGuiKey_KeypadAdd,      SDLK_KP_PLUS,      "Keypad Add",       "KP_PLUS" },
	{ Key_KeypadEnter,    ImGuiKey_KeypadEnter,    SDLK_KP_ENTER,     "Keypad Enter",     "KP_ENTER" },
	{ Key_KeypadEqual,    ImGuiKey_KeypadEqual,    SDLK_KP_EQUALS,    "Keypad Equal",     "KP_EQUAL" },
	{ Key_Keypad0,        ImGuiKey_Keypad0,        SDLK_KP_0,         "Keypad 0",         "KP_INS" },
	{ Key_Keypad1,        ImGuiKey_Keypad1,        SDLK_KP_1,         "Keypad 1",         "KP_END" },
	{ Key_Keypad2,        ImGuiKey_Keypad2,        SDLK_KP_2,         "Keypad 2",         "KP_DOWNARROW" },
	{ Key_Keypad3,        ImGuiKey_Keypad3,        SDLK_KP_3,         "Keypad 3",         "KP_PGDN" },
	{ Key_Keypad4,        ImGuiKey_Keypad4,        SDLK_KP_4,         "Keypad 4",         "KP_LEFTARROW" },
	{ Key_Keypad5,        ImGuiKey_Keypad5,        SDLK_KP_5,         "Keypad 5",         "KP_5" },
	{ Key_Keypad6,        ImGuiKey_Keypad6,        SDLK_KP_6,         "Keypad 6",         "KP_RIGHTARROW" },
	{ Key_Keypad7,        ImGuiKey_Keypad7,        SDLK_KP_7,         "Keypad 7",         "KP_HOME" },
	{ Key_Keypad8,        ImGuiKey_Keypad8,        SDLK_KP_8,         "Keypad 8",         "KP_UPARROW" },
	{ Key_Keypad9,        ImGuiKey_Keypad9,        SDLK_KP_9,         "Keypad 9",         "KP_PGUP" },

	{ Key_MouseLeft,      ImGuiKey_MouseLeft,      NONE,              "Left Mouse",       "MOUSE1" },
	{ Key_MouseRight,     ImGuiKey_MouseRight,     NONE,              "Right Mouse",      "MOUSE2" },
	{ Key_MouseMiddle,    ImGuiKey_MouseMiddle,    NONE,              "Middle Mouse",     "MOUSE3" },
	{ Key_Mouse4,         ImGuiKey_MouseX1,        NONE,              "Mouse 4",          "MOUSE4" },
	{ Key_Mouse5,         ImGuiKey_MouseX2,        NONE,              "Mouse 5",          "MOUSE5" },
	{ Key_MouseWheelUp,   ImGuiKey_None,           NONE,              "Mouse Wheel Up",   "MWHEELUP" },
	{ Key_MouseWheelDown, ImGuiKey_None,           NONE,              "Mouse Wheel Down", "MWHEELDOWN" },
};

static Optional< Key > KeyFromName( Span< const char > name ) {
	for( const KeyInfo & info : keys ) {
		if( StrCaseEqual( info.config_name, name ) ) {
			return info.key;
		}
	}

	return NONE;
}

static void BindKeyCmd( const Tokenized & args ) {
	if( args.tokens.n < 2 ) {
		Com_Printf( "Usage: bind <key> [command] - bind key to command or print an existing bind\n" );
		return;
	}

	Optional< Key > key = KeyFromName( args.tokens[ 1 ] );
	if( !key.exists ) {
		Com_GGPrint( "\"{}\" isn't a valid key", args.tokens[ 1 ] );
		return;
	}

	if( args.tokens.n == 2 ) {
		if( binds[ key.value ] != "" ) {
			Com_GGPrint( "\"{}\" = \"{}\"\n", args.tokens[ 1 ], binds[ key.value ] );
		}
		else {
			Com_GGPrint( "\"{}\" is not bound\n", args.tokens[ 1 ] );
		}
		return;
	}

	Span< const char > command = Span< const char >( args.tokens[ 2 ].ptr, args.tokens[ args.tokens.n - 1 ].end() - args.tokens[ 2 ].ptr );
	SetKeyBind( key.value, command );
}

static void UnbindKeyCmd( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "Usage: unbind <key>\n" );
		return;
	}

	Optional< Key > key = KeyFromName( args.tokens[ 1 ] );
	if( !key.exists ) {
		Com_GGPrint( "\"{}\" isn't a valid key", args.tokens[ 1 ] );
		return;
	}

	SetKeyBind( key.value, "" );
}

void InitKeys() {
	AddCommand( "bind", BindKeyCmd );
	AddCommand( "unbind", UnbindKeyCmd );
	AddCommand( "unbindall", []( const Tokenized & args ) { UnbindAllKeys(); } );

	memset( binds, 0, sizeof( binds ) );
	memset( pressed, 0, sizeof( pressed ) );
}

void ShutdownKeys() {
	RemoveCommand( "bind" );
	RemoveCommand( "unbind" );
	RemoveCommand( "unbindall" );

	UnbindAllKeys();
}

void KeyEvent( Key key, bool down ) {
	if( key == Key_Escape ) {
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
		Span< const char > command = binds[ key ];
		if( command != "" ) {
			if( StartsWith( command, "+" ) ) {
				Cmd_Execute( &temp, "{}{} {}", down ? "+" : "-", command + 1, key );
			}
			else if( down ) {
				Cmd_ExecuteLine( &temp, command, true );
			}
		}
	}

	pressed[ key ] = down;
}

void AllKeysUp() {
	for( Key i = Key( 0 ); i < Key_Count; i++ ) {
		if( pressed[ i ] ) {
			KeyEvent( i, false );
		}
	}
}

void SetKeyBind( Key key, Span< const char > binding ) {
	Free( sys_allocator, binds[ key ].ptr );
	binds[ key ] = { };

	if( binding != "" ) {
		binds[ key ] = CloneSpan( sys_allocator, binding );
	}
}

Span< const char > GetKeyBind( Key key ) {
	return binds[ key ];
}

void UnbindKey( Key key ) {
	SetKeyBind( key, "" );
}

void UnbindAllKeys() {
	for( Key i = Key( 0 ); i < Key_Count; i++ ) {
		UnbindKey( i );
	}
}

static KeyInfo GetKeyInfo( Key key ) {
	for( const auto & info : keys ) {
		if( info.key == key ) {
			return info;
		}
	}

	Assert( false );
	return { };
}

Span< const char > KeyName( Key key ) {
	return GetKeyInfo( key ).name;
}


void GetKeyBindsForCommand( Span< const char > command, Optional< Key > * key1, Optional< Key > * key2 ) {
	*key1 = NONE;
	*key2 = NONE;

	if( command == "" )
		return;

	for( Key i = Key( 0 ); i < Key_Count; i++ ) {
		if( !StrCaseEqual( GetKeyBind( i ), command ) )
			continue;

		if( !key1->exists ) {
			*key1 = i;
		}
		else if( !key2->exists ) {
			*key2 = i;
			break;
		}
	}
}

ImGuiKey KeyToImGui( Key key ) {
	return GetKeyInfo( key ).imgui;
}

Optional< Key > KeyFromSDL( SDL_Keycode sdl ) {
	for( const KeyInfo & info : keys ) {
		if( info.sdl == sdl ) {
			return info.key;
		}
	}

	return NONE;
}

Optional< Key > KeyFromImGui( ImGuiKey imgui ) {
	for( const KeyInfo & info : keys ) {
		if( info.imgui == imgui ) {
			return info.key;
		}
	}

	return NONE;
}

void WriteKeyBindingsConfig( DynamicString * config ) {
	config->append( "unbindall\r\n" );

	for( Key i = Key( 0 ); i < Key_Count; i++ ) {
		if( binds[ i ] != "" ) {
			config->append( "bind {} \"{}\"\r\n", GetKeyInfo( i ).config_name, binds[ i ] );
		}
	}
}
