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

#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"

Cvar *cg_centerTime;
Cvar *cg_showPointedPlayer;
Cvar *cg_draw2D;

Cvar *cg_crosshair_size;

static constexpr float playerNamesAlpha = 0.4f;
static constexpr float playerNamesZfar = 1024.0f;

static int64_t scr_damagetime = 0;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

static char scr_centerstring[1024];
static int scr_centertime_off;

/*
* CG_CenterPrint
*
* Called for important messages that should stay in the center of the screen
* for a few moments
*/
void CG_CenterPrint( const char *str ) {
	Q_strncpyz( scr_centerstring, str, sizeof( scr_centerstring ) );
	scr_centertime_off = cg_centerTime->number * 1000.0f;
}

static void CG_DrawCenterString() {
	DrawText( cgs.fontNormal, cgs.textSizeMedium, scr_centerstring, Alignment_CenterTop, frame_static.viewport_width * 0.5f, frame_static.viewport_height * 0.75f, vec4_white, true );
}

//============================================================================

void CG_ScreenInit() {
	cg_draw2D =     NewCvar( "cg_draw2D", "1", 0 );
	cg_centerTime =     NewCvar( "cg_centerTime", "2.5", 0 );

	cg_crosshair_size = NewCvar( "cg_crosshair_size", "3", CvarFlag_Archive );
}

void CG_DrawNet( int x, int y, int w, int h, Alignment alignment, Vec4 color ) {
	if( cgs.demoPlaying ) {
		return;
	}

	int64_t incomingAcknowledged, outgoingSequence;
	CL_GetCurrentState( &incomingAcknowledged, &outgoingSequence, NULL );
	if( outgoingSequence - incomingAcknowledged < CMD_BACKUP - 1 ) {
		return;
	}
	x = CG_HorizontalAlignForWidth( x, alignment, w );
	y = CG_VerticalAlignForHeight( y, alignment, h );
	Draw2DBox( x, y, w, h, FindMaterial( "gfx/hud/net" ), color );
}

void CG_ScreenCrosshairDamageUpdate() {
	scr_damagetime = cls.monotonicTime;
}

static void CG_FillRect( int x, int y, int w, int h, Vec4 color ) {
	Draw2DBox( x, y, w, h, cls.white_material, color );
}

void CG_DrawCrosshair() {
	if( cg.predictedPlayerState.health <= 0 )
		return;

	WeaponType weapon = cg.predictedPlayerState.weapon;
	if( weapon == Weapon_Knife || weapon == Weapon_Sniper )
		return;
	if( weapon == Weapon_AutoSniper && cg.predictedPlayerState.zoom_time > 0 )
		return;

	Vec4 color = cls.monotonicTime - scr_damagetime <= 300 ? vec4_red : vec4_white;

	int w = frame_static.viewport_width;
	int h = frame_static.viewport_height;
	int size = cg_crosshair_size->integer > 0 ? cg_crosshair_size->integer : 0;

	CG_FillRect( w / 2 - 2, h / 2 - 2 - size, 4, 4 + 2 * size, vec4_black );
	CG_FillRect( w / 2 - 2 - size, h / 2 - 2, 4 + 2 * size, 4, vec4_black );
	CG_FillRect( w / 2 - 1, h / 2 - 1 - size, 2, 2 + 2 * size, color );
	CG_FillRect( w / 2 - 1 - size, h / 2 - 1, 2 + 2 * size, 2, color );
}

void CG_DrawClock( int x, int y, Alignment alignment, const Font * font, float font_size, Vec4 color, bool border ) {
	int64_t clocktime, startTime, duration, curtime;
	char string[12];

	if( client_gs.gameState.match_state > MatchState_Playing ) {
		return;
	}

	if( client_gs.gameState.clock_override != 0 ) {
		clocktime = client_gs.gameState.clock_override;
		if( clocktime < 0 )
			return;
	}
	else {
		curtime = ( GS_MatchWaiting( &client_gs ) || GS_MatchPaused( &client_gs ) ) ? cg.frame.serverTime : cl.serverTime;
		duration = client_gs.gameState.match_duration;
		startTime = client_gs.gameState.match_state_start_time;

		// count downwards when having a duration
		if( duration ) {
			if( duration + startTime < curtime ) {
				duration = curtime - startTime; // avoid negative results
			}
			clocktime = startTime + duration - curtime;
		}
		else {
			if( curtime >= startTime ) { // avoid negative results
				clocktime = curtime - startTime;
			}
			else {
				clocktime = 0;
			}
		}
	}

	double seconds = (double)clocktime * 0.001;
	int minutes = (int)( seconds / 60 );
	seconds -= minutes * 60;

	snprintf( string, sizeof( string ), "%i:%02i", minutes, (int)seconds );

	DrawText( font, font_size, string, alignment, x, y, color, border );
}

void CG_DrawPlayerNames( const Font * font, float font_size, Vec4 color, bool border ) {
	// static vec4_t alphagreen = { 0, 1, 0, 0 }, alphared = { 1, 0, 0, 0 }, alphayellow = { 1, 1, 0, 0 }, alphamagenta = { 1, 0, 1, 1 }, alphagrey = { 0.85, 0.85, 0.85, 1 };
	for( int i = 0; i < client_gs.maxclients; i++ ) {
		if( strlen( PlayerName( i ) ) == 0 || ISVIEWERENTITY( i + 1 ) ) {
			continue;
		}

		const centity_t * cent = &cg_entities[i + 1];
		if( cent->serverFrame != cg.frame.serverFrame ) {
			continue;
		}

		//only show the players in your team
		if( cent->current.team != cg.predictedPlayerState.team ) {
			continue;
		}

		if( cent->current.type != ET_PLAYER ) {
			continue;
		}

		// Kill if behind the view
		Vec3 dir = cent->interpolated.origin - cg.view.origin;
		float dist = Length( dir ) * cg.view.fracDistFOV;
		dir = SafeNormalize( dir );

		if( Dot( dir, FromQFAxis( cg.view.axis, AXIS_FORWARD ) ) < 0 ) {
			continue;
		}

		Vec4 tmpcolor = color;

		float fadeFrac;
		if( cent->current.number != cg.pointedNum ) {
			if( dist > playerNamesZfar ) {
				continue;
			}

			fadeFrac = Clamp01( ( playerNamesZfar - dist ) / ( playerNamesZfar * 0.25f ) );

			tmpcolor.w = playerNamesAlpha * color.w * fadeFrac;
		} else {
			fadeFrac = Clamp01( ( cg.pointRemoveTime - cl.serverTime ) / 150.0f );

			tmpcolor.w = color.w * fadeFrac;
		}

		if( tmpcolor.w <= 0.0f ) {
			continue;
		}

		trace_t trace;
		CG_Trace( &trace, cg.view.origin, Vec3( 0.0f ), Vec3( 0.0f ), cent->interpolated.origin, cg.predictedPlayerState.POVnum, MASK_OPAQUE );
		if( trace.fraction < 1.0f && trace.ent != cent->current.number ) {
			continue;
		}

		Vec3 drawOrigin = cent->interpolated.origin + Vec3( 0.0f, 0.0f, playerbox_stand_maxs.z + 8 );

		Vec2 coords = WorldToScreen( drawOrigin );
		if( ( coords.x < 0 || coords.x > frame_static.viewport_width ) || ( coords.y < 0 || coords.y > frame_static.viewport_height ) ) {
			continue;
		}

		DrawText( font, font_size, PlayerName( i ), Alignment_CenterBottom, coords.x, coords.y, tmpcolor, border );
	}
}

//=============================================================================

static const char * mini_obituaries[] = {
#include "mini_obituaries.h"
};

constexpr int MINI_OBITUARY_DAMAGE = 255;

struct DamageNumber {
	Vec3 origin;
	float drift;
	int64_t t;
	const char * obituary;
	int damage;
	bool headshot;
};

static DamageNumber damage_numbers[ 16 ];
size_t damage_numbers_head;

void CG_InitDamageNumbers() {
	damage_numbers_head = 0;
	for( DamageNumber & dn : damage_numbers ) {
		dn.damage = 0;
	}
}

void CG_AddDamageNumber( SyncEntityState * ent, u64 parm ) {
	DamageNumber * dn = &damage_numbers[ damage_numbers_head ];

	dn->t = cl.serverTime;
	dn->damage = parm >> 1;
	dn->headshot = ( parm & 1 ) != 0;
	dn->drift = RandomFloat11( &cls.rng ) > 0.0f ? 1.0f : -1.0f;
	dn->obituary = RandomElement( &cls.rng, mini_obituaries );

	float distance_jitter = 4;
	dn->origin = ent->origin;
	dn->origin.x += RandomFloat11( &cls.rng ) * distance_jitter;
	dn->origin.y += RandomFloat11( &cls.rng ) * distance_jitter;
	dn->origin.z += 48;

	damage_numbers_head = ( damage_numbers_head + 1 ) % ARRAY_COUNT( damage_numbers );
}

void CG_DrawDamageNumbers() {
	for( const DamageNumber & dn : damage_numbers ) {
		if( dn.damage == 0 )
			continue;

		bool obituary = dn.damage == MINI_OBITUARY_DAMAGE;

		float lifetime = obituary ? 1150.0f : Lerp( 750.0f, Unlerp01( 0, dn.damage, 50 ), 1000.0f );
		float frac = ( cl.serverTime - dn.t ) / lifetime;
		if( frac > 1 )
			continue;

		Vec3 origin = dn.origin;

		if( obituary ) {
			origin.z += 256.0f * frac - 512.0f * frac * frac;
		}
		else {
			origin.z += frac * 32.0f;
		}

		if( Dot( -frame_static.V.row2().xyz(), origin - frame_static.position ) <= 0 )
			continue;

		Vec2 coords = WorldToScreen( origin );
		coords.x += dn.drift * frac * ( obituary ? 512.0f : 8.0f );
		if( ( coords.x < 0 || coords.x > frame_static.viewport_width ) || ( coords.y < 0 || coords.y > frame_static.viewport_height ) ) {
			continue;
		}

		char buf[ 16 ];
		Vec4 color;
		float font_size;
		if( obituary ) {
			Q_strncpyz( buf, dn.obituary, sizeof( buf ) );
			color = AttentionGettingColor();
			font_size = Lerp( cgs.textSizeSmall, frac * frac, 0.0f );
		}
		else {
			snprintf( buf, sizeof( buf ), "%d", dn.damage );
			color = dn.headshot ? sRGBToLinear( rgba8_diesel_yellow ) : vec4_white;
			font_size = Lerp( cgs.textSizeTiny, Unlerp01( 0, dn.damage, 50 ), cgs.textSizeSmall );
		}

		float alpha = 1 - Max2( 0.0f, frac - 0.75f ) / 0.25f;
		color.w *= alpha;

		DrawText( cgs.fontNormal, font_size, buf, Alignment_CenterBottom, coords.x, coords.y, color, true );
	}
}

//=============================================================================

struct BombSite {
	Vec3 origin;
	char letter;
};

enum BombState {
	BombState_Carried,
	BombState_Dropped,
	BombState_Planting,
	BombState_Planted,
};

struct Bomb {
	BombState state;
	Vec3 origin;
};

static BombSite bomb_sites[ 26 ];
static size_t num_bomb_sites;
static Bomb bomb;

void CG_AddBomb( centity_t * cent ) {
	if( cent->current.svflags & SVF_ONLYTEAM ) {
		bomb.state = cent->current.radius == BombDown_Dropped ? BombState_Dropped : BombState_Planting;
	}
	else {
		bomb.state = BombState_Planted;
	}

	bomb.origin = cent->interpolated.origin;

	// TODO: this really does not belong here...
	if( cent->interpolated.animating ) {
		const Model * model = FindModel( "models/bomb/bomb" );
		if( model == NULL )
			return;

		u8 tip_node;
		if( !FindNodeByName( model, Hash32( "a" ), &tip_node ) )
			return;

		TempAllocator temp = cls.frame_arena.temp();

		Span< TRS > pose = SampleAnimation( &temp, model, cent->interpolated.animation_time );
		MatrixPalettes palettes = ComputeMatrixPalettes( &temp, model, pose );

		Vec3 bomb_origin = cent->interpolated.origin - Vec3( 0.0f, 0.0f, 32.0f ); // BOMB_HUD_OFFSET

		Mat4 transform = FromAxisAndOrigin( cent->interpolated.axis, bomb_origin );
		Vec3 tip = ( transform * model->transform * palettes.node_transforms[ tip_node ] * Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xyz();

		DoVisualEffect( "models/bomb/fuse", tip );
	}
}

void CG_AddBombSite( centity_t * cent ) {
	assert( num_bomb_sites < ARRAY_COUNT( bomb_sites ) );

	BombSite * site = &bomb_sites[ num_bomb_sites ];
	site->origin = cent->current.origin;
	site->letter = cent->current.counterNum;

	num_bomb_sites++;
}

void CG_DrawBombHUD() {
	if( client_gs.gameState.match_state > MatchState_Playing )
		return;

	int my_team = cg.predictedPlayerState.team;
	bool show_labels = my_team != TEAM_SPECTATOR && client_gs.gameState.match_state == MatchState_Playing;

	Vec4 yellow = sRGBToLinear( rgba8_diesel_yellow );

	// TODO: draw arrows when clamped

	if( bomb.state == BombState_Carried || bomb.state == BombState_Dropped ) {
		for( size_t i = 0; i < num_bomb_sites; i++ ) {
			const BombSite * site = &bomb_sites[ i ];
			bool clamped;
			Vec2 coords = WorldToScreenClamped( site->origin, Vec2( cgs.fontSystemMediumSize * 2 ), &clamped );

			char buf[ 4 ];
			snprintf( buf, sizeof( buf ), "%c", site->letter );
			DrawText( cgs.fontNormal, cgs.textSizeMedium, buf, Alignment_CenterMiddle, coords.x, coords.y, yellow, true );

			if( show_labels && !clamped && bomb.state != BombState_Dropped ) {
				const char * msg = my_team == client_gs.gameState.bomb.attacking_team ? "ATTACK" : "DEFEND";
				coords.y += ( cgs.fontSystemMediumSize * 7 ) / 8;
				DrawText( cgs.fontNormal, cgs.textSizeTiny, msg, Alignment_CenterMiddle, coords.x, coords.y, yellow, true );
			}
		}
	}

	if( bomb.state != BombState_Carried ) {
		bool clamped;
		Vec2 coords = WorldToScreenClamped( bomb.origin, Vec2( cgs.fontSystemMediumSize * 2 ), &clamped );

		if( clamped ) {
			int icon_size = ( cgs.fontSystemMediumSize * frame_static.viewport_height ) / 600;
			Draw2DBox( coords.x - icon_size / 2, coords.y - icon_size / 2, icon_size, icon_size, FindMaterial( "gfx/bomb" ) );
		}
		else {
			if( show_labels ) {
				Vec4 color = vec4_white;
				const char * msg = "";

				if( bomb.state == BombState_Dropped ) {
					msg = "RETRIEVE";
					color = AttentionGettingColor();

					// TODO: lol
					DoVisualEffect( "models/bomb/pickup_sparkle", bomb.origin - Vec3( 0.0f, 0.0f, 32.0f ), Vec3( 0.0f, 0.0f, 1.0f ), 1.0f, AttentionGettingColor() );
				}
				else if( bomb.state == BombState_Planting ) {
					msg = "PLANTING";
					color = AttentionGettingColor();
				}
				else if( bomb.state == BombState_Planted ) {
					if( my_team == client_gs.gameState.bomb.attacking_team ) {
						msg = "PROTECT";
					}
					else {
						msg = "DEFUSE";
						color = AttentionGettingColor();
					}
				}

				float y = coords.y - cgs.fontSystemTinySize / 2;
				DrawText( cgs.fontNormal, cgs.textSizeSmall, msg, Alignment_CenterMiddle, coords.x, y, color, true );
			}
		}

	}
}

void CG_ResetBombHUD() {
	num_bomb_sites = 0;
	bomb.state = BombState_Carried;
}

//=============================================================================

void CG_EscapeKey() {
	if( cgs.demoPlaying ) {
		UI_ShowDemoMenu();
		return;
	}

	UI_ShowGameMenu();
}

static Vec4 CG_CalcColorBlend() {
	const SyncPlayerState * ps = &cg.predictedPlayerState;
	float flashed = Unlerp01( u16( 0 ), ps->flashed, u16( U16_MAX * 0.75f ) );
	return Vec4( Vec3( 1.0f ), flashed * flashed );
}

static void CG_SCRDrawViewBlend() {
	Vec4 color = CG_CalcColorBlend();

	if( color.w < 0.01f ) {
		return;
	}

	Draw2DBox( 0, 0, frame_static.viewport_width, frame_static.viewport_height, cls.white_material, color );
}

void AddDamageEffect( float x ) {
	constexpr float max = 1.0f;
	if( x == 0.0f )
		x = max;

	cg.damage_effect = Min2( max, cg.damage_effect + x );
}

void CG_Draw2DView() {
	ZoneScoped;

	if( !cg.view.draw2D ) {
		return;
	}

	CG_SCRDrawViewBlend();

	scr_centertime_off -= cls.frametime;
	if( CG_ScoreboardShown() ) {
		CG_DrawScoreboard();
	}
	else if( scr_centertime_off > 0 ) {
		CG_DrawCenterString();
	}

	CG_DrawHUD();
	CG_DrawChat();
}

void CG_Draw2D() {
	CG_DrawScope();

	if( !cg_draw2D->integer ) {
		return;
	}

	CG_Draw2DView();
	CG_DrawDemocam2D();
}
