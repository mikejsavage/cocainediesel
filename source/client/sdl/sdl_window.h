#pragma once

#include "qcommon/types.h"

struct SDL_Window;
extern SDL_Window * sdl_window;

enum FullScreenMode {
	FullScreenMode_Windowed,
	FullScreenMode_Fullscreen,
	FullScreenMode_FullscreenBorderless,
};

struct VideoMode {
	int width, height;
	int frequency;
};

struct WindowMode {
	VideoMode video_mode;
	int monitor;
	int x, y;
	FullScreenMode fullscreen;

	bool operator==( const WindowMode & right ) {
		return ( ( ( right.video_mode.width == this->video_mode.width && //On borderless it happends that the values aren't equal for the checks we use... And videomode shouldn't matter anyway
				     right.video_mode.height == this->video_mode.height &&
				     right.video_mode.frequency == this->video_mode.frequency ) ||
				   this->fullscreen == FullScreenMode_FullscreenBorderless ) &&
				 right.monitor == this->monitor &&
				 right.x == this->x && right.y == this->y &&
				 right.fullscreen == this->fullscreen );
	}
};

enum VsyncEnabled {
	VsyncEnabled_Disabled,
	VsyncEnabled_Enabled,
};

int VID_GetNumVideoModes();
VideoMode VID_GetVideoMode( int i );
VideoMode VID_GetCurrentVideoMode();
void VID_SetVideoMode( VideoMode mode );

void VID_WindowInit( WindowMode mode );
void VID_WindowShutdown();

void VID_EnableVsync( VsyncEnabled enabled );

WindowMode VID_GetWindowMode();
void VID_SetWindowMode( WindowMode mode );

bool VID_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp );
void VID_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp );

void VID_Swap();

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts );
void format( FormatBuffer * fb, WindowMode mode, const FormatOpts & opts );
