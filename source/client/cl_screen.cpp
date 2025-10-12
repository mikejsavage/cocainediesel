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
#include "client/clay_init.h"
#include "client/renderer/renderer.h"
#include "client/renderer/shader_shared.h"
#include "cgame/cg_local.h"
#include "qcommon/time.h"

static Cvar *scr_netgraph;
static Cvar *scr_timegraph;
static Cvar *scr_debuggraph;
static Cvar *scr_graphheight;
static Cvar *scr_graphscale;
static Cvar *scr_graphshift;

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

typedef struct {
	float value;
	Vec4 color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

static void SCR_DebugGraph( float value, float r, float g, float b ) {
	values[current].value = value;
	values[current].color = Vec4( r, g, b, 1.0f );

	current = ( current + 1 ) % ARRAY_COUNT( values );
}

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
	ping = cls.realtime - cl.cmd_time[ cls.ucmdAcknowledged % ARRAY_COUNT( cl.cmd_time ) ];
	ping /= 30;
	if( ping > 30 ) {
		ping = 30;
	}
	SCR_DebugGraph( ping, 1.0f, 0.75f, 0.06f );
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
	SCR_DrawFillRect( x, y - scr_graphheight->integer, w, scr_graphheight->integer, black.vec4 );

	int s = ( w + 1024 - 1 ) / 1024; //scale for resolutions with width >1024

	for( int a = 0; a < w; a++ ) {
		int i = ( current - 1 - a + 1024 ) % ARRAY_COUNT( values );
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
	scr_netgraph = NewCvar( "netgraph", "0" );
	scr_timegraph = NewCvar( "timegraph", "0" );
	scr_debuggraph = NewCvar( "debuggraph", "0" );
	scr_graphheight = NewCvar( "graphheight", "32" );
	scr_graphscale = NewCvar( "graphscale", "1" );
	scr_graphshift = NewCvar( "graphshift", "0" );
}

static void SCR_RenderView() {
	if( cl.map != NULL ) {
		CL_GameModule_RenderView();
	}
}

static void FlashStage( float begin, float t, float end, float from, float to, float * flash ) {
	if( t < begin || t >= end )
		return;

	float frac = Unlerp01( begin, t, end );

	*flash = Lerp( from, frac, to );
}

static void SubmitPostprocessPass() {
	TracyZoneScoped;

	float damage_effect = cg.view.type == ViewType_Player ? cg.damage_effect : 0.0f;

	float contrast = 1.0f;
	if( client_gs.gameState.exploding ) {
		constexpr float duration = 4000.0f;
		float t = ( cl.serverTime - client_gs.gameState.exploded_at ) / duration;

		FlashStage( 0.00f, t, 0.05f, 1.0f, -1.0f, &contrast );
		FlashStage( 0.05f, t, 0.10f, -1.0f, 1.0f, &contrast );
		FlashStage( 0.10f, t, 0.15f, 1.0f, -1.0f, &contrast );
		FlashStage( 0.15f, t, 0.50f, -1.0f, -1.0f, &contrast );
		FlashStage( 0.50f, t, 0.80f, -1.0f, -1.0f, &contrast );
		FlashStage( 0.80f, t, 1.00f, -1.0f, 1.0f, &contrast );
	}

	static float chasing_amount = 0.0f;
	constexpr float chasing_speed = 4.0f;
	bool chasing = cls.cgameActive && !CL_DemoPlaying() && cg.predictedPlayerState.team != Team_None && cg.predictedPlayerState.POVnum != cgs.playerNum + 1;
	if( chasing ) {
		chasing_amount += cls.frametime * 0.001f * chasing_speed;
	} else {
		chasing_amount -= cls.frametime * 0.001f * chasing_speed;
	}
	chasing_amount = Clamp01( chasing_amount );

	PostprocessUniforms uniforms = {
		.time = ToSeconds( cls.shadertoy_time ),
		.damage = damage_effect,
		.crt = chasing_amount,
		.brightness = 0.0f,
		.contrast = contrast,
	};

	frame_static.render_passes[ RenderPass_Postprocessing ] = NewRenderPass( RenderPassConfig {
		.name = "Postprocessing",
		.color_targets = {
			RenderPassConfig::ColorTarget {
				.texture = NONE,
				.preserve_contents = false,
			},
		},
		.representative_shader = shaders.postprocess,
		.bindings = {
			.buffers = {
				{ "u_View", frame_static.ortho_view_uniforms },
				{ "u_PostProcess", NewTempBuffer( uniforms ) },
			},
			.textures = {
				{ "u_Screen", frame_static.render_targets.resolved_color },
				{ "u_Noise", RGBNoiseTexture() },
			},
			.samplers = { { "u_Sampler", Sampler_Standard } },
		},
	} );

	PipelineState pipeline = {
		.shader = shaders.postprocess,
		.dynamic_state = { .depth_func = DepthFunc_AlwaysNoWrite },
	};

	Draw( RenderPass_Postprocessing, pipeline, FullscreenMesh() );
}

void MaybeResetShadertoyTime( bool respawned ) {
	bool early_reset = respawned && cls.shadertoy_time > Hours( 1 );
	bool force_reset = cls.shadertoy_time > Hours( 1.5f );
	if( early_reset || force_reset ) {
		cls.shadertoy_time = { };
	}
}

void SCR_UpdateScreen() {
	CL_ForceVsync( cls.state == CA_DISCONNECTED );

	MaybeResetShadertoyTime( false );

	CL_ImGuiBeginFrame();

	if( cls.state == CA_ACTIVE ) {
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

	ClaySubmitFrame();
	CL_ImGuiEndFrame();
}
