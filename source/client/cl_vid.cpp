#include "client/client.h"
#include "client/renderer/renderer.h"

Cvar * vid_mode;
static Cvar * vid_vsync;
static bool force_vsync;

static bool ParseWindowMode( const char * str, WindowMode * mode ) {
	*mode = { };

	// windowed shorthand
	{
		int comps = sscanf( str, "%dx%d", &mode->video_mode.width, &mode->video_mode.height );
		if( comps == 2 ) {
			mode->fullscreen = FullscreenMode_Windowed;
			mode->x = -1;
			mode->y = -1;
			return mode->video_mode.width > 0 && mode->video_mode.height > 0;
		}
	}

	// windowed
	{
		int comps = sscanf( str, "W %dx%d %dx%d", &mode->video_mode.width, &mode->video_mode.height, &mode->x, &mode->y );
		if( comps == 4 ) {
			mode->fullscreen = FullscreenMode_Windowed;
			return mode->video_mode.width > 0 && mode->video_mode.height > 0;
		}
	}

	// borderless
	{
		int comps = sscanf( str, "B %d", &mode->monitor );
		if( comps == 1 ) {
			mode->fullscreen = FullscreenMode_Borderless;
			return true;
		}
	}

	// fullscreen
	{
		int comps = sscanf( str, "F %d %dx%d %fHz", &mode->monitor, &mode->video_mode.width, &mode->video_mode.height, &mode->video_mode.refresh_rate );
		if( comps == 4 ) {
			mode->fullscreen = FullscreenMode_Fullscreen;
			return mode->video_mode.width > 0 && mode->video_mode.height > 0;
		}
	}

	return false;
}

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts ) {
	ggformat_impl( fb, "{}x{} {.1}Hz", mode.width, mode.height, mode.refresh_rate );
}

void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts ) {
	switch( mode.fullscreen ) {
		case FullscreenMode_Windowed:
			ggformat_impl( fb, "W {}x{} {}x{}", mode.video_mode.width, mode.video_mode.height, mode.x, mode.y );
			break;

		case FullscreenMode_Borderless:
			ggformat_impl( fb, "B {}", mode.monitor );
			break;

		case FullscreenMode_Fullscreen:
			ggformat_impl( fb, "F {} {}x{} {.1}Hz", mode.monitor, mode.video_mode.width, mode.video_mode.height, mode.video_mode.refresh_rate );
			break;
	}
}

bool operator!=( WindowMode lhs, WindowMode rhs ) {
	if( lhs.fullscreen != rhs.fullscreen )
		return true;

	if( lhs.fullscreen != FullscreenMode_Windowed ) {
		if( lhs.video_mode.refresh_rate != rhs.video_mode.refresh_rate || lhs.monitor != rhs.monitor ) {
			return true;
		}
	}
	else {
		if( lhs.x != rhs.x ) {
			return true;
		}
	}

	return lhs.video_mode.width != rhs.video_mode.width || lhs.video_mode.height != rhs.video_mode.height;
}

static void UpdateVidModeCvar() {
	WindowMode mode = GetWindowMode();
	TempAllocator temp = cls.frame_arena.temp();
	Cvar_Set( "vid_mode", temp.sv( "{}", mode ) );
	vid_mode->modified = false;
}

void VID_CheckChanges() {
	if( vid_vsync->modified ) {
		EnableVSync( force_vsync || vid_vsync->integer != 0 );
		vid_vsync->modified = false;
	}

	if( !vid_mode->modified )
		return;
	vid_mode->modified = false;

	WindowMode mode;
	if( ParseWindowMode( vid_mode->value, &mode ) ) {
		SetWindowMode( mode );
	}
	else {
		Com_Printf( "Invalid vid_mode string\n" );
	}

	UpdateVidModeCvar();
}

void VID_Init() {
	TracyZoneScoped;

	vid_mode = NewCvar( "vid_mode", "", CvarFlag_Archive );
	vid_mode->modified = false;

	vid_vsync = NewCvar( "vid_vsync", "0", CvarFlag_Archive );
	force_vsync = false;

	WindowMode mode;
	if( !ParseWindowMode( vid_mode->value, &mode ) ) {
		mode = { };
		mode.video_mode = GetVideoMode( 0 );
		mode.fullscreen = FullscreenMode_Fullscreen;
	}

	CreateWindow( mode );
	UpdateVidModeCvar();
}

void CL_ForceVsync( bool force ) {
	if( force != force_vsync ) {
		force_vsync = force;
		vid_vsync->modified = true;
	}
}
