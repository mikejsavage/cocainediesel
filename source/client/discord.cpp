#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/library.h"
#include "qcommon/string.h"
#include "client/discord.h"
#include "cgame/cg_local.h"

#include <time.h>

#include "discord/discord_game_sdk.h"

constexpr DiscordClientId CLIENT_ID = 882369979406753812LL;

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

static bool loaded;
static Library discord_sdk_module;
static IDiscordCore * core;
static IDiscordActivityManager * activities;

static RichPresence old_presence;

static void LogOnError( void * data, EDiscordResult result ) {
	if( result != DiscordResult_Ok ) {
		Com_GGPrint( "Discord: ActivityCallback: {}", result );
	}
}

static const char * DiscordLogLevelToString( EDiscordLogLevel level ) {
	switch( level ) {
		case DiscordLogLevel_Error: return "ERROR";
		case DiscordLogLevel_Warn: return "WARN";
		case DiscordLogLevel_Info: return "INFO";
		case DiscordLogLevel_Debug: return "DEBUG";
	}

	assert( false );
	return NULL;
}

void InitDiscord() {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	loaded = false;
	old_presence = { };

	const char * sdk_module_path = temp( "{}/discord_game_sdk", RootDirPath() );
	discord_sdk_module = OpenLibrary( &temp, sdk_module_path );
	if( discord_sdk_module.handle == NULL )
		return;

	using tDiscordCreate = decltype( &DiscordCreate );
	tDiscordCreate pDiscordCreate = tDiscordCreate( GetLibraryFunction( discord_sdk_module, "DiscordCreate" ) );
	if( pDiscordCreate == NULL )
		return;

	DiscordCreateParams params;
	DiscordCreateParamsSetDefault( &params );
	params.client_id = CLIENT_ID;
	params.flags = DiscordCreateFlags_NoRequireDiscord;

	EDiscordResult result = pDiscordCreate( DISCORD_VERSION, &params, &core );
	if( result != DiscordResult_Ok ) {
		if( result != DiscordResult_NotRunning ) {
			Com_GGPrint( S_COLOR_YELLOW "Discord not initialized: {}", result );
		}
		return;
	}

	EDiscordLogLevel log_level = is_public_build ? DiscordLogLevel_Warn : DiscordLogLevel_Debug;
	core->set_log_hook( core, log_level, NULL, []( void * data, EDiscordLogLevel level, const char * message ) {
		Com_GGPrint( "Discord {}: {}", DiscordLogLevelToString( level ), message );
	} );

	loaded = true;
	activities = core->get_activity_manager( core );
}

void DiscordFrame() {
	TracyZoneScoped;

	if( !loaded )
		return;

	RichPresence presence = { };

	if( cls.state == CA_ACTIVE ) {
		presence.playing = true;

		if( cg.predictedPlayerState.real_team == TEAM_SPECTATOR ) {
			presence.first_line.format( "SPECTATING" );
		}
		else if( client_gs.gameState.match_state <= MatchState_Warmup ) {
			presence.first_line.format( "WARMUP" );
		}
		else if( client_gs.gameState.match_state <= MatchState_Playing ) {
			presence.first_line.format( "ROUND {}", client_gs.gameState.round_num );
		}

		bool is_bomb = GS_TeamBasedGametype( &client_gs );
		if( is_bomb ) {
			u8 alpha_score = client_gs.gameState.teams[ TEAM_ALPHA ].score;
			u8 beta_score = client_gs.gameState.teams[ TEAM_BETA ].score;
			presence.second_line.format( "{} - {}", alpha_score, beta_score );
		}

		presence.large_image.format( "map-{}", cl.map->name );
		presence.large_image_tooltip.format( "Playing on {}", cl.map->name );

		const char * gt = is_bomb ? "bomb" : "gladiator";
		presence.small_image.format( "gt-{}", gt );
	}
	else {
		presence.playing = false;
		presence.first_line.format( "SCARED AND SHAKING" );
		presence.second_line.format( "FROM FEAR" );
		presence.large_image.format( "mainmenu" );
	}

	if( presence != old_presence ) {
		DiscordActivity activity = { };

		activity.instance = presence.playing;
		ggformat( activity.details, "{}", presence.first_line );
		ggformat( activity.state, "{}", presence.second_line );
		ggformat( activity.assets.large_image, "{}", presence.large_image );
		ggformat( activity.assets.large_text, "{}", presence.large_image_tooltip );
		ggformat( activity.assets.small_image, "{}", presence.small_image );
		ggformat( activity.assets.small_text, "{}", presence.small_image_tooltip );

		activities->update_activity( activities, &activity, NULL, LogOnError );

		old_presence = presence;
	}

	EDiscordResult result = core->run_callbacks( core );
	if( result != DiscordResult_Ok ) {
		Com_GGPrint( S_COLOR_YELLOW "Discord: run_callbacks failed: {}", result );
		ShutdownDiscord();
	}
}

void ShutdownDiscord() {
	TracyZoneScoped;

	if( !loaded )
		return;

	core->destroy( core );

	if( discord_sdk_module.handle != NULL ) {
		CloseLibrary( discord_sdk_module );
	}

	loaded = false;
}
