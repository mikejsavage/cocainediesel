#pragma once

#include "qcommon/types.h"

enum Key {
	Key_Tab,
	Key_Enter,
	Key_Escape,
	Key_Space,
	Key_Backspace,
	Key_CapsLock,
	Key_ScrollLock,
	Key_NumLock,
	Key_Pause,

	Key_UpArrow,
	Key_DownArrow,
	Key_LeftArrow,
	Key_RightArrow,

	Key_Insert,
	Key_Delete,
	Key_PageUp,
	Key_PageDown,
	Key_Home,
	Key_End,

	Key_LeftAlt,
	Key_RightAlt,
	Key_LeftCtrl,
	Key_RightCtrl,
	Key_LeftShift,
	Key_RightShift,

	Key_F1,
	Key_F2,
	Key_F3,
	Key_F4,
	Key_F5,
	Key_F6,
	Key_F7,
	Key_F8,
	Key_F9,
	Key_F10,
	Key_F11,
	Key_F12,

	Key_0,
	Key_1,
	Key_2,
	Key_3,
	Key_4,
	Key_5,
	Key_6,
	Key_7,
	Key_8,
	Key_9,

	Key_A,
	Key_B,
	Key_C,
	Key_D,
	Key_E,
	Key_F,
	Key_G,
	Key_H,
	Key_I,
	Key_J,
	Key_K,
	Key_L,
	Key_M,
	Key_N,
	Key_O,
	Key_P,
	Key_Q,
	Key_R,
	Key_S,
	Key_T,
	Key_U,
	Key_V,
	Key_W,
	Key_X,
	Key_Y,
	Key_Z,

	Key_Apostrophe,
	Key_Comma,
	Key_Minus,
	Key_Period,
	Key_Slash,
	Key_Semicolon,
	Key_Equal,
	Key_LeftBracket,
	Key_Backslash,
	Key_RightBracket,

	Key_KeypadDecimal,
	Key_KeypadDivide,
	Key_KeypadMultiply,
	Key_KeypadSubtract,
	Key_KeypadAdd,
	Key_KeypadEnter,
	Key_KeypadEqual,
	Key_Keypad0,
	Key_Keypad1,
	Key_Keypad2,
	Key_Keypad3,
	Key_Keypad4,
	Key_Keypad5,
	Key_Keypad6,
	Key_Keypad7,
	Key_Keypad8,
	Key_Keypad9,

	Key_MouseLeft,
	Key_MouseRight,
	Key_MouseMiddle,
	Key_Mouse4,
	Key_Mouse5,
	Key_Mouse6,
	Key_Mouse7,
	Key_MouseWheelUp,
	Key_MouseWheelDown,

	Key_Count
};

void InitKeys();
void ShutdownKeys();

void KeyEvent( Key key, bool down );
void AllKeysUp();

void SetKeyBind( Key key, Span< const char > command );
Span< const char > GetKeyBind( Key key );
void UnbindKey( Key key );
void UnbindAllKeys();

Span< const char > KeyName( Key key );

void GetKeyBindsForCommand( Span< const char > command, Optional< Key > * key1, Optional< Key > * key2 );

class DynamicString;
void WriteKeyBindingsConfig( DynamicString * config );
