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
#include "qcommon/time.h"

Cvar * cg_showPointedPlayer;
Cvar * cg_draw2D;

Cvar * cg_crosshair_size;
Cvar * cg_crosshair_gap;
Cvar * cg_crosshair_dynamic;

static constexpr float playerNamesAlpha = 0.4f;
static constexpr float playerNamesZfar = 1024.0f;
static constexpr float playerNamesZclose = 112.0f;
static constexpr float playerNamesZgrow = 2.5f;

static Time scr_damagetime = { };
static Time dynamic_crosshair_recover_time = { };

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

static constexpr int centerTimeOff = 2500;
static char scr_centerstring[1024];
static int scr_centertime_off;

/*
* CG_CenterPrint
*
* Called for important messages that should stay in the center of the screen
* for a few moments
*/
void CG_CenterPrint( Span< const char > str ) {
	ggformat( scr_centerstring, sizeof( scr_centerstring ), "{}", str );
	scr_centertime_off = centerTimeOff;
}

static void CG_DrawCenterString() {
	DrawText( cgs.fontNormal, cgs.textSizeMedium, scr_centerstring, Alignment_CenterTop, frame_static.viewport_width * 0.5f, frame_static.viewport_height * 0.75f, white.vec4, true );
}

//============================================================================

void CG_ScreenInit() {
	cg_draw2D = NewCvar( "cg_draw2D", "1", CvarFlag_Cheat );
	cg_crosshair_size = NewCvar( "cg_crosshair_size", "3", CvarFlag_Archive );
	cg_crosshair_gap = NewCvar( "cg_crosshair_gap", "0", CvarFlag_Archive );
	cg_crosshair_dynamic = NewCvar( "cg_crosshair_dynamic", "1", CvarFlag_Archive );
}

void CG_ScreenCrosshairDamageUpdate() {
	scr_damagetime = cls.monotonicTime;
}

static bool CG_IsShownCrosshair() {
	WeaponType weapon = cg.predictedPlayerState.weapon;
	return cg.predictedPlayerState.health > 0 &&
			!( weapon == Weapon_Knife || weapon == Weapon_Sniper || weapon == Weapon_Bat ) &&
			!( weapon == Weapon_Scout && cg.predictedPlayerState.zoom_time > 0 );
}

void CG_ScreenCrosshairShootUpdate( u16 refire_time ) {
	if( !CG_IsShownCrosshair() )
		return;
	constexpr float min_gap = 5.0f;
	float gap = min_gap + ToSeconds( Milliseconds( refire_time ) ) * 10.0f;
	dynamic_crosshair_recover_time = cls.monotonicTime + Seconds( logf( gap ) * 0.1f );
}

static void CG_FillRect( int x, int y, int w, int h, Vec4 color ) {
	Draw2DBox( x, y, w, h, cls.white_material, color );
}

void CG_DrawCrosshair( int x, int y ) {
	static constexpr int maxCrosshairSize = 50;
	static constexpr int maxCrosshairGapSize = 50;

	if( !CG_IsShownCrosshair() )
		return;

	static constexpr Time crosshairDamageTime = Milliseconds( 200 );
	Vec4 color = cls.monotonicTime - scr_damagetime <= crosshairDamageTime ? red.vec4 : white.vec4;

	int size = Clamp( 1, cg_crosshair_size->integer, maxCrosshairSize );
	int gap = Clamp( 0, cg_crosshair_gap->integer, maxCrosshairGapSize );
	if( cg_crosshair_dynamic->integer && cls.monotonicTime < dynamic_crosshair_recover_time ) {
		float dt = ToSeconds( dynamic_crosshair_recover_time - cls.monotonicTime );
		float diff = expf( dt * 10.0f );
		gap += diff;
		size += diff * 0.5f;
	}

	CG_FillRect( x - 2, y - 2 - size - gap, 4, 4 + size, black.vec4 );
	CG_FillRect( x - 2, y - 2 + gap, 4, 4 + size, black.vec4 );

	CG_FillRect( x - 2 - size - gap, y - 2, 4 + size, 4, black.vec4 );
	CG_FillRect( x - 2 + gap, y - 2, 4 + size, 4, black.vec4 );

	CG_FillRect( x - 1, y - 1 - gap - size, 2, 2 + size, color );
	CG_FillRect( x - 1, y - 1 + gap, 2, 2 + size, color );

	CG_FillRect( x - 1 - size - gap, y - 1, 2 + size, 2, color );
	CG_FillRect( x - 1 + gap, y - 1, 2 + size, 2, color );
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

			fadeFrac = Clamp01( Min2( ( playerNamesZfar - dist ) / ( playerNamesZfar * 0.25f ), ( dist - playerNamesZclose ) / playerNamesZclose ) );

			tmpcolor.w = playerNamesAlpha * color.w * fadeFrac;
		} else {
			fadeFrac = Clamp01( ( cg.pointRemoveTime - cl.serverTime ) / 150.0f );

			tmpcolor.w = color.w * fadeFrac;
		}

		if( tmpcolor.w <= 0.0f ) {
			continue;
		}

		Vec3 headpos = Vec3( 0.0f, 0.0f, 34.0f * cent->interpolated.scale.z );
		trace_t trace = CG_Trace( cg.view.origin, MinMax3( 0.0f ), cent->interpolated.origin + headpos, cg.predictedPlayerState.POVnum, SolidMask_Opaque );
		if( trace.HitSomething() && trace.ent != cent->current.number ) {
			continue;
		}

		Vec3 drawOrigin = cent->interpolated.origin + Vec3( 0.0f, 0.0f, playerbox_stand.maxs.z + 8 );

		Vec2 coords = WorldToScreen( drawOrigin );
		if( ( coords.x < 0 || coords.x > frame_static.viewport_width ) || ( coords.y < 0 || coords.y > frame_static.viewport_height ) ) {
			continue;
		}

		float size = font_size * playerNamesZgrow * ( 1.0f - ( dist / ( playerNamesZfar * 1.8f ) ) );
		DrawText( font, size, PlayerName( i ), Alignment_CenterBottom, coords.x, coords.y, tmpcolor, border );
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

void CG_DrawDamageNumbers( float obi_size, float dmg_size ) {
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
			SafeStrCpy( buf, dn.obituary, sizeof( buf ) );
			color = AttentionGettingColor();
			font_size = Lerp( obi_size, frac * frac, 0.0f );
		}
		else {
			snprintf( buf, sizeof( buf ), "%d", dn.damage );
			color = dn.headshot ? sRGBToLinear( diesel_yellow.rgba8 ) : white.vec4;
			font_size = Lerp( dmg_size, Unlerp01( 0, dn.damage, 50 ), cgs.textSizeSmall );
		}

		float alpha = 1 - Max2( 0.0f, frac - 0.75f ) / 0.25f;
		color.w *= alpha;

		DrawText( cgs.fontNormal, font_size, buf, Alignment_CenterBottom, coords.x, coords.y, color, true );
	}
}

//=============================================================================

struct BombSiteIndicator {
	Vec3 origin;
	char letter;
};

enum BombState {
	BombState_Carried,
	BombState_Dropped,
	BombState_Planting,
	BombState_Planted,
};

struct BombIndicator {
	BombState state;
	Vec3 origin;
};

static BombSiteIndicator bomb_sites[ 26 ];
static size_t num_bomb_sites;
static BombIndicator bomb;

void CG_AddBombIndicator( const centity_t * cent ) {
	if( cent->current.svflags & SVF_ONLYTEAM ) {
		bomb.state = cent->current.radius == BombDown_Dropped ? BombState_Dropped : BombState_Planting;
	}
	else {
		bomb.state = BombState_Planted;
	}

	bomb.origin = cent->interpolated.origin;

	// TODO: this really does not belong here...
	if( cent->interpolated.animating ) {
		const GLTFRenderData * model = FindGLTFRenderData( "models/bomb/bomb" );
		if( model == NULL )
			return;

		u8 tip_node;
		if( !FindNodeByName( model, "a", &tip_node ) )
			return;

		TempAllocator temp = cls.frame_arena.temp();

		Span< Transform > pose = SampleAnimation( &temp, model, cent->interpolated.animation_time );
		MatrixPalettes palettes = ComputeMatrixPalettes( &temp, model, pose );

		Vec3 bomb_origin = cent->interpolated.origin - Vec3( 0.0f, 0.0f, 32.0f ); // BOMB_HUD_OFFSET

		Mat3x4 transform = FromAxisAndOrigin( cent->interpolated.axis, bomb_origin );
		Vec3 tip = ( transform * model->transform * palettes.node_transforms[ tip_node ] * Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xyz();

		DoVisualEffect( "models/bomb/fuse", tip );
	}
}

void CG_AddBombSiteIndicator( const centity_t * cent ) {
	Assert( num_bomb_sites < ARRAY_COUNT( bomb_sites ) );

	BombSiteIndicator * site = &bomb_sites[ num_bomb_sites ];
	site->origin = cent->current.origin;
	site->letter = cent->current.site_letter;

	num_bomb_sites++;
}

void CG_DrawBombHUD( int name_size, int goal_size, int bomb_msg_size ) {
	if( client_gs.gameState.match_state > MatchState_Playing )
		return;

	Team my_team = cg.predictedPlayerState.team;
	bool show_labels = my_team != Team_None && client_gs.gameState.match_state == MatchState_Playing;

	Vec4 yellow = sRGBToLinear( diesel_yellow.rgba8 );

	// TODO: draw arrows when clamped

	if( bomb.state == BombState_Carried || bomb.state == BombState_Dropped ) {
		for( size_t i = 0; i < num_bomb_sites; i++ ) {
			const BombSiteIndicator * site = &bomb_sites[ i ];
			bool clamped;
			Vec2 coords = WorldToScreenClamped( site->origin, Vec2( cgs.fontSystemMediumSize * 2 ), &clamped );

			char buf[ 4 ];
			snprintf( buf, sizeof( buf ), "%c", site->letter );
			DrawText( cgs.fontNormal, name_size, buf, Alignment_CenterMiddle, coords.x, coords.y, yellow, true );

			if( show_labels && !clamped && bomb.state != BombState_Dropped ) {
				const char * msg = my_team == client_gs.gameState.bomb.attacking_team ? "ATTACK" : "DEFEND";
				coords.y += name_size * 0.6f;
				DrawText( cgs.fontNormal, goal_size, msg, Alignment_CenterMiddle, coords.x, coords.y, yellow, true );
			}
		}
	}

	if( bomb.state != BombState_Carried ) {
		bool clamped;
		Vec2 coords = WorldToScreenClamped( bomb.origin, Vec2( cgs.fontSystemMediumSize * 2 ), &clamped );

		if( clamped ) {
			int icon_size = ( cgs.fontSystemMediumSize * frame_static.viewport_height ) / 600;
			Draw2DBox( coords.x - icon_size / 2, coords.y - icon_size / 2, icon_size, icon_size, FindMaterial( "hud/icons/bomb" ) );
		}
		else {
			if( show_labels ) {
				Vec4 color = white.vec4;
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

				float y = coords.y - name_size / 3;
				DrawText( cgs.fontNormal, goal_size, msg, Alignment_CenterMiddle, coords.x, y, color, true );
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
	TracyZoneScoped;

	if( !cg.view.draw2D ) {
		return;
	}

	CG_SCRDrawViewBlend();

	scr_centertime_off -= cls.frametime;
	if( scr_centertime_off > 0 ) {
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
}
