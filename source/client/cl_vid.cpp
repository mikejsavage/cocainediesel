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

cvar_t * vid_mode;
static cvar_t * vid_vsync;
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
		int comps = sscanf( str, "F %d %dx%d %dHz", &mode->monitor, &mode->video_mode.width, &mode->video_mode.height, &mode->video_mode.frequency );
		if( comps == 4 ) {
			mode->fullscreen = FullscreenMode_Fullscreen;
			return mode->video_mode.width > 0 && mode->video_mode.height > 0;
		}
	}

	return false;
}

void format( FormatBuffer * fb, VideoMode mode, const FormatOpts & opts ) {
	ggformat_impl( fb, "{}x{} {}Hz", mode.width, mode.height, mode.frequency );
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
			ggformat_impl( fb, "F {} {}x{} {}Hz", mode.monitor, mode.video_mode.width, mode.video_mode.height, mode.video_mode.frequency );
			break;
	}
}

bool operator!=( WindowMode lhs, WindowMode rhs ) {
	if( lhs.fullscreen != rhs.fullscreen )
		return true;

	if( lhs.fullscreen ) {
		if( lhs.video_mode.frequency != rhs.video_mode.frequency || lhs.monitor != rhs.monitor ) {
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
	Cvar_Set( vid_mode->name, temp( "{}", mode ) );
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
	if( ParseWindowMode( vid_mode->string, &mode ) ) {
		SetWindowMode( mode );
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
	if( !ParseWindowMode( vid_mode->string, &mode ) ) {
		mode = { };
		mode.video_mode = GetVideoMode( mode.monitor );
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
