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

#include "client.h"

static bool scr_initialized;    // ready to draw

static int scr_draw_loading;

static cvar_t *scr_netgraph;
static cvar_t *scr_timegraph;
static cvar_t *scr_debuggraph;
static cvar_t *scr_graphheight;
static cvar_t *scr_graphscale;
static cvar_t *scr_graphshift;

/*
* SCR_DrawFillRect
*
* Fills a box of pixels with a single color
*/
void SCR_DrawFillRect( int x, int y, int w, int h, const vec4_t color ) {
	Draw2DBox( frame_static.ui_pass, x, y, w, h, cls.whiteTexture, FromQF4( color ) );
}

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
* CL_AddNetgraph
*
* A new packet was just parsed
*/
void CL_AddNetgraph( void ) {
	int i;
	int ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if( scr_timegraph->integer ) {
		return;
	}

	for( i = 0; i < cls.netchan.dropped; i++ )
		SCR_DebugGraph( 30.0f, 0.655f, 0.231f, 0.169f );

	// see what the latency was on this packet
	ping = cls.realtime - cl.cmd_time[cls.ucmdAcknowledged & CMD_MASK];
	ping /= 30;
	if( ping > 30 ) {
		ping = 30;
	}
	SCR_DebugGraph( ping, 1.0f, 0.75f, 0.06f );
}


typedef struct {
	float value;
	vec4_t color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
* SCR_DebugGraph
*/
void SCR_DebugGraph( float value, float r, float g, float b ) {
	values[current].value = value;
	values[current].color[0] = r;
	values[current].color[1] = g;
	values[current].color[2] = b;
	values[current].color[3] = 1.0f;

	current++;
	current &= 1023;
}

/*
* SCR_DrawDebugGraph
*/
static void SCR_DrawDebugGraph( void ) {
	int a, x, y, w, i, h, s;
	float v;

	//
	// draw the graph
	//
	w = frame_static.viewport_width;
	x = 0;
	y = 0 + frame_static.viewport_height;
	SCR_DrawFillRect( x, y - scr_graphheight->integer,
					  w, scr_graphheight->integer, colorBlack );

	s = ( w + 1024 - 1 ) / 1024; //scale for resolutions with width >1024

	for( a = 0; a < w; a++ ) {
		i = ( current - 1 - a + 1024 ) & 1023;
		v = values[i].value;
		v = v * scr_graphscale->integer + scr_graphshift->integer;

		if( v < 0 ) {
			v += scr_graphheight->integer * ( 1 + (int)( -v / scr_graphheight->integer ) );
		}
		h = (int)v % scr_graphheight->integer;
		SCR_DrawFillRect( x + w - 1 - a * s, y - h, s, h, values[i].color );
	}
}

//============================================================================

/*
* SCR_InitScreen
*/
void SCR_InitScreen( void ) {
	scr_netgraph = Cvar_Get( "netgraph", "0", 0 );
	scr_timegraph = Cvar_Get( "timegraph", "0", 0 );
	scr_debuggraph = Cvar_Get( "debuggraph", "0", 0 );
	scr_graphheight = Cvar_Get( "graphheight", "32", 0 );
	scr_graphscale = Cvar_Get( "graphscale", "1", 0 );
	scr_graphshift = Cvar_Get( "graphshift", "0", 0 );

	scr_initialized = true;
}

/*
* SCR_ShutdownScreen
*/
void SCR_ShutdownScreen( void ) {
	scr_initialized = false;
}


//=============================================================================

/*
* SCR_BeginLoadingPlaque
*/
void SCR_BeginLoadingPlaque( void ) {
	S_StopAllSounds( true );

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );

	scr_draw_loading = 2;   // clear to black first
}

/*
* SCR_EndLoadingPlaque
*/
void SCR_EndLoadingPlaque( void ) {
	cls.disable_screen = 0;
}


//=======================================================

/*
* SCR_RegisterConsoleMedia
*/
void SCR_RegisterConsoleMedia() {
	cls.whiteTexture = FindTexture( "$whiteimage" );
}

//============================================================================

/*
* SCR_RenderView
*/
static void SCR_RenderView() {
	// frame is not valid until we load the CM data
	if( cl.cms != NULL ) {
		CL_GameModule_RenderView();
	}
}

/*
* SCR_UpdateScreen
*/
void SCR_UpdateScreen() {
	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if( cls.disable_screen ) {
		if( Sys_Milliseconds() - cls.disable_screen > 120000 ) {
			cls.disable_screen = 0;
			Com_Printf( "Loading plaque timed out.\n" );
		}
		return;
	}

	if( !scr_initialized || !cls.mediaInitialized ) {
		return; // not ready yet
	}

	CL_ForceVsync( cls.state == CA_DISCONNECTED );

	CL_ImGuiBeginFrame();

	if( scr_draw_loading == 2 ) {
		// loading plaque over APP_STARTUP_COLOR screen
		scr_draw_loading = 0;
		UI_UpdateConnectScreen();
	} else if( cls.state == CA_DISCONNECTED ) {
		UI_Refresh();
	} else if( cls.state == CA_CONNECTING || cls.state == CA_HANDSHAKE ) {
		UI_UpdateConnectScreen();
	} else if( cls.state == CA_CONNECTED ) {
		if( cls.cgameActive ) {
			UI_UpdateConnectScreen();
			SCR_RenderView();
		} else {
			UI_UpdateConnectScreen();
		}
	} else if( cls.state == CA_ACTIVE ) {
		SCR_RenderView();

		UI_Refresh();

		if( scr_timegraph->integer ) {
			SCR_DebugGraph( cls.frametime * 0.3f, 1, 1, 1 );
		}

		if( scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer ) {
			SCR_DrawDebugGraph();
		}
	}

	CL_ImGuiEndFrame();
}
