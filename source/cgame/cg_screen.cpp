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

cvar_t *cg_centerTime;
cvar_t *cg_showFPS;
cvar_t *cg_showPointedPlayer;
cvar_t *cg_draw2D;

cvar_t *cg_crosshair_size;

cvar_t *cg_showSpeed;
cvar_t *cg_showAwards;

cvar_t *cg_showPlayerNames;
cvar_t *cg_showPlayerNames_alpha;
cvar_t *cg_showPlayerNames_zfar;
cvar_t *cg_showPlayerNames_barWidth;

cvar_t *cg_showPressedKeys;

cvar_t *cg_showViewBlends;

static int64_t scr_damagetime = 0;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char scr_centerstring[1024];
int scr_centertime_off;
int scr_erase_center;

/*
* CG_CenterPrint
*
* Called for important messages that should stay in the center of the screen
* for a few moments
*/
void CG_CenterPrint( const char *str ) {
	Q_strncpyz( scr_centerstring, str, sizeof( scr_centerstring ) );
	scr_centertime_off = cg_centerTime->value * 1000.0f;
}

static void CG_DrawCenterString( void ) {
	DrawText( cgs.fontMontserrat, cgs.textSizeMedium, scr_centerstring, Alignment_CenterTop, frame_static.viewport_width * 0.5f, frame_static.viewport_height * 0.75f, vec4_white, true );
}

//============================================================================

/*
* CG_ScreenInit
*/
void CG_ScreenInit( void ) {
	cg_showFPS =        Cvar_Get( "cg_showFPS", "0", CVAR_ARCHIVE );
	cg_draw2D =     Cvar_Get( "cg_draw2D", "1", 0 );
	cg_centerTime =     Cvar_Get( "cg_centerTime", "2.5", 0 );

	cg_crosshair_size = Cvar_Get( "cg_crosshair_size", "3", CVAR_ARCHIVE );

	cg_showSpeed =      Cvar_Get( "cg_showSpeed", "0", CVAR_ARCHIVE );
	cg_showPointedPlayer =  Cvar_Get( "cg_showPointedPlayer", "1", CVAR_ARCHIVE );
	cg_showViewBlends = Cvar_Get( "cg_showViewBlends", "1", CVAR_ARCHIVE );
	cg_showAwards =     Cvar_Get( "cg_showAwards", "1", CVAR_ARCHIVE );

	cg_showPlayerNames =        Cvar_Get( "cg_showPlayerNames", "2", CVAR_ARCHIVE );
	cg_showPlayerNames_alpha =  Cvar_Get( "cg_showPlayerNames_alpha", "0.4", CVAR_ARCHIVE );
	cg_showPlayerNames_zfar =   Cvar_Get( "cg_showPlayerNames_zfar", "1024", CVAR_ARCHIVE );
	cg_showPlayerNames_barWidth =   Cvar_Get( "cg_showPlayerNames_barWidth", "8", CVAR_ARCHIVE );

	cg_showPressedKeys = Cvar_Get( "cg_showPressedKeys", "0", CVAR_ARCHIVE );
}

/*
* CG_DrawNet
*/
void CG_DrawNet( int x, int y, int w, int h, Alignment alignment, Vec4 color ) {
	if( cgs.demoPlaying ) {
		return;
	}

	int64_t incomingAcknowledged, outgoingSequence;
	trap_NET_GetCurrentState( &incomingAcknowledged, &outgoingSequence, NULL );
	if( outgoingSequence - incomingAcknowledged < CMD_BACKUP - 1 ) {
		return;
	}
	x = CG_HorizontalAlignForWidth( x, alignment, w );
	y = CG_VerticalAlignForHeight( y, alignment, h );
	Draw2DBox( x, y, w, h, cgs.media.shaderNet, color );
}

void CG_ScreenCrosshairDamageUpdate( void ) {
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

	Vec4 color = cls.monotonicTime - scr_damagetime <= 300 ? vec4_red : vec4_white;

	int w = frame_static.viewport_width;
	int h = frame_static.viewport_height;
	int size = cg_crosshair_size->integer > 0 ? cg_crosshair_size->integer : 0;

	CG_FillRect( w / 2 - 2, h / 2 - 2 - size, 4, 4 + 2 * size, vec4_black );
	CG_FillRect( w / 2 - 2 - size, h / 2 - 2, 4 + 2 * size, 4, vec4_black );
	CG_FillRect( w / 2 - 1, h / 2 - 1 - size, 2, 2 + 2 * size, color );
	CG_FillRect( w / 2 - 1 - size, h / 2 - 1, 2 + 2 * size, 2, color );
}

void CG_DrawKeyState( int x, int y, int w, int h, const char *key ) {
	int i;
	bool pressed = false;

	if( !cg_showPressedKeys->integer ) {
		return;
	}

	if( !key ) {
		return;
	}

	for( i = 0; i < KEYICON_TOTAL; i++ )
		if( !Q_stricmp( key, gs_keyicon_names[i] ) ) {
			break;
		}

	if( i == KEYICON_TOTAL ) {
		return;
	}

	if( cg.predictedPlayerState.plrkeys & ( 1 << i ) ) {
		pressed = 1;
	}

	Vec4 color = vec4_white;
	if( !pressed ) {
		color.w = 0.5f;
	}

	Draw2DBox( x, y, w, h, cgs.media.shaderKeyIcon[i], color );
}

/*
* CG_DrawClock
*/
void CG_DrawClock( int x, int y, Alignment alignment, const Font * font, float font_size, Vec4 color, bool border ) {
	int64_t clocktime, startTime, duration, curtime;
	char string[12];

	if( GS_MatchState( &client_gs ) > MATCH_STATE_PLAYTIME ) {
		return;
	}

	if( GS_MatchClockOverride( &client_gs ) ) {
		clocktime = GS_MatchClockOverride( &client_gs );
		if( clocktime < 0 )
			return;
	}
	else {
		curtime = ( GS_MatchWaiting( &client_gs ) || GS_MatchPaused( &client_gs ) ) ? cg.frame.serverTime : cl.serverTime;
		duration = GS_MatchDuration( &client_gs );
		startTime = GS_MatchStartTime( &client_gs );

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

/*
* CG_ClearPointedNum
*/
void CG_ClearPointedNum( void ) {
	cg.pointedNum = 0;
	cg.pointRemoveTime = 0;
	cg.pointedHealth = 0;
}

/*
* CG_UpdatePointedNum
*/
static void CG_UpdatePointedNum( void ) {
	// disable cases
	if( cg.view.thirdperson || cg.view.type != VIEWDEF_PLAYERVIEW || !cg_showPointedPlayer->integer ) {
		CG_ClearPointedNum();
		return;
	}

	if( cg.predictedPlayerState.pointed_player ) {
		cg.pointedNum = cg.predictedPlayerState.pointed_player;
		cg.pointRemoveTime = cl.serverTime + 150;
		cg.pointedHealth = cg.predictedPlayerState.pointed_health;
	}

	if( cg.pointRemoveTime <= cl.serverTime ) {
		CG_ClearPointedNum();
	}

	if( cg.pointedNum ) {
		if( cg_entities[cg.pointedNum].current.team != cg.predictedPlayerState.team ) {
			CG_ClearPointedNum();
		}
	}
}

/*
* CG_DrawPlayerNames
*/
void CG_DrawPlayerNames( const Font * font, float font_size, Vec4 color, bool border ) {
	// static vec4_t alphagreen = { 0, 1, 0, 0 }, alphared = { 1, 0, 0, 0 }, alphayellow = { 1, 1, 0, 0 }, alphamagenta = { 1, 0, 1, 1 }, alphagrey = { 0.85, 0.85, 0.85, 1 };
	Vec3 dir, drawOrigin;
	float dist, fadeFrac;
	trace_t trace;

	if( !cg_showPlayerNames->integer && !cg_showPointedPlayer->integer ) {
		return;
	}

	CG_UpdatePointedNum();

	for( int i = 0; i < client_gs.maxclients; i++ ) {
		if( !cgs.clientInfo[i].name[0] || ISVIEWERENTITY( i + 1 ) ) {
			continue;
		}

		const centity_t * cent = &cg_entities[i + 1];
		if( cent->serverFrame != cg.frame.serverFrame ) {
			continue;
		}

		// only show the pointed player
		if( !cg_showPlayerNames->integer && ( cent->current.number != cg.pointedNum ) ) {
			continue;
		}

		if( cg_showPlayerNames->integer == 2 && cent->current.team != cg.predictedPlayerState.team ) {
			continue;
		}

		if( !cent->current.solid || cent->current.solid == SOLID_BMODEL || cent->current.team == TEAM_SPECTATOR ) {
			continue;
		}

		// Kill if behind the view
		dir = cent->ent.origin - cg.view.origin;
		dist = Length( dir ) * cg.view.fracDistFOV;
		dir = Normalize( dir );

		if( Dot( dir, FromQFAxis( cg.view.axis, AXIS_FORWARD ) ) < 0 ) {
			continue;
		}

		Vec4 tmpcolor = color;

		if( cent->current.number != cg.pointedNum ) {
			if( dist > cg_showPlayerNames_zfar->value ) {
				continue;
			}

			fadeFrac = Clamp01( ( cg_showPlayerNames_zfar->value - dist ) / ( cg_showPlayerNames_zfar->value * 0.25f ) );

			tmpcolor.w = cg_showPlayerNames_alpha->value * color.w * fadeFrac;
		} else {
			fadeFrac = Clamp01( ( cg.pointRemoveTime - cl.serverTime ) / 150.0f );

			tmpcolor.w = color.w * fadeFrac;
		}

		if( tmpcolor.w <= 0.0f ) {
			continue;
		}

		CG_Trace( &trace, cg.view.origin, Vec3( 0.0f ), Vec3( 0.0f ), cent->ent.origin, cg.predictedPlayerState.POVnum, MASK_OPAQUE );
		if( trace.fraction < 1.0f && trace.ent != cent->current.number ) {
			continue;
		}

		drawOrigin = Vec3( cent->ent.origin.x, cent->ent.origin.y, cent->ent.origin.z + playerbox_stand_maxs.z + 8 );

		Vec2 coords = WorldToScreen( drawOrigin );
		if( ( coords.x < 0 || coords.x > frame_static.viewport_width ) || ( coords.y < 0 || coords.y > frame_static.viewport_height ) ) {
			continue;
		}

		DrawText( font, font_size, cgs.clientInfo[i].name, Alignment_CenterBottom, coords.x, coords.y, tmpcolor, border );
	}
}

//=============================================================================

static const char * mini_obituaries[] = {
	"69",
	"102",
	"420",
	"1337",
	"1515",
	"ACHOO",
	"AHA",
	"AHH",
	"ARF",
	"ARGH",
	"BAH",
	"BAM",
	"BANG",
	"BARF",
	"BASH",
	"BEEP",
	"BIFF",
	"BING",
	"BLAB",
	"BLAM",
	"BLAST",
	"BLEEP",
	"BLESS",
	"BLING",
	"BLIP",
	"BLOOP",
	"BLUP",
	"BLURP",
	"BOING",
	"BOINK",
	"BONG",
	"BONK",
	"BOO",
	"BOOM",
	"BOOSH",
	"BOP",
	"BRRR",
	"BUCK",
	"BURP",
	"BUZZ",
	"BWAK",
	"BYE",
	"BZZZ",
	"CHEERS",
	"CHING",
	"CHUNK"
	,"CLACK"
	,"CLANG"
	,"CLANK"
	,"CLAP"
	,"CLASH"
	,"CLICK"
	,"CLINK"
	,"CLOP"
	,"CLOUT"
	,"CLUCK"
	,"CLUNK"
	,"COOL",
	"CRACK",
	"CRISP",
	"CRUNCH",
	"CYA",
	"DAB",
	"DING",
	"DOINK",
	"DONG",
	"DOOK",
	"DRIP",
	"DUH",
	"EEK",
	"EEYORE",
	"EHHH",
	"ESPORT",
	"EWW",
	"FART",
	"FINCH",
	"FIZZ",
	"FLAP",
	"FLASH",
	"FLEX",
	"FLICK",
	"FLIP",
	"FLOG",
	"FLOP",
	"FLUSH",
	"GAG",
	"GASP",
	"GG",
	"GNASH",
	"GNAW",
	"GONG",
	"GOSH",
	"GOT",
	"GOTEEM",
	"GRRR",
	"GULP",
	"GUSH",
	"GYUH",
	"HAH",
	"HAHA",
	"HAX",
	"HEH",
	"HEHE",
	"HEY",
	"HIP",
	"HISS",
	"HMPF",
	"HO",
	"HOHO",
	"HOOT",
	"HUFF",
	"HUMPF",
	"HUSH",
	"ICE",
	"ICKY",
	"ITCH",
	"JINGLE",
	"KLOK",
	"KLUNK",
	"KNOCK",
	"KRACH",
	"KURAC",
	"KURWA",
	"L8R",
	"LALA",
	"LIT",
	"LOL",
	"MEOW",
	"MMMMM",
	"MOO",
	"MROW",
	"MUNCH",
	"NAH",
	"NEIGH",
	"NOPE",
	"NYAH",
	"OHHH",
	"OINK",
	"OMG",
	"OOMPAH",
	"OOPS",
	"OOZE",
	"OUCH",
	"OW",
	"PEEP",
	"PEW",
	"PFF",
	"PHEW",
	"PING",
	"PIZDEC",
	"PLINK",
	"PLONK",
	"PLOOP",
	"PLOP",
	"PLZ",
	"POOF",
	"POP",
	"POW",
	"PRRR",
	"PSST",
	"PUFF",
	"PUMP",
	"QUACK",
	"QUEEF",
	"RAWR",
	"REKT",
	"RIBBIT",
	"RING",
	"RIP",
	"rm -rf",
	"ROFL",
	"ROWR",
	"RUFF",
	"SCAT",
	"SCHLIP",
	"SCRATCH",
	"SHHH",
	"SHIT",
	"SHOO",
	"SHOOP",
	"SIGH",
	"SKRA",
	"SKRRT",
	"SLAM",
	"SLASH",
	"SLIP",
	"SLUMP",
	"SMACK",
	"SMASH",
	"SNAP",
	"SNEEZE",
	"SNIP",
	"SNORT",
	"SPIT",
	"SPLAT",
	"SPLISH",
	"SPLOSH",
	"SPOOT",
	"SQUIRT",
	"SQUISH",
	"STOMP",
	"SUKA",
	"SUP",
	"SWASH",
	"SWOOP",
	"SWOOSH",
	"TACK",
	"TAP",
	"THROB",
	"THUD",
	"THUMP",
	"THUNK",
	"TING",
	"TKTK",
	"TONG",
	"TOOT",
	"TRILL",
	"TUFF",
	"TUG",
	"TWEET",
	"UGH",
	"UH-OH",
	"UNTZ",
	"VROOM",
	"WAAA",
	"WACK",
	"WAFFLE",
	"WANK",
	"WHACK",
	"WHAM",
	"WHEW",
	"WHIFF",
	"WHIP",
	"WHIRL",
	"WHIZ",
	"WHIZZ",
	"WHOA",
	"WHOO",
	"WHOOP",
	"WHOOPS",
	"WIZZ",
	"WOOF",
	"WOOSH",
	"WOW",
	"WTF",
	"YADDA",
	"YANK",
	"YAP",
	"YAWN",
	"YAWP",
	"YAY",
	"YEAH",
	"YEET",
	"YIKES",
	"YOINK",
	"YOOO",
	"YUCK",
	"YUMMY",
	"ZAP",
	"ZING",
	"ZIP",
	"ZLOPP",
	"ZONK",
	"ZOOM",
	"ZZZ" };

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
	dn->drift = random_float11( &cls.rng ) > 0.0f ? 1.0f : -1.0f;
	dn->obituary = random_select( &cls.rng, mini_obituaries );

	float distance_jitter = 4;
	dn->origin = ent->origin;
	dn->origin.x += random_float11( &cls.rng ) * distance_jitter;
	dn->origin.y += random_float11( &cls.rng ) * distance_jitter;
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

		DrawText( cgs.fontMontserrat, font_size, buf, Alignment_CenterBottom, coords.x, coords.y, color, true );
	}
}

//=============================================================================

struct BombSite {
	Vec3 origin;
	int team;
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
	int team;
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

	bomb.team = cent->current.team;
	bomb.origin = cent->current.origin;

	// TODO: this really does not belong here...
	if( bomb.state == BombState_Planted ) {
		ParticleEmitter emitter = { };

		Mat2 r = Mat2Rotation( cent->current.angles.y );
		emitter.position = bomb.origin + Vec3( r * Vec2( -12.0f, 3.0f ), -12.0f ); // TODO lol

		emitter.start_speed = 128.0f;
		emitter.end_speed = 128.0f;

		emitter.start_color = Vec4( 1.0f, 0.69f, 0.0f, 1.0f );
		emitter.end_color = Vec3( 0.8f, 0.1f, 0.0f );

		emitter.red_distribution.type = RandomDistributionType_Uniform;
		emitter.red_distribution.uniform = 0.1f;
		emitter.green_distribution.type = RandomDistributionType_Uniform;
		emitter.green_distribution.uniform = 0.05f;

		emitter.start_size = 24.0f;
		emitter.end_size = 0.0f;

		emitter.size_distribution.type = RandomDistributionType_Uniform;
		emitter.size_distribution.uniform = 2.0f;

		emitter.lifetime = 0.2f;
		emitter.lifetime_distribution.type = RandomDistributionType_Uniform;
		emitter.lifetime_distribution.uniform = 0.05f;

		emitter.emission_rate = 100;

		EmitParticles( &cgs.bullet_sparks, emitter );
	}
}

void CG_AddBombSite( centity_t * cent ) {
	assert( num_bomb_sites < ARRAY_COUNT( bomb_sites ) );

	BombSite * site = &bomb_sites[ num_bomb_sites ];
	site->origin = cent->current.origin;
	site->team = cent->current.team;
	site->letter = cent->current.counterNum;

	num_bomb_sites++;
}

void CG_DrawBombHUD() {
	if( GS_MatchState( &client_gs ) > MATCH_STATE_PLAYTIME )
		return;

	int my_team = cg.predictedPlayerState.team;
	bool show_labels = my_team != TEAM_SPECTATOR && GS_MatchState( &client_gs ) == MATCH_STATE_PLAYTIME;

	Vec4 yellow = sRGBToLinear( rgba8_diesel_yellow );

	// TODO: draw arrows when clamped

	if( bomb.state == BombState_Carried || bomb.state == BombState_Dropped ) {
		for( size_t i = 0; i < num_bomb_sites; i++ ) {
			const BombSite * site = &bomb_sites[ i ];
			bool clamped;
			Vec2 coords = WorldToScreenClamped( site->origin, Vec2( cgs.fontSystemMediumSize * 2 ), &clamped );

			char buf[ 4 ];
			snprintf( buf, sizeof( buf ), "%c", site->letter );
			DrawText( cgs.fontMontserrat, cgs.textSizeMedium, buf, Alignment_CenterMiddle, coords.x, coords.y, yellow, true );

			if( show_labels && !clamped && bomb.state != BombState_Dropped ) {
				const char * msg = my_team == site->team ? "DEFEND" : "ATTACK";
				coords.y += ( cgs.fontSystemMediumSize * 7 ) / 8;
				DrawText( cgs.fontMontserrat, cgs.textSizeTiny, msg, Alignment_CenterMiddle, coords.x, coords.y, yellow, true );
			}
		}
	}

	if( bomb.state != BombState_Carried ) {
		bool clamped;
		Vec2 coords = WorldToScreenClamped( bomb.origin, Vec2( cgs.fontSystemMediumSize * 2 ), &clamped );

		if( clamped ) {
			int icon_size = ( cgs.fontSystemMediumSize * frame_static.viewport_height ) / 600;
			Draw2DBox( coords.x - icon_size / 2, coords.y - icon_size / 2, icon_size, icon_size, cgs.media.shaderBombIcon );
		}
		else {
			if( show_labels ) {
				Vec4 color = vec4_white;
				const char * msg;

				if( bomb.state == BombState_Dropped ) {
					msg = "RETRIEVE";
					color = AttentionGettingColor();

					// TODO: lol
					{
						ParticleEmitter emitter = { };

						emitter.position = bomb.origin - Vec3( 0.0f, 0.0f, 32.0f );
						emitter.position_distribution.type = RandomDistribution3DType_Sphere;
						emitter.position_distribution.sphere.radius = 20.0f;

						emitter.start_speed = 8.0f;
						emitter.end_speed = 0.0f;

						emitter.start_color = AttentionGettingColor();
						emitter.end_color = emitter.start_color.xyz();

						emitter.start_size = 16.0f;

						emitter.lifetime = 1.0f;
						emitter.emission_rate = 50;

						EmitParticles( &cgs.rain, emitter );
					}
				}
				else if( bomb.state == BombState_Planting ) {
					msg = "PLANTING";
					color = AttentionGettingColor();
				}
				else if( bomb.state == BombState_Planted ) {
					if( my_team == bomb.team ) {
						msg = "PROTECT";
					}
					else {
						msg = "DEFUSE";
						color = AttentionGettingColor();
					}
				}

				float y = coords.y - cgs.fontSystemTinySize / 2;
				DrawText( cgs.fontMontserrat, cgs.textSizeSmall, msg, Alignment_CenterMiddle, coords.x, y, color, true );
			}
		}

	}
}

void CG_ResetBombHUD() {
	num_bomb_sites = 0;
	bomb.state = BombState_Carried;
}

//=============================================================================

/*
* CG_EscapeKey
*/
void CG_EscapeKey( void ) {
	if( cgs.demoPlaying ) {
		UI_ShowDemoMenu();
		return;
	}

	UI_ShowGameMenu();
}

static Vec4 CG_CalcColorBlend() {
	int contents = CG_PointContents( cg.view.origin );
	if( contents & CONTENTS_WATER )
		return Vec4( 0.0f, 0.1f, 1.0f, 0.2f );
	if( contents & CONTENTS_LAVA )
		return Vec4( 1.0f, 0.3f, 0.0f, 0.6f );
	if( contents & CONTENTS_SLIME )
		return Vec4( 0.0f, 0.1f, 0.05f, 0.6f );

	return Vec4( 0 );
}

/*
* CG_SCRDrawViewBlend
*/
static void CG_SCRDrawViewBlend( void ) {
	if( !cg_showViewBlends->integer ) {
		return;
	}

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

static void CG_DrawScope() {
	if( cg.predictedPlayerState.weapon == Weapon_Sniper && cg.predictedPlayerState.zoom_time > 0 ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.ui_pass;
		pipeline.shader = &shaders.scope;
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Blend;
		pipeline.write_depth = false;
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		DrawFullscreenMesh( pipeline );
	}
}

/*
* CG_Draw2DView
*/
void CG_Draw2DView( void ) {
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

	CG_ExecuteLayoutProgram( cg.statusBar );
	CG_DrawChat();
}

/*
* CG_Draw2D
*/
void CG_Draw2D( void ) {
	CG_DrawScope();

	if( !cg_draw2D->integer ) {
		return;
	}

	CG_Draw2DView();
	CG_DrawDemocam2D();
}
