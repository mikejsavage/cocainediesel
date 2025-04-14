#include "qcommon/base.h"
#include "qcommon/cmd.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/keys.h"

#include "imgui/imgui.h"

#include "sdl/SDL3/SDL_scancode.h"
#include "sdl/SDL3/SDL_keycode.h"
#include "sdl/SDL3/SDL_keyboard.h"

static Span< char > binds[ Key_Count ];
static bool pressed[ Key_Count ];

struct KeyInfo {
	Key key;
	ImGuiKey imgui;
	Optional< SDL_Scancode > sdl; // don't use SDLK_UNKNOWN because GLFW would emit GLFW_KEY_UNKNOWN so maybe SDL does too
	Span< const char > config_name;
	Span< const char > name;
};

static constexpr KeyInfo keys[] = {
	{ Key_Tab,            ImGuiKey_Tab,            SDL_SCANCODE_TAB,          "TAB" },
	{ Key_Enter,          ImGuiKey_Enter,          SDL_SCANCODE_RETURN,       "ENTER" },
	{ Key_Escape,         ImGuiKey_Escape,         SDL_SCANCODE_ESCAPE,       "ESCAPE" },
	{ Key_Space,          ImGuiKey_Space,          SDL_SCANCODE_SPACE,        "SPACE" },
	{ Key_Backspace,      ImGuiKey_Backspace,      SDL_SCANCODE_BACKSPACE,    "BACKSPACE" },
	{ Key_CapsLock,       ImGuiKey_CapsLock,       SDL_SCANCODE_CAPSLOCK,     "CAPSLOCK" },
	{ Key_ScrollLock,     ImGuiKey_ScrollLock,     SDL_SCANCODE_SCROLLLOCK,   "SCROLLLOCK" },
	{ Key_Pause,          ImGuiKey_Pause,          SDL_SCANCODE_PAUSE,        "PAUSE" },

	{ Key_UpArrow,        ImGuiKey_UpArrow,        SDL_SCANCODE_UP,           "UPARROW" },
	{ Key_DownArrow,      ImGuiKey_DownArrow,      SDL_SCANCODE_DOWN,         "DOWNARROW" },
	{ Key_LeftArrow,      ImGuiKey_LeftArrow,      SDL_SCANCODE_LEFT,         "LEFTARROW" },
	{ Key_RightArrow,     ImGuiKey_RightArrow,     SDL_SCANCODE_RIGHT,        "RIGHTARROW" },

	{ Key_Insert,         ImGuiKey_Insert,         SDL_SCANCODE_INSERT,       "INSERT" },
	{ Key_Delete,         ImGuiKey_Delete,         SDL_SCANCODE_DELETE,       "DEL" },
	{ Key_PageUp,         ImGuiKey_PageUp,         SDL_SCANCODE_PAGEUP,       "PGUP" },
	{ Key_PageDown,       ImGuiKey_PageDown,       SDL_SCANCODE_PAGEDOWN,     "PGDN" },
	{ Key_Home,           ImGuiKey_Home,           SDL_SCANCODE_HOME,         "HOME" },
	{ Key_End,            ImGuiKey_End,            SDL_SCANCODE_END,          "END" },

	{ Key_LeftAlt,        ImGuiKey_LeftAlt,        SDL_SCANCODE_LALT,         "LALT" },
	{ Key_RightAlt,       ImGuiKey_RightAlt,       SDL_SCANCODE_RALT,         "RALT" },
	{ Key_LeftCtrl,       ImGuiKey_LeftCtrl,       SDL_SCANCODE_LCTRL,        "LCTRL" },
	{ Key_RightCtrl,      ImGuiKey_RightCtrl,      SDL_SCANCODE_RCTRL,        "RCTRL" },
	{ Key_LeftShift,      ImGuiKey_LeftShift,      SDL_SCANCODE_LSHIFT,       "LSHIFT" },
	{ Key_RightShift,     ImGuiKey_RightShift,     SDL_SCANCODE_RSHIFT,       "RSHIFT" },

	{ Key_F1,             ImGuiKey_F1,             SDL_SCANCODE_F1,           "F1" },
	{ Key_F2,             ImGuiKey_F2,             SDL_SCANCODE_F2,           "F2" },
	{ Key_F3,             ImGuiKey_F3,             SDL_SCANCODE_F3,           "F3" },
	{ Key_F4,             ImGuiKey_F4,             SDL_SCANCODE_F4,           "F4" },
	{ Key_F5,             ImGuiKey_F5,             SDL_SCANCODE_F5,           "F5" },
	{ Key_F6,             ImGuiKey_F6,             SDL_SCANCODE_F6,           "F6" },
	{ Key_F7,             ImGuiKey_F7,             SDL_SCANCODE_F7,           "F7" },
	{ Key_F8,             ImGuiKey_F8,             SDL_SCANCODE_F8,           "F8" },
	{ Key_F9,             ImGuiKey_F9,             SDL_SCANCODE_F9,           "F9" },
	{ Key_F10,            ImGuiKey_F10,            SDL_SCANCODE_F10,          "F10" },
	{ Key_F11,            ImGuiKey_F11,            SDL_SCANCODE_F11,          "F11" },
	{ Key_F12,            ImGuiKey_F12,            SDL_SCANCODE_F12,          "F12" },

	{ Key_0,              ImGuiKey_0,              SDL_SCANCODE_0,            "0" },
	{ Key_1,              ImGuiKey_1,              SDL_SCANCODE_1,            "1" },
	{ Key_2,              ImGuiKey_2,              SDL_SCANCODE_2,            "2" },
	{ Key_3,              ImGuiKey_3,              SDL_SCANCODE_3,            "3" },
	{ Key_4,              ImGuiKey_4,              SDL_SCANCODE_4,            "4" },
	{ Key_5,              ImGuiKey_5,              SDL_SCANCODE_5,            "5" },
	{ Key_6,              ImGuiKey_6,              SDL_SCANCODE_6,            "6" },
	{ Key_7,              ImGuiKey_7,              SDL_SCANCODE_7,            "7" },
	{ Key_8,              ImGuiKey_8,              SDL_SCANCODE_8,            "8" },
	{ Key_9,              ImGuiKey_9,              SDL_SCANCODE_9,            "9" },

	{ Key_A,              ImGuiKey_A,              SDL_SCANCODE_A,            "A" },
	{ Key_B,              ImGuiKey_B,              SDL_SCANCODE_B,            "B" },
	{ Key_C,              ImGuiKey_C,              SDL_SCANCODE_C,            "C" },
	{ Key_D,              ImGuiKey_D,              SDL_SCANCODE_D,            "D" },
	{ Key_E,              ImGuiKey_E,              SDL_SCANCODE_E,            "E" },
	{ Key_F,              ImGuiKey_F,              SDL_SCANCODE_F,            "F" },
	{ Key_G,              ImGuiKey_G,              SDL_SCANCODE_G,            "G" },
	{ Key_H,              ImGuiKey_H,              SDL_SCANCODE_H,            "H" },
	{ Key_I,              ImGuiKey_I,              SDL_SCANCODE_I,            "I" },
	{ Key_J,              ImGuiKey_J,              SDL_SCANCODE_J,            "J" },
	{ Key_K,              ImGuiKey_K,              SDL_SCANCODE_K,            "K" },
	{ Key_L,              ImGuiKey_L,              SDL_SCANCODE_L,            "L" },
	{ Key_M,              ImGuiKey_M,              SDL_SCANCODE_M,            "M" },
	{ Key_N,              ImGuiKey_N,              SDL_SCANCODE_N,            "N" },
	{ Key_O,              ImGuiKey_O,              SDL_SCANCODE_O,            "O" },
	{ Key_P,              ImGuiKey_P,              SDL_SCANCODE_P,            "P" },
	{ Key_Q,              ImGuiKey_Q,              SDL_SCANCODE_Q,            "Q" },
	{ Key_R,              ImGuiKey_R,              SDL_SCANCODE_R,            "R" },
	{ Key_S,              ImGuiKey_S,              SDL_SCANCODE_S,            "S" },
	{ Key_T,              ImGuiKey_T,              SDL_SCANCODE_T,            "T" },
	{ Key_U,              ImGuiKey_U,              SDL_SCANCODE_U,            "U" },
	{ Key_V,              ImGuiKey_V,              SDL_SCANCODE_V,            "V" },
	{ Key_W,              ImGuiKey_W,              SDL_SCANCODE_W,            "W" },
	{ Key_X,              ImGuiKey_X,              SDL_SCANCODE_X,            "X" },
	{ Key_Y,              ImGuiKey_Y,              SDL_SCANCODE_Y,            "Y" },
	{ Key_Z,              ImGuiKey_Z,              SDL_SCANCODE_Z,            "Z" },

	{ Key_Apostrophe,     ImGuiKey_Apostrophe,     SDL_SCANCODE_APOSTROPHE,   "'" },
	{ Key_Comma,          ImGuiKey_Comma,          SDL_SCANCODE_COMMA,        "," },
	{ Key_Minus,          ImGuiKey_Minus,          SDL_SCANCODE_MINUS,        "-" },
	{ Key_Period,         ImGuiKey_Period,         SDL_SCANCODE_PERIOD,       "." },
	{ Key_Slash,          ImGuiKey_Slash,          SDL_SCANCODE_SLASH,        "/" },
	{ Key_Semicolon,      ImGuiKey_Semicolon,      SDL_SCANCODE_SEMICOLON,    ";" },
	{ Key_Equal,          ImGuiKey_Equal,          SDL_SCANCODE_EQUALS,       "=" },
	{ Key_LeftBracket,    ImGuiKey_LeftBracket,    SDL_SCANCODE_LEFTBRACKET,  "[" },
	{ Key_Backslash,      ImGuiKey_Backslash,      SDL_SCANCODE_BACKSLASH,    "\\" },
	{ Key_RightBracket,   ImGuiKey_RightBracket,   SDL_SCANCODE_RIGHTBRACKET, "]" },

	{ Key_KeypadDecimal,  ImGuiKey_KeypadDecimal,  SDL_SCANCODE_KP_DECIMAL,   "KP_DEL" },
	{ Key_KeypadDivide,   ImGuiKey_KeypadDivide,   SDL_SCANCODE_KP_DIVIDE,    "KP_SLASH" },
	{ Key_KeypadMultiply, ImGuiKey_KeypadMultiply, SDL_SCANCODE_KP_MULTIPLY,  "KP_MULT" },
	{ Key_KeypadSubtract, ImGuiKey_KeypadSubtract, SDL_SCANCODE_KP_MINUS,     "KP_MINUS" },
	{ Key_KeypadAdd,      ImGuiKey_KeypadAdd,      SDL_SCANCODE_KP_PLUS,      "KP_PLUS" },
	{ Key_KeypadEnter,    ImGuiKey_KeypadEnter,    SDL_SCANCODE_KP_ENTER,     "KP_ENTER" },
	{ Key_KeypadEqual,    ImGuiKey_KeypadEqual,    SDL_SCANCODE_KP_EQUALS,    "KP_EQUAL" },
	{ Key_Keypad0,        ImGuiKey_Keypad0,        SDL_SCANCODE_KP_0,         "KP_INS" },
	{ Key_Keypad1,        ImGuiKey_Keypad1,        SDL_SCANCODE_KP_1,         "KP_END" },
	{ Key_Keypad2,        ImGuiKey_Keypad2,        SDL_SCANCODE_KP_2,         "KP_DOWNARROW" },
	{ Key_Keypad3,        ImGuiKey_Keypad3,        SDL_SCANCODE_KP_3,         "KP_PGDN" },
	{ Key_Keypad4,        ImGuiKey_Keypad4,        SDL_SCANCODE_KP_4,         "KP_LEFTARROW" },
	{ Key_Keypad5,        ImGuiKey_Keypad5,        SDL_SCANCODE_KP_5,         "KP_5" },
	{ Key_Keypad6,        ImGuiKey_Keypad6,        SDL_SCANCODE_KP_6,         "KP_RIGHTARROW" },
	{ Key_Keypad7,        ImGuiKey_Keypad7,        SDL_SCANCODE_KP_7,         "KP_HOME" },
	{ Key_Keypad8,        ImGuiKey_Keypad8,        SDL_SCANCODE_KP_8,         "KP_UPARROW" },
	{ Key_Keypad9,        ImGuiKey_Keypad9,        SDL_SCANCODE_KP_9,         "KP_PGUP" },

	{ Key_MouseLeft,      ImGuiKey_MouseLeft,      NONE,                      "MOUSE1",           "Left Mouse" },
	{ Key_MouseRight,     ImGuiKey_MouseRight,     NONE,                      "MOUSE2",           "Right Mouse" },
	{ Key_MouseMiddle,    ImGuiKey_MouseMiddle,    NONE,                      "MOUSE3",           "Middle Mouse" },
	{ Key_Mouse4,         ImGuiKey_MouseX1,        NONE,                      "MOUSE4",           "Mouse 4" },
	{ Key_Mouse5,         ImGuiKey_MouseX2,        NONE,                      "MOUSE5",           "Mouse 5" },
	{ Key_MouseWheelUp,   ImGuiKey_None,           NONE,                      "MWHEELUP",         "Mouse Wheel Up" },
	{ Key_MouseWheelDown, ImGuiKey_None,           NONE,                      "MWHEELDOWN",       "Mouse Wheel Down" },
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
	KeyInfo info = GetKeyInfo( key );
	return info.sdl.exists ?
		MakeSpan( SDL_GetKeyName( SDL_GetKeyFromScancode( GetKeyInfo( key ).sdl.value, SDL_KMOD_NONE, false ) ) ) :
		info.name;
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

Optional< Key > KeyFromSDL( SDL_Scancode sdl ) {
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
