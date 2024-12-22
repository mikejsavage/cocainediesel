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
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
#include "cgame/cg_local.h"

#include "clay/clay.h"

#include "imgui/imgui.h"

#include "luau/lua.h"
#include "luau/lualib.h"
#include "luau/luacode.h"

static const Vec4 light_gray = sRGBToLinear( RGBA8( 96, 96, 96, 255 ) );

static lua_State * hud_L;

static Clay_Arena clay_arena;

static bool show_debugger;

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

static const char * normal_obituaries[] = {
#include "obituaries.h"
};

static const char * prefixes[] = {
#include "prefixes.h"
};

static const char * suicide_prefixes[] = {
	"AUTO",
	"SELF",
	"SHANKS",
	"SOLO",
	"TIMMA",
};

static const char * void_obituaries[] = {
	"ATE",
	"HOLED",
	"RECLAIMED",
	"TOOK",
};

static const char * spike_obituaries[] = {
	"DISEMBOWELED",
	"GORED",
	"IMPALED",
	"PERFORATED",
	"POKED",
	"SKEWERED",
	"SLASHED",
};

static const char * conjunctions[] = {
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

static const char * RandomPrefix( RNG * rng, float p ) {
	if( !Probability( rng, p ) )
		return "";
	return RandomElement( rng, prefixes );
}

static char * Uppercase( Allocator * a, const char * str ) {
	char * upper = AllocMany< char >( a, strlen( str ) + 1 );
	for( size_t i = 0; i < strlen( str ); i++ ) {
		upper[ i ] = ToUpperASCII( str[ i ] );
	}
	upper[ strlen( str ) ] = '\0';
	return upper;
}

static char * MakeObituary( Allocator * a, RNG * rng, int type, DamageType damage_type ) {
	Span< const char * > obituaries = StaticSpan( normal_obituaries );
	if( damage_type == WorldDamage_Void ) {
		obituaries = StaticSpan( void_obituaries );
	}
	else if( damage_type == WorldDamage_Spike ) {
		obituaries = StaticSpan( spike_obituaries );
	}

	const char * prefix1 = "";
	if( type == OBITUARY_SUICIDE ) {
		prefix1 = RandomElement( rng, suicide_prefixes );
	}

	// do these in order because arg evaluation order is undefined
	const char * prefix2 = RandomPrefix( rng, 0.05f );
	const char * prefix3 = RandomPrefix( rng, 0.5f );

	return ( *a )( "{}{}{}{}", prefix1, prefix2, prefix3, obituaries[ RandomUniform( rng, 0, obituaries.n ) ] );
}

void CG_SC_Obituary( const Tokenized & args ) {
	int victimNum = SpanToInt( args.tokens[ 1 ], 0 );
	int attackerNum = SpanToInt( args.tokens[ 2 ], 0 );
	int topAssistorNum = SpanToInt( args.tokens[ 3 ], 0 );
	DamageType damage_type;
	damage_type.encoded = SpanToInt( args.tokens[ 4 ], 0 );
	bool wallbang = SpanToInt( args.tokens[ 5 ], 0 ) == 1;
	u64 entropy = SpanToU64( args.tokens[ 6 ], 0 );

	const char * victim = PlayerName( victimNum - 1 );
	const char * attacker = attackerNum == 0 ? NULL : PlayerName( attackerNum - 1 );
	const char * assistor = topAssistorNum == -1 ? NULL : PlayerName( topAssistorNum - 1 );

	cg_obituaries_current = ( cg_obituaries_current + 1 ) % ARRAY_COUNT( cg_obituaries );
	obituary_t * current = &cg_obituaries[ cg_obituaries_current ];
	current->type = OBITUARY_NORMAL;
	current->time = cls.monotonicTime;
	current->damage_type = damage_type;
	current->wallbang = wallbang;

	if( victim != NULL ) {
		SafeStrCpy( current->victim, victim, sizeof( current->victim ) );
		current->victim_team = cg_entities[ victimNum ].current.team;
	}
	if( attacker != NULL ) {
		SafeStrCpy( current->attacker, attacker, sizeof( current->attacker ) );
		current->attacker_team = cg_entities[ attackerNum ].current.team;
	}

	Team assistor_team = assistor == NULL ? Team_None : cg_entities[ topAssistorNum ].current.team;

	if( cg.view.playerPrediction && ISVIEWERENTITY( victimNum ) ) {
		self_obituary.entropy = 0;
	}

	TempAllocator temp = cls.frame_arena.temp();
	RNG rng = NewRNG( entropy, 0 );

	const char * attacker_name = attacker == NULL ? NULL : temp( "{}{}", ImGuiColorToken( CG_TeamColor( current->attacker_team ) ), Uppercase( &temp, attacker ) );
	const char * victim_name = temp( "{}{}", ImGuiColorToken( CG_TeamColor( current->victim_team ) ), Uppercase( &temp, victim ) );
	const char * assistor_name = assistor == NULL ? NULL : temp( "{}{}", ImGuiColorToken( CG_TeamColor( assistor_team ) ), Uppercase( &temp, assistor ) );

	if( attackerNum == 0 ) {
		current->type = OBITUARY_ACCIDENT;

		if( damage_type == WorldDamage_Void ) {
			attacker_name = temp( "{}{}", ImGuiColorToken( black.rgba8 ), "THE VOID" );
		}
		else if( damage_type == WorldDamage_Spike ) {
			attacker_name = temp( "{}{}", ImGuiColorToken( black.rgba8 ), "A SPIKE" );
		}
		else {
			return;
		}
	}

	const char * obituary = MakeObituary( &temp, &rng, current->type, damage_type );

	if( cg.view.playerPrediction && ISVIEWERENTITY( victimNum ) ) {
		self_obituary.time = cls.monotonicTime;
		self_obituary.entropy = entropy;
		self_obituary.type = current->type;
		self_obituary.damage_type = damage_type;
	}

	if( assistor == NULL ) {
		CG_AddChat( temp.sv( "{} {}{} {}",
			attacker_name,
			ImGuiColorToken( diesel_yellow.rgba8 ), obituary,
			victim_name
		) );
	}
	else {
		const char * conjugation = RandomElement( &rng, conjunctions );
		CG_AddChat( temp.sv( "{} {}{} {} {}{} {}",
			attacker_name,
			ImGuiColorToken( 255, 255, 255, 255 ), conjugation,
			assistor_name,
			ImGuiColorToken( diesel_yellow.rgba8 ), obituary,
			victim_name
		) );
	}

	if( ISVIEWERENTITY( attackerNum ) && attacker != victim ) {
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

				DrawText( cgs.fontItalic, cgs.textSizeSmall, msg, Alignment_RightTop, frame_static.viewport_width / 2 - offset, frame_static.viewport_height / 2 + offset, red.vec4 );
			}

			if( trace.ent > 0 && trace.ent <= MAX_CLIENTS ) {
				Vec4 color = AttentionGettingColor();
				color.w = frac;

				RNG obituary_rng = NewRNG( cls.monotonicTime.flicks / GGTIME_FLICKS_PER_SECOND, 0 );
				char * msg = temp( "{}?", RandomElement( &obituary_rng, normal_obituaries ) );
				GlitchText( Span< char >( msg, strlen( msg ) - 1 ) );

				DrawText( cgs.fontItalic, cgs.textSizeSmall, msg, Alignment_LeftTop, frame_static.viewport_width / 2 + offset, frame_static.viewport_height / 2 + offset, color, black.vec4 );
			}
		}
	}
}

//=============================================================================

static void PushLuaAsset( lua_State * L, StringHash s ) {
	lua_pushlightuserdata( L, checked_cast< void * >( checked_cast< uintptr_t >( s.hash ) ) );
}

static bool CallWithStackTrace( lua_State * L, int args, int results ) {
	if( lua_pcall( L, args, results, 1 ) != 0 ) {
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
	Com_Printf( "%s\n", luaL_checkstring( hud_L, 1 ) );
	return 0;
}

static int LuauAsset( lua_State * L ) {
	StringHash hash( luaL_checkstring( hud_L, 1 ) );
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
	};

	return alignments[ luaL_checkoption( L, idx, names[ 0 ], names ) ];
}

static const Font * CheckFont( lua_State * L, int idx ) {
	const Font * fonts[] = {
		cgs.fontNormal,
		cgs.fontNormalBold,
		cgs.fontItalic,
		cgs.fontBoldItalic,
	};

	constexpr const char * names[] = {
		"normal",
		"bold",
		"italic",
		"bolditalic",
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

	bool border = false;
	Vec4 border_color = black.vec4;
	lua_getfield( L, 1, "border" );
	if( !lua_isnil( L, -1 ) ) {
		border = true;
		border_color = CheckColor( L, -1 );
	}
	lua_pop( L, 1 );

	lua_getfield( L, 1, "alignment" );
	Alignment alignment = CheckAlignment( L, -1 );
	lua_pop( L, 1 );

	float x = luaL_checknumber( L, 2 );
	float y = luaL_checknumber( L, 3 );
	const char * str = luaL_checkstring( hud_L, 4 );

	if( border ) {
		DrawText( font, font_size, str, alignment, x, y, color, border_color );
	}
	else {
		DrawText( font, font_size, str, alignment, x, y, color, false );
	}

	return 0;
}

static int LuauGetBind( lua_State * L ) {
	char keys[ 128 ];
	if( !CG_GetBoundKeysString( luaL_checkstring( L, 1 ), keys, sizeof( keys ) ) ) {
		snprintf( keys, sizeof( keys ), "[%s]", luaL_checkstring( L, 1 ) );
	}

	lua_newtable( L );
	lua_pushstring( L, keys );

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

static int LuauGetPlayerName( lua_State * L ) {
	int index = luaL_checknumber( L, 1 ) - 1;

	if( index >= 0 && index < client_gs.maxclients ) {
		lua_newtable( L );
		lua_pushstring( L, PlayerName( index ) );

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
	CG_DrawPlayerNames( cgs.fontNormalBold, luaL_checknumber( L, 1 ), CheckColor( L, 2 ), luaL_checknumber( L, 3 ) );
	return 0;
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

	const Font * font = cgs.fontNormalBold;

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

		float attacker_width = TextBounds( font, font_size, obr->attacker ).maxs.x;
		float victim_width = TextBounds( font, font_size, obr->victim ).maxs.x;

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
			DrawText( font, font_size, obr->attacker, x + xoffset, obituary_y, color, true );
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
		DrawText( font, font_size, obr->victim, x + xoffset, obituary_y, color, true );

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
				const char * obituary = MakeObituary( &temp, &rng, self_obituary.type, self_obituary.damage_type );

				float size = Lerp( h * 0.5f, Unlerp01( 1.0f, t, 10.0f ), h * 5.0f );
				Vec4 color = AttentionGettingColor();
				color.w = Unlerp01( 0.5f, t, 1.0f );
				DrawText( cgs.fontNormal, size, obituary, Alignment_CenterMiddle, frame_static.viewport.x * 0.5f, frame_static.viewport.y * 0.5f, color );
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
		return Clay_SizingAxis {
			.sizeMinMax = { 0.0f, FLT_MAX },
			.type = CLAY__SIZING_TYPE_FIT,
		};
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
		padding.x = all_padding.value;
		padding.y = all_padding.value;
	}

	padding.x = Default( CheckU16( L, idx, "padding_x" ), padding.x );
	padding.y = Default( CheckU16( L, idx, "padding_y" ), padding.y );

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
	};

	return alignments[ luaL_checkoption( L, idx, names[ 0 ], names ) ];
}

static Optional< Clay_Color > CheckClayColor( lua_State * L, int idx, const char * key ) {
	lua_getfield( L, idx, key );
	defer { lua_pop( L, 1 ); };
	if( lua_isnoneornil( L, -1 ) )
		return NONE;

	RGBA8 srgb = LinearTosRGB( CheckColor( L, -1 ) );
	return Clay_Color { float( srgb.r ), float( srgb.g ), float( srgb.b ), float( srgb.a ) };
}

static Clay_CornerRadius CheckClayBorderRadius( lua_State * L, int idx ) {
	Optional< float > radius = CheckFloat( L, idx, "border_radius" );
	float r = Default( radius, 0.0f );
	return Clay_CornerRadius { r, r, r, r };
}

static Clay_LayoutConfig CheckClayLayoutConfig( lua_State * L, int idx ) {
	return Clay_LayoutConfig {
		.sizing = {
			.width = Default( CheckClaySize( L, idx, "width" ), CLAY_LAYOUT_DEFAULT.sizing.width ),
			.height = Default( CheckClaySize( L, idx, "height" ), CLAY_LAYOUT_DEFAULT.sizing.height ),
		},
		.padding = CheckClayPadding( L, idx ),
		.childGap = Default( CheckU16( L, idx, "gap" ), u16( 0 ) ),
		.childAlignment = Default( CheckClayChildAlignment( L, idx, "alignment" ), CLAY_LAYOUT_DEFAULT.childAlignment ),
		.layoutDirection = Default( CheckClayLayoutDirection( L, idx, "flow" ), CLAY_LEFT_TO_RIGHT ),
	};
}

static Optional< Clay_RectangleElementConfig > CheckClayRectangleConfig( lua_State * L, int idx ) {
	Optional< Clay_Color > background = CheckClayColor( L, idx, "background" );
	if( !background.exists ) {
		return NONE;
	}

	return Clay_RectangleElementConfig {
		.color = background.value,
		.cornerRadius = CheckClayBorderRadius( L, idx ),
	};
}

static Optional< Clay_BorderElementConfig > CheckClayBorderConfig( lua_State * L, int idx ) {
	Clay_BorderElementConfig border = { };

	Optional< u16 > all_width = CheckU16( L, idx, "border" );
	if( all_width.exists ) {
		border.left.width = all_width.value;
		border.right.width = all_width.value;
		border.top.width = all_width.value;
		border.bottom.width = all_width.value;
	}

	Optional< Clay_Color > color = CheckClayColor( L, idx, "border_color" );
	if( color.exists ) {
		border.left.color = color.value;
		border.right.color = color.value;
		border.top.color = color.value;
		border.bottom.color = color.value;
	}

	border.left.width = Default( CheckU16( L, idx, "border_left" ), u16( border.left.width ) );
	border.right.width = Default( CheckU16( L, idx, "border_right" ), u16( border.right.width ) );
	border.top.width = Default( CheckU16( L, idx, "border_top" ), u16( border.top.width ) );
	border.bottom.width = Default( CheckU16( L, idx, "border_bottom" ), u16( border.bottom.width ) );

	if( border.left.width == 0 && border.right.width == 0 && border.top.width == 0 && border.bottom.width == 0 ) {
		return NONE;
	}

	border.cornerRadius = CheckClayBorderRadius( L, -1 );

	return border;
}

static Clay_RenderCommandType CheckClayNodeType( lua_State * L, int idx ) {
	luaL_checktype( L, -1, LUA_TLIGHTUSERDATA );
	return checked_cast< Clay_RenderCommandType >( checked_cast< uintptr_t >( lua_touserdata( L, idx ) ) );
}

static void DrawClayNodeRecursive( lua_State * L ) {
	luaL_checktype( L, -1, LUA_TTABLE );

	Clay__OpenElement();
	Clay__AttachLayoutConfig( Clay__StoreLayoutConfig( CheckClayLayoutConfig( L, -1 ) ) );

	Optional< Clay_RectangleElementConfig > rectangle_config = CheckClayRectangleConfig( L, -1 );
	if( rectangle_config.exists ) {
		Clay__AttachElementConfig( Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig( rectangle_config.value ) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE );
	}

	Optional< Clay_BorderElementConfig > border_config = CheckClayBorderConfig( L, -1 );
	if( border_config.exists ) {
		Clay__AttachElementConfig( Clay_ElementConfigUnion { .borderElementConfig = Clay__StoreBorderElementConfig( border_config.value ) }, CLAY__ELEMENT_CONFIG_TYPE_BORDER_CONTAINER );
	}

	Clay__ElementPostConfiguration();

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

	Clay__CloseElement();
}

static int LuauRender( lua_State * L ) {
	TracyZoneScoped;
	DrawClayNodeRecursive( L );
	return 0;
}

static int LuauNode( lua_State * L ) {
	luaL_checktype( L, 1, LUA_TTABLE );
	luaL_argcheck( L, lua_type( L, 2 ) == LUA_TNONE || lua_type( L, 2 ) == LUA_TTABLE, 2, "children must be a table or none" );

	lua_pushvalue( L, 1 );
	if( lua_type( L, 2 ) == LUA_TTABLE ) {
		lua_pushvalue( L, 2 );
		lua_setfield( hud_L, -2, "children" );
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
	show_debugger = false;

	AddCommand( "toggleuidebugger", []( const Tokenized & args ) {
		show_debugger = !show_debugger;
		Clay_SetDebugModeEnabled( show_debugger );
	} );

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
		{ "getPlayerName", LuauGetPlayerName },

		{ "getWeaponIcon", LuauGetWeaponIcon },
		{ "getGadgetIcon", LuauGetGadgetIcon },
		{ "getPerkIcon", LuauGetPerkIcon },

		{ "getWeaponReloadTime", LuauGetWeaponReloadTime },
		{ "getWeaponStagedReload", LuauGetWeaponStagedReload },

		{ "getGadgetAmmo", LuauGetGadgetAmmo },

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

	u32 size = Clay_MinMemorySize();
	clay_arena = Clay_CreateArenaWithCapacityAndMemory( size, sys_allocator->allocate( size, 16 ) );
	Clay_Initialize( clay_arena, Clay_Dimensions { 1000, 1000 } );

	auto measure_text = []( Clay_String * text, Clay_TextElementConfig * config ) -> Clay_Dimensions {
		return { };
	};
	Clay_SetMeasureTextFunction( measure_text );
}

void CG_ShutdownHUD() {
	if( hud_L != NULL ) {
		lua_close( hud_L );
	}

	Free( sys_allocator, clay_arena.memory );

	RemoveCommand( "toggleuidebugger" );
}

static Vec4 ClayToCD( Clay_Color color ) {
	RGBA8 clamped = RGBA8(
		Clamp( 0.0f, color.r, 255.0f ),
		Clamp( 0.0f, color.g, 255.0f ),
		Clamp( 0.0f, color.b, 255.0f ),
		Clamp( 0.0f, color.a, 255.0f )
	);
	return Vec4( sRGBToLinear( clamped ) );
}

static ImU32 ClayToImGui( Clay_Color clay ) {
	RGBA8 rgba8 = LinearTosRGB( ClayToCD( clay ) );
	return IM_COL32( rgba8.r, rgba8.g, rgba8.b, rgba8.a );
}

enum Corner {
	// ordered by CW rotations where 0deg = +X
	Corner_BottomRight,
	Corner_BottomLeft,
	Corner_TopLeft,
	Corner_TopRight,
};

static float _0To1_1ToNeg1( float x ) {
	return ( 1.0f - x ) * 2.0f - 1.0f;
}

static void DrawBorderCorner( Corner corner, Clay_BoundingBox bounds, const Clay_BorderElementConfig * border, float width, Clay_Color color ) {
	struct {
		float x, y, radius;
	} corners[] = {
		{ 1.0f, 1.0f, border->cornerRadius.bottomRight },
		{ 0.0f, 1.0f, border->cornerRadius.bottomLeft },
		{ 0.0f, 0.0f, border->cornerRadius.topLeft },
		{ 1.0f, 0.0f, border->cornerRadius.topRight },
	};

	float cx = corners[ corner ].x;
	float cy = corners[ corner ].y;
	float r = corners[ corner ].radius;
	float angle = int( corner ) * 90.0f;

	Vec2 arc_origin = Vec2(
		Lerp( bounds.x, cx, bounds.x + bounds.width ) + r * _0To1_1ToNeg1( cx ),
		Lerp( bounds.y, cy, bounds.y + bounds.height ) + r * _0To1_1ToNeg1( cy )
	);

	ImDrawList * draw_list = ImGui::GetBackgroundDrawList();
	draw_list->PathArcTo( arc_origin, r - width * 0.5f, Radians( angle ), Radians( angle + 90.0f ) );
	draw_list->PathStroke( ClayToImGui( color ), 0, width );
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
		bool was_showing_debugger = show_debugger;
		CG_ShutdownHUD();
		CG_InitHUD();
		show_debugger = was_showing_debugger;
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
		lua_pushlstring( hud_L, def->name.ptr, def->name.n );
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
			lua_pushlstring( hud_L, player.name, strlen( player.name ) );
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

	{
		TracyZoneScopedN( "Clay_BeginLayout" );
		Clay_BeginLayout();
	}

	bool hud_lua_ran_ok;
	{
		TracyZoneScopedN( "Luau" );
		hud_lua_ran_ok = CallWithStackTrace( hud_L, 1, 0 );
	}

	// don't run clay layout if hud.lua failed because it might have left clay in a bad state
	if( !hud_lua_ran_ok )
		return;

	Clay_RenderCommandArray render_commands;
	{
		TracyZoneScopedN( "Clay_EndLayout" );
		render_commands = Clay_EndLayout();
	}

	{
		TracyZoneScopedN( "Clay submit draw calls" );
		for( u32 i = 0; i < render_commands.length; i++ ) {
			const Clay_RenderCommand & command = render_commands.internalArray[ i ];
			const Clay_BoundingBox & bounds = command.boundingBox;
			switch( command.commandType ) {
				case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
					const Clay_RectangleElementConfig * rect = command.config.rectangleElementConfig;
					ImDrawList * draw_list = ImGui::GetBackgroundDrawList();
					draw_list->AddRectFilled( Vec2( bounds.x, bounds.y ), Vec2( bounds.x + bounds.width, bounds.y + bounds.height ), ClayToImGui( rect->color ), rect->cornerRadius.topLeft );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_BORDER: {
					const Clay_BorderElementConfig * border = command.config.borderElementConfig;

					// top
					Draw2DBox( bounds.x + border->cornerRadius.topLeft, bounds.y,
						bounds.width - border->cornerRadius.topLeft - border->cornerRadius.topRight, border->top.width,
						cls.white_material, ClayToCD( border->top.color ) );
					// right
					Draw2DBox( bounds.x + bounds.width - border->right.width, bounds.y + border->cornerRadius.topRight,
						border->right.width, bounds.height - border->cornerRadius.topRight - border->cornerRadius.bottomRight,
						cls.white_material, ClayToCD( border->right.color ) );
					// left
					Draw2DBox( bounds.x, bounds.y + border->cornerRadius.topLeft,
						border->left.width, bounds.height - border->cornerRadius.topLeft - border->cornerRadius.bottomLeft,
						cls.white_material, ClayToCD( border->left.color ) );
					// bottom
					Draw2DBox( bounds.x + border->cornerRadius.bottomLeft, bounds.y + bounds.height - border->bottom.width,
						bounds.width - border->cornerRadius.bottomLeft - border->cornerRadius.bottomRight, border->bottom.width,
						cls.white_material, ClayToCD( border->bottom.color ) );

					// this doesn't do the right thing for different border sizes/colours
					DrawBorderCorner( Corner_TopLeft, bounds, border, border->top.width, border->top.color );
					DrawBorderCorner( Corner_TopRight, bounds, border, border->right.width, border->right.color );
					DrawBorderCorner( Corner_BottomLeft, bounds, border, border->bottom.width, border->bottom.color );
					DrawBorderCorner( Corner_BottomRight, bounds, border, border->right.width, border->right.color );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_TEXT: {
					const Clay_TextElementConfig * text = command.config.textElementConfig;
				} break;

				case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
					const Clay_ImageElementConfig * image = command.config.imageElementConfig;
					Draw2DBox( bounds.x, bounds.y, bounds.width, bounds.height, FindMaterial( StringHash( bit_cast< u64 >( image->imageData ) ) ) );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
					ImGui::PushClipRect( Vec2( bounds.x, bounds.y ), Vec2( bounds.x + bounds.width, bounds.y + bounds.height ), true );
					break;

				case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
					ImGui::PopClipRect();
					break;

				default:
					assert( false );
					break;
			}
		}
	}
}
