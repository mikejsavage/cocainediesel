/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client/client.h"
#include "client/renderer/renderer.h"
#include "qcommon/string.h"
#include "sdl/sdl_window.h"

static cvar_t *vid_mode;
static cvar_t *vid_vsync;
static bool force_vsync;

static bool vid_app_active;
static bool vid_app_minimized;

static VideoMode startup_video_mode;

void VID_AppActivate( bool active, bool minimize ) {
	vid_app_active = active;
	vid_app_minimized = minimize;
}

bool VID_AppIsActive() {
	return vid_app_active;
}

bool VID_AppIsMinimized() {
	return vid_app_minimized;
}

static bool ParseWindowMode( const char * str, WindowMode * mode ) {
	*mode = { };

	// windowed shorthand
	{
		int comps = sscanf( str, "%dx%d", &mode->video_mode.width, &mode->video_mode.height );
		if( comps == 2 ) {
			mode->fullscreen = FullScreenMode_Windowed;
			mode->x = -1;
			mode->y = -1;
			return true;
		}
	}

	// windowed
	{
		int comps = sscanf( str, "W %dx%d %dx%d", &mode->video_mode.width, &mode->video_mode.height, &mode->x, &mode->y );
		if( comps == 4 ) {
			mode->fullscreen = FullScreenMode_Windowed;
			return true;
		}
	}

	// borderless
	{
		int comps = sscanf( str, "B %d", &mode->monitor );
		if( comps == 1 ) {
			mode->fullscreen = FullScreenMode_FullscreenBorderless;
			return true;
		}
	}

	// fullscreen
	{
		int comps = sscanf( str, "F %d %dx%d %dHz", &mode->monitor, &mode->video_mode.width, &mode->video_mode.height, &mode->video_mode.frequency );
		if( comps == 4 ) {
			mode->fullscreen = FullScreenMode_Fullscreen;
			return true;
		}
	}

	return false;
}

static void UpdateVidModeCvar() {
	WindowMode mode = VID_GetWindowMode();
	String< 128 > buf( "{}", mode );
	Cvar_Set( vid_mode->name, buf.c_str() );
	vid_mode->modified = false;
}

void VID_CheckChanges() {
	if( vid_vsync->modified ) {
		VID_EnableVsync( force_vsync || vid_vsync->integer != 0 ? VsyncEnabled_Enabled : VsyncEnabled_Disabled );
		vid_vsync->modified = false;
	}

	if( !vid_mode->modified )
		return;
	vid_mode->modified = false;

	WindowMode mode;
	if( ParseWindowMode( vid_mode->string, &mode ) ) {
		VID_SetWindowMode( mode );
	}
	else {
		Com_Printf( "Invalid vid_mode string\n" );
	}

	UpdateVidModeCvar();
}

void VID_Init() {
	ZoneScoped;

	vid_mode = Cvar_Get( "vid_mode", "", CVAR_ARCHIVE );
	vid_mode->modified = false;

	vid_vsync = Cvar_Get( "vid_vsync", "0", CVAR_ARCHIVE );
	force_vsync = false;

	WindowMode mode;
	startup_video_mode = VID_GetCurrentVideoMode();

	if( !ParseWindowMode( vid_mode->string, &mode ) ) {
		mode = { };
		mode.video_mode = startup_video_mode;
		mode.fullscreen = FullScreenMode_FullscreenBorderless;
		mode.x = -1;
		mode.y = -1;
	}

	VID_WindowInit( mode );
	UpdateVidModeCvar();

	InitRenderer();

	if( !S_Init() ) {
		Com_Printf( S_COLOR_RED "Couldn't initialise audio engine\n" );
	}

	// TODO: what is this?
	if( cls.cgameActive ) {
		CL_GameModule_Init();
		CL_SetKeyDest( key_game );
	} else {
		CL_SetKeyDest( key_menu );
	}
}

void CL_ForceVsync( bool force ) {
	if( force != force_vsync ) {
		force_vsync = force;
		vid_vsync->modified = true;
	}
}

void VID_Shutdown() {
	ShutdownRenderer();

	VID_SetVideoMode( startup_video_mode );

	VID_WindowShutdown();
}
