#include "client/client.h"
#include "client/icon.h"
#include "client/renderer/renderer.h"

#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include "glfw3/GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "stb/stb_image.h"

const bool is_dedicated_server = false;

GLFWwindow * window = NULL;

static bool running_in_debugger;
static bool route_inputs_to_imgui;

static int framebuffer_width, framebuffer_height;

// TODO
extern Cvar * vid_mode;

static void UpdateVidModeCvar() {
	TempAllocator temp = cls.frame_arena.temp();
	Cvar_Set( "vid_mode", temp( "{}", GetWindowMode() ) );
	vid_mode->modified = false;
}

static void AssignMonitorNumbers() {
	int num_monitors;
	GLFWmonitor ** monitors = glfwGetMonitors( &num_monitors );

	for( int i = 0; i < num_monitors; i++ ) {
		glfwSetMonitorUserPointer( monitors[ i ], ( void * ) uintptr_t( i ) );
	}
}

static void OnMonitorConfigChange( GLFWmonitor * monitor, int event ) {
	AssignMonitorNumbers();
}

static void OnWindowMoved( GLFWwindow *, int x, int y ) {
	if( IsWindowFocused() ) {
		UpdateVidModeCvar();
	}
}

static void OnWindowResized( GLFWwindow *, int w, int h ) {
	if( w > 0 && h > 0 ) {
		UpdateVidModeCvar();
		glfwGetFramebufferSize( window, &framebuffer_width, &framebuffer_height );
	}
}

static void OnMouseClicked( GLFWwindow *, int button, int action, int mods ) {
	int key;
	switch( button ) {
		case GLFW_MOUSE_BUTTON_LEFT:
			key = K_MOUSE1;
			break;

		case GLFW_MOUSE_BUTTON_RIGHT:
			key = K_MOUSE2;
			break;

		case GLFW_MOUSE_BUTTON_MIDDLE:
			key = K_MOUSE3;
			break;

		case GLFW_MOUSE_BUTTON_4:
			key = K_MOUSE4;
			break;

		case GLFW_MOUSE_BUTTON_5:
			key = K_MOUSE5;
			break;

		case GLFW_MOUSE_BUTTON_6:
			key = K_MOUSE6;
			break;

		case GLFW_MOUSE_BUTTON_7:
			key = K_MOUSE7;
			break;

		default:
			return;
	}

	bool down = action == GLFW_PRESS;
	ImGui::GetIO().KeysDown[ key ] = down;

	if( !route_inputs_to_imgui ) {
		Key_Event( key, down );
	}
}

static void OnScroll( GLFWwindow *, double x, double y ) {
	if( y != 0 ) {
		int key = y > 0 ? K_MWHEELUP : K_MWHEELDOWN;
		if( !route_inputs_to_imgui ) {
			Key_Event( key, true );
			Key_Event( key, false );
		}
		ImGui::GetIO().KeysDownDuration[ key ] = 0;
	}

	ImGui::GetIO().MouseWheelH += x;
	ImGui::GetIO().MouseWheel += y;
}

static int TranslateGLFWKey( int glfw ) {
	switch( glfw ) {
		case GLFW_KEY_TAB:           return K_TAB;
		case GLFW_KEY_ENTER:         return K_ENTER;
		case GLFW_KEY_ESCAPE:        return K_ESCAPE;
		case GLFW_KEY_SPACE:         return K_SPACE;
		case GLFW_KEY_CAPS_LOCK:     return K_CAPSLOCK;
		case GLFW_KEY_SCROLL_LOCK:   return K_SCROLLLOCK;
		case GLFW_KEY_NUM_LOCK:      return K_NUMLOCK;
		case GLFW_KEY_BACKSPACE:     return K_BACKSPACE;
		case GLFW_KEY_UP:            return K_UPARROW;
		case GLFW_KEY_DOWN:          return K_DOWNARROW;
		case GLFW_KEY_LEFT:          return K_LEFTARROW;
		case GLFW_KEY_RIGHT:         return K_RIGHTARROW;
		case GLFW_KEY_LEFT_ALT:      return K_LALT;
		case GLFW_KEY_RIGHT_ALT:     return K_RALT;
		case GLFW_KEY_LEFT_CONTROL:  return K_LCTRL;
		case GLFW_KEY_RIGHT_CONTROL: return K_RCTRL;
		case GLFW_KEY_LEFT_SHIFT:    return K_LSHIFT;
		case GLFW_KEY_RIGHT_SHIFT:   return K_RSHIFT;
		case GLFW_KEY_F1:            return K_F1;
		case GLFW_KEY_F2:            return K_F2;
		case GLFW_KEY_F3:            return K_F3;
		case GLFW_KEY_F4:            return K_F4;
		case GLFW_KEY_F5:            return K_F5;
		case GLFW_KEY_F6:            return K_F6;
		case GLFW_KEY_F7:            return K_F7;
		case GLFW_KEY_F8:            return K_F8;
		case GLFW_KEY_F9:            return K_F9;
		case GLFW_KEY_F10:           return K_F10;
		case GLFW_KEY_F11:           return K_F11;
		case GLFW_KEY_F12:           return K_F12;
		case GLFW_KEY_INSERT:        return K_INS;
		case GLFW_KEY_DELETE:        return K_DEL;
		case GLFW_KEY_PAGE_UP:       return K_PGUP;
		case GLFW_KEY_PAGE_DOWN:     return K_PGDN;
		case GLFW_KEY_HOME:          return K_HOME;
		case GLFW_KEY_END:           return K_END;

		case GLFW_KEY_A:             return 'a';
		case GLFW_KEY_B:             return 'b';
		case GLFW_KEY_C:             return 'c';
		case GLFW_KEY_D:             return 'd';
		case GLFW_KEY_E:             return 'e';
		case GLFW_KEY_F:             return 'f';
		case GLFW_KEY_G:             return 'g';
		case GLFW_KEY_H:             return 'h';
		case GLFW_KEY_I:             return 'i';
		case GLFW_KEY_J:             return 'j';
		case GLFW_KEY_K:             return 'k';
		case GLFW_KEY_L:             return 'l';
		case GLFW_KEY_M:             return 'm';
		case GLFW_KEY_N:             return 'n';
		case GLFW_KEY_O:             return 'o';
		case GLFW_KEY_P:             return 'p';
		case GLFW_KEY_Q:             return 'q';
		case GLFW_KEY_R:             return 'r';
		case GLFW_KEY_S:             return 's';
		case GLFW_KEY_T:             return 't';
		case GLFW_KEY_U:             return 'u';
		case GLFW_KEY_V:             return 'v';
		case GLFW_KEY_W:             return 'w';
		case GLFW_KEY_X:             return 'x';
		case GLFW_KEY_Y:             return 'y';
		case GLFW_KEY_Z:             return 'z';

		case GLFW_KEY_1:             return '1';
		case GLFW_KEY_2:             return '2';
		case GLFW_KEY_3:             return '3';
		case GLFW_KEY_4:             return '4';
		case GLFW_KEY_5:             return '5';
		case GLFW_KEY_6:             return '6';
		case GLFW_KEY_7:             return '7';
		case GLFW_KEY_8:             return '8';
		case GLFW_KEY_9:             return '9';
		case GLFW_KEY_0:             return '0';

		case GLFW_KEY_MINUS:         return '-';
		case GLFW_KEY_EQUAL:         return '=';
		case GLFW_KEY_BACKSLASH:     return '\\';
		case GLFW_KEY_COMMA:         return ',';
		case GLFW_KEY_PERIOD:        return '.';
		case GLFW_KEY_SLASH:         return '/';
		case GLFW_KEY_LEFT_BRACKET:  return '[';
		case GLFW_KEY_RIGHT_BRACKET: return ']';
		case GLFW_KEY_SEMICOLON:     return ';';
		case GLFW_KEY_APOSTROPHE:    return '\'';
		case GLFW_KEY_WORLD_1:       return '>';
		case GLFW_KEY_WORLD_2:       return '<';

		case GLFW_KEY_KP_0:          return KP_INS;
		case GLFW_KEY_KP_1:          return KP_END;
		case GLFW_KEY_KP_2:          return KP_DOWNARROW;
		case GLFW_KEY_KP_3:          return KP_PGDN;
		case GLFW_KEY_KP_4:          return KP_LEFTARROW;
		case GLFW_KEY_KP_5:          return KP_5;
		case GLFW_KEY_KP_6:          return KP_RIGHTARROW;
		case GLFW_KEY_KP_7:          return KP_HOME;
		case GLFW_KEY_KP_8:          return KP_UPARROW;
		case GLFW_KEY_KP_9:          return KP_PGUP;
		case GLFW_KEY_KP_ENTER:      return KP_ENTER;
		case GLFW_KEY_KP_DECIMAL:    return KP_DEL;
		case GLFW_KEY_KP_ADD:        return KP_PLUS;
		case GLFW_KEY_KP_SUBTRACT:   return KP_MINUS;
		case GLFW_KEY_KP_DIVIDE:     return KP_SLASH;
		case GLFW_KEY_KP_MULTIPLY:   return KP_STAR;
		case GLFW_KEY_KP_EQUAL:      return KP_EQUAL;
	}
	return 0;
}

static void OnKeyPressed( GLFWwindow *, int glfw_key, int scancode, int action, int mods ) {
	if( action == GLFW_REPEAT )
		return;

	bool down = action == GLFW_PRESS;

	if( glfw_key == GLFW_KEY_GRAVE_ACCENT ) {
		if( down ) {
			Con_ToggleConsole();
		}
		return;
	}

	int key = TranslateGLFWKey( glfw_key );
	if( key == 0 )
		return;

	ImGuiIO & io = ImGui::GetIO();

	io.KeyCtrl = io.KeysDown[ K_LCTRL ] || io.KeysDown[ K_RCTRL ];
	io.KeyShift = io.KeysDown[ K_LSHIFT ] || io.KeysDown[ K_RSHIFT ];
	io.KeyAlt = io.KeysDown[ K_LALT ] || io.KeysDown[ K_RALT ];

	io.KeysDown[ key ] = down;

	if( !route_inputs_to_imgui ) {
		Key_Event( key, down );
	}
}

static void OnCharTyped( GLFWwindow *, unsigned int codepoint ) {
	ImGui::GetIO().AddInputCharacter( codepoint );
}

static void OnGlfwError( int code, const char * message ) {
	// ignore clipboard conversion errors
	if( code == GLFW_FORMAT_UNAVAILABLE && strstr( message, "Failed to convert" ) )
		return;

	if( code == GLFW_VERSION_UNAVAILABLE ) {
		Fatal( "Your PC is too old. You need a GPU that can support OpenGL 4.5" );
	}

	Fatal( "GLFW error %d: %s", code, message );
}

static GLFWmonitor * GetMonitorByIdx( int i ) {
	int num_monitors;
	GLFWmonitor ** monitors = glfwGetMonitors( &num_monitors );
	return i < num_monitors ? monitors[ i ] : monitors[ 0 ];
}

static WindowMode CompleteWindowMode( WindowMode mode ) {
	if( mode.fullscreen == FullscreenMode_Windowed ) {
		if( mode.x == -1 && mode.y == -1 ) {
			const GLFWvidmode * primary_mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );
			mode.x = primary_mode->width / 2 - mode.video_mode.width / 2;
			mode.y = primary_mode->height / 2 - mode.video_mode.height / 2;
		}
	}
	else if( mode.fullscreen == FullscreenMode_Borderless ) {
		GLFWmonitor * monitor = GetMonitorByIdx( mode.monitor );

		const GLFWvidmode * monitor_mode = glfwGetVideoMode( monitor );
		mode.video_mode.width = monitor_mode->width;
		mode.video_mode.height = monitor_mode->height;

		glfwGetMonitorPos( monitor, &mode.x, &mode.y );
	}

	return mode;
}

void CreateWindow( WindowMode mode ) {
	TracyZoneScoped;

	glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 5 );
	glfwWindowHint( GLFW_SRGB_CAPABLE, GLFW_TRUE );
	glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

	if( is_public_build ) {
		glfwWindowHint( GLFW_CONTEXT_NO_ERROR, GLFW_TRUE );
	}
	else {
		glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );
	}

	mode = CompleteWindowMode( mode );

	if( mode.fullscreen == FullscreenMode_Windowed ) {
		window = glfwCreateWindow( mode.video_mode.width, mode.video_mode.height, APPLICATION, NULL, NULL );
		glfwSetWindowPos( window, mode.x, mode.y );
	}
	else if( mode.fullscreen == FullscreenMode_Borderless ) {
		glfwWindowHint( GLFW_DECORATED, GLFW_FALSE );
		window = glfwCreateWindow( mode.video_mode.width, mode.video_mode.height, APPLICATION, NULL, NULL );
		glfwSetWindowPos( window, mode.x, mode.y );
	}
	else if( mode.fullscreen == FullscreenMode_Fullscreen ) {
		glfwWindowHint( GLFW_REFRESH_RATE, mode.video_mode.frequency );
		GLFWmonitor * monitor = GetMonitorByIdx( mode.monitor );
		window = glfwCreateWindow( mode.video_mode.width, mode.video_mode.height, APPLICATION, monitor, NULL );
	}

	glfwGetFramebufferSize( window, &framebuffer_width, &framebuffer_height );

	{
		TracyZoneScopedN( "Set window icon" );

		GLFWimage icon;
		icon.pixels = stbi_load_from_memory( icon_png, icon_png_len, &icon.width, &icon.height, NULL, 4 );
		assert( icon.pixels != NULL );
		glfwSetWindowIcon( window, 1, &icon );
		stbi_image_free( icon.pixels );
	}

	if( glfwRawMouseMotionSupported() ) {
		glfwSetInputMode( window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE );
	}

	glfwSetWindowPosCallback( window, OnWindowMoved );
	glfwSetWindowSizeCallback( window, OnWindowResized );
	glfwSetMouseButtonCallback( window, OnMouseClicked );
	glfwSetScrollCallback( window, OnScroll );
	glfwSetKeyCallback( window, OnKeyPressed );
	glfwSetCharCallback( window, OnCharTyped );

	glfwMakeContextCurrent( window );

	{
		TracyZoneScopedN( "Load OpenGL" );
		if( gladLoadGLLoader( ( GLADloadproc ) glfwGetProcAddress ) != 1 ) {
			Fatal( "Couldn't load GL" );
		}
	}
}

void DestroyWindow() {
	TracyZoneScoped;
	glfwDestroyWindow( window );
}

void GetFramebufferSize( int * width, int * height ) {
	*width = framebuffer_width;
	*height = framebuffer_height;
}

void FlashWindow() {
	glfwRequestWindowAttention( window );
}

VideoMode GetVideoMode( int monitor ) {
	const GLFWvidmode * glfw_mode = glfwGetVideoMode( GetMonitorByIdx( monitor ) );

	VideoMode mode;
	mode.width = glfw_mode->width;
	mode.height = glfw_mode->height;
	mode.frequency = glfw_mode->refreshRate;
	return mode;
}

WindowMode GetWindowMode() {
	WindowMode mode = { };

	glfwGetWindowPos( window, &mode.x, &mode.y );
	glfwGetWindowSize( window, &mode.video_mode.width, &mode.video_mode.height );

	GLFWmonitor * monitor = glfwGetWindowMonitor( window );
	if( monitor != NULL ) {
		const GLFWvidmode * monitor_mode = glfwGetVideoMode( monitor );
		mode.fullscreen = FullscreenMode_Fullscreen;
		mode.video_mode.frequency = monitor_mode->refreshRate;
		mode.monitor = int( uintptr_t( glfwGetMonitorUserPointer( monitor ) ) );
	}
	else {
		bool borderless = glfwGetWindowAttrib( window, GLFW_DECORATED ) == GLFW_FALSE;
		mode.fullscreen = borderless ? FullscreenMode_Borderless : FullscreenMode_Windowed;

		if( borderless ) {
			int num_monitors;
			GLFWmonitor ** monitors = glfwGetMonitors( &num_monitors );

			for( int i = 0; i < num_monitors; i++ ) {
				int x, y;
				glfwGetMonitorPos( monitors[ i ], &x, &y );

				if( x == mode.x && y == mode.y ) {
					mode.monitor = i;
					break;
				}
			}
		}
	}

	return mode;
}

void SetWindowMode( WindowMode mode ) {
	mode = CompleteWindowMode( mode );

	if( mode.fullscreen == FullscreenMode_Windowed ) {
		glfwSetWindowAttrib( window, GLFW_DECORATED, GLFW_TRUE );
		glfwSetWindowMonitor( window, NULL, mode.x, mode.y, mode.video_mode.width, mode.video_mode.height, 0 );
	}
	else if( mode.fullscreen == FullscreenMode_Borderless ) {
		glfwSetWindowAttrib( window, GLFW_DECORATED, GLFW_FALSE );
		glfwSetWindowMonitor( window, NULL, mode.x, mode.y, mode.video_mode.width, mode.video_mode.height, 0 );
	}
	else {
		GLFWmonitor * monitor = GetMonitorByIdx( mode.monitor );
		glfwSetWindowMonitor( window, monitor, mode.x, mode.y, mode.video_mode.width, mode.video_mode.height, mode.video_mode.frequency );
	}
}

void EnableVSync( bool enabled ) {
	glfwSwapInterval( enabled ? 1 : 0 );
}

bool IsWindowFocused() {
	return IFDEF( PLATFORM_LINUX ) ? true : glfwGetWindowAttrib( window, GLFW_FOCUSED );
}

Vec2 GetJoystickMovement() {
	if( route_inputs_to_imgui )
		return Vec2( 0.0f );

	Vec2 acc = Vec2( 0.0f );

	for( int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++ ) {
		if( !glfwJoystickPresent( i ) )
			continue;

		int n;
		const float * axes = glfwGetJoystickAxes( i, &n );

		if( n < 2 )
			continue;

		acc.x += axes[ GLFW_GAMEPAD_AXIS_LEFT_X ];
		acc.y -= axes[ GLFW_GAMEPAD_AXIS_LEFT_Y ];

		constexpr float deadzone = 0.25f;
		acc.x = Unlerp01( deadzone, Abs( acc.x ), 1.0f ) * SignedOne( acc.x );
		acc.y = Unlerp01( deadzone, Abs( acc.y ), 1.0f ) * SignedOne( acc.y );
	}

	return acc;
}

static double last_mouse_x, last_mouse_y;
static Vec2 relative_mouse_movement;

Vec2 GetRelativeMouseMovement() {
	return relative_mouse_movement;
}

static void InputFrame() {
	// route inputs
	route_inputs_to_imgui = false;

	const ImGuiContext * ctx = ImGui::GetCurrentContext();
	for( const ImGuiWindow * w : ctx->Windows ) {
		if( w->Active && w->ParentWindow == NULL && ( w->Flags & ImGuiWindowFlags_Interactive ) != 0 ) {
			route_inputs_to_imgui = true;
			break;
		}
	}

	// handle mouse movement
	double mouse_x, mouse_y;
	glfwGetCursorPos( window, &mouse_x, &mouse_y );

	if( route_inputs_to_imgui ) {
		relative_mouse_movement = Vec2( 0.0f );
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
		Key_ClearStates();
	}
	else if( running_in_debugger ) {
		// don't grab input if we're running a debugger
		relative_mouse_movement = Vec2( mouse_x, mouse_y ) - frame_static.viewport * 0.5f;
		glfwSetCursorPos( window, frame_static.viewport.x * 0.5f, frame_static.viewport.y * 0.5f );
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
	}
	else {
		relative_mouse_movement = Vec2( mouse_x - last_mouse_x, mouse_y - last_mouse_y );
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
	}

	last_mouse_x = mouse_x;
	last_mouse_y = mouse_y;

	// break bools
	break1 = glfwGetKey( window, GLFW_KEY_F1 );
	break2 = glfwGetKey( window, GLFW_KEY_F2 );
	break3 = glfwGetKey( window, GLFW_KEY_F3 );
	break4 = glfwGetKey( window, GLFW_KEY_F4 );
}

void SwapBuffers() {
	TracyZoneScoped;
	glfwSwapBuffers( window );
}

int main( int argc, char ** argv ) {
	running_in_debugger = !is_public_build && Sys_BeingDebugged();

	{
		TracyZoneScopedN( "Init GLFW" );

		glfwSetErrorCallback( OnGlfwError );

		if( !glfwInit() ) {
			Fatal( "glfwInit" );
		}

		glfwSetMonitorCallback( OnMonitorConfigChange );
		AssignMonitorNumbers();
	}

	Con_Init();
	Qcommon_Init( argc, argv );

	s64 oldtime = Sys_Milliseconds();
	while( !glfwWindowShouldClose( window ) ) {
		s64 dt = 0;
		{
			TracyZoneScopedN( "Interframe" );
			while( dt == 0 ) {
				dt = Sys_Milliseconds() - oldtime;
			}
			oldtime += dt;
		}

		InputFrame();

		{
			TracyZoneScopedN( "glfwPollEvents" );
			glfwPollEvents();
		}

		if( !Qcommon_Frame( dt ) ) {
			break;
		}
	}

	Qcommon_Shutdown();

	glfwTerminate();

	return 0;
}
