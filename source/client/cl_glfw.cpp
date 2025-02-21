#include "client/client.h"
#include "client/icon.h"
#include "client/keys.h"
#include "client/renderer/renderer.h"
#include "qcommon/fpe.h"
#include "qcommon/renderdoc.h"

#define GLFW_INCLUDE_NONE
#include "glfw3/GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "stb/stb_image.h"

const bool is_dedicated_server = false;

GLFWwindow * window = NULL;

static bool running_in_debugger;
static bool running_in_renderdoc;
static bool route_inputs_to_imgui;

static int framebuffer_width, framebuffer_height;

// TODO
extern Cvar * vid_mode;

static void UpdateVidModeCvar() {
	TempAllocator temp = cls.frame_arena.temp();
	Cvar_Set( "vid_mode", temp.sv( "{}", GetWindowMode() ) );
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

static void OnMouseClicked( GLFWwindow *, int glfw, int action, int mods ) {
	Key key;
	Optional< ImGuiMouseButton > imgui = NONE;
	switch( glfw ) {
		case GLFW_MOUSE_BUTTON_LEFT:
			key = Key_MouseLeft;
			imgui = ImGuiMouseButton_Left;
			break;

		case GLFW_MOUSE_BUTTON_RIGHT:
			key = Key_MouseRight;
			imgui = ImGuiMouseButton_Right;
			break;

		case GLFW_MOUSE_BUTTON_MIDDLE:
			key = Key_MouseMiddle;
			imgui = ImGuiMouseButton_Middle;
			break;

		case GLFW_MOUSE_BUTTON_4:
			key = Key_Mouse4;
			imgui = 3;
			break;

		case GLFW_MOUSE_BUTTON_5:
			key = Key_Mouse5;
			imgui = 4;
			break;

		default:
			return;
	}

	bool down = action == GLFW_PRESS;
	if( imgui.exists ) {
		ImGui::GetIO().AddMouseButtonEvent( imgui.value, down );
	}

	if( !route_inputs_to_imgui ) {
		KeyEvent( key, down );
	}
}

static void OnScroll( GLFWwindow *, double x, double y ) {
	if( !route_inputs_to_imgui && y != 0 ) {
		Key key = y > 0 ? Key_MouseWheelUp : Key_MouseWheelDown;
		KeyEvent( key, true );
		KeyEvent( key, false );
	}

	ImGui::GetIO().AddMouseWheelEvent( x, y );
}

ImGuiKey KeyToImGui( Key key );
Optional< Key > KeyFromGLFW( int glfw );

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

	// renderdoc uses F12 to trigger a capture
	if( glfw_key == GLFW_KEY_F12 && running_in_renderdoc ) {
		return;
	}

	Optional< Key > key = KeyFromGLFW( glfw_key );
	if( !key.exists )
		return;

	ImGui::GetIO().AddKeyEvent( KeyToImGui( key.value ), down );

	bool is_f_key = key.value >= Key_F1 && key.value <= Key_F12;
	if( !route_inputs_to_imgui || is_f_key ) {
		KeyEvent( key.value, down );
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
		Fatal( "Your GPU is too old, you need a GPU that supports OpenGL 4.5" );
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

#if PLATFORM_MACOS
	DisableFPEScoped;
#endif

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
		Assert( icon.pixels != NULL );
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

		if( !glfwJoystickIsGamepad( i ) )
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
		AllKeysUp();
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
	break1 = glfwGetKey( window, GLFW_KEY_F1 ) == GLFW_PRESS;
	break2 = glfwGetKey( window, GLFW_KEY_F2 ) == GLFW_PRESS;
	break3 = glfwGetKey( window, GLFW_KEY_F3 ) == GLFW_PRESS;
	break4 = glfwGetKey( window, GLFW_KEY_F4 ) == GLFW_PRESS;
}

void SwapBuffers() {
	TracyZoneScoped;
	glfwSwapBuffers( window );
}

int main( int argc, char ** argv ) {
	running_in_debugger = !is_public_build && Sys_BeingDebugged();
	running_in_renderdoc = IsRenderDocAttached();

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
