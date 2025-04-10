#include "client/client.h"
#include "client/icon.h"
#include "client/keys.h"
#include "client/audio/backend.h"
#include "client/platform/renderdoc.h"
#include "qcommon/array.h"
#include "qcommon/version.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_sdl3.h"

#define SDL_MAIN_USE_CALLBACKS
#include "sdl/SDL3/SDL_audio.h"
#include "sdl/SDL3/SDL_events.h"
#include "sdl/SDL3/SDL_init.h"
#include "sdl/SDL3/SDL_main.h"
#include "sdl/SDL3/SDL_mouse.h"
#include "sdl/SDL3/SDL_video.h"

#include "stb/stb_image.h"

const bool is_dedicated_server = false;

SDL_Window * window;
static SDL_GLContext gl_context;

static bool running_in_renderdoc;
static bool route_inputs_to_imgui;

static float content_scale;

static int framebuffer_width, framebuffer_height;

template< typename R = bool, typename F, typename... Rest >
static R TrySDLImpl( const char * function_name, F f, const Rest & ... args ) {
	R res = f( args... );
	if( !res ) {
		Fatal( "%s: %s", function_name, SDL_GetError() );
	}
	return res;
}

#define TrySDL( f, ... ) TrySDLImpl( #f, f, ##__VA_ARGS__ )
#define TrySDLR( R, f, ... ) TrySDLImpl< R >( #f, f, ##__VA_ARGS__ )

static SDL_DisplayID DisplayIndexToID( int monitor ) {
	int num_monitors;
	SDL_DisplayID * monitors = TrySDLR( SDL_DisplayID *, SDL_GetDisplays, &num_monitors );
	defer { SDL_free( monitors ); };
	return monitors[ monitor < num_monitors ? monitor : 0 ];
}

void CreateWindow( WindowMode mode ) {
	TracyZoneScoped;

	TrySDL( SDL_GL_SetAttribute, SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	TrySDL( SDL_GL_SetAttribute, SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
	TrySDL( SDL_GL_SetAttribute, SDL_GL_CONTEXT_MINOR_VERSION, 5 );
	TrySDL( SDL_GL_SetAttribute, SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, true );

	SDL_GLContextFlag gl_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
	if( is_public_build ) {
		TrySDL( SDL_GL_SetAttribute, SDL_GL_CONTEXT_NO_ERROR, true );
	}
	else {
		gl_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	}
	TrySDL( SDL_GL_SetAttribute, SDL_GL_CONTEXT_FLAGS, gl_flags );

	SDL_PropertiesID props = TrySDLR( SDL_PropertiesID, SDL_CreateProperties );
	defer { SDL_DestroyProperties( props ); };

	TrySDL( SDL_SetStringProperty, props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, APPLICATION );
	TrySDL( SDL_SetBooleanProperty, props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true );
	TrySDL( SDL_SetBooleanProperty, props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true );
	TrySDL( SDL_SetBooleanProperty, props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true );
	TrySDL( SDL_SetNumberProperty, props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, mode.video_mode.width );
	TrySDL( SDL_SetNumberProperty, props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, mode.video_mode.height );

	SDL_DisplayID monitor = DisplayIndexToID( mode.monitor );

	if( mode.fullscreen == FullscreenMode_Windowed ) {
		TrySDL( SDL_SetNumberProperty, props, SDL_PROP_WINDOW_CREATE_X_NUMBER, mode.x == -1 ? SDL_WINDOWPOS_CENTERED : mode.x );
		TrySDL( SDL_SetNumberProperty, props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, mode.y == -1 ? SDL_WINDOWPOS_CENTERED : mode.y );
	}
	else {
		TrySDL( SDL_SetBooleanProperty, props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true );
		TrySDL( SDL_SetNumberProperty, props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_UNDEFINED_DISPLAY( monitor ) );
		TrySDL( SDL_SetNumberProperty, props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_UNDEFINED_DISPLAY( monitor ) );
	}

	window = TrySDLR( SDL_Window *, SDL_CreateWindowWithProperties, props );

	if( mode.fullscreen == FullscreenMode_Fullscreen ) {
		SDL_DisplayMode closest;
		if( !SDL_GetClosestFullscreenDisplayMode( monitor, mode.video_mode.width, mode.video_mode.height, mode.video_mode.refresh_rate, true, &closest ) ) {
			closest = *TrySDLR( const SDL_DisplayMode *, SDL_GetDesktopDisplayMode, monitor );
		}
		TrySDL( SDL_SetWindowFullscreenMode, window, &closest );
	}

	gl_context = TrySDLR( SDL_GLContext, SDL_GL_CreateContext, window );

	TrySDL( SDL_GetWindowSizeInPixels, window, &framebuffer_width, &framebuffer_height );

	content_scale = TrySDLR( float, SDL_GetWindowDisplayScale, window );

	{
		TracyZoneScopedN( "Set window icon" );

		int w, h;
		u8 * pixels = stbi_load_from_memory( icon_png, icon_png_len, &w, &h, NULL, 4 );
		Assert( pixels != NULL );
		SDL_Surface * icon = TrySDLR( SDL_Surface *, SDL_CreateSurfaceFrom, w, h, SDL_PIXELFORMAT_RGBA8888, pixels, w * 4 );
		TrySDL( SDL_SetWindowIcon, window, icon );
		SDL_DestroySurface( icon );
		stbi_image_free( pixels );
	}
}

void DestroyWindow() {
	TracyZoneScoped;
	TrySDL( SDL_GL_DestroyContext, gl_context );
	SDL_DestroyWindow( window );
}

void GetFramebufferSize( int * width, int * height ) {
	*width = framebuffer_width;
	*height = framebuffer_height;
}

float GetContentScale() {
	return content_scale;
}

void FlashWindow() {
	SDL_FlashWindow( window, SDL_FLASH_UNTIL_FOCUSED );
}

VideoMode GetVideoMode( int monitor ) {
	const SDL_DisplayMode * sdl_mode = SDL_GetDesktopDisplayMode( DisplayIndexToID( monitor ) );
	return VideoMode {
		.width = sdl_mode->w,
		.height = sdl_mode->h,
		.refresh_rate = sdl_mode->refresh_rate,
	};
}

// top secret SDL API
// obviously you can write this yourself but not without allocating
extern "C" int SDL_GetDisplayIndex( SDL_DisplayID displayID );

WindowMode GetWindowMode() {
	WindowMode mode = { .fullscreen = FullscreenMode_Windowed };

	TrySDL( SDL_GetWindowPosition, window, &mode.x, &mode.y );
	TrySDL( SDL_GetWindowSizeInPixels, window, &mode.video_mode.width, &mode.video_mode.height );

	if( HasAllBits( SDL_GetWindowFlags( window ), SDL_WINDOW_FULLSCREEN ) ) {
		const SDL_DisplayMode * fullscreen = SDL_GetWindowFullscreenMode( window );
		if( fullscreen != NULL ) {
			mode.fullscreen = FullscreenMode_Fullscreen;
			mode.video_mode.refresh_rate = fullscreen->refresh_rate;
			mode.monitor = SDL_GetDisplayIndex( fullscreen->displayID );
		}
		else {
			mode.fullscreen = FullscreenMode_Borderless;

			int num_monitors;
			SDL_DisplayID * monitors = SDL_GetDisplays( &num_monitors );
			defer { SDL_free( monitors ); };

			for( int i = 0; i < num_monitors; i++ ) {
				SDL_Rect rect;
				TrySDL( SDL_GetDisplayBounds, monitors[ i ], &rect );
				if( rect.x == mode.x && rect.y == mode.y ) {
					mode.monitor = i;
				}
			}
		}
	}

	return mode;
}

void SetWindowMode( WindowMode mode ) {
	switch( mode.fullscreen ) {
		case FullscreenMode_Windowed:
			TrySDL( SDL_SetWindowFullscreen, window, false );
			TrySDL( SDL_SetWindowSize, window, mode.video_mode.width, mode.video_mode.height );
			TrySDL( SDL_SetWindowPosition, window, mode.x == -1 ? SDL_WINDOWPOS_CENTERED : mode.x, mode.y == -1 ? SDL_WINDOWPOS_CENTERED : mode.y );
			break;

		case FullscreenMode_Borderless: {
			SDL_DisplayID monitor = DisplayIndexToID( mode.monitor );
			SDL_Rect rect;
			TrySDL( SDL_GetDisplayBounds, monitor, &rect );
			TrySDL( SDL_SetWindowFullscreen, window, true );
			TrySDL( SDL_SetWindowFullscreenMode, window, ( SDL_DisplayMode * ) NULL );
			TrySDL( SDL_SetWindowPosition, window, rect.x, rect.y );
			// TrySDL( SDL_SetWindowSize, window, rect.w, rect.h );
		} break;

		case FullscreenMode_Fullscreen: {
			SDL_DisplayID monitor = DisplayIndexToID( mode.monitor );
			SDL_DisplayMode closest;
			if( !SDL_GetClosestFullscreenDisplayMode( monitor, mode.video_mode.width, mode.video_mode.height, mode.video_mode.refresh_rate, true, &closest ) ) {
				closest = *TrySDLR( const SDL_DisplayMode *, SDL_GetDesktopDisplayMode, monitor );
			}
			TrySDL( SDL_SetWindowFullscreen, window, true );
			TrySDL( SDL_SetWindowFullscreenMode, window, &closest );
		} break;
	}
}

static Optional< bool > has_gsync;
void EnableVSync( bool enabled ) {
	if( enabled ) {
		if( !has_gsync.exists ) {
			has_gsync = SDL_GL_SetSwapInterval( -1 );
		}
		TrySDL( SDL_GL_SetSwapInterval, has_gsync.value ? -1 : 1 );
	}
	else {
		TrySDL( SDL_GL_SetSwapInterval, 0 );
	}
}

bool IsWindowFocused() {
	return HasAllBits( SDL_GetWindowFlags( window ), SDL_WINDOW_INPUT_FOCUS );
}

Vec2 GetJoystickMovement() {
	return ImGui::GetKeyMagnitude2d( ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp );
}

static Vec2 relative_mouse_movement;

Vec2 GetRelativeMouseMovement() {
	return relative_mouse_movement;
}

void SwapBuffers() {
	TracyZoneScopedNC( "SDL_GL_SwapWindow", 0xff0000 );
	TrySDL( SDL_GL_SwapWindow, window );
}

Optional< Key > KeyFromSDL( SDL_Scancode sdl );
static void OnKeyPressed( const SDL_KeyboardEvent & e ) {
	if( e.repeat )
		return;

	// break bools
	if( e.scancode == SDL_SCANCODE_F1 ) break1 = e.down;
	if( e.scancode == SDL_SCANCODE_F2 ) break2 = e.down;
	if( e.scancode == SDL_SCANCODE_F3 ) break3 = e.down;
	if( e.scancode == SDL_SCANCODE_F4 ) break4 = e.down;

	// console key
	if( e.scancode == SDL_SCANCODE_GRAVE ) {
		if( e.down ) {
			Con_ToggleConsole();
		}
		return;
	}

	// renderdoc uses F12 to trigger a capture
	if( e.scancode == SDL_SCANCODE_F12 && running_in_renderdoc ) {
		return;
	}

	Optional< Key > key = KeyFromSDL( e.scancode );
	if( !key.exists )
		return;

	bool is_f_key = key.value >= Key_F1 && key.value <= Key_F12;
	if( !route_inputs_to_imgui || is_f_key ) {
		KeyEvent( key.value, e.down );
	}
}

static void OnMouseClicked( const SDL_MouseButtonEvent & e ) {
	if( route_inputs_to_imgui )
		return;

	Key key;
	switch( e.button ) {
		case SDL_BUTTON_LEFT: key = Key_MouseLeft; break;
		case SDL_BUTTON_RIGHT: key = Key_MouseRight; break;
		case SDL_BUTTON_MIDDLE: key = Key_MouseMiddle; break;
		case SDL_BUTTON_X1: key = Key_Mouse4; break;
		case SDL_BUTTON_X2: key = Key_Mouse5; break;
		default: return;
	}

	KeyEvent( key, e.down );
}

static void OnMouseMoved( const SDL_MouseMotionEvent & e ) {
	if( !route_inputs_to_imgui ) {
		relative_mouse_movement += Vec2( e.xrel, e.yrel );
	}
}

static void OnScroll( const SDL_MouseWheelEvent & e ) {
	if( !route_inputs_to_imgui && e.y != 0 ) {
		Key key = e.y > 0 ? Key_MouseWheelUp : Key_MouseWheelDown;
		KeyEvent( key, true );
		KeyEvent( key, false );
	}
}

static void OnWindowResizedOrMoved( const SDL_WindowEvent & e ) {
	if( e.type != SDL_EVENT_WINDOW_RESIZED || ( e.data1 > 0 && e.data2 > 0 ) ) {
		TempAllocator temp = cls.frame_arena.temp();
		Cvar_Set( "vid_mode", temp.sv( "{}", GetWindowMode() ) );
		vid_mode->modified = false;
	}

	if( e.type == SDL_EVENT_WINDOW_RESIZED && e.data1 > 0 && e.data2 > 0 ) {
		framebuffer_width = e.data1;
		framebuffer_height = e.data2;
	}
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

	// grab cursor
	if( route_inputs_to_imgui ) {
		TrySDL( SDL_SetWindowRelativeMouseMode, window, false );
		AllKeysUp();
	}
	else {
		TrySDL( SDL_SetWindowRelativeMouseMode, window, true );
	}
}

/*
 * audio
 */

static SDL_AudioStream * sdl_audio;

Span< Span< const char > > GetAudioDeviceNames( TempAllocator * temp ) {
	NonRAIIDynamicArray< Span< const char > > names( temp );

	int num_devices;
	SDL_AudioDeviceID * devices = TrySDLR( SDL_AudioDeviceID *, SDL_GetAudioPlaybackDevices, &num_devices );
	defer { SDL_free( devices ); };

	for( int i = 0; i < num_devices; i++ ) {
		const char * name = TrySDLR( const char *, SDL_GetAudioDeviceName, devices[ i ] );
		names.add( CloneSpan( temp, MakeSpan( name ) ) );
	}

	return names.span();
}

static void SDLAudioCallback( void * userdata, SDL_AudioStream * stream, int additional_amount, int total_amount ) {
	AudioBackendCallback callback = AudioBackendCallback( userdata );

	Vec2 samples[ 4096 ];
	size_t num_samples = additional_amount / sizeof( Vec2 );

	for( size_t i = 0; i < num_samples; i += ARRAY_COUNT( samples ) ) {
		size_t chunk = Min2( num_samples - i, ARRAY_COUNT( samples ) );
		callback( Span< Vec2 >( samples, chunk ) );
		TrySDL( SDL_PutAudioStreamData, stream, samples, chunk * sizeof( samples[ 0 ] ) );
	}
}

bool InitAudioDevice( const char * preferred_device, AudioBackendCallback callback ) {
	TracyZoneScoped;

	SDL_AudioSpec sdl_audio_spec = {
		.format = SDL_AUDIO_F32,
		.channels = 2,
		.freq = AUDIO_BACKEND_SAMPLE_RATE,
	};

	SDL_AudioDeviceID id = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
	if( !StrEqual( preferred_device, "" ) ) {
		int num_devices;
		SDL_AudioDeviceID * devices = TrySDLR( SDL_AudioDeviceID *, SDL_GetAudioPlaybackDevices, &num_devices );
		defer { SDL_free( devices ); };

		for( int i = 0; i < num_devices; i++ ) {
			const char * name = TrySDLR( const char *, SDL_GetAudioDeviceName, devices[ i ] );
			if( StrEqual( name, preferred_device ) ) {
				id = devices[ i ];
				break;
			}
		}

		if( id == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ) {
			Com_Printf( S_COLOR_YELLOW "Can't find audio device %s, using default\n", preferred_device );
		}
	}

	sdl_audio = SDL_OpenAudioDeviceStream( id, &sdl_audio_spec, SDLAudioCallback, ( void * ) callback );
	if( sdl_audio == NULL ) {
		Com_Printf( S_COLOR_RED "Can't open audio device: %s\n", SDL_GetError() );
		return false;
	}

	TrySDL( SDL_ResumeAudioStreamDevice, sdl_audio );

	return true;
}

void ShutdownAudioDevice() {
	TracyZoneScoped;
	SDL_DestroyAudioStream( sdl_audio );
}

/*
 * main
 */

static s64 oldtime;

SDL_AppResult SDL_AppInit( void ** appstate, int argc, char ** argv ) {
	running_in_renderdoc = IsRenderDocAttached();

	{
		TracyZoneScopedN( "Init SDL" );
		TrySDL( SDL_SetAppMetadata, APPLICATION, APP_VERSION, "fun.cocainediesel" );
		TrySDL( SDL_Init, SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD );
	}

	Con_Init();
	Qcommon_Init( argc, argv );

	relative_mouse_movement = Vec2( 0.0f );
	oldtime = Sys_Milliseconds();

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent( void * appstate, SDL_Event * event ) {
	ImGui_ImplSDL3_ProcessEvent( event );

	switch( event->type ) {
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			OnKeyPressed( event->key );
			break;

		case SDL_EVENT_MOUSE_MOTION:
			OnMouseMoved( event->motion );
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			OnMouseClicked( event->button );
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			OnScroll( event->wheel );
			break;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_MOVED:
			OnWindowResizedOrMoved( event->window );
			break;

		default: break;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate( void * appstate ) {
	s64 dt = 0;
	{
		TracyZoneScopedNC( "Interframe", 0xff0000 );
		while( dt == 0 ) {
			dt = Sys_Milliseconds() - oldtime;
		}
		oldtime += dt;
	}

	InputFrame();

	if( !Qcommon_Frame( dt ) )
		return SDL_APP_SUCCESS;

	relative_mouse_movement = Vec2( 0.0f );
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit( void * appstate, SDL_AppResult result ) {
	Qcommon_Shutdown();
	SDL_Quit();
}
