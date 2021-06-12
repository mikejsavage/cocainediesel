#include "client/client.h"
#include "client/icon.h"
#include "client/renderer/renderer.h"

#include "glad/glad.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "sdl2/SDL.h"

#include "stb/stb_image.h"

static SDL_Window * window;
static Vec2 mouse_movement;

static bool running_in_debugger = false;
const bool is_dedicated_server = false;

void Sys_Error( const char * format, ... ) {
	va_list argptr;
	char msg[ 1024 ];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Sys_ShowErrorMessage( msg );

	abort();
}

void Sys_Quit() {
	Qcommon_Shutdown();
	SDL_Quit();
	exit( 0 );
}

// TODO
extern cvar_t * vid_mode;

static void UpdateVidModeCvar() {
	TempAllocator temp = cls.frame_arena.temp();
	Cvar_Set( "vid_mode", temp( "{}", GetWindowMode() ) );
	vid_mode->modified = false;
}

static void OnWindowEvent() {
	if( IsWindowFocused() ) {
		UpdateVidModeCvar();
	}
}

static void OnMouseClicked( const SDL_MouseButtonEvent * ev ) {
	int key;
	switch( ev->button ) {
		case SDL_BUTTON_LEFT:
			key = K_MOUSE1;
			break;

		case SDL_BUTTON_RIGHT:
			key = K_MOUSE2;
			break;

		case SDL_BUTTON_MIDDLE:
			key = K_MOUSE3;
			break;

		case SDL_BUTTON_X1:
			key = K_MOUSE4;
			break;

		case SDL_BUTTON_X2:
			key = K_MOUSE5;
			break;

		default:
			return;
	}

	bool down = ev->type == SDL_MOUSEBUTTONDOWN;
	ImGui::GetIO().KeysDown[ key ] = down;
	Key_Event( key, down );
}

static void OnMouseMotion( const SDL_MouseMotionEvent * ev ) {
	mouse_movement += Vec2( ev->xrel, ev->yrel );
}

static void OnScroll( const SDL_MouseWheelEvent * ev ) {
	if( ev->y != 0 ) {
		int key = ev->y > 0 ? K_MWHEELUP : K_MWHEELDOWN;
		Key_Event( key, true );
		Key_Event( key, false );
		ImGui::GetIO().KeysDownDuration[ key ] = 0;

		ImGui::GetIO().MouseWheel += ev->y > 0 ? 1 : -1;
	}

	if( ev->x != 0 ) {
		ImGui::GetIO().MouseWheelH += ev->x > 0 ? 1 : -1;
	}
}

static int TranslateSDLScancode( SDL_Scancode key ) {
	switch( key ) {
		case SDL_SCANCODE_TAB:            return K_TAB;
		case SDL_SCANCODE_RETURN:         return K_ENTER;
		case SDL_SCANCODE_ESCAPE:         return K_ESCAPE;
		case SDL_SCANCODE_SPACE:          return K_SPACE;
		case SDL_SCANCODE_CAPSLOCK:       return K_CAPSLOCK;
		case SDL_SCANCODE_SCROLLLOCK:     return K_SCROLLLOCK;
		case SDL_SCANCODE_NUMLOCKCLEAR:   return K_NUMLOCK;
		case SDL_SCANCODE_BACKSPACE:      return K_BACKSPACE;
		case SDL_SCANCODE_UP:             return K_UPARROW;
		case SDL_SCANCODE_DOWN:           return K_DOWNARROW;
		case SDL_SCANCODE_LEFT:           return K_LEFTARROW;
		case SDL_SCANCODE_RIGHT:          return K_RIGHTARROW;
		case SDL_SCANCODE_LALT:           return K_LALT;
		case SDL_SCANCODE_RALT:           return K_RALT;
		case SDL_SCANCODE_LCTRL:          return K_LCTRL;
		case SDL_SCANCODE_RCTRL:          return K_RCTRL;
		case SDL_SCANCODE_LSHIFT:         return K_LSHIFT;
		case SDL_SCANCODE_RSHIFT:         return K_RSHIFT;
		case SDL_SCANCODE_F1:             return K_F1;
		case SDL_SCANCODE_F2:             return K_F2;
		case SDL_SCANCODE_F3:             return K_F3;
		case SDL_SCANCODE_F4:             return K_F4;
		case SDL_SCANCODE_F5:             return K_F5;
		case SDL_SCANCODE_F6:             return K_F6;
		case SDL_SCANCODE_F7:             return K_F7;
		case SDL_SCANCODE_F8:             return K_F8;
		case SDL_SCANCODE_F9:             return K_F9;
		case SDL_SCANCODE_F10:            return K_F10;
		case SDL_SCANCODE_F11:            return K_F11;
		case SDL_SCANCODE_F12:            return K_F12;
		case SDL_SCANCODE_F13:            return K_F13;
		case SDL_SCANCODE_F14:            return K_F14;
		case SDL_SCANCODE_F15:            return K_F15;
		case SDL_SCANCODE_INSERT:         return K_INS;
		case SDL_SCANCODE_DELETE:         return K_DEL;
		case SDL_SCANCODE_PAGEUP:         return K_PGUP;
		case SDL_SCANCODE_PAGEDOWN:       return K_PGDN;
		case SDL_SCANCODE_GRAVE:          return '~';
		case SDL_SCANCODE_NONUSBACKSLASH: return '<';
		case SDL_SCANCODE_HOME:           return K_HOME;
		case SDL_SCANCODE_END:            return K_END;

		case SDL_SCANCODE_A:              return 'a';
		case SDL_SCANCODE_B:              return 'b';
		case SDL_SCANCODE_C:              return 'c';
		case SDL_SCANCODE_D:              return 'd';
		case SDL_SCANCODE_E:              return 'e';
		case SDL_SCANCODE_F:              return 'f';
		case SDL_SCANCODE_G:              return 'g';
		case SDL_SCANCODE_H:              return 'h';
		case SDL_SCANCODE_I:              return 'i';
		case SDL_SCANCODE_J:              return 'j';
		case SDL_SCANCODE_K:              return 'k';
		case SDL_SCANCODE_L:              return 'l';
		case SDL_SCANCODE_M:              return 'm';
		case SDL_SCANCODE_N:              return 'n';
		case SDL_SCANCODE_O:              return 'o';
		case SDL_SCANCODE_P:              return 'p';
		case SDL_SCANCODE_Q:              return 'q';
		case SDL_SCANCODE_R:              return 'r';
		case SDL_SCANCODE_S:              return 's';
		case SDL_SCANCODE_T:              return 't';
		case SDL_SCANCODE_U:              return 'u';
		case SDL_SCANCODE_V:              return 'v';
		case SDL_SCANCODE_W:              return 'w';
		case SDL_SCANCODE_X:              return 'x';
		case SDL_SCANCODE_Y:              return 'y';
		case SDL_SCANCODE_Z:              return 'z';

		case SDL_SCANCODE_1:              return '1';
		case SDL_SCANCODE_2:              return '2';
		case SDL_SCANCODE_3:              return '3';
		case SDL_SCANCODE_4:              return '4';
		case SDL_SCANCODE_5:              return '5';
		case SDL_SCANCODE_6:              return '6';
		case SDL_SCANCODE_7:              return '7';
		case SDL_SCANCODE_8:              return '8';
		case SDL_SCANCODE_9:              return '9';
		case SDL_SCANCODE_0:              return '0';

		case SDL_SCANCODE_MINUS:          return '-';
		case SDL_SCANCODE_EQUALS:         return '=';
		case SDL_SCANCODE_BACKSLASH:      return '\\';
		case SDL_SCANCODE_COMMA:          return ',';
		case SDL_SCANCODE_PERIOD:         return '.';
		case SDL_SCANCODE_SLASH:          return '/';
		case SDL_SCANCODE_LEFTBRACKET:    return '[';
		case SDL_SCANCODE_RIGHTBRACKET:   return ']';
		case SDL_SCANCODE_SEMICOLON:      return ';';
		case SDL_SCANCODE_APOSTROPHE:     return '\'';

		case SDL_SCANCODE_KP_0:           return KP_INS;
		case SDL_SCANCODE_KP_1:           return KP_END;
		case SDL_SCANCODE_KP_2:           return KP_DOWNARROW;
		case SDL_SCANCODE_KP_3:           return KP_PGDN;
		case SDL_SCANCODE_KP_4:           return KP_LEFTARROW;
		case SDL_SCANCODE_KP_5:           return KP_5;
		case SDL_SCANCODE_KP_6:           return KP_RIGHTARROW;
		case SDL_SCANCODE_KP_7:           return KP_HOME;
		case SDL_SCANCODE_KP_8:           return KP_UPARROW;
		case SDL_SCANCODE_KP_9:           return KP_PGUP;
		case SDL_SCANCODE_KP_ENTER:       return KP_ENTER;
		case SDL_SCANCODE_KP_PERIOD:      return KP_DEL;
		case SDL_SCANCODE_KP_PLUS:        return KP_PLUS;
		case SDL_SCANCODE_KP_MINUS:       return KP_MINUS;
		case SDL_SCANCODE_KP_DIVIDE:      return KP_SLASH;
		case SDL_SCANCODE_KP_MULTIPLY:    return KP_STAR;
		case SDL_SCANCODE_KP_EQUALS:      return KP_EQUAL;
	}
	return 0;
}

static void OnKeyPressed( const SDL_KeyboardEvent * ev ) {
	bool down = ev->state == SDL_PRESSED;

	if( ev->keysym.scancode == SDL_SCANCODE_GRAVE ) {
		if( down ) {
			Con_ToggleConsole();
		}
		return;
	}

	int key = TranslateSDLScancode( ev->keysym.scancode );
	if( key == 0 )
		return;

	ImGuiIO & io = ImGui::GetIO();

	io.KeyCtrl = io.KeysDown[ K_LCTRL ] || io.KeysDown[ K_RCTRL ];
	io.KeyShift = io.KeysDown[ K_LSHIFT ] || io.KeysDown[ K_RSHIFT ];
	io.KeyAlt = io.KeysDown[ K_LALT ] || io.KeysDown[ K_RALT ];

	io.KeysDown[ key ] = down;

	Key_Event( key, down );
}

static void OnText( const SDL_TextInputEvent * ev ) {
	ImGui::GetIO().AddInputCharactersUTF8( ev->text );
}

// static GLFWmonitor * GetMonitorByIdx( int i ) {
// 	int num_monitors;
// 	GLFWmonitor ** monitors = glfwGetMonitors( &num_monitors );
// 	return i < num_monitors ? monitors[ i ] : monitors[ 0 ];
// }

static WindowMode CompleteWindowMode( WindowMode mode ) {
	// if( mode.fullscreen == FullscreenMode_Windowed ) {
	// 	if( mode.x == -1 && mode.y == -1 ) {
	// 		const GLFWvidmode * primary_mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );
	// 		mode.x = primary_mode->width / 2 - mode.video_mode.width / 2;
	// 		mode.y = primary_mode->height / 2 - mode.video_mode.height / 2;
	// 	}
	// }
	// else if( mode.fullscreen == FullscreenMode_Borderless ) {
	// 	GLFWmonitor * monitor = GetMonitorByIdx( mode.monitor );
        //
	// 	const GLFWvidmode * monitor_mode = glfwGetVideoMode( monitor );
	// 	mode.video_mode.width = monitor_mode->width;
	// 	mode.video_mode.height = monitor_mode->height;
        //
	// 	glfwGetMonitorPos( monitor, &mode.x, &mode.y );
	// }

	return mode;
}

void CreateWindow( WindowMode mode ) {
	ZoneScoped;

	uint32_t window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if( mode.fullscreen == FullscreenMode_Fullscreen ) {
		window_flags |= SDL_WINDOW_FULLSCREEN;
		mode.x = SDL_WINDOWPOS_UNDEFINED_DISPLAY( mode.monitor );
		mode.y = SDL_WINDOWPOS_UNDEFINED_DISPLAY( mode.monitor );
	}
	else if( mode.fullscreen == FullscreenMode_Borderless ) {
		window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		mode.x = SDL_WINDOWPOS_UNDEFINED_DISPLAY( mode.monitor );
		mode.y = SDL_WINDOWPOS_UNDEFINED_DISPLAY( mode.monitor );
	}

	if( mode.x == -1 && mode.y == -1 ) {
		mode.x = SDL_WINDOWPOS_CENTERED;
		mode.y = SDL_WINDOWPOS_CENTERED;
	}

	{
		ZoneScopedN( "SDL_CreateWindow" );
		window = SDL_CreateWindow( APPLICATION, mode.x, mode.y, mode.video_mode.width, mode.video_mode.height, window_flags );
		if( window == NULL ) {
			Sys_Error( "Couldn't create window: \"%s\"", SDL_GetError() );
		}
	}

	if( mode.fullscreen == FullscreenMode_Fullscreen ) {
		// TODO: set refresh rate
	}

	int w, h;
	uint8_t * icon = stbi_load_from_memory( icon_png, icon_png_len, &w, &h, NULL, 4 );
	assert( icon != NULL );

	SDL_Surface * surface = SDL_CreateRGBSurfaceFrom( icon, w, h, 32, w * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 );
	if( surface == NULL ) {
		Sys_Error( "Can't allocate SDL surface" );
	}

	SDL_SetWindowIcon( window, surface );

	SDL_FreeSurface( surface );
	free( icon );

	int gl_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
#if PUBLIC_BUILD
	gl_flags |= SDL_GL_CONTEXT_NO_ERROR;
#else
	gl_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
#endif

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, gl_flags );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );

	SDL_GLContext context = SDL_GL_CreateContext( window );

	if( SDL_GL_MakeCurrent( window, context ) < 0 ) {
		Sys_Error( "SDL_GL_MakeCurrent failed: \"%s\"\n", SDL_GetError() );
	}

	if( gladLoadGLLoader( SDL_GL_GetProcAddress ) != 1 ) {
		Sys_Error( "Couldn't load GL" );
	}

	mouse_movement = Vec2( 0.0f );
}

void DestroyWindow() {
	SDL_DestroyWindow( window );
}

void GetFramebufferSize( int * width, int * height ) {
	SDL_GL_GetDrawableSize( window, width, height );
}

VideoMode GetVideoMode( int monitor ) {
	SDL_DisplayMode sdl_mode;
	SDL_GetDesktopDisplayMode( monitor, &sdl_mode );

	VideoMode mode;
	mode.width = sdl_mode.w;
	mode.height = sdl_mode.h;
	mode.frequency = sdl_mode.refresh_rate;
	return mode;
}

WindowMode GetWindowMode() {
	WindowMode mode = { };

	SDL_GetWindowPosition( window, &mode.x, &mode.y );
	SDL_GetWindowSize( window, &mode.video_mode.width, &mode.video_mode.height );

	int monitor = SDL_GetWindowDisplayIndex( window );

	uint32_t flags = SDL_GetWindowFlags( window );
	if( flags & SDL_WINDOW_FULLSCREEN ) {
		SDL_DisplayMode sdl_mode;
		SDL_GetWindowDisplayMode( window, &sdl_mode );

		mode.video_mode.frequency = sdl_mode.refresh_rate;
		mode.monitor = monitor;
		mode.fullscreen = FullscreenMode_Fullscreen;
	}
	else if( ( flags & SDL_WINDOW_FULLSCREEN_DESKTOP ) == SDL_WINDOW_FULLSCREEN_DESKTOP ) {
		mode.monitor = monitor;
		mode.fullscreen = FullscreenMode_Borderless;
	}
	else {
		mode.fullscreen = FullscreenMode_Windowed;
	}

	return mode;
}

void SetWindowMode( WindowMode mode ) {
	mode = CompleteWindowMode( mode );

	if( mode.fullscreen == FullscreenMode_Windowed ) {
		SDL_SetWindowFullscreen( window, 0 );
		SDL_SetWindowSize( window, mode.video_mode.width, mode.video_mode.height );
		if( mode.x == -1 && mode.y == -1 ) {
			mode.x = SDL_WINDOWPOS_CENTERED;
			mode.y = SDL_WINDOWPOS_CENTERED;
		}
		SDL_SetWindowPosition( window, mode.x, mode.y );
	}
	else if( mode.fullscreen == FullscreenMode_Borderless ) {
		SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN_DESKTOP );
		int pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY( mode.monitor );
		SDL_SetWindowPosition( window, pos, pos );
	}
	else {
		SDL_DisplayMode desktop_mode;
		SDL_GetDesktopDisplayMode( mode.monitor, &desktop_mode );

		SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN );

		int pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY( mode.monitor );
		SDL_SetWindowPosition( window, pos, pos );

		SDL_DisplayMode sdl_mode;
		sdl_mode.format = desktop_mode.format;
		sdl_mode.w = mode.video_mode.width;
		sdl_mode.h = mode.video_mode.height;
		sdl_mode.refresh_rate = mode.video_mode.frequency;
		sdl_mode.driverdata = NULL;

		SDL_SetWindowDisplayMode( window, &sdl_mode );
	}
}

void EnableVSync( bool enabled ) {
	SDL_GL_SetSwapInterval( enabled ? 1 : 0 );
}

bool IsWindowFocused() {
	return ( SDL_GetWindowFlags( window ) & SDL_WINDOW_INPUT_FOCUS ) != 0;
}

static double last_mouse_x, last_mouse_y;

Vec2 GetMouseMovement() {
	Vec2 movement = mouse_movement;
	mouse_movement = Vec2( 0.0f );
	return movement;
}

void GlfwInputFrame() {
	// show cursor if there are any imgui windows accepting inputs
	bool gui_active = false;
	const ImGuiContext * ctx = ImGui::GetCurrentContext();
	for( const ImGuiWindow * w : ctx->Windows ) {
		if( w->Active && w->ParentWindow == NULL && ( w->Flags & ImGuiWindowFlags_NoMouseInputs ) == 0 ) {
			gui_active = true;
			break;
		}
	}

	if( gui_active ) {
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
	}
	else if( running_in_debugger ) {
		// don't grab input if we're running a debugger
		last_mouse_x = frame_static.viewport_width / 2;
		last_mouse_y = frame_static.viewport_height / 2;
		glfwSetCursorPos( window, last_mouse_x, last_mouse_y );
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
	}
	else {
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
	}

	break1 = glfwGetKey( window, GLFW_KEY_F1 );
	break2 = glfwGetKey( window, GLFW_KEY_F2 );
	break3 = glfwGetKey( window, GLFW_KEY_F3 );
	break4 = glfwGetKey( window, GLFW_KEY_F4 );
}

void SwapBuffers() {
	ZoneScoped;
	SDL_GL_SwapWindow( window );
}

static bool DispatchEvents() {
	bool quit = false;

	SDL_PumpEvents();

	SDL_Event ev;
	while( SDL_PollEvent( &ev ) != 0 ) {
		switch( ev.type ) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				OnKeyPressed( &ev.key );
				break;

			case SDL_TEXTINPUT:
				OnText( &ev.text );
				break;

			case SDL_MOUSEMOTION:
				OnMouseMotion( &ev.motion );
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				OnMouseClicked( &ev.button );
				break;

			case SDL_MOUSEWHEEL:
				OnScroll( &ev.wheel );
				break;

			case SDL_QUIT:
				quit = true;
				break;

			case SDL_WINDOWEVENT:
				if( ev.window.event == SDL_WINDOWEVENT_MOVED || ev.window.event == SDL_WINDOWEVENT_RESIZED ) {
					OnWindowEvent();
				}
				break;
		}
	}

	return quit;
}

int main( int argc, char ** argv ) {
#if PUBLIC_BUILD
	running_in_debugger = false;
#else
	running_in_debugger = Sys_BeingDebugged();
#endif

	{
		ZoneScopedN( "SDL_Init" );
		if( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
			Sys_Error( "SDL_Init: %s", SDL_GetError() );
		}
	}

	Con_Init();
	Qcommon_Init( argc, argv );

	bool quit = false;
	int64_t oldtime = Sys_Milliseconds();
	while( !quit ) {
		int64_t newtime;
		int dt;
		{
			ZoneScopedN( "Interframe" );

			// find time spent rendering last frame
			do {
				newtime = Sys_Milliseconds();
				dt = newtime - oldtime;
			} while( dt == 0 );
			oldtime = newtime;
		}

		quit = DispatchEvents();

		Qcommon_Frame( dt );
	}

	Com_Quit();

	return 0;
}
