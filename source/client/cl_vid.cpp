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
#include "qcommon/string.h"
#include "ftlib/ftlib_public.h"
#include "renderer/r_frontend.h"
#include "sdl/sdl_window.h"

static cvar_t *vid_mode;
static cvar_t *vid_vsync;
static bool force_vsync;

viddef_t viddef; // global video state; used by other modules

ref_export_t re;

static bool vid_app_active;
static bool vid_app_minimized;

static VideoMode startup_video_mode;

void VID_AppActivate( bool active, bool minimize ) {
	vid_app_active = active;
	vid_app_minimized = minimize;
	re.AppActivate( active, minimize );
}

bool VID_AppIsActive() {
	return vid_app_active;
}

bool VID_AppIsMinimized() {
	return vid_app_minimized;
}

int VID_GetWindowWidth() {
	return viddef.width;
}

int VID_GetWindowHeight() {
	return viddef.height;
}

static void VID_LoadRefresh() {
	static ref_import_t import;

	import.Com_Error = &Com_Error;
	import.Com_Printf = &Com_Printf;
	import.Com_DPrintf = &Com_DPrintf;

	import.Sys_Milliseconds = &Sys_Milliseconds;
	import.Sys_Microseconds = &Sys_Microseconds;
	import.Sys_Sleep = &Sys_Sleep;

	import.Cvar_Get = &Cvar_Get;
	import.Cvar_Set = &Cvar_Set;
	import.Cvar_ForceSet = &Cvar_ForceSet;
	import.Cvar_SetValue = &Cvar_SetValue;
	import.Cvar_String = &Cvar_String;
	import.Cvar_Value = &Cvar_Value;

	import.Cmd_Argc = &Cmd_Argc;
	import.Cmd_Argv = &Cmd_Argv;
	import.Cmd_Args = &Cmd_Args;
	import.Cmd_AddCommand = &Cmd_AddCommand;
	import.Cmd_RemoveCommand = &Cmd_RemoveCommand;
	import.Cmd_Execute = &Cbuf_Execute;
	import.Cmd_ExecuteText = &Cbuf_ExecuteText;
	import.Cmd_SetCompletionFunc = &Cmd_SetCompletionFunc;

	import.FS_FOpenFile = &FS_FOpenFile;
	import.FS_FOpenAbsoluteFile = &FS_FOpenAbsoluteFile;
	import.FS_Read = &FS_Read;
	import.FS_Write = &FS_Write;
	import.FS_Printf = &FS_Printf;
	import.FS_Tell = &FS_Tell;
	import.FS_Seek = &FS_Seek;
	import.FS_Eof = &FS_Eof;
	import.FS_Flush = &FS_Flush;
	import.FS_FCloseFile = &FS_FCloseFile;
	import.FS_RemoveFile = &FS_RemoveFile;
	import.FS_GetFileList = &FS_GetFileList;
	import.FS_FirstExtension = &FS_FirstExtension;
	import.FS_MoveFile = &FS_MoveFile;
	import.FS_RemoveDirectory = &FS_RemoveDirectory;
	import.FS_WriteDirectory = &FS_WriteDirectory;

	import.Mutex_Create = QMutex_Create;
	import.Mutex_Destroy = QMutex_Destroy;
	import.Mutex_Lock = QMutex_Lock;
	import.Mutex_Unlock = QMutex_Unlock;

	import.BufPipe_Create = QBufPipe_Create;
	import.BufPipe_Destroy = QBufPipe_Destroy;
	import.BufPipe_Finish = QBufPipe_Finish;
	import.BufPipe_WriteCmd = QBufPipe_WriteCmd;
	import.BufPipe_ReadCmds = QBufPipe_ReadCmds;
	import.BufPipe_Wait = QBufPipe_Wait;

	// load succeeded
	ref_export_t * rep = GetRefAPI( &import );
	re = *rep;
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

// this is shit and should not exist but i am sick of working on this
void Retarded_SetWindowSize( int w, int h ) {
	viddef.width = w;
	viddef.height = h;
	glConfig.width = w;
	glConfig.height = h;
}

static void UpdateVidModeCvar() {
	WindowMode mode = VID_GetWindowMode();
	String< 128 > buf( "{}", mode );
	Cvar_Set( vid_mode->name, buf.c_str() );
	vid_mode->modified = false;

	Retarded_SetWindowSize( mode.video_mode.width, mode.video_mode.height );
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
		RF_ResizeFramebuffers();
	}
	else {
		Com_Printf( "Invalid vid_mode string\n" );
	}

	UpdateVidModeCvar();
}

void VID_Init() {
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

	CL_Profiler_InitGL();

	VID_LoadRefresh();

	if( !R_Init() ) {
		Sys_Error( "R_Init() failed" );
	}

	if( !S_Init() ) {
		Com_Printf( S_COLOR_RED "Couldn't initialise audio engine\n" );
	}

	RF_BeginRegistration();

	FTLIB_LoadLibrary( false );
	FTLIB_PrecacheFonts( true );

	CL_InitMedia();

	cls.disable_screen = 0;

	Con_Close();

	// TODO: what is this?
	if( cls.cgameActive ) {
		CL_GameModule_Init();
		CL_SetKeyDest( key_game );

		// this will precache game assets
		SCR_UpdateScreen();
	} else {
		CL_SetKeyDest( key_menu );
	}

	RF_EndRegistration();
}

void CL_ForceVsync( bool force ) {
	if( force != force_vsync ) {
		force_vsync = force;
		vid_vsync->modified = true;
	}
}

void VID_Shutdown() {
	CL_ShutdownMedia();

	FTLIB_FreeFonts( false );

	RF_Shutdown( false );

	FTLIB_Shutdown();

	VID_SetVideoMode( startup_video_mode );

	VID_WindowShutdown();

	CL_Profiler_ShutdownGL();
}
