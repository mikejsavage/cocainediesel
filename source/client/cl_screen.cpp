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
#include "cgame/cg_local.h"
#include "qcommon/cmodel.h"

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
void CL_AddNetgraph() {
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

void SCR_DebugGraph( float value, float r, float g, float b ) {
	values[current].value = value;
	values[current].color = Vec4( r, g, b, 1.0f );

	current++;
	current &= 1023;
}

static void SCR_DrawFillRect( int x, int y, int w, int h, Vec4 color ) {
	Draw2DBox( x, y, w, h, cls.white_material, color );
}

static void SCR_DrawDebugGraph() {
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

void SCR_InitScreen() {
	scr_netgraph = Cvar_Get( "netgraph", "0", 0 );
	scr_timegraph = Cvar_Get( "timegraph", "0", 0 );
	scr_debuggraph = Cvar_Get( "debuggraph", "0", 0 );
	scr_graphheight = Cvar_Get( "graphheight", "32", 0 );
	scr_graphscale = Cvar_Get( "graphscale", "1", 0 );
	scr_graphshift = Cvar_Get( "graphshift", "0", 0 );
}

static void SCR_RenderView() {
	cl.map = FindMap( client_gs.gameState.map );
	if( cl.map != NULL ) {
		cl.cms = cl.map->cms;
		if( cl.cms->checksum != client_gs.gameState.map_checksum ) {
			// TODO: hotloading breaks this because server checksum doesn't get updated until the next server frame
			// Com_Error( ERR_DROP, "Local map version differs from server: %u != '%u'", cl.cms->checksum, client_gs.gameState.map_checksum );
		}

		CL_GameModule_RenderView();
	}
}

static void FlashStage( float begin, float t, float end, float from, float to, float * flash ) {
	if( t < begin || t >= end )
		return;

	float frac = Unlerp01( begin, t, end );

	*flash = Lerp( from, frac, to );
}

struct PostprocessUniforms {
	float time;
	float damage;
	float crt;
	float brightness;
	float contrast;
};

static UniformBlock UploadPostprocessUniforms( PostprocessUniforms uniforms ) {
	return UploadUniformBlock( uniforms.time, uniforms.damage, uniforms.crt, uniforms.brightness, uniforms.contrast );
}

static void SubmitPostprocessPass() {
	ZoneScoped;

	PipelineState pipeline;
	pipeline.pass = frame_static.postprocess_pass;
	pipeline.depth_func = DepthFunc_Disabled;
	pipeline.shader = &shaders.postprocess;

	const Framebuffer & fb = frame_static.postprocess_fb;
	pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
	pipeline.set_texture( "u_Screen", &fb.albedo_texture );
	pipeline.set_texture( "u_Noise", FindMaterial( "textures/noise" )->texture );
	float damage_effect = cg.view.type == VIEWDEF_PLAYERVIEW ? cg.damage_effect : 0.0f;

	float contrast = 1.0f;
	if( client_gs.gameState.bomb.exploding ) {
		constexpr float duration = 4000.0f;
		float t = ( cl.serverTime - client_gs.gameState.bomb.exploded_at ) / duration;

		FlashStage( 0.00f, t, 0.05f, 1.0f, -1.0f, &contrast );
		FlashStage( 0.05f, t, 0.10f, -1.0f, 1.0f, &contrast );
		FlashStage( 0.10f, t, 0.15f, 1.0f, -1.0f, &contrast );
		FlashStage( 0.15f, t, 0.50f, -1.0f, -1.0f, &contrast );
		FlashStage( 0.50f, t, 0.80f, -1.0f, -1.0f, &contrast );
		FlashStage( 0.80f, t, 1.00f, -1.0f, 1.0f, &contrast );
	}

	static float chasing_amount = 0.0f;
	constexpr float chasing_speed = 4.0f;
	bool chasing = cls.cgameActive && !cls.demo.playing && cg.predictedPlayerState.team != TEAM_SPECTATOR && cg.predictedPlayerState.POVnum != cgs.playerNum + 1;
	if( chasing ) {
		chasing_amount += cls.frametime * 0.001f * chasing_speed;
	} else {
		chasing_amount -= cls.frametime * 0.001f * chasing_speed;
	}
	chasing_amount = Clamp01( chasing_amount );

	PostprocessUniforms uniforms = { };
	uniforms.time = float( Sys_Milliseconds() ) * 0.001f;
	uniforms.damage = damage_effect;
	uniforms.crt = chasing_amount;
	uniforms.brightness = 0.0f;
	uniforms.contrast = contrast;

	pipeline.set_uniform( "u_PostProcess", UploadPostprocessUniforms( uniforms ) );

	DrawFullscreenMesh( pipeline );
}

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
	else {
		cg.damage_effect = 0.0f;
	}

	SubmitPostprocessPass();

	UI_Refresh();

	CL_ImGuiEndFrame();

	CL_Ultralight_Frame();
}
