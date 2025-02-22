#include "qcommon/base.h"
#include "qcommon/cmd.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/keys.h"

#define GLFW_INCLUDE_NONE
#include "glfw3/GLFW/glfw3.h"

#include "imgui/imgui.h"

static Span< char > binds[ Key_Count ];
static bool pressed[ Key_Count ];

struct KeyInfo {
	Key key;
	ImGuiKey imgui;
	Optional< int > glfw; // don't use GLFW_KEY_UNKNOWN because GLFW can emit that
	Span< const char > name;
	Span< const char > config_name;
};

static constexpr KeyInfo keys[] = {
	{ Key_Tab,            ImGuiKey_Tab,            GLFW_KEY_TAB,           "Tab",              "TAB" },
	{ Key_Enter,          ImGuiKey_Enter,          GLFW_KEY_ENTER,         "Enter",            "ENTER" },
	{ Key_Escape,         ImGuiKey_Escape,         GLFW_KEY_ESCAPE,        "Escape",           "ESCAPE" },
	{ Key_Space,          ImGuiKey_Space,          GLFW_KEY_SPACE,         "Space",            "SPACE" },
	{ Key_Backspace,      ImGuiKey_Backspace,      GLFW_KEY_BACKSPACE,     "Backspace",        "BACKSPACE" },
	{ Key_CapsLock,       ImGuiKey_CapsLock,       GLFW_KEY_CAPS_LOCK,     "Caps Lock",        "CAPSLOCK" },
	{ Key_ScrollLock,     ImGuiKey_ScrollLock,     GLFW_KEY_SCROLL_LOCK,   "Scroll Lock",      "SCROLLLOCK" },
	{ Key_NumLock,        ImGuiKey_NumLock,        GLFW_KEY_NUM_LOCK,      "Num Lock",         "KP_NUMLOCK" },
	{ Key_Pause,          ImGuiKey_Pause,          GLFW_KEY_PAUSE,         "Pause",            "PAUSE" },

	{ Key_UpArrow,        ImGuiKey_UpArrow,        GLFW_KEY_UP,            "Up Arrow",         "UPARROW" },
	{ Key_DownArrow,      ImGuiKey_DownArrow,      GLFW_KEY_DOWN,          "Down Arrow",       "DOWNARROW" },
	{ Key_LeftArrow,      ImGuiKey_LeftArrow,      GLFW_KEY_LEFT,          "Left Arrow",       "LEFTARROW" },
	{ Key_RightArrow,     ImGuiKey_RightArrow,     GLFW_KEY_RIGHT,         "Right Arrow",      "RIGHTARROW" },

	{ Key_Insert,         ImGuiKey_Insert,         GLFW_KEY_INSERT,        "Insert",           "INSERT" },
	{ Key_Delete,         ImGuiKey_Delete,         GLFW_KEY_DELETE,        "Delete",           "DEL" },
	{ Key_PageUp,         ImGuiKey_PageUp,         GLFW_KEY_PAGE_UP,       "Page Up",          "PGUP" },
	{ Key_PageDown,       ImGuiKey_PageDown,       GLFW_KEY_PAGE_DOWN,     "Page Down",        "PGDN" },
	{ Key_Home,           ImGuiKey_Home,           GLFW_KEY_HOME,          "Home",             "HOME" },
	{ Key_End,            ImGuiKey_End,            GLFW_KEY_END,           "End",              "END" },

	{ Key_LeftAlt,        ImGuiKey_LeftAlt,        GLFW_KEY_LEFT_ALT,      "Left Alt",         "LALT" },
	{ Key_RightAlt,       ImGuiKey_RightAlt,       GLFW_KEY_RIGHT_ALT,     "Right Alt",        "RALT" },
	{ Key_LeftCtrl,       ImGuiKey_LeftCtrl,       GLFW_KEY_LEFT_CONTROL,  "Left Ctrl",        "LCTRL" },
	{ Key_RightCtrl,      ImGuiKey_RightCtrl,      GLFW_KEY_RIGHT_CONTROL, "Right Ctrl",       "RCTRL" },
	{ Key_LeftShift,      ImGuiKey_LeftShift,      GLFW_KEY_LEFT_SHIFT,    "Left Shift",       "LSHIFT" },
	{ Key_RightShift,     ImGuiKey_RightShift,     GLFW_KEY_RIGHT_SHIFT,   "Right Shift",      "RSHIFT" },

	{ Key_F1,             ImGuiKey_F1,             GLFW_KEY_F1,            "F1",               "F1" },
	{ Key_F2,             ImGuiKey_F2,             GLFW_KEY_F2,            "F2",               "F2" },
	{ Key_F3,             ImGuiKey_F3,             GLFW_KEY_F3,            "F3",               "F3" },
	{ Key_F4,             ImGuiKey_F4,             GLFW_KEY_F4,            "F4",               "F4" },
	{ Key_F5,             ImGuiKey_F5,             GLFW_KEY_F5,            "F5",               "F5" },
	{ Key_F6,             ImGuiKey_F6,             GLFW_KEY_F6,            "F6",               "F6" },
	{ Key_F7,             ImGuiKey_F7,             GLFW_KEY_F7,            "F7",               "F7" },
	{ Key_F8,             ImGuiKey_F8,             GLFW_KEY_F8,            "F8",               "F8" },
	{ Key_F9,             ImGuiKey_F9,             GLFW_KEY_F9,            "F9",               "F9" },
	{ Key_F10,            ImGuiKey_F10,            GLFW_KEY_F10,           "F10",              "F10" },
	{ Key_F11,            ImGuiKey_F11,            GLFW_KEY_F11,           "F11",              "F11" },
	{ Key_F12,            ImGuiKey_F12,            GLFW_KEY_F12,           "F12",              "F12" },

	{ Key_0,              ImGuiKey_0,              GLFW_KEY_0,             "0",                "0" },
	{ Key_1,              ImGuiKey_1,              GLFW_KEY_1,             "1",                "1" },
	{ Key_2,              ImGuiKey_2,              GLFW_KEY_2,             "2",                "2" },
	{ Key_3,              ImGuiKey_3,              GLFW_KEY_3,             "3",                "3" },
	{ Key_4,              ImGuiKey_4,              GLFW_KEY_4,             "4",                "4" },
	{ Key_5,              ImGuiKey_5,              GLFW_KEY_5,             "5",                "5" },
	{ Key_6,              ImGuiKey_6,              GLFW_KEY_6,             "6",                "6" },
	{ Key_7,              ImGuiKey_7,              GLFW_KEY_7,             "7",                "7" },
	{ Key_8,              ImGuiKey_8,              GLFW_KEY_8,             "8",                "8" },
	{ Key_9,              ImGuiKey_9,              GLFW_KEY_9,             "9",                "9" },

	{ Key_A,              ImGuiKey_A,              GLFW_KEY_A,             "A",                "A" },
	{ Key_B,              ImGuiKey_B,              GLFW_KEY_B,             "B",                "B" },
	{ Key_C,              ImGuiKey_C,              GLFW_KEY_C,             "C",                "C" },
	{ Key_D,              ImGuiKey_D,              GLFW_KEY_D,             "D",                "D" },
	{ Key_E,              ImGuiKey_E,              GLFW_KEY_E,             "E",                "E" },
	{ Key_F,              ImGuiKey_F,              GLFW_KEY_F,             "F",                "F" },
	{ Key_G,              ImGuiKey_G,              GLFW_KEY_G,             "G",                "G" },
	{ Key_H,              ImGuiKey_H,              GLFW_KEY_H,             "H",                "H" },
	{ Key_I,              ImGuiKey_I,              GLFW_KEY_I,             "I",                "I" },
	{ Key_J,              ImGuiKey_J,              GLFW_KEY_J,             "J",                "J" },
	{ Key_K,              ImGuiKey_K,              GLFW_KEY_K,             "K",                "K" },
	{ Key_L,              ImGuiKey_L,              GLFW_KEY_L,             "L",                "L" },
	{ Key_M,              ImGuiKey_M,              GLFW_KEY_M,             "M",                "M" },
	{ Key_N,              ImGuiKey_N,              GLFW_KEY_N,             "N",                "N" },
	{ Key_O,              ImGuiKey_O,              GLFW_KEY_O,             "O",                "O" },
	{ Key_P,              ImGuiKey_P,              GLFW_KEY_P,             "P",                "P" },
	{ Key_Q,              ImGuiKey_Q,              GLFW_KEY_Q,             "Q",                "Q" },
	{ Key_R,              ImGuiKey_R,              GLFW_KEY_R,             "R",                "R" },
	{ Key_S,              ImGuiKey_S,              GLFW_KEY_S,             "S",                "S" },
	{ Key_T,              ImGuiKey_T,              GLFW_KEY_T,             "T",                "T" },
	{ Key_U,              ImGuiKey_U,              GLFW_KEY_U,             "U",                "U" },
	{ Key_V,              ImGuiKey_V,              GLFW_KEY_V,             "V",                "V" },
	{ Key_W,              ImGuiKey_W,              GLFW_KEY_W,             "W",                "W" },
	{ Key_X,              ImGuiKey_X,              GLFW_KEY_X,             "X",                "X" },
	{ Key_Y,              ImGuiKey_Y,              GLFW_KEY_Y,             "Y",                "Y" },
	{ Key_Z,              ImGuiKey_Z,              GLFW_KEY_Z,             "Z",                "Z" },

	{ Key_Apostrophe,     ImGuiKey_Apostrophe,     GLFW_KEY_APOSTROPHE,    "'",                "'" },
	{ Key_Comma,          ImGuiKey_Comma,          GLFW_KEY_COMMA,         ",",                "," },
	{ Key_Minus,          ImGuiKey_Minus,          GLFW_KEY_MINUS,         "-",                "-" },
	{ Key_Period,         ImGuiKey_Period,         GLFW_KEY_PERIOD,        ".",                "." },
	{ Key_Slash,          ImGuiKey_Slash,          GLFW_KEY_SLASH,         "/",                "/" },
	{ Key_Semicolon,      ImGuiKey_Semicolon,      GLFW_KEY_SEMICOLON,     ";",                ";" },
	{ Key_Equal,          ImGuiKey_Equal,          GLFW_KEY_EQUAL,         "=",                "=" },
	{ Key_LeftBracket,    ImGuiKey_LeftBracket,    GLFW_KEY_LEFT_BRACKET,  "[",                "[" },
	{ Key_Backslash,      ImGuiKey_Backslash,      GLFW_KEY_BACKSLASH,     "\\",               "\\" },
	{ Key_RightBracket,   ImGuiKey_RightBracket,   GLFW_KEY_RIGHT_BRACKET, "]",                "]" },

	{ Key_KeypadDecimal,  ImGuiKey_KeypadDecimal,  GLFW_KEY_KP_DECIMAL,    "Keypad Decimal",   "KP_DEL" },
	{ Key_KeypadDivide,   ImGuiKey_KeypadDivide,   GLFW_KEY_KP_DIVIDE,     "Keypad Divide",    "KP_SLASH" },
	{ Key_KeypadMultiply, ImGuiKey_KeypadMultiply, GLFW_KEY_KP_MULTIPLY,   "Keypad Multiply",  "KP_MULT" },
	{ Key_KeypadSubtract, ImGuiKey_KeypadSubtract, GLFW_KEY_KP_SUBTRACT,   "Keypad Subtract",  "KP_MINUS" },
	{ Key_KeypadAdd,      ImGuiKey_KeypadAdd,      GLFW_KEY_KP_ADD,        "Keypad Add",       "KP_PLUS" },
	{ Key_KeypadEnter,    ImGuiKey_KeypadEnter,    GLFW_KEY_KP_ENTER,      "Keypad Enter",     "KP_ENTER" },
	{ Key_KeypadEqual,    ImGuiKey_KeypadEqual,    GLFW_KEY_KP_EQUAL,      "Keypad Equal",     "KP_EQUAL" },
	{ Key_Keypad0,        ImGuiKey_Keypad0,        GLFW_KEY_KP_0,          "Keypad 0",         "KP_INS" },
	{ Key_Keypad1,        ImGuiKey_Keypad1,        GLFW_KEY_KP_1,          "Keypad 1",         "KP_END" },
	{ Key_Keypad2,        ImGuiKey_Keypad2,        GLFW_KEY_KP_2,          "Keypad 2",         "KP_DOWNARROW" },
	{ Key_Keypad3,        ImGuiKey_Keypad3,        GLFW_KEY_KP_3,          "Keypad 3",         "KP_PGDN" },
	{ Key_Keypad4,        ImGuiKey_Keypad4,        GLFW_KEY_KP_4,          "Keypad 4",         "KP_LEFTARROW" },
	{ Key_Keypad5,        ImGuiKey_Keypad5,        GLFW_KEY_KP_5,          "Keypad 5",         "KP_5" },
	{ Key_Keypad6,        ImGuiKey_Keypad6,        GLFW_KEY_KP_6,          "Keypad 6",         "KP_RIGHTARROW" },
	{ Key_Keypad7,        ImGuiKey_Keypad7,        GLFW_KEY_KP_7,          "Keypad 7",         "KP_HOME" },
	{ Key_Keypad8,        ImGuiKey_Keypad8,        GLFW_KEY_KP_8,          "Keypad 8",         "KP_UPARROW" },
	{ Key_Keypad9,        ImGuiKey_Keypad9,        GLFW_KEY_KP_9,          "Keypad 9",         "KP_PGUP" },

	{ Key_MouseLeft,      ImGuiKey_MouseLeft,      NONE,                   "Left Mouse",       "MOUSE1" },
	{ Key_MouseRight,     ImGuiKey_MouseRight,     NONE,                   "Right Mouse",      "MOUSE2" },
	{ Key_MouseMiddle,    ImGuiKey_MouseMiddle,    NONE,                   "Middle Mouse",     "MOUSE3" },
	{ Key_Mouse4,         ImGuiKey_MouseX1,        NONE,                   "Mouse 4",          "MOUSE4" },
	{ Key_Mouse5,         ImGuiKey_MouseX2,        NONE,                   "Mouse 5",          "MOUSE5" },
	{ Key_MouseWheelUp,   ImGuiKey_None,           NONE,                   "Mouse Wheel Up",   "MWHEELUP" },
	{ Key_MouseWheelDown, ImGuiKey_None,           NONE,                   "Mouse Wheel Down", "MWHEELDOWN" },
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

Optional< Key > KeyFromGLFW( int glfw ) {
	if( glfw == GLFW_KEY_UNKNOWN )
		return NONE;

	for( const KeyInfo & info : keys ) {
		if( info.glfw == glfw ) {
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
