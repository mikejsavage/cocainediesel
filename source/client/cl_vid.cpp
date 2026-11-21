#include "client/client.h"
#include "client/renderer/renderer.h"

Cvar * vid_mode;
static Cvar * vid_vsync;
static bool force_vsync;

static Optional< WindowMode > ParseWindowMode( const char * str ) {
	WindowMode mode = { };

	// windowed shorthand
	{
		int comps = sscanf( str, "%dx%d", &mode.width, &mode.height );
		if( comps == 2 ) {
			mode.x = -1;
			mode.y = -1;
			return mode.width > 0 && mode.height > 0 ? Optional( mode ) : NONE;
		}
	}

	// windowed
	{
		int comps = sscanf( str, "W %dx%d %dx%d", &mode.width, &mode.height, &mode.x, &mode.y );
		if( comps == 4 ) {
			return mode.width > 0 && mode.height > 0 ? Optional( mode ) : NONE;
		}
	}

	// fullscreen
	{
		int monitor;
		int comps = sscanf( str, "F %d", &monitor );
		if( comps == 1 ) {
			return WindowMode {
				.fullscreen = true,
				.monitor = monitor,
			};
		}
	}

	return NONE;
}

void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts ) {
	if( mode.fullscreen ) {
		ggformat_impl( fb, "F {}", mode.monitor );
	}
	else {
		ggformat_impl( fb, "W {}x{} {}x{}", mode.width, mode.height, mode.x, mode.y );
	}
}

bool operator!=( WindowMode lhs, WindowMode rhs ) {
	if( lhs.fullscreen != rhs.fullscreen )
		return true;

	if( lhs.fullscreen ) {
		return lhs.monitor != rhs.monitor;
	}

	return lhs.x != rhs.x || lhs.y != rhs.y || lhs.width != rhs.width || lhs.height != rhs.height;
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

	Optional< WindowMode > mode = ParseWindowMode( vid_mode->value );
	if( mode.exists ) {
		SetWindowMode( *mode );
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

	WindowMode mode = Default( ParseWindowMode( vid_mode->value ), WindowMode {
		.fullscreen = true,
	} );

	CreateWindow( mode );
	UpdateVidModeCvar();
}

void CL_ForceVsync( bool force ) {
	if( force != force_vsync ) {
		force_vsync = force;
		vid_vsync->modified = true;
	}
}
