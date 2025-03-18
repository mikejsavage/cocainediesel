/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.


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

#include "qcommon/base.h"
#include "qcommon/fpe.h"
#include "qcommon/time.h"
#include "client/assets.h"
#include "client/clay.h"
#include "client/keys.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
#include "cgame/cg_local.h"

#include "imgui/imgui.h"

#include "luau/lua.h"
#include "luau/lualib.h"
#include "luau/luacode.h"

static const Vec4 light_gray = sRGBToLinear( RGBA8( 96, 96, 96, 255 ) );

static lua_State * hud_L;

static u32 clay_element_counter;

template< typename T >
struct LuauConst {
	const char * name;
	T value;
};

static constexpr LuauConst< int > numeric_constants[] = {
	{ "Team_None", Team_None },
	{ "Team_One", Team_One },
	{ "Team_Two", Team_Two },
	{ "Team_Three", Team_Three },
	{ "Team_Four", Team_Four },
	{ "Team_Five", Team_Five },
	{ "Team_Six", Team_Six },
	{ "Team_Sevent", Team_Seven },
	{ "Team_Eight", Team_Eight },
	{ "Team_Count", Team_Count },

	{ "WeaponState_Reloading", WeaponState_Reloading },
	{ "WeaponState_StagedReloading", WeaponState_StagedReloading },

	{ "Gadget_None", Gadget_None },

	{ "Perk_Ninja", Perk_Ninja },
	{ "Perk_Hooligan", Perk_Hooligan },
	{ "Perk_Midget", Perk_Midget },
	{ "Perk_Wheel", Perk_Wheel },
	{ "Perk_Jetpack", Perk_Jetpack },
	{ "Perk_Boomer", Perk_Boomer },

	{ "Stamina_Normal", Stamina_Normal },
	{ "Stamina_Reloading", Stamina_Reloading },
	{ "Stamina_UsingAbility", Stamina_UsingAbility },
	{ "Stamina_UsedAbility", Stamina_UsedAbility },

	{ "Gametype_Bomb", Gametype_Bomb },
	{ "Gametype_Gladiator", Gametype_Gladiator },

	{ "MatchState_Warmup", MatchState_Warmup },
	{ "MatchState_Countdown", MatchState_Countdown },
	{ "MatchState_Playing", MatchState_Playing },
	{ "MatchState_PostMatch", MatchState_PostMatch },
	{ "MatchState_WaitExit", MatchState_WaitExit },

	{ "BombProgress_Nothing", BombProgress_Nothing },
	{ "BombProgress_Planting", BombProgress_Planting },
	{ "BombProgress_Defusing", BombProgress_Defusing },

	{ "RoundState_Countdown", RoundState_Countdown },
	{ "RoundState_Round", RoundState_Round },
	{ "RoundState_Finished", RoundState_Finished },
	{ "RoundState_Post", RoundState_Post },

	{ "RoundType_Normal", RoundType_Normal },
	{ "RoundType_MatchPoint", RoundType_MatchPoint },
	{ "RoundType_Overtime", RoundType_Overtime },
	{ "RoundType_OvertimeMatchPoint", RoundType_OvertimeMatchPoint },

	{ "BombProgress_Planting", BombProgress_Planting },
	{ "BombProgress_Defusing", BombProgress_Defusing },
};

static constexpr LuauConst< StringHash > asset_constants[] = {
	{ "diagonal_pattern", "hud/diagonal_pattern" },
	{ "bomb", "hud/icons/bomb" },
	{ "guy", "hud/icons/guy" },
	{ "net", "hud/icons/net" },
	{ "star", "hud/icons/star" },
};

static int CG_GetSpeed() {
	return Length( cg.predictedPlayerState.pmove.velocity.xy() );
}

static int CG_GetFPS() {
	static int frameTimes[ 32 ];

	if( cg_showFPS->modified ) {
		memset( frameTimes, 0, sizeof( frameTimes ) );
		cg_showFPS->modified = false;
	}

	frameTimes[cg.frameCount % ARRAY_COUNT( frameTimes )] = cls.realFrameTime;

	float average = 0.0f;
	for( size_t i = 0; i < ARRAY_COUNT( frameTimes ); i++ ) {
		average += frameTimes[( cg.frameCount - i ) % ARRAY_COUNT( frameTimes )];
	}
	average /= ARRAY_COUNT( frameTimes );
	return int( 1000.0f / average + 0.5f );
}

static bool CG_IsLagging() {
	int64_t incomingAcknowledged, outgoingSequence;
	CL_GetCurrentState( &incomingAcknowledged, &outgoingSequence, NULL );
	return !cgs.demoPlaying && (outgoingSequence - incomingAcknowledged) >= (CMD_BACKUP - 1);
}

//=============================================================================

static constexpr size_t MAX_OBITUARIES = 32;

enum obituary_type_t {
	OBITUARY_NONE,
	OBITUARY_NORMAL,
	OBITUARY_SUICIDE,
	OBITUARY_ACCIDENT,
};

struct obituary_t {
	obituary_type_t type;
	Time time;
	char victim[MAX_INFO_VALUE];
	Team victim_team;
	char attacker[MAX_INFO_VALUE];
	Team attacker_team;
	DamageType damage_type;
	bool wallbang;
};

static obituary_t cg_obituaries[MAX_OBITUARIES];
static int cg_obituaries_current = -1;

struct {
	Time time;
	u64 entropy;
	obituary_type_t type;
	DamageType damage_type;
} self_obituary;

void CG_SC_ResetObituaries() {
	memset( cg_obituaries, 0, sizeof( cg_obituaries ) );
	cg_obituaries_current = -1;
	self_obituary = { };
}

static Span< const char > normal_obituaries[] = {
#include "obituaries.h"
};

static Span< const char > prefixes[] = {
#include "prefixes.h"
};

static Span< const char > suicide_prefixes[] = {
	"AUTO",
	"SELF",
	"SHANKS",
	"SOLO",
	"TIMMA",
};

static Span< const char > void_obituaries[] = {
	"ATE",
	"HOLED",
	"RECLAIMED",
	"TOOK",
};

static Span< const char > spike_obituaries[] = {
	"DISEMBOWELED",
	"GORED",
	"IMPALED",
	"PERFORATED",
	"POKED",
	"SKEWERED",
	"SLASHED",
};

static Span< const char > conjunctions[] = {
	"+",
	"&",
	"&&",
	"ALONG WITH",
	"AND",
	"AS WELL AS",
	"ASSISTED BY",
	"FEAT.",
	"GEULIGO",
	"JA",
	"N'",
	"PLUS",
	"UND",
	"W/",
	"WITH",
	"X",
};

static Span< const char > RandomPrefix( RNG * rng, float p ) {
	if( !Probability( rng, p ) )
		return "";
	return RandomElement( rng, prefixes );
}

static Span< char > Uppercase( Allocator * a, Span< const char > str ) {
	Span< char > upper = AllocSpan< char >( a, str.n );
	for( size_t i = 0; i < str.n; i++ ) {
		upper[ i ] = ToUpperASCII( str[ i ] );
	}
	return upper;
}

static Span< char > MakeObituary( Allocator * a, RNG * rng, int type, DamageType damage_type ) {
	Span< Span< const char > > obituaries = StaticSpan( normal_obituaries );
	if( damage_type == WorldDamage_Void ) {
		obituaries = StaticSpan( void_obituaries );
	}
	else if( damage_type == WorldDamage_Spike ) {
		obituaries = StaticSpan( spike_obituaries );
	}

	Span< const char > prefix1 = "";
	if( type == OBITUARY_SUICIDE ) {
		prefix1 = RandomElement( rng, suicide_prefixes );
	}

	// do these in order because arg evaluation order is undefined
	Span< const char > prefix2 = RandomPrefix( rng, 0.05f );
	Span< const char > prefix3 = RandomPrefix( rng, 0.5f );

	return a->sv( "{}{}{}{}", prefix1, prefix2, prefix3, obituaries[ RandomUniform( rng, 0, obituaries.n ) ] );
}

void CG_SC_Obituary( const Tokenized & args ) {
	int victimNum = SpanToInt( args.tokens[ 1 ], 0 );
	int attackerNum = SpanToInt( args.tokens[ 2 ], 0 );
	int topAssistorNum = SpanToInt( args.tokens[ 3 ], 0 );
	DamageType damage_type;
	damage_type.encoded = SpanToInt( args.tokens[ 4 ], 0 );
	bool wallbang = SpanToInt( args.tokens[ 5 ], 0 ) == 1;
	u64 entropy = SpanToU64( args.tokens[ 6 ], 0 );

	Span< const char > victim = PlayerName( victimNum - 1 );
	Span< const char > attacker = attackerNum == 0 ? ""_sp : PlayerName( attackerNum - 1 );
	Span< const char > assistor = topAssistorNum == -1 ? ""_sp : PlayerName( topAssistorNum - 1 );

	cg_obituaries_current = ( cg_obituaries_current + 1 ) % ARRAY_COUNT( cg_obituaries );
	obituary_t * current = &cg_obituaries[ cg_obituaries_current ];
	current->type = OBITUARY_NORMAL;
	current->time = cls.monotonicTime;
	current->damage_type = damage_type;
	current->wallbang = wallbang;

	if( victim != "" ) {
		ggformat( current->victim, sizeof( current->victim ), "{}", victim );
		current->victim_team = cg_entities[ victimNum ].current.team;
	}
	if( attacker != "" ) {
		ggformat( current->attacker, sizeof( current->attacker ), "{}", attacker );
		current->attacker_team = cg_entities[ attackerNum ].current.team;
	}

	Team assistor_team = assistor == "" ? Team_None : cg_entities[ topAssistorNum ].current.team;

	if( cg.view.playerPrediction && ISVIEWERENTITY( victimNum ) ) {
		self_obituary.entropy = 0;
	}

	TempAllocator temp = cls.frame_arena.temp();
	RNG rng = NewRNG( entropy, 0 );

	Span< const char > attacker_name = attacker == "" ? ""_sp : temp.sv( "{}{}", ImGuiColorToken( CG_TeamColor( current->attacker_team ) ), Uppercase( &temp, attacker ) );
	Span< const char > victim_name = temp.sv( "{}{}", ImGuiColorToken( CG_TeamColor( current->victim_team ) ), Uppercase( &temp, victim ) );
	Span< const char > assistor_name = assistor == "" ? ""_sp : temp.sv( "{}{}", ImGuiColorToken( CG_TeamColor( assistor_team ) ), Uppercase( &temp, assistor ) );

	if( attackerNum == 0 ) {
		current->type = OBITUARY_ACCIDENT;

		if( damage_type == WorldDamage_Void ) {
			attacker_name = temp.sv( "{}{}", ImGuiColorToken( black.rgba8 ), "THE VOID" );
		}
		else if( damage_type == WorldDamage_Spike ) {
			attacker_name = temp.sv( "{}{}", ImGuiColorToken( black.rgba8 ), "A SPIKE" );
		}
		else {
			return;
		}
	}

	Span< const char > obituary = MakeObituary( &temp, &rng, current->type, damage_type );

	if( cg.view.playerPrediction && ISVIEWERENTITY( victimNum ) ) {
		self_obituary.time = cls.monotonicTime;
		self_obituary.entropy = entropy;
		self_obituary.type = current->type;
		self_obituary.damage_type = damage_type;
	}

	if( assistor == "" ) {
		CG_AddChat( temp.sv( "{} {}{} {}",
			attacker_name,
			ImGuiColorToken( diesel_yellow.rgba8 ), obituary,
			victim_name
		) );
	}
	else {
		Span< const char > conjugation = RandomElement( &rng, conjunctions );
		CG_AddChat( temp.sv( "{} {}{} {} {}{} {}",
			attacker_name,
			ImGuiColorToken( 255, 255, 255, 255 ), conjugation,
			assistor_name,
			ImGuiColorToken( diesel_yellow.rgba8 ), obituary,
			victim_name
		) );
	}

	if( ISVIEWERENTITY( attackerNum ) && attacker.ptr != victim.ptr ) {
		CG_CenterPrint( temp.sv( "{} {}", obituary, Uppercase( &temp, victim ) ) );
	}
}

static const Material * DamageTypeToIcon( DamageType type ) {
	WeaponType weapon;
	GadgetType gadget;
	WorldDamage world;
	DamageCategory category = DecodeDamageType( type, &weapon, &gadget, &world );

	if( category == DamageCategory_Weapon ) {
		return FindMaterial( cgs.media.shaderWeaponIcon[ weapon ] );
	}

	if( category == DamageCategory_Gadget ) {
		return FindMaterial( cgs.media.shaderGadgetIcon[ gadget ] );
	}

	switch( world ) {
		case WorldDamage_Crush:
			return FindMaterial( "hud/icons/obituaries/crush" );
		case WorldDamage_Telefrag:
			return FindMaterial( "hud/icons/obituaries/telefrag" );
		case WorldDamage_Suicide:
			return FindMaterial( "hud/icons/obituaries/suicide" );
		case WorldDamage_Explosion:
			return FindMaterial( "hud/icons/obituaries/explosion" );
		case WorldDamage_Trigger:
			return FindMaterial( "hud/icons/obituaries/trigger" );
		case WorldDamage_Laser:
			return FindMaterial( "hud/icons/obituaries/laser" );
		case WorldDamage_Spike:
			return FindMaterial( "hud/icons/obituaries/spike" );
		case WorldDamage_Void:
			return FindMaterial( "hud/icons/obituaries/void" );
	}

	return FindMaterial( "" );
}

//=============================================================================

static void GlitchText( Span< char > msg ) {
	constexpr char glitches[] = { '#', '@', '~', '$' };

	RNG rng = NewRNG( cls.monotonicTime.flicks / ( GGTIME_FLICKS_PER_SECOND / 14 ), 0 );

	for( char & c : msg ) {
		if( Probability( &rng, 0.03f ) ) {
			c = RandomElement( &rng, glitches );
		}
	}
}

void CG_DrawScope() {
	const WeaponDef * def = GS_GetWeaponDef( cg.predictedPlayerState.weapon );
	if( def->zoom_fov != 0 && cg.predictedPlayerState.zoom_time > 0 ) {
		float frac = cg.predictedPlayerState.zoom_time / float( ZOOMTIME );

		PipelineState pipeline;
		pipeline.pass = frame_static.ui_pass;
		pipeline.shader = &shaders.scope;
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Blend;
		pipeline.write_depth = false;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
		DrawFullscreenMesh( pipeline );

		if( cg.predictedPlayerState.weapon == Weapon_Sniper ) {
			Vec3 forward = -frame_static.V.row2().xyz();
			Vec3 end = cg.view.origin + forward * 10000.0f;
			trace_t trace = CG_Trace( cg.view.origin, MinMax3( 0.0f ), end, cg.predictedPlayerState.POVnum, SolidMask_Shot );

			TempAllocator temp = cls.frame_arena.temp();
			float offset = Min2( frame_static.viewport_width, frame_static.viewport_height ) * 0.1f;

			{
				float distance = Length( trace.endpos - cg.view.origin ) + Sin( cls.monotonicTime, Milliseconds( 804 ) ) * 0.5f + Sin( cls.monotonicTime, Milliseconds( 1619 ) ) * 0.25f;

				char * msg = temp( "{.2}m", distance / 32.0f );
				GlitchText( Span< char >( msg + strlen( msg ) - 3, 2 ) );

				DrawText( cls.fontItalic, cgs.textSizeSmall, msg, Alignment_RightTop, frame_static.viewport_width / 2 - offset, frame_static.viewport_height / 2 + offset, red.vec4 );
			}

			if( trace.ent > 0 && trace.ent <= MAX_CLIENTS ) {
				Vec4 color = AttentionGettingColor();
				color.w = frac;

				RNG obituary_rng = NewRNG( cls.monotonicTime.flicks / GGTIME_FLICKS_PER_SECOND, 0 );
				char * msg = temp( "{}?", RandomElement( &obituary_rng, normal_obituaries ) );
				GlitchText( Span< char >( msg, strlen( msg ) - 1 ) );

				DrawText( cls.fontItalic, cgs.textSizeSmall, msg, Alignment_LeftTop, frame_static.viewport_width / 2 + offset, frame_static.viewport_height / 2 + offset, color, black.vec4 );
			}
		}
	}
}

//=============================================================================

static void PushLuaAsset( lua_State * L, StringHash s ) {
	lua_pushlightuserdata( L, checked_cast< void * >( checked_cast< uintptr_t >( s.hash ) ) );
}

static bool CallWithStackTrace( lua_State * L, int args, int results ) {
	int debug_traceback_idx = 1;
	if( lua_pcall( L, args, results, debug_traceback_idx ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "%s\n", lua_tostring( L, -1 ) );
		lua_pop( L, 1 );
		return false;
	}

	return true;
}

static Span< const char > LuaToSpan( lua_State * L, int idx ) {
	size_t len;
	const char * str = lua_tolstring( L, idx, &len );
	return Span< const char >( str, len );
}

static Span< const char > LuaCheckSpan( lua_State * L, int idx ) {
	size_t len;
	const char * str = luaL_checklstring( L, idx, &len );
	return Span< const char >( str, len );
}

static void LuaPushSpan( lua_State * L, Span< const char > str ) {
	lua_pushlstring( L, str.ptr, str.n );
}

static u8 CheckRGBA8Component( lua_State * L, int idx, int narg ) {
	float val = lua_tonumber( L, idx );
	luaL_argcheck( L, float( u8( val ) ) == val, narg, "RGBA8 colors must have u8 components" );
	return u8( val );
}

static bool IsHex( char c ) {
	return ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
}

static bool ParseHexDigit( u8 * digit, char c ) {
	if( !IsHex( c ) )
		return false;
	if( c >= '0' && c <= '9' ) *digit = c - '0';
	if( c >= 'a' && c <= 'f' ) *digit = 10 + c - 'a';
	if( c >= 'A' && c <= 'F' ) *digit = 10 + c - 'A';
	return true;
}

static bool ParseHexByte( u8 * byte, char a, char b ) {
	u8 x, y;
	if( !ParseHexDigit( &x, a ) || !ParseHexDigit( &y, b ) )
		return false;
	*byte = x * 16 + y;
	return true;
}

static bool ParseHexColor( RGBA8 * rgba, Span< const char > str ) {
	if( str.n == 0 || str[ 1 ] == '#' )
		return false;

	str++;

	char digits[ 8 ];
	digits[ 6 ] = 'f';
	digits[ 7 ] = 'f';

	if( str.n == 3 || str.n == 4 ) {
		// #rgb #rgba
		for( size_t i = 0; i < str.n; i++ ) {
			digits[ i * 2 + 0 ] = str[ i ];
			digits[ i * 2 + 1 ] = str[ i ];
		}
	}
	else if( str.n == 6 || str.n == 8 ) {
		// #rrggbb #rrggbbaa
		for( size_t i = 0; i < str.n; i++ ) {
			digits[ i ] = str[ i ];
		}
	}
	else {
		return false;
	}

	bool ok = true;
	ok = ok && ParseHexByte( &rgba->r, digits[ 0 ], digits[ 1 ] );
	ok = ok && ParseHexByte( &rgba->g, digits[ 2 ], digits[ 3 ] );
	ok = ok && ParseHexByte( &rgba->b, digits[ 4 ], digits[ 5 ] );
	ok = ok && ParseHexByte( &rgba->a, digits[ 6 ], digits[ 7 ] );
	return ok;
}

static Vec4 CheckColor( lua_State * L, int narg ) {
	int type = lua_type( L, narg );
	luaL_argcheck( L, type == LUA_TSTRING || type == LUA_TTABLE, narg, "colors must be strings or tables" );

	if( lua_type( L, narg ) == LUA_TSTRING ) {
		RGBA8 rgba;
		if( !ParseHexColor( &rgba, LuaToSpan( L, narg ) ) ) {
			luaL_error( L, "color doesn't parse as a hex string: %s", lua_tostring( L, narg ) );
		}
		return sRGBToLinear( rgba );
	}

	lua_getfield( L, narg, "srgb" );
	luaL_argcheck( L, lua_isboolean( L, -1 ), narg, "color.srgb must be a boolean" );
	bool srgb = lua_toboolean( L, -1 );
	lua_pop( L, 1 );

	if( srgb ) {
		RGBA8 rgba;

		lua_getfield( L, narg, "r" );
		rgba.r = CheckRGBA8Component( L, -1, narg );
		lua_pop( L, 1 );

		lua_getfield( L, narg, "g" );
		rgba.g = CheckRGBA8Component( L, -1, narg );
		lua_pop( L, 1 );

		lua_getfield( L, narg, "b" );
		rgba.b = CheckRGBA8Component( L, -1, narg );
		lua_pop( L, 1 );

		lua_getfield( L, narg, "a" );
		rgba.a = lua_isnil( L, -1 ) ? 255 : CheckRGBA8Component( L, -1, narg );
		lua_pop( L, 1 );

		return sRGBToLinear( rgba );
	}

	Vec4 linear;

	lua_getfield( L, narg, "r" );
	linear.x = lua_tonumber( L, -1 );
	lua_pop( L, 1 );

	lua_getfield( L, narg, "g" );
	linear.y = lua_tonumber( L, -1 );
	lua_pop( L, 1 );

	lua_getfield( L, narg, "b" );
	linear.z = lua_tonumber( L, -1 );
	lua_pop( L, 1 );

	lua_getfield( L, narg, "a" );
	linear.w = lua_isnil( L, -1 ) ? 1.0f : lua_tonumber( L, -1 );
	lua_pop( L, 1 );

	return linear;
}

static int LuauRGBA8( lua_State * L ) {
	RGBA8 rgba;
	rgba.r = CheckRGBA8Component( L, 1, 1 );
	rgba.g = CheckRGBA8Component( L, 2, 2 );
	rgba.b = CheckRGBA8Component( L, 3, 3 );
	rgba.a = lua_isnoneornil( L, 4 ) ? 255 : CheckRGBA8Component( L, 4, 4 );

	lua_newtable( L );

	lua_pushboolean( L, true );
	lua_setfield( L, -2, "srgb" );

	lua_pushnumber( L, rgba.r );
	lua_setfield( L, -2, "r" );

	lua_pushnumber( L, rgba.g );
	lua_setfield( L, -2, "g" );

	lua_pushnumber( L, rgba.b );
	lua_setfield( L, -2, "b" );

	lua_pushnumber( L, rgba.a );
	lua_setfield( L, -2, "a" );

	return 1;
}

static int LuauRGBALinear( lua_State * L ) {
	lua_newtable( L );

	lua_pushboolean( L, false );
	lua_setfield( L, -2, "srgb" );

	lua_pushvalue( L, 1 );
	lua_setfield( L, -2, "r" );

	lua_pushvalue( L, 2 );
	lua_setfield( L, -2, "g" );

	lua_pushvalue( L, 3 );
	lua_setfield( L, -2, "b" );

	lua_pushvalue( L, 4 );
	lua_setfield( L, -2, "a" );

	return 1;
}

static int LuausRGBToLinear( lua_State * L ) {
	float i = luaL_checknumber( L, 1 );
	lua_newtable( L );
	lua_pushnumber( L, sRGBToLinear( i ) );
	return 1;
}

static int LuauPrint( lua_State * L ) {
	Com_Printf( "%s\n", luaL_checkstring( L, 1 ) );
	return 0;
}

static int LuauAsset( lua_State * L ) {
	StringHash hash( luaL_checkstring( L, 1 ) );
	PushLuaAsset( L, hash );
	return 1;
}

static StringHash CheckHash( lua_State * L, int idx ) {
	if( lua_isnoneornil( L, idx ) )
		return EMPTY_HASH;
	luaL_checktype( L, idx, LUA_TLIGHTUSERDATA );
	return StringHash( checked_cast< u64 >( checked_cast< uintptr_t >( lua_touserdata( L, idx ) ) ) );
}

static int LuauDraw2DBox( lua_State * L ) {
	float x = luaL_checknumber( L, 1 );
	float y = luaL_checknumber( L, 2 );
	float w = luaL_checknumber( L, 3 );
	float h = luaL_checknumber( L, 4 );
	Vec4 color = CheckColor( L, 5 );
	StringHash material = CheckHash( L, 6 );
	Draw2DBox( x, y, w, h, material == EMPTY_HASH ? cls.white_material : FindMaterial( material ), color );
	return 0;
}

static int LuauDraw2DBoxUV( lua_State * L ) {
	float x = luaL_checknumber( L, 1 );
	float y = luaL_checknumber( L, 2 );
	float w = luaL_checknumber( L, 3 );
	float h = luaL_checknumber( L, 4 );
	Vec2 topleft_uv = Vec2( luaL_checknumber( L, 5 ), luaL_checknumber( L, 6 ) );
	Vec2 bottomright_uv = Vec2( luaL_checknumber( L, 7 ), luaL_checknumber( L, 8 ) );
	Vec4 color = CheckColor( L, 9 );
	StringHash material = CheckHash( L, 10 );
	Draw2DBoxUV( x, y, w, h, topleft_uv, bottomright_uv, material == EMPTY_HASH ? cls.white_material : FindMaterial( material ), color );
	return 0;
}

static Alignment CheckAlignment( lua_State * L, int idx ) {
	constexpr Alignment alignments[] = {
		Alignment_LeftTop,
		Alignment_CenterTop,
		Alignment_RightTop,
		Alignment_LeftMiddle,
		Alignment_CenterMiddle,
		Alignment_RightMiddle,
		Alignment_LeftBottom,
		Alignment_CenterBottom,
		Alignment_RightBottom,

		{ XAlignment_Left, YAlignment_Ascent },
		{ XAlignment_Center, YAlignment_Ascent },
		{ XAlignment_Right, YAlignment_Ascent },
		{ XAlignment_Left, YAlignment_Baseline },
		{ XAlignment_Center, YAlignment_Baseline },
		{ XAlignment_Right, YAlignment_Baseline },
		{ XAlignment_Left, YAlignment_Descent },
		{ XAlignment_Center, YAlignment_Descent },
		{ XAlignment_Right, YAlignment_Descent },
	};

	constexpr const char * names[] = {
		"left top",
		"center top",
		"right top",
		"left middle",
		"center middle",
		"right middle",
		"left bottom",
		"center bottom",
		"right bottom",

		"left ascent",
		"center ascent",
		"right ascent",
		"left baseline",
		"center baseline",
		"right baseline",
		"left descent",
		"center descent",
		"right descent",

		NULL
	};

	return alignments[ luaL_checkoption( L, idx, names[ 0 ], names ) ];
}

static const Font * CheckFont( lua_State * L, int idx ) {
	const Font * fonts[] = {
		cls.fontNormal,
		cls.fontNormalBold,
		cls.fontItalic,
		cls.fontBoldItalic,
	};

	constexpr const char * names[] = {
		"normal",
		"bold",
		"italic",
		"bolditalic",

		NULL
	};

	return fonts[ luaL_checkoption( L, idx, names[ 0 ], names ) ];
}

static int LuauDrawText( lua_State * L ) {
	luaL_checktype( L, 1, LUA_TTABLE );

	lua_getfield( L, 1, "color" );
	Vec4 color = CheckColor( L, -1 );
	lua_pop( L, 1 );

	lua_getfield( L, 1, "font" );
	const Font * font = CheckFont( L, -1 );
	lua_pop( L, 1 );

	lua_getfield( L, 1, "font_size" );
	luaL_argcheck( L, lua_type( L, -1 ) == LUA_TNUMBER, 1, "options.font_size must be a number" );
	float font_size = lua_tonumber( L, -1 );
	lua_pop( L, 1 );

	Optional< Vec4 > border_color = NONE;
	lua_getfield( L, 1, "border" );
	if( !lua_isnil( L, -1 ) ) {
		border_color = CheckColor( L, -1 );
	}
	lua_pop( L, 1 );

	lua_getfield( L, 1, "alignment" );
	Alignment alignment = CheckAlignment( L, -1 );
	lua_pop( L, 1 );

	float x = luaL_checknumber( L, 2 );
	float y = luaL_checknumber( L, 3 );
	Span< const char > str = LuaCheckSpan( L, 4 );

	DrawText( font, font_size, str, alignment, x, y, color, border_color );

	return 0;
}

static int LuauGetBind( lua_State * L ) {
	TempAllocator temp = cls.frame_arena.temp();

	Span< const char > command = LuaCheckSpan( L, 1 );
	Optional< Key > key1, key2;
	GetKeyBindsForCommand( command, &key1, &key2 );

	Span< const char > bind;
	if( key1.exists && key2.exists ) {
		bind = temp.sv( "{} or {}", KeyName( key1.value ), KeyName( key2.value ) );
	}
	else if( key1.exists ) {
		bind = KeyName( key1.value );
	}
	else {
		bind = temp.sv( "[{}]", command );
	}

	lua_newtable( L );
	LuaPushSpan( L, bind );

	return 1;
}

static int Vec4ToLuauColor( lua_State * L, Vec4 color ) {
	lua_newtable( L );

	lua_pushboolean( L, false );
	lua_setfield( L, -2, "srgb" );

	lua_pushnumber( L, color.x );
	lua_setfield( L, -2, "r" );

	lua_pushnumber( L, color.y );
	lua_setfield( L, -2, "g" );

	lua_pushnumber( L, color.z );
	lua_setfield( L, -2, "b" );

	lua_pushnumber( L, color.w );
	lua_setfield( L, -2, "a" );

	return 1;
}

static int LuauGetTeamColor( lua_State * L ) {
	return Vec4ToLuauColor( L, CG_TeamColorVec4( Team( luaL_checkinteger( L, 1 ) ) ) ); // TODO: check in range
}

static int LuauAllyColor( lua_State * L ) {
	return Vec4ToLuauColor( L, AllyColorVec4() );
}

static int LuauEnemyColor( lua_State * L ) {
	return Vec4ToLuauColor( L, EnemyColorVec4() );
}

static int LuauAttentionGettingColor( lua_State * L ) {
	return Vec4ToLuauColor( L, AttentionGettingColor() );
}

static int LuauPlantableColor( lua_State * L ) {
	return Vec4ToLuauColor( L, PlantableColor() );
}

static int LuauAttentionGettingRed( lua_State * L ) {
	return Vec4ToLuauColor( L, AttentionGettingRed() );
}

static int LuauGetPlayerName( lua_State * L ) {
	int index = luaL_checknumber( L, 1 ) - 1;

	if( index >= 0 && index < client_gs.maxclients ) {
		lua_newtable( L );
		LuaPushSpan( L, PlayerName( index ) );

		return 1;
	}

	return 0;
}

static int LuauGetWeaponIcon( lua_State * L ) {
	u8 w = luaL_checknumber( L, 1 );
	lua_newtable( L );
	PushLuaAsset( L, cgs.media.shaderWeaponIcon[ w ] );
	return 1;
}

static int LuauGetGadgetIcon( lua_State * L ) {
	u8 g = luaL_checknumber( L, 1 );
	lua_newtable( L );
	PushLuaAsset( L, cgs.media.shaderGadgetIcon[ g ] );
	return 1;
}

static int LuauGetPerkIcon( lua_State * L ) {
	u8 p = luaL_checknumber( L, 1 );
	lua_newtable( L );
	PushLuaAsset( L, cgs.media.shaderPerkIcon[ p ] );
	return 1;
}

static int LuauGetWeaponReloadTime( lua_State * L ) {
	u8 w = luaL_checknumber( L, 1 );
	lua_newtable( L );
	lua_pushnumber( L, GS_GetWeaponDef( WeaponType( w ) )->reload_time );
	return 1;
}

static int LuauGetWeaponStagedReload( lua_State * L ) {
	u8 w = luaL_checknumber( L, 1 );
	lua_newtable( L );
	lua_pushboolean( L, GS_GetWeaponDef( WeaponType( w ) )->staged_reload );
	return 1;
}

static int LuauGetGadgetAmmo( lua_State * L ) {
	u8 g = luaL_checknumber( L, 1 );
	lua_newtable( L );
	lua_pushnumber( L, GetGadgetDef( GadgetType( g ) )->uses );
	return 1;
}

static int LuauGetClockTime( lua_State * L ) {
	lua_newtable( L );
	int64_t clocktime, startTime, duration, curtime;
	int64_t zero = 0;

	if( client_gs.gameState.round_state >= RoundState_Finished ) {
		lua_pushnumber( L, 0 );
		return 1;
	}

	if( client_gs.gameState.clock_override != 0 ) {
		clocktime = client_gs.gameState.clock_override;
		if( clocktime < 0 ) {
			lua_pushnumber( L, clocktime );
			return 1;
		}
	}
	else {
		curtime = client_gs.gameState.match_state == MatchState_Warmup || client_gs.gameState.paused ? cg.frame.serverTime : cl.serverTime;
		duration = client_gs.gameState.match_duration;
		startTime = client_gs.gameState.match_state_start_time;

		// count downwards when having a duration
		if( duration ) {
			duration = Max2( curtime - startTime, zero ); // avoid negative results
			clocktime = startTime + duration - curtime;
		} else {
			clocktime = Max2( curtime - startTime, zero );
		}
	}
	lua_pushnumber( L, clocktime );
	return 1;
}

static int HUD_DrawBombIndicators( lua_State * L ) {
	CG_DrawBombHUD( luaL_checknumber( L, 1 ), luaL_checknumber( L, 2 ), luaL_checknumber( L, 3 ) );
	return 0;
}

static int HUD_DrawCrosshair( lua_State * L ) {
	CG_DrawCrosshair( frame_static.viewport_width / 2, frame_static.viewport_height / 2 );
	return 0;
}

static int HUD_DrawDamageNumbers( lua_State * L ) {
	CG_DrawDamageNumbers( luaL_checknumber( L, 1 ), luaL_checknumber( L, 2 ) );
	return 0;
}

static int HUD_DrawPointed( lua_State * L ) {
	CG_DrawPlayerNames( cls.fontNormalBold, luaL_checknumber( L, 1 ), CheckColor( L, 2 ) );
	return 0;
}

static int CG_HorizontalAlignForWidth( int x, Alignment alignment, int width ) {
	if( alignment.x == XAlignment_Left )
		return x;
	if( alignment.x == XAlignment_Center )
		return x - width / 2;
	return x - width;
}

static int CG_VerticalAlignForHeight( int y, Alignment alignment, int height ) {
	if( alignment.y == YAlignment_Ascent )
		return y;
	if( alignment.y == YAlignment_Descent )
		return y - height / 2;
	return y - height;
}

static int HUD_DrawObituaries( lua_State * L ) {
	int x = luaL_checknumber( L, 1 );
	int y = luaL_checknumber( L, 2 );
	int width = lua_tonumber( L, 3 );
	int height = lua_tonumber( L, 4 );
	unsigned int icon_size = lua_tonumber( L, 5 );
	float font_size = luaL_checknumber( L, 6 );
	Alignment alignment = CheckAlignment( L, 7 );

	const int icon_padding = 4;

	unsigned line_height = Max2( 1u, Max2( unsigned( font_size ), icon_size ) );
	int num_max = height / line_height;

	const Font * font = cls.fontNormalBold;

	int next = cg_obituaries_current + 1;
	if( next >= MAX_OBITUARIES ) {
		next = 0;
	}

	int num = 0;
	int i = next;
	do {
		if( cg_obituaries[i].type != OBITUARY_NONE && cls.monotonicTime - cg_obituaries[i].time <= Seconds( 5 ) ) {
			num++;
		}
		if( ++i >= MAX_OBITUARIES ) {
			i = 0;
		}
	} while( i != next );

	int skip;
	if( num > num_max ) {
		skip = num - num_max;
		num = num_max;
	} else {
		skip = 0;
	}

	y = CG_VerticalAlignForHeight( y, alignment, height );
	x = CG_HorizontalAlignForWidth( x, alignment, width );

	int xoffset = 0;
	int yoffset = 0;

	i = next;
	do {
		const obituary_t * obr = &cg_obituaries[i];
		if( ++i >= MAX_OBITUARIES ) {
			i = 0;
		}

		if( obr->type == OBITUARY_NONE || cls.monotonicTime - obr->time > Seconds( 5 ) ) {
			continue;
		}

		if( skip > 0 ) {
			skip--;
			continue;
		}

		const Material * pic = DamageTypeToIcon( obr->damage_type );

		float attacker_width = TextVisualBounds( font, font_size, MakeSpan( obr->attacker ) ).maxs.x;
		float victim_width = TextVisualBounds( font, font_size, MakeSpan( obr->victim ) ).maxs.x;

		int w = 0;
		if( obr->type != OBITUARY_ACCIDENT ) {
			w += attacker_width;
		}
		w += icon_padding;
		w += icon_size;
		w += icon_padding;
		w += victim_width;

		if( obr->wallbang ) {
			w += icon_size;
			w += icon_padding;
		}

		xoffset = width - w;

		int obituary_y = y + yoffset + ( line_height - font_size ) / 2;
		if( obr->type != OBITUARY_ACCIDENT ) {
			Vec4 color = CG_TeamColorVec4( obr->attacker_team );
			DrawText( font, font_size, obr->attacker, x + xoffset, obituary_y, color, black.vec4 );
			xoffset += attacker_width;
		}

		xoffset += icon_padding;

		Draw2DBox( x + xoffset, y + yoffset + ( line_height - icon_size ) / 2, icon_size, icon_size, pic, AttentionGettingColor() );
		xoffset += icon_size + icon_padding;

		if( obr->wallbang ) {
			Draw2DBox( x + xoffset, y + yoffset + ( line_height - icon_size ) / 2, icon_size, icon_size, FindMaterial( "loadout/wallbang_icon" ), AttentionGettingColor() );
			xoffset += icon_size + icon_padding;
		}

		Vec4 color = CG_TeamColorVec4( obr->victim_team );
		DrawText( font, font_size, obr->victim, x + xoffset, obituary_y, color, black.vec4 );

		yoffset += line_height;
	} while( i != next );

	if( cg.predictedPlayerState.health <= 0 && cg.predictedPlayerState.team != Team_None ) {
		if( self_obituary.entropy != 0 ) {
			float h = 128.0f;
			float yy = frame_static.viewport.y * 0.5f - h * 0.5f;

			float t = ToSeconds( cls.monotonicTime - self_obituary.time );

			Draw2DBox( 0, yy, frame_static.viewport.x, h, cls.white_material, Vec4( 0, 0, 0, Min2( 0.5f, t ) ) );

			if( t >= 1.0f ) {
				RNG rng = NewRNG( self_obituary.entropy, 0 );

				TempAllocator temp = cls.frame_arena.temp();
				Span< const char > obituary = MakeObituary( &temp, &rng, self_obituary.type, self_obituary.damage_type );

				float size = Lerp( h * 0.5f, Unlerp01( 1.0f, t, 10.0f ), h * 5.0f );
				Vec4 color = AttentionGettingColor();
				color.w = Unlerp01( 0.5f, t, 1.0f );
				DrawText( cls.fontNormal, size, obituary, Alignment_CenterMiddle, frame_static.viewport.x * 0.5f, frame_static.viewport.y * 0.5f, color );
			}
		}
	}

	return 0;
}

static Optional< float > CheckFloat( lua_State * L, int idx, const char * key, bool dont_error = false ) {
	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	if( lua_type( L, -1 ) == LUA_TNUMBER ) {
		return lua_tonumber( L, -1 );
	}

	if( lua_type( L, -1 ) == LUA_TSTRING ) {
		Span< const char > str = LuaToSpan( L, -1 );
		if( EndsWith( str, "vw" ) || EndsWith( str, "vh" ) ) {
			float v;
			if( !TrySpanToFloat( str.slice( 0, str.n - 2 ), &v ) ) {
				luaL_error( L, "%s doesn't parse as a vw/vh: %s", key, str.ptr );
			}
			return v * 0.01f * checked_cast< float >( EndsWith( str, "vw" ) ? frame_static.viewport_width : frame_static.viewport_height );
		}
	}

	if( !dont_error ) {
		luaL_error( L, "%s isn't a number", key );
	}

	return NONE;
}

static Optional< u16 > CheckU16( lua_State * L, int idx, const char * key ) {
	Optional< float > x = CheckFloat( L, idx, key );
	if( !x.exists )
		return NONE;
	if( x.value < 0.0f || x.value > U16_MAX ) {
		luaL_error( L, "%s doesn't fit in a u16: %f", key, x.value );
	}
	return u16( x.value );
}

static Optional< Clay_SizingAxis > CheckClaySize( lua_State * L, int idx, const char * key ) {
	Optional< float > x = CheckFloat( L, idx, key, true );
	if( x.exists ) {
		return CLAY_SIZING_FIXED( x.value );
	}

	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	Span< const char > str = LuaToSpan( L, -1 );
	if( str == "fit" ) {
		return CLAY_SIZING_FIT( 0.0f );
	}

	if( str == "grow" ) {
		return CLAY_SIZING_GROW( 0.0f );
	}

	if( EndsWith( str, "%" ) ) {
		float percent;
		if( !TrySpanToFloat( str.slice( 0, str.n - 1 ), &percent ) ) {
			luaL_error( L, "size doesn't parse as a percent: %s", str.ptr );
		}
		return CLAY_SIZING_PERCENT( 0.01f * percent );
	}

	luaL_error( L, "bad size: %s = %s", key, str.ptr );
	return NONE;
}

static Clay_Padding CheckClayPadding( lua_State * L, int idx ) {
	Clay_Padding padding = { };

	Optional< u16 > all_padding = CheckU16( L, idx, "padding" );
	if( all_padding.exists ) {
		padding.left = all_padding.value;
		padding.right = all_padding.value;
		padding.top = all_padding.value;
		padding.bottom = all_padding.value;
	}

	padding.left   = Default( CheckU16( L, idx, "padding_x" ), padding.left );
	padding.right  = Default( CheckU16( L, idx, "padding_x" ), padding.right );
	padding.top    = Default( CheckU16( L, idx, "padding_y" ), padding.top );
	padding.bottom = Default( CheckU16( L, idx, "padding_y" ), padding.bottom );

	padding.left   = Default( CheckU16( L, idx, "padding_left"   ), padding.left );
	padding.right  = Default( CheckU16( L, idx, "padding_right"  ), padding.right );
	padding.top    = Default( CheckU16( L, idx, "padding_top"    ), padding.top );
	padding.bottom = Default( CheckU16( L, idx, "padding_bottom" ), padding.bottom );

	return padding;
}

static Optional< Clay_LayoutDirection > CheckClayLayoutDirection( lua_State * L, int idx, const char * key ) {
	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	constexpr Clay_LayoutDirection layout_directions[] = {
		CLAY_LEFT_TO_RIGHT,
		CLAY_TOP_TO_BOTTOM,
	};

	constexpr const char * names[] = {
		"horizontal",
		"vertical",

		NULL
	};

	return layout_directions[ luaL_checkoption( L, idx, names[ 0 ], names ) ];
}

static Optional< Clay_ChildAlignment > CheckClayChildAlignment( lua_State * L, int idx, const char * key ) {
	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	constexpr Clay_ChildAlignment alignments[] = {
		{ CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
		{ CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		{ CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP },

		{ CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
		{ CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		{ CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },

		{ CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM },
		{ CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM },
		{ CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_BOTTOM },
	};

	constexpr const char * names[] = {
		"top-left", "top-center", "top-right",
		"middle-left", "middle-center", "middle-right",
		"bottom-left", "bottom-center", "bottom-right",

		NULL
	};

	return alignments[ luaL_checkoption( L, -1, names[ 0 ], names ) ];
}

static Optional< Vec4 > CheckOptionalColor( lua_State * L, int idx, const char * key ) {
	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;
	return CheckColor( L, -1 );
}

static Optional< Clay_Color > GetOptionalClayColor( lua_State * L, int idx, const char * key ) {
	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	RGBA8 srgb = LinearTosRGB( CheckColor( L, -1 ) );
	return Clay_Color { float( srgb.r ), float( srgb.g ), float( srgb.b ), float( srgb.a ) };
}

static Clay_CornerRadius GetClayBorderRadius( lua_State * L, int idx ) {
	Optional< float > radius = CheckFloat( L, idx, "border_radius" );
	float r = Default( radius, 0.0f );
	return Clay_CornerRadius { r, r, r, r };
}

static Optional< u16 > CheckClayFont( lua_State * L, int idx ) {
	lua_getfield( L, idx, "font" );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	constexpr const char * names[ ClayFont_Count + 1 ] = { "regular", "bold", "italic", "bold-italic", NULL };

	return luaL_checkoption( L, -1, names[ 0 ], names );
}

struct ClayTextAndConfig {
	Clay_String text;
	Clay_TextElementConfig config;
	Optional< Vec4 > border_color;

	struct FittedText {
		XAlignment x_alignment;
		Clay_Padding padding;
	};

	Optional< FittedText > fitted_text;
};

static Optional< ClayTextAndConfig::FittedText > CheckClayFittedText( lua_State * L, int idx ) {
	lua_getfield( L, idx, "fit" );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	constexpr XAlignment alignments[] = { XAlignment_Left, XAlignment_Center, XAlignment_Right };
	constexpr const char * names[] = { "left", "center", "right", NULL };

	return ClayTextAndConfig::FittedText {
		.x_alignment = alignments[ luaL_checkoption( L, -1, names[ 0 ], names ) ],
		.padding = CheckClayPadding( L, idx - 1 ),
	};
}

static Optional< ClayTextAndConfig > GetOptionalClayTextConfig( lua_State * L, int idx ) {
	if( lua_getfield( L, idx, "text" ) == LUA_TNIL ) {
		lua_pop( L, 1 );
		return NONE;
	}

	Clay_String text = AllocateClayString( LuaToSpan( L, -1 ) );
	lua_pop( L, 1 );

	// default 1vh
	// TODO NOMERGE do u16s here need to be rounded or is floor ok? probably in checku16 too
	u16 size = Default( CheckU16( L, -1, "font_size" ), u16( frame_static.viewport_height * 0.01f ) );

	return ClayTextAndConfig {
		.text = text,
		.config = Clay_TextElementConfig {
			.textColor = Default( GetOptionalClayColor( L, -1, "color" ), { 255, 255, 255, 255 } ),
			.fontId = Default( CheckClayFont( L, -1 ), 0_u16 ),
			.fontSize = u16( size ),
			.letterSpacing = 0,
			.lineHeight = u16( Default( CheckFloat( L, -1, "line_height" ), 1.0f ) * size ),
			.wrapMode = CLAY_TEXT_WRAP_NONE,
			.hashStringContents = true,
		},
		.border_color = CheckOptionalColor( L, -1, "text_border" ),
		.fitted_text = CheckClayFittedText( L, idx ),
	};
}

static Clay_LayoutConfig GetClayLayoutConfig( lua_State * L, int idx ) {
	return Clay_LayoutConfig {
		.sizing = {
			.width = Default( CheckClaySize( L, idx, "width" ), CLAY_LAYOUT_DEFAULT.sizing.width ),
			.height = Default( CheckClaySize( L, idx, "height" ), CLAY_LAYOUT_DEFAULT.sizing.height ),
		},
		.padding = CheckClayPadding( L, idx ),
		.childGap = Default( CheckU16( L, idx, "gap" ), 0_u16 ),
		.childAlignment = Default( CheckClayChildAlignment( L, idx, "alignment" ), CLAY_LAYOUT_DEFAULT.childAlignment ),
		.layoutDirection = Default( CheckClayLayoutDirection( L, idx, "flow" ), CLAY_LEFT_TO_RIGHT ),
	};
}

static Clay_BorderElementConfig GetClayBorderConfig( lua_State * L, int idx ) {
	Clay_BorderElementConfig border = { };

	Optional< u16 > all_width = CheckU16( L, idx, "border" );
	if( all_width.exists ) {
		border.width.left = all_width.value;
		border.width.right = all_width.value;
		border.width.top = all_width.value;
		border.width.bottom = all_width.value;
	}

	border.color = Default( GetOptionalClayColor( L, idx, "border_color" ), { } );

	border.width.left   = Default( CheckU16( L, idx, "border_left"   ), u16( border.width.left ) );
	border.width.right  = Default( CheckU16( L, idx, "border_right"  ), u16( border.width.right ) );
	border.width.top    = Default( CheckU16( L, idx, "border_top"    ), u16( border.width.top ) );
	border.width.bottom = Default( CheckU16( L, idx, "border_bottom" ), u16( border.width.bottom ) );

	return border;
}

static Clay_ImageElementConfig GetClayImageConfig( lua_State * L, int idx, Vec4 * tint ) {
	lua_getfield( L, idx, "image" );
	StringHash name = CheckHash( L, -1 );
	lua_pop( L, 1 );

	if( name == EMPTY_HASH )
		return { };

	*tint = Default( CheckOptionalColor( L, -1, "color" ), white.vec4 );

	return Clay_ImageElementConfig {
		.imageData = bit_cast< void * >( name.hash ),
		.sourceDimensions = { 1, 1 }, // TODO
	};
}

static Optional< Clay_FloatingAttachPoints > CheckClayFloatAttachment( lua_State * L, int idx ) {
	lua_getfield( L, idx, "float" );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	Span< const char > element_parent = LuaToSpan( L, -1 );
	Span< const char > cursor = element_parent;
	Span< const char > element = ParseToken( &cursor, Parse_StopOnNewLine );
	Span< const char > parent = ParseToken( &cursor, Parse_StopOnNewLine );

	constexpr struct {
		Clay_FloatingAttachPointType type;
		Span< const char > name;
	} attachments[] = {
		{ CLAY_ATTACH_POINT_LEFT_TOP, "top-left" },
		{ CLAY_ATTACH_POINT_CENTER_TOP, "top-center" },
		{ CLAY_ATTACH_POINT_RIGHT_TOP, "top-right" },

		{ CLAY_ATTACH_POINT_LEFT_CENTER, "middle-left" },
		{ CLAY_ATTACH_POINT_CENTER_CENTER, "middle-center" },
		{ CLAY_ATTACH_POINT_RIGHT_CENTER, "middle-right" },

		{ CLAY_ATTACH_POINT_LEFT_BOTTOM, "bottom-left" },
		{ CLAY_ATTACH_POINT_CENTER_BOTTOM, "bottom-center" },
		{ CLAY_ATTACH_POINT_RIGHT_BOTTOM, "bottom-right" },
	};

	Optional< Clay_FloatingAttachPointType > clay_element = NONE;
	Optional< Clay_FloatingAttachPointType > clay_parent = NONE;
	for( auto [ type, name ] : attachments ) {
		if( StrEqual( name, element ) ) {
			clay_element = type;
		}
		if( StrEqual( name, parent ) ) {
			clay_parent = type;
		}
	}

	if( !clay_element.exists || !clay_parent.exists ) {
		luaL_error( L, "bad float config: %s", element_parent.ptr );
	}

	return Clay_FloatingAttachPoints {
		.element = clay_element.value,
		.parent = clay_parent.value,
	};
}

static Clay_FloatingElementConfig GetClayFloatConfig( lua_State * L, int idx ) {
	Optional< Clay_FloatingAttachPoints > attachment = CheckClayFloatAttachment( L, idx );
	if( !attachment.exists )
		return { };

	return Clay_FloatingElementConfig {
		.offset = {
			.x = Default( CheckFloat( L, -1, "x_offset" ), 0.0f ),
			.y = Default( CheckFloat( L, -1, "y_offset" ), 0.0f ),
		},
		.zIndex = checked_cast< s16 >( Default( CheckU16( L, -1, "z_index" ), 0_u16 ) ),
		.attachPoints = attachment.value,
		.attachTo = CLAY_ATTACH_TO_PARENT,
	};
}

static Optional< ClayCustomElementConfig > GetOptionalClayCallbackConfig( lua_State * L, int idx ) {
	lua_getfield( L, idx, "callback" );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;
	lua_getfield( L, idx - 1, "callback_arg" );
	defer { lua_pop( L, 1 ); };
	return ClayCustomElementConfig {
		.type = ClayCustomElementType_Lua,
		.lua = {
			.callback_ref = lua_tointeger( L, -2 ),
			.arg_ref = lua_tointeger( L, -1 ),
		},
	};
}

static void DrawClayNodeRecursive( lua_State * L ) {
	if( !lua_toboolean( L, -1 ) )
		return;
	luaL_checktype( L, -1, LUA_TTABLE );

	Optional< ClayCustomElementConfig > custom = NONE;
	Optional< ClayTextAndConfig > text_config = GetOptionalClayTextConfig( L, -1 );
	if( text_config.exists ) {
		if( text_config.value.fitted_text.exists ) {
			custom = ClayCustomElementConfig {
				.type = ClayCustomElementType_FittedText,
				.fitted_text = {
					.text = text_config.value.text,
					.config = text_config.value.config,
					.alignment = text_config.value.fitted_text.value.x_alignment,
					.padding = text_config.value.fitted_text.value.padding,
					.border_color = text_config.value.border_color,
				},
			};
		}
	}
	else {
		custom = GetOptionalClayCallbackConfig( L, -1 );
	}

	Vec4 image_tint;
	Clay_ImageElementConfig image_config = GetClayImageConfig( L, -1, &image_tint );

	Clay__OpenElement();
	Clay__ConfigureOpenElement( Clay_ElementDeclaration {
		.id = { clay_element_counter++ },
		.layout = GetClayLayoutConfig( L, -1 ),
		.backgroundColor = Default( GetOptionalClayColor( L, -1, "background" ), { } ),
		.cornerRadius = GetClayBorderRadius( L, -1 ),
		.image = image_config,
		.floating = GetClayFloatConfig( L, -1 ),
		.custom = Clay_CustomElementConfig {
			.customData = custom.exists ? Clone( ClayAllocator(), custom.value ) : NULL,
		},
		.border = GetClayBorderConfig( L, -1 ),
		.userData = Clone( ClayAllocator(), image_tint ),
	} );

	if( text_config.exists && !text_config.value.fitted_text.exists ) {
		text_config.value.config.userData = Clone( ClayAllocator(), text_config.value.border_color );
		Clay__OpenTextElement( text_config.value.text, Clay__StoreTextElementConfig( text_config.value.config ) );
	}
	else {
		if( lua_getfield( L, -1, "children" ) != LUA_TNONE ) {
			int n = lua_objlen( L, -1 );
			for( int i = 0; i < n; i++ ) {
				lua_pushnumber( L, i + 1 );
				lua_gettable( L, -2 );
				DrawClayNodeRecursive( L );
				lua_pop( L, 1 );
			}
		}
		lua_pop( L, 1 );
	}

	Clay__CloseElement();
}

static int LuauRender( lua_State * L ) {
	TracyZoneScoped;
	DrawClayNodeRecursive( L );
	return 0;
}

static void CopyLuaTable( lua_State * L, int idx ) {
	lua_newtable( L );
	lua_pushnil( L );
	while( lua_next( L , idx ) != 0 ) {
		lua_pushvalue( L, -2 );
		lua_insert( L, -2 );
		lua_settable( L, -4 );
	}
}

static int LuauNode( lua_State * L ) {
	luaL_checktype( L, 1, LUA_TTABLE );
	// luaL_argcheck( L, lua_type( L, 2 ) == LUA_TNONE || lua_type( L, 2 ) == LUA_TTABLE, 2, "children must be a table or none" );

	CopyLuaTable( L, 1 );

	switch( lua_type( L, 2 ) ) {
		case LUA_TTABLE:
			lua_pushvalue( L, 2 );
			lua_setfield( L, -2, "children" );
			break;

		case LUA_TSTRING:
			lua_pushvalue( L, 2 );
			lua_setfield( L, -2, "text" );
			break;

		case LUA_TLIGHTUSERDATA:
			lua_pushvalue( L, 2 );
			lua_setfield( L, -2, "image" );
			break;

		case LUA_TFUNCTION:
			int callback_ref = lua_ref( L, 2 );
			int arg_ref = lua_ref( L, 3 );
			lua_pushinteger( L, callback_ref );
			lua_setfield( L, -2, "callback" );
			lua_pushinteger( L, arg_ref );
			lua_setfield( L, -2, "callback_arg" );
			break;
	}

	return 1;
}

bool CG_ScoreboardShown() {
	if( client_gs.gameState.match_state > MatchState_Playing ) {
		return true;
	}

	return cg.showScoreboard;
}

void CG_InitHUD() {
	TracyZoneScoped;

	hud_L = NULL;

	Span< const char > src = AssetString( StringHash( "hud/hud.lua" ) );
	size_t bytecode_size;
	char * bytecode = luau_compile( src.ptr, src.n, NULL, &bytecode_size );
	defer { free( bytecode ); };
	if( bytecode == NULL ) {
		Fatal( "luau_compile" );
	}

	hud_L = luaL_newstate();
	if( hud_L == NULL ) {
		Fatal( "luaL_newstate" );
	}

	constexpr luaL_Reg cdlib[] = {
		{ "print", LuauPrint },
		{ "asset", LuauAsset },
		{ "box", LuauDraw2DBox },
		{ "boxuv", LuauDraw2DBoxUV },
		{ "text", LuauDrawText },

		{ "getBind", LuauGetBind },
		{ "getTeamColor", LuauGetTeamColor },
		{ "allyColor", LuauAllyColor },
		{ "enemyColor", LuauEnemyColor },
		{ "attentionGettingColor", LuauAttentionGettingColor },
		{ "plantableColor", LuauPlantableColor },
		{ "attentionGettingRed", LuauAttentionGettingRed },
		{ "getPlayerName", LuauGetPlayerName },

		{ "getWeaponIcon", LuauGetWeaponIcon },
		{ "getGadgetIcon", LuauGetGadgetIcon },
		{ "getPerkIcon", LuauGetPerkIcon },

		{ "getWeaponReloadTime", LuauGetWeaponReloadTime },
		{ "getWeaponStagedReload", LuauGetWeaponStagedReload },

		{ "getGadgetMaxAmmo", LuauGetGadgetAmmo },

		{ "getClockTime", LuauGetClockTime },

		{ "drawBombIndicators", HUD_DrawBombIndicators },
		{ "drawCrosshair", HUD_DrawCrosshair },
		{ "drawObituaries", HUD_DrawObituaries },
		{ "drawPointed", HUD_DrawPointed },
		{ "drawDamageNumbers", HUD_DrawDamageNumbers },

		{ "render", LuauRender },
		{ "node", LuauNode },

		{ NULL, NULL }
	};

	for( const auto & kv : numeric_constants ) {
		lua_pushnumber( hud_L, kv.value );
		lua_setfield( hud_L, LUA_GLOBALSINDEX, kv.name );
	}

	{
		lua_newtable( hud_L );

		for( const auto & kv : asset_constants ) {
			PushLuaAsset( hud_L, kv.value );
			lua_setfield( hud_L, -2, kv.name );
		}

		lua_setfield( hud_L, LUA_GLOBALSINDEX, "assets" );
	}

	luaL_openlibs( hud_L ); // TODO: don't open all libs?
	luaL_register( hud_L, "cd", cdlib );
	lua_pop( hud_L, 1 );

	lua_pushcfunction( hud_L, LuauRGBA8, "RGBA8" );
	lua_setfield( hud_L, LUA_GLOBALSINDEX, "RGBA8" );

	lua_pushcfunction( hud_L, LuauRGBALinear, "RGBALinear" );
	lua_setfield( hud_L, LUA_GLOBALSINDEX, "RGBALinear" );

	lua_pushcfunction( hud_L, LuausRGBToLinear, "sRGBToLinear" );
	lua_setfield( hud_L, LUA_GLOBALSINDEX, "sRGBToLinear" );

	luaL_sandbox( hud_L );

	lua_getglobal( hud_L, "debug" );
	lua_getfield( hud_L, -1, "traceback" );
	lua_remove( hud_L, -2 );

	int ok = luau_load( hud_L, "hud.lua", bytecode, bytecode_size, 0 );
	if( ok == 0 ) {
		if( !CallWithStackTrace( hud_L, 0, 1 ) || lua_type( hud_L, -1 ) != LUA_TFUNCTION ) {
			Com_Printf( S_COLOR_RED "hud.lua must return a function\n" );
			lua_close( hud_L );
			hud_L = NULL;
		}
	}
	else {
		Com_Printf( S_COLOR_RED "Luau compilation error: %s\n", lua_tostring( hud_L, -1 ) );
		lua_close( hud_L );
		hud_L = NULL;
	}
}

void CG_ShutdownHUD() {
	if( hud_L != NULL ) {
		lua_close( hud_L );
	}
}

void CG_DrawHUD() {
	TracyZoneScoped;

	bool hotload = false;
	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( StartsWith( path, "hud/" ) && EndsWith( path, ".lua" ) ) {
			hotload = true;
			break;
		}
	}

	if( hotload ) {
		CG_ShutdownHUD();
		CG_InitHUD();
	}

	if( hud_L == NULL )
		return;

	lua_pushvalue( hud_L, -1 );
	lua_newtable( hud_L );

	lua_pushnumber( hud_L, cg.predictedPlayerState.POVnum );
	lua_setfield( hud_L, -2, "current_player" );

	lua_pushboolean( hud_L, cg.predictedPlayerState.ready );
	lua_setfield( hud_L, -2, "ready" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.health );
	lua_setfield( hud_L, -2, "health" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.max_health );
	lua_setfield( hud_L, -2, "max_health" );

	lua_pushboolean( hud_L, cg.predictedPlayerState.zoom_time > 0 );
	lua_setfield( hud_L, -2, "zooming" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.weapon );
	lua_setfield( hud_L, -2, "weapon" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.weapon_state );
	lua_setfield( hud_L, -2, "weapon_state" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.weapon_state_time );
	lua_setfield( hud_L, -2, "weapon_state_time" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.gadget );
	lua_setfield( hud_L, -2, "gadget" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.gadget_ammo );
	lua_setfield( hud_L, -2, "gadget_ammo" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.perk );
	lua_setfield( hud_L, -2, "perk" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.pmove.stamina );
	lua_setfield( hud_L, -2, "stamina" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.pmove.stamina_stored );
	lua_setfield( hud_L, -2, "stamina_stored" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.pmove.stamina_state );
	lua_setfield( hud_L, -2, "stamina_state" );

	lua_pushboolean( hud_L, cg.predictedPlayerState.pmove.pm_type == PM_SPECTATOR );
	lua_setfield( hud_L, -2, "ghost" );

	if( cg.predictedPlayerState.team != Team_None ) {
		lua_pushnumber( hud_L, cg.predictedPlayerState.team );
		lua_setfield( hud_L, -2, "team" );
	}

	if( cg.predictedPlayerState.real_team != Team_None ) {
		lua_pushnumber( hud_L, cg.predictedPlayerState.real_team );
		lua_setfield( hud_L, -2, "real_team" );
	}

	lua_pushboolean( hud_L, cg.predictedPlayerState.carrying_bomb );
	lua_setfield( hud_L, -2, "is_carrier" );

	lua_pushboolean( hud_L, cg.predictedPlayerState.can_plant );
	lua_setfield( hud_L, -2, "can_plant" );

	lua_pushboolean( hud_L, cg.predictedPlayerState.can_change_loadout );
	lua_setfield( hud_L, -2, "can_change_loadout" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.progress );
	lua_setfield( hud_L, -2, "bomb_progress" );

	lua_pushnumber( hud_L, cg.predictedPlayerState.progress_type );
	lua_setfield( hud_L, -2, "bomb_progress_type" );

	lua_pushnumber( hud_L, client_gs.gameState.gametype );
	lua_setfield( hud_L, -2, "gametype" );

	lua_pushnumber( hud_L, client_gs.gameState.match_state );
	lua_setfield( hud_L, -2, "match_state" );

	lua_pushnumber( hud_L, client_gs.gameState.scorelimit );
	lua_setfield( hud_L, -2, "scorelimit" );

	lua_pushnumber( hud_L, client_gs.gameState.bomb.attacking_team );
	lua_setfield( hud_L, -2, "attacking_team" );

	lua_pushnumber( hud_L, client_gs.gameState.round_state );
	lua_setfield( hud_L, -2, "round_state" );

	lua_pushnumber( hud_L, client_gs.gameState.round_type );
	lua_setfield( hud_L, -2, "round_type" );

	lua_pushnumber( hud_L, client_gs.gameState.teams[ Team_One ].score );
	lua_setfield( hud_L, -2, "scoreAlpha" );

	lua_pushnumber( hud_L, client_gs.gameState.bomb.alpha_players_alive );
	lua_setfield( hud_L, -2, "aliveAlpha" );

	lua_pushnumber( hud_L, client_gs.gameState.bomb.alpha_players_total );
	lua_setfield( hud_L, -2, "totalAlpha" );

	lua_pushnumber( hud_L, client_gs.gameState.teams[ Team_Two ].score );
	lua_setfield( hud_L, -2, "scoreBeta" );

	lua_pushnumber( hud_L, client_gs.gameState.bomb.beta_players_alive );
	lua_setfield( hud_L, -2, "aliveBeta" );

	lua_pushnumber( hud_L, client_gs.gameState.bomb.beta_players_total );
	lua_setfield( hud_L, -2, "totalBeta" );

	if( cg.predictedPlayerState.POVnum != cgs.playerNum + 1 ) {
		lua_pushnumber( hud_L, cg.predictedPlayerState.POVnum );
		lua_setfield( hud_L, -2, "chasing" );
	}

	lua_pushstring( hud_L, client_gs.gameState.callvote );
	lua_setfield( hud_L, -2, "vote" );

	lua_pushnumber( hud_L, client_gs.gameState.callvote_required_votes );
	lua_setfield( hud_L, -2, "votes_required" );

	lua_pushnumber( hud_L, client_gs.gameState.callvote_yes_votes );
	lua_setfield( hud_L, -2, "votes_total" );

	lua_pushboolean( hud_L, cg.predictedPlayerState.voted );
	lua_setfield( hud_L, -2, "has_voted" );

	lua_pushboolean( hud_L, CG_IsLagging() );
	lua_setfield( hud_L, -2, "lagging" );

	lua_pushboolean( hud_L, Cvar_Bool( "cg_showFPS" ) );
	lua_setfield( hud_L, -2, "show_fps" );

	lua_pushboolean( hud_L, Cvar_Bool( "cg_showHotkeys" ) );
	lua_setfield( hud_L, -2, "show_hotkeys" );

	lua_pushnumber( hud_L, CG_GetFPS() );
	lua_setfield( hud_L, -2, "fps" );

	lua_pushboolean( hud_L, Cvar_Bool( "cg_showSpeed" ) );
	lua_setfield( hud_L, -2, "show_speed" );

	lua_pushnumber( hud_L, CG_GetSpeed() );
	lua_setfield( hud_L, -2, "speed" );

	lua_pushnumber( hud_L, frame_static.viewport_width );
	lua_setfield( hud_L, -2, "viewport_width" );

	lua_pushnumber( hud_L, frame_static.viewport_height );
	lua_setfield( hud_L, -2, "viewport_height" );

	lua_createtable( hud_L, Weapon_Count - 1, 0 );
	for( size_t i = 0; i < ARRAY_COUNT( cg.predictedPlayerState.weapons ); i++ ) {
		const WeaponDef * def = GS_GetWeaponDef( cg.predictedPlayerState.weapons[ i ].weapon );

		if( cg.predictedPlayerState.weapons[ i ].weapon == Weapon_None )
			continue;

		lua_pushnumber( hud_L, i + 1 ); // arrays start at 1 in lua
		lua_createtable( hud_L, 0, 4 );

		lua_pushnumber( hud_L, cg.predictedPlayerState.weapons[ i ].weapon );
		lua_setfield( hud_L, -2, "weapon" );
		LuaPushSpan( hud_L, def->name );
		lua_setfield( hud_L, -2, "name" );
		lua_pushnumber( hud_L, cg.predictedPlayerState.weapons[ i ].ammo );
		lua_setfield( hud_L, -2, "ammo" );
		lua_pushnumber( hud_L, def->clip_size );
		lua_setfield( hud_L, -2, "max_ammo" );

		lua_settable( hud_L, -3 );

	}
	lua_setfield( hud_L, -2, "weapons" );

	lua_createtable( hud_L, Team_Count, 0 );
	for( int i = Team_One; i < Team_Count; i++ ) {
		lua_pushnumber( hud_L, i );
		lua_createtable( hud_L, 0, 3 );

		const SyncTeamState & team = client_gs.gameState.teams[ i ];

		lua_pushnumber( hud_L, team.score );
		lua_setfield( hud_L, -2, "score" );

		lua_pushnumber( hud_L, team.num_players );
		lua_setfield( hud_L, -2, "num_players" );

		lua_createtable( hud_L, 0, team.num_players );
		for( u8 p = 0; p < team.num_players; p++ ) {
			lua_pushnumber( hud_L, p + 1 );

			lua_createtable( hud_L, 0, 7 );

			const SyncScoreboardPlayer & player = client_gs.gameState.players[ team.player_indices[ p ] - 1 ];
			lua_pushnumber( hud_L, team.player_indices[ p ] );
			lua_setfield( hud_L, -2, "id" );
			lua_pushstring( hud_L, player.name );
			lua_setfield( hud_L, -2, "name" );
			lua_pushnumber( hud_L, player.ping );
			lua_setfield( hud_L, -2, "ping" );
			lua_pushnumber( hud_L, player.score );
			lua_setfield( hud_L, -2, "score" );
			lua_pushnumber( hud_L, player.kills );
			lua_setfield( hud_L, -2, "kills" );
			lua_pushboolean( hud_L, player.ready );
			lua_setfield( hud_L, -2, "ready" );
			lua_pushboolean( hud_L, player.carrier );
			lua_setfield( hud_L, -2, "carrier" );
			lua_pushboolean( hud_L, player.alive );
			lua_setfield( hud_L, -2, "alive" );

			lua_settable( hud_L, -3 );
		}
		lua_setfield( hud_L, -2, "players" );

		lua_settable( hud_L, -3 );
	}
	lua_setfield( hud_L, -2, "teams" );

	lua_pushnumber( hud_L, client_gs.gameState.teams[ Team_None ].num_players );
	lua_setfield( hud_L, -2, "spectating" );

	lua_pushboolean( hud_L, CG_ScoreboardShown() );
	lua_setfield( hud_L, -2, "scoreboard" );

	bool hud_lua_ran_ok;
	clay_element_counter = 1;
	{
		TracyZoneScopedN( "Luau" );
		hud_lua_ran_ok = CallWithStackTrace( hud_L, 1, 0 );
	}

	// TODO: don't run clay layout if hud.lua failed because it might have left clay in a bad state
	// TODO: put custom element callbacks in their own table and delete the table here so they don't leak

	Assert( lua_gettop( hud_L ) == 2 );
}

void CG_DrawClayLuaHUDElement( const Clay_BoundingBox & bounds, const ClayCustomElementConfig * config ) {
	lua_getref( hud_L, config->lua.callback_ref );
	lua_pushnumber( hud_L, bounds.x );
	lua_pushnumber( hud_L, bounds.y );
	lua_pushnumber( hud_L, bounds.width );
	lua_pushnumber( hud_L, bounds.height );
	lua_getref( hud_L, config->lua.arg_ref );
	CallWithStackTrace( hud_L, 5, 0 );
	lua_unref( hud_L, config->lua.callback_ref );
	lua_unref( hud_L, config->lua.arg_ref );
}
