#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "gameshared/q_shared.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "client/client.h"
#include "cgame/cg_local.h"
#include "client/discord.h"

#include "discord/discord_rpc.h"

struct RichPresence {
	bool playing;
	String< 127 > first_line;
	String< 127 > second_line;
	String< 127 > large_image;
	String< 127 > large_image_tooltip;
	String< 127 > small_image;
	String< 127 > small_image_tooltip;
};

static bool operator==( const RichPresence & a, const RichPresence & b ) {
	return true
		&& a.playing == b.playing
		&& StrEqual( a.first_line.span(), b.first_line.span() )
		&& StrEqual( a.second_line.span(), b.second_line.span() )
		&& StrEqual( a.large_image.span(), b.large_image.span() )
		&& StrEqual( a.large_image_tooltip.span(), b.large_image_tooltip.span() )
		&& StrEqual( a.small_image.span(), b.small_image.span() )
		&& StrEqual( a.small_image_tooltip.span(), b.small_image_tooltip.span() )
		;
}

static bool operator!=( const RichPresence & a, const RichPresence & b ) {
	return !( a == b );
}

template< size_t N, typename... Rest >
size_t ggformat( char ( &buf )[ N ], const char * fmt, const Rest & ... rest ) {
	return ggformat( buf, N, fmt, rest... );
}

static RichPresence old_presence;

void InitDiscord() {
	TracyZoneScoped;

	constexpr const char * application_id = "882369979406753812";
	Discord_Initialize( application_id, NULL, 1, NULL );

	old_presence = { };
}

void DiscordFrame() {
	TracyZoneScoped;

	RichPresence presence = { };

	if( !is_public_build ) {
		presence.first_line.format( "gamedev lol" );
	}
	else if( cls.state == CA_ACTIVE ) {
		presence.playing = true;

		if( cg.predictedPlayerState.real_team == Team_None ) {
			presence.first_line.format( "SPECTATING" );
		}
		else if( client_gs.gameState.match_state <= MatchState_Warmup ) {
			presence.first_line.format( "WARMUP" );
		}
		else if( client_gs.gameState.match_state <= MatchState_Playing ) {
			presence.first_line.format( "ROUND {}", client_gs.gameState.round_num );
		}

		if( client_gs.gameState.gametype == Gametype_Bomb ) {
			u8 alpha_score = client_gs.gameState.teams[ Team_One ].score;
			u8 beta_score = client_gs.gameState.teams[ Team_Two ].score;

			if( client_gs.gameState.match_state >= MatchState_Playing ) {
				presence.second_line.format( "{} - {}", alpha_score, beta_score );
			}
		}

		presence.large_image.format( "map-{}", cl.map->name );
		presence.large_image_tooltip.format( "Playing on {}", cl.map->name );

		const char * gt = client_gs.gameState.gametype == Gametype_Bomb ? "bomb" : "gladiator";
		presence.small_image.format( "gt-{}", gt );
	}
	else {
		presence.playing = false;
		presence.first_line.format( "SCARED AND SHAKING" );
		presence.second_line.format( "FROM FEAR" );
		presence.large_image.format( "mainmenu" );
	}

	if( presence != old_presence ) {
		DiscordRichPresence discord = { };

		discord.instance = presence.playing;
		discord.details = presence.first_line.c_str();
		discord.state = presence.second_line.c_str();
		discord.largeImageKey = presence.large_image.c_str();
		discord.largeImageText = presence.large_image_tooltip.c_str();
		discord.smallImageKey = presence.small_image.c_str();
		discord.smallImageText = presence.small_image_tooltip.c_str();

		Discord_UpdatePresence( &discord );

		old_presence = presence;
	}

	Discord_RunCallbacks();
}

void ShutdownDiscord() {
	TracyZoneScoped;
	Discord_Shutdown();
}
