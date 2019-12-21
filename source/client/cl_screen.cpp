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

static cvar_t *scr_netgraph;
static cvar_t *scr_timegraph;
static cvar_t *scr_debuggraph;
static cvar_t *scr_graphheight;
static cvar_t *scr_graphscale;
static cvar_t *scr_graphshift;

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
	Vec4 color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
* SCR_DebugGraph
*/
void SCR_DebugGraph( float value, float r, float g, float b ) {
	values[current].value = value;
	values[current].color = Vec4( r, g, b, 1.0f );

	current++;
	current &= 1023;
}

static void SCR_DrawFillRect( int x, int y, int w, int h, Vec4 color ) {
	Draw2DBox( x, y, w, h, cls.whiteTexture, color );
}

/*
* SCR_DrawDebugGraph
*/
static void SCR_DrawDebugGraph( void ) {
	//
	// draw the graph
	//
	int w = frame_static.viewport_width;
	int x = 0;
	int y = 0 + frame_static.viewport_height;
	SCR_DrawFillRect( x, y - scr_graphheight->integer, w, scr_graphheight->integer, vec4_black );

	int s = ( w + 1024 - 1 ) / 1024; //scale for resolutions with width >1024

	for( int a = 0; a < w; a++ ) {
		int i = ( current - 1 - a + 1024 ) & 1023;
		float v = values[i].value;
		v = v * scr_graphscale->integer + scr_graphshift->integer;

		if( v < 0 ) {
			v += scr_graphheight->integer * ( 1 + (int)( -v / scr_graphheight->integer ) );
		}
		int h = (int)v % scr_graphheight->integer;
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
}

//=============================================================================

/*
* SCR_RegisterConsoleMedia
*/
void SCR_RegisterConsoleMedia() {
	cls.whiteTexture = FindMaterial( "$whiteimage" );
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
	CL_ForceVsync( cls.state == CA_DISCONNECTED );

	CL_ImGuiBeginFrame();

	if( cls.state == CA_CONNECTED || cls.state == CA_ACTIVE ) {
		SCR_RenderView();

		if( scr_timegraph->integer ) {
			SCR_DebugGraph( cls.frametime * 0.3f, 1, 1, 1 );
		}

		if( scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer ) {
			SCR_DrawDebugGraph();
		}
	}

	UI_Refresh();

	CL_ImGuiEndFrame();
}
