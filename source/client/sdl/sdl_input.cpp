#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "client/client.h"

#include "sdl/SDL.h"

extern SDL_Window * sdl_window;

static bool warped = false;

static int mx, my;
static int rx, ry;

static bool running_in_debugger = false;

static void mouse_motion_event( const SDL_MouseMotionEvent * event ) {
	mx = event->x;
	my = event->y;

	if( !warped ) {
		int scale = running_in_debugger ? -1 : 1;
		rx += event->xrel * scale;
		ry += event->yrel * scale;
	}

	warped = false;
}

static void mouse_button_event( const SDL_MouseButtonEvent * event, bool down ) {
	int key = 0;
	switch( event->button ) {
		case SDL_BUTTON_LEFT:
			key = K_MOUSE1;
			break;
		case SDL_BUTTON_MIDDLE:
			key = K_MOUSE3;
			break;
		case SDL_BUTTON_RIGHT:
			key = K_MOUSE2;
			break;
		case SDL_BUTTON_X1:
			key = K_MOUSE4;
			break;
		case SDL_BUTTON_X2:
			key = K_MOUSE5;
			break;
		case 6:
			key = K_MOUSE6;
			break;
		case 7:
			key = K_MOUSE7;
			break;
		case 8:
			key = K_MOUSE4;
			break;
		case 9:
			key = K_MOUSE5;
			break;
		case 10:
			key = K_MOUSE8;
			break;
	}

	if( key == 0 )
		return;

	ImGui::GetIO().KeysDown[ key ] = down;
	Key_Event( key, down );
}

static void mouse_wheel_event( const SDL_MouseWheelEvent * event ) {
	int key = event->y > 0 ? K_MWHEELUP : K_MWHEELDOWN;
	Key_Event( key, true );
	Key_Event( key, false );

	if( event->x != 0 ) {
		ImGui::GetIO().MouseWheelH += event->x > 0 ? 1 : -1;
	}
	if( event->y != 0 ) {
		ImGui::GetIO().MouseWheel += event->y > 0 ? 1 : -1;
	}

	ImGui::GetIO().KeysDownDuration[ key ] = 0;
}

static int TranslateSDLScancode( SDL_Scancode scancode ) {
	switch( scancode ) {
		case SDL_SCANCODE_TAB:          return K_TAB;
		case SDL_SCANCODE_RETURN:       return K_ENTER;
		case SDL_SCANCODE_ESCAPE:       return K_ESCAPE;
		case SDL_SCANCODE_SPACE:        return K_SPACE;
		case SDL_SCANCODE_CAPSLOCK:     return K_CAPSLOCK;
		case SDL_SCANCODE_SCROLLLOCK:   return K_SCROLLLOCK;
		case SDL_SCANCODE_NUMLOCKCLEAR: return K_NUMLOCK;
		case SDL_SCANCODE_BACKSPACE:    return K_BACKSPACE;
		case SDL_SCANCODE_UP:           return K_UPARROW;
		case SDL_SCANCODE_DOWN:         return K_DOWNARROW;
		case SDL_SCANCODE_LEFT:         return K_LEFTARROW;
		case SDL_SCANCODE_RIGHT:        return K_RIGHTARROW;
#if defined( __APPLE__ )
		case SDL_SCANCODE_LALT:         return K_OPTION;
		case SDL_SCANCODE_RALT:         return K_OPTION;
#else
		case SDL_SCANCODE_LALT:         return K_LALT;
		case SDL_SCANCODE_RALT:         return K_RALT;
#endif
		case SDL_SCANCODE_LCTRL:        return K_LCTRL;
		case SDL_SCANCODE_RCTRL:        return K_RCTRL;
		case SDL_SCANCODE_LSHIFT:       return K_LSHIFT;
		case SDL_SCANCODE_RSHIFT:       return K_RSHIFT;
		case SDL_SCANCODE_F1:           return K_F1;
		case SDL_SCANCODE_F2:           return K_F2;
		case SDL_SCANCODE_F3:           return K_F3;
		case SDL_SCANCODE_F4:           return K_F4;
		case SDL_SCANCODE_F5:           return K_F5;
		case SDL_SCANCODE_F6:           return K_F6;
		case SDL_SCANCODE_F7:           return K_F7;
		case SDL_SCANCODE_F8:           return K_F8;
		case SDL_SCANCODE_F9:           return K_F9;
		case SDL_SCANCODE_F10:          return K_F10;
		case SDL_SCANCODE_F11:          return K_F11;
		case SDL_SCANCODE_F12:          return K_F12;
		case SDL_SCANCODE_F13:          return K_F13;
		case SDL_SCANCODE_F14:          return K_F14;
		case SDL_SCANCODE_F15:          return K_F15;
		case SDL_SCANCODE_INSERT:       return K_INS;
		case SDL_SCANCODE_DELETE:       return K_DEL;
		case SDL_SCANCODE_PAGEUP:       return K_PGUP;
		case SDL_SCANCODE_PAGEDOWN:     return K_PGDN;
		case SDL_SCANCODE_HOME:         return K_HOME;
		case SDL_SCANCODE_END:          return K_END;
		case SDL_SCANCODE_NONUSBACKSLASH: return '<';
		case SDL_SCANCODE_LGUI:         return K_COMMAND;
		case SDL_SCANCODE_RGUI:         return K_COMMAND;

		case SDL_SCANCODE_A:            return 'a';
		case SDL_SCANCODE_B:            return 'b';
		case SDL_SCANCODE_C:            return 'c';
		case SDL_SCANCODE_D:            return 'd';
		case SDL_SCANCODE_E:            return 'e';
		case SDL_SCANCODE_F:            return 'f';
		case SDL_SCANCODE_G:            return 'g';
		case SDL_SCANCODE_H:            return 'h';
		case SDL_SCANCODE_I:            return 'i';
		case SDL_SCANCODE_J:            return 'j';
		case SDL_SCANCODE_K:            return 'k';
		case SDL_SCANCODE_L:            return 'l';
		case SDL_SCANCODE_M:            return 'm';
		case SDL_SCANCODE_N:            return 'n';
		case SDL_SCANCODE_O:            return 'o';
		case SDL_SCANCODE_P:            return 'p';
		case SDL_SCANCODE_Q:            return 'q';
		case SDL_SCANCODE_R:            return 'r';
		case SDL_SCANCODE_S:            return 's';
		case SDL_SCANCODE_T:            return 't';
		case SDL_SCANCODE_U:            return 'u';
		case SDL_SCANCODE_V:            return 'v';
		case SDL_SCANCODE_W:            return 'w';
		case SDL_SCANCODE_X:            return 'x';
		case SDL_SCANCODE_Y:            return 'y';
		case SDL_SCANCODE_Z:            return 'z';

		case SDL_SCANCODE_1:            return '1';
		case SDL_SCANCODE_2:            return '2';
		case SDL_SCANCODE_3:            return '3';
		case SDL_SCANCODE_4:            return '4';
		case SDL_SCANCODE_5:            return '5';
		case SDL_SCANCODE_6:            return '6';
		case SDL_SCANCODE_7:            return '7';
		case SDL_SCANCODE_8:            return '8';
		case SDL_SCANCODE_9:            return '9';
		case SDL_SCANCODE_0:            return '0';

		case SDL_SCANCODE_MINUS:        return '-';
		case SDL_SCANCODE_EQUALS:       return '=';
		case SDL_SCANCODE_BACKSLASH:    return '\\';
		case SDL_SCANCODE_COMMA:        return ',';
		case SDL_SCANCODE_PERIOD:       return '.';
		case SDL_SCANCODE_SLASH:        return '/';
		case SDL_SCANCODE_LEFTBRACKET:  return '[';
		case SDL_SCANCODE_RIGHTBRACKET: return ']';
		case SDL_SCANCODE_SEMICOLON:    return ';';
		case SDL_SCANCODE_APOSTROPHE:   return '\'';

		case SDL_SCANCODE_KP_0:         return KP_INS;
		case SDL_SCANCODE_KP_1:         return KP_END;
		case SDL_SCANCODE_KP_2:         return KP_DOWNARROW;
		case SDL_SCANCODE_KP_3:         return KP_PGDN;
		case SDL_SCANCODE_KP_4:         return KP_LEFTARROW;
		case SDL_SCANCODE_KP_5:         return KP_5;
		case SDL_SCANCODE_KP_6:         return KP_RIGHTARROW;
		case SDL_SCANCODE_KP_7:         return KP_HOME;
		case SDL_SCANCODE_KP_8:         return KP_UPARROW;
		case SDL_SCANCODE_KP_9:         return KP_PGUP;
		case SDL_SCANCODE_KP_ENTER:     return KP_ENTER;
		case SDL_SCANCODE_RETURN2:      return KP_ENTER;
		case SDL_SCANCODE_KP_PERIOD:    return KP_DEL;
		case SDL_SCANCODE_KP_PLUS:      return KP_PLUS;
		case SDL_SCANCODE_KP_MINUS:     return KP_MINUS;
		case SDL_SCANCODE_KP_DIVIDE:    return KP_SLASH;
		case SDL_SCANCODE_KP_MULTIPLY:  return KP_STAR;
		case SDL_SCANCODE_KP_EQUALS:    return KP_EQUAL;

		default: break;
	}
	return 0;
}

static void key_event( const SDL_KeyboardEvent *event, bool down ) {
	if( event->keysym.scancode == SDL_SCANCODE_GRAVE ) {
		if( down ) {
			Con_ToggleConsole();
		}
		return;
	}

	int key = TranslateSDLScancode( event->keysym.scancode );
	if( key == 0 )
		return;

	if( key == K_LCTRL || key == K_RCTRL ) {
		ImGui::GetIO().KeyCtrl = down;
	}
	else if( key == K_LSHIFT || key == K_RSHIFT ) {
		ImGui::GetIO().KeyShift = down;
	}
	else if( key == K_LALT || key == K_RALT ) {
		ImGui::GetIO().KeyAlt = down;
	}

	ImGui::GetIO().KeysDown[ key ] = down;

	Key_Event( key, down );
}

/*****************************************************************************/

static void AppActivate( bool active ) {
	bool minimized = ( SDL_GetWindowFlags( sdl_window ) & SDL_WINDOW_MINIMIZED ) != 0;
	S_SetWindowFocus( active );
	VID_AppActivate( active, minimized );
}

static void IN_HandleEvents( void ) {
	rx = 0;
	ry = 0;

	SDL_PumpEvents();
	SDL_Event event;

	while( SDL_PollEvent( &event ) ) {
		switch( event.type ) {
			case SDL_KEYDOWN:
				key_event( &event.key, true );
				break;

			case SDL_KEYUP:
				key_event( &event.key, false );
				break;

			case SDL_TEXTINPUT:
				ImGui::GetIO().AddInputCharactersUTF8( event.text.text );
				break;

			case SDL_MOUSEMOTION:
				mouse_motion_event( &event.motion );
				break;

			case SDL_MOUSEBUTTONDOWN:
				mouse_button_event( &event.button, true );
				break;

			case SDL_MOUSEBUTTONUP:
				mouse_button_event( &event.button, false );
				break;

			case SDL_MOUSEWHEEL:
				mouse_wheel_event( &event.wheel );
				break;

			case SDL_QUIT:
				Cbuf_ExecuteText( EXEC_NOW, "quit" );
				break;

			case SDL_WINDOWEVENT:
				switch( event.window.event ) {
					case SDL_WINDOWEVENT_SHOWN:
					case SDL_WINDOWEVENT_HIDDEN:
						AppActivate( event.window.event == SDL_WINDOWEVENT_SHOWN );
						break;
					case SDL_WINDOWEVENT_CLOSE:
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
					case SDL_WINDOWEVENT_FOCUS_LOST:
						AppActivate( event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED );
						break;
				}
				break;
		}
	}
}

static void IN_WarpMouseToCenter() {
	warped = true;
	int w, h;
	SDL_GetWindowSize( sdl_window, &w, &h );
	SDL_WarpMouseInWindow( sdl_window, w / 2, h / 2 );
}

MouseMovement IN_GetMouseMovement() {
	MouseMovement movement;
	movement.relx = rx;
	movement.rely = ry;
	movement.absx = mx;
	movement.absy = my;
	rx = ry = 0;
	return movement;
}

#if PUBLIC_BUILD
static bool being_debugged() {
	return false;
}
#endif

#if PLATFORM_WINDOWS && !defined( PUBLIC_BUILD )
#define _WIN32_WINNT 0x4000
#include <windows.h>

static bool being_debugged() {
	return IsDebuggerPresent() != 0;
}
#endif

#if PLATFORM_LINUX && !defined( PUBLIC_BUILD )
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

static bool being_debugged() {
	pid_t parent_pid = getpid();
	pid_t child_pid = fork();
	if( child_pid == -1 ) {
		err( 1, "fork" );
	}

	if( child_pid == 0 ) {
		// if we can't ptrace the parent then gdb is already there
		if( ptrace( PTRACE_ATTACH, parent_pid, NULL, NULL ) != 0 ) {
			if( errno == EPERM ) {
				printf( "! echo 0 > /proc/sys/kernel/yama/ptrace_scope\n" );
				printf( "! or\n" );
				printf( "! sysctl kernel.yama.ptrace_scope=0\n" );
				_exit( 0 );
			}
			_exit( 1 );
		}

		// ptrace automatically stops the process so wait for SIGSTOP and send PTRACE_CONT
		waitpid( parent_pid, NULL, 0 );
		ptrace( PTRACE_CONT, NULL, NULL );

		// detach
		ptrace( PTRACE_DETACH, parent_pid, NULL, NULL );
		_exit( 0 );
	}

	int status;
	waitpid( child_pid, &status, 0 );
	if( !WIFEXITED( status ) ) {
		_exit( 1 );
	}

	return WEXITSTATUS( status ) == 1;
}
#endif

void IN_Init() {
	SDL_version linked;

	SDL_GetVersion( &linked );

	running_in_debugger = being_debugged();

	SDL_ShowCursor( running_in_debugger ? SDL_ENABLE : SDL_DISABLE );

	SDL_SetHint( SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0" );
}

void IN_Frame() {
	// show cursor if there are any imgui windows accepting inputs
	bool gui_active = false;
	const ImGuiContext * ctx = ImGui::GetCurrentContext();
	for( const ImGuiWindow * window : ctx->Windows ) {
		if( window->Active && window->ParentWindow == NULL && ( window->Flags & ImGuiWindowFlags_NoMouseInputs ) == 0 ) {
			gui_active = true;
			break;
		}
	}

	if( gui_active ) {
		SDL_SetRelativeMouseMode( SDL_FALSE );
		SDL_ShowCursor( SDL_ENABLE );
	}
	else if( running_in_debugger ) {
		// don't grab input if we're running a debugger
		IN_WarpMouseToCenter();
		SDL_SetRelativeMouseMode( SDL_FALSE );
		SDL_ShowCursor( SDL_ENABLE );
	}
	else {
		SDL_SetRelativeMouseMode( SDL_TRUE );
	}

	IN_HandleEvents();

	const u8 * keys = SDL_GetKeyboardState( NULL );
	break1 = keys[ SDL_SCANCODE_F1 ];
	break2 = keys[ SDL_SCANCODE_F2 ];
	break3 = keys[ SDL_SCANCODE_F3 ];
	break4 = keys[ SDL_SCANCODE_F4 ];
}
