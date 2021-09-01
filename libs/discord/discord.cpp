
//#include "qcommon/qcommon.h"
#include "cgame/cg_local.h"
#include "windows/miniwindows.h"
#include <time.h>
#include "discord_game_sdk.h"

#define CLIENT_ID 882369979406753812

using tDiscordCreate = decltype(&DiscordCreate);

static HMODULE discord_sdk;
static tDiscordCreate pDiscordCreate = nullptr;

struct Application {
	IDiscordCore* core;
	IDiscordUserManager* users;
	IDiscordActivityManager* activities;
	DiscordUserId user_id;
};


template< size_t size >
void Q_strncpyz( char (&dest)[size], const char * src )
{
	Q_strncpyz( dest, src, size );
}



void OnUserUpdated(void* data)
{
	Application* app = (Application*)data;
	DiscordUser user;
	app->users->get_current_user(app->users, &user);
	app->user_id = user.id;

	Com_GGPrint("Discord: Hello {}#{}", user.username, user.discriminator);
}

void UpdateActivityCallback(void* data, EDiscordResult result)
{
	Com_GGPrint("Discord: ActivityCallback: {}", result);
}

const char* log2str(EDiscordLogLevel level)
{
	switch(level)
	{
		case DiscordLogLevel_Error: return "ERR ";
		case DiscordLogLevel_Warn: return "WARN";
		case DiscordLogLevel_Info: return "INFO";
		case DiscordLogLevel_Debug: return "DBG ";
		default: return "?--?";
	}
}

void OnLog(void* hook_data, EDiscordLogLevel level, const char* message)
{
	Com_GGPrint("Discord: [{}]: {}", log2str(level), message);
}

static Application app = {};
static IDiscordUserEvents users_events = {};
static IDiscordActivityEvents activities_events = {};


void InitDiscord()
{
	ZoneScoped;

	discord_sdk = LoadLibraryW(L"discord_game_sdk.dll");
	if (discord_sdk == NULL)
		return;

	pDiscordCreate = (tDiscordCreate)GetProcAddress(discord_sdk, "DiscordCreate");
	if (pDiscordCreate == NULL)
		return;


	users_events.on_current_user_update = OnUserUpdated;

	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);
	params.client_id = CLIENT_ID;
	params.flags = DiscordCreateFlags_NoRequireDiscord;	// Fail if discord is not running instead of closing the game
	params.event_data = &app;
	params.user_events = &users_events;
	params.activity_events = &activities_events;
	EDiscordResult result = pDiscordCreate(DISCORD_VERSION, &params, &app.core);
	if (result != DiscordResult_Ok) {
		Com_GGPrint("Discord: not initialized({})", result);
		// Be sure it's disabled
		app.core = nullptr;
	} else {
		app.core->set_log_hook(app.core, DiscordLogLevel_Debug, nullptr, OnLog);

		app.users = app.core->get_user_manager(app.core);
		app.activities = app.core->get_activity_manager(app.core);
	}
}


static connstate_t old_state = CA_UNINITIALIZED;
static int old_team = -1;
static int old_match_state = -1;
static int old_round = -1;


void DiscordFrame()
{
	ZoneScoped;

	if (app.activities)
	{
		if( (cls.state == CA_CONNECTED || cls.state == CA_ACTIVE) && client_gs.module )
		{

			if (old_state != cls.state ||
				old_team != cg.predictedPlayerState.real_team ||
				old_match_state != client_gs.gameState.match_state ||
				old_round != client_gs.gameState.round_num)
			{
				old_state = cls.state;
				old_team = cg.predictedPlayerState.real_team;
				old_match_state = client_gs.gameState.match_state;
				old_round = client_gs.gameState.round_num;

				DiscordActivity activity = {};
				//activity.type = DiscordActivityType_Playing;

				if (cg.predictedPlayerState.real_team == TEAM_SPECTATOR) {
					Q_strncpyz(activity.details, "SPECTATING");
				} else if (client_gs.gameState.match_state <= MatchState_Warmup) {
					Q_strncpyz(activity.details, "WARMUP");
				} else if (client_gs.gameState.match_state <= MatchState_Playing) {
					ggformat(activity.details, "ROUND {}", client_gs.gameState.round_num);
				}

				bool is_bomb = GS_TeamBasedGametype( &client_gs );
				if (is_bomb) {
					auto& alpha = client_gs.gameState.teams[ TEAM_ALPHA ];
					auto& beta = client_gs.gameState.teams[ TEAM_BETA ];
					ggformat(activity.state, "bomb: {}-{}", alpha.score, beta.score );
				} else {
					Q_strncpyz(activity.state, "gladiator" );
				}

				if (client_gs.gameState.match_state_start_time) {
					time_t now = time( NULL );
					activity.timestamps.start = now - client_gs.gameState.match_state_start_time / 1000;
				}

				if (cl.map && !Q_strnicmp(cl.map->name, "maps/", 5))
				{
					char tmpmap[128];
					ggformat(tmpmap, cl.map->name + 5);
					COM_StripExtension(tmpmap);
					ggformat(activity.assets.large_image, "map-{}", tmpmap);
					ggformat(activity.assets.large_text, "Playing on map {}", tmpmap);
				}
				auto gt = is_bomb ? "bomb" : "gladiator";
				ggformat(activity.assets.small_image, "gt-{}", is_bomb ? "bomb" : "gladiator");
				ggformat(activity.assets.small_text, "Gametype: {}", gt);
				activity.instance = 1;

				Com_GGPrint("Discord update: {}, {}, {}, {}", activity.details, activity.state, activity.assets.large_text, activity.assets.small_text);
				app.activities->update_activity(app.activities, &activity, &app, UpdateActivityCallback);
			}
		}
		else if (old_state != CA_DISCONNECTED) {
			old_state = CA_DISCONNECTED;
			old_team = -1;
			old_match_state = -1;
			old_round = -1;

			DiscordActivity activity = {};
			Q_strncpyz(activity.details, "MENU");
			Q_strncpyz(activity.assets.large_image, "mainmenu");

			Com_GGPrint("Discord update: {}", activity.details);
			app.activities->update_activity(app.activities, &activity, &app, UpdateActivityCallback);
		}
	}


	if (app.core)
	{
		EDiscordResult result = app.core->run_callbacks(app.core);
		if (result != DiscordResult_Ok) {
			// Now what?
			Com_GGPrint("Discord: run_callbacks failed: {}", result);
		}
	}
}

void ShutdownDiscord()
{
	ZoneScoped;

	if (app.activities) {
		// Clear current activity
		app.activities->clear_activity(app.activities, &app, UpdateActivityCallback);
	}

	if (app.core) {
		// Cleanup
		app.core->destroy(app.core);
		app = {};
	}
}

