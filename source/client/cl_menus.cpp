#include "client/client.h"
#include "qcommon/version.h"
#include "qcommon/string.h"
#include "client/sdl/sdl_window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "cgame/cg_local.h"

enum UIState {
	UIState_Hidden,
	UIState_MainMenu,
	UIState_Connecting,
	UIState_GameMenu,
	UIState_DemoMenu,
};

enum MainMenuState {
	MainMenuState_ServerBrowser,
	MainMenuState_CreateServer,
	MainMenuState_Settings,
};

enum GameMenuState {
	GameMenuState_Menu,
	GameMenuState_Loadout,
	GameMenuState_Settings,
};

enum DemoMenuState {

};

enum SettingsState {
	SettingsState_General,
	SettingsState_Mouse,
	SettingsState_Keys,
	SettingsState_Video,
	SettingsState_Audio,
};

struct Server {
	const char * address;
	const char * info;
};

static Server servers[ 1024 ];
static int num_servers = 0;

static UIState uistate;

static MainMenuState mainmenu_state;
static int selected_server;
static int selected_map;

static GameMenuState gamemenu_state;
static bool is_spectating;
static bool is_ready;
static size_t selected_primary;
static size_t selected_secondary;

static SettingsState settings_state;
static bool reset_video_settings;
static int pressed_key;

static void ResetServerBrowser() {
	for( int i = 0; i < num_servers; i++ ) {
		free( const_cast< char * >( servers[ i ].address ) );
		free( const_cast< char * >( servers[ i ].info ) );
	}
	num_servers = 0;
	selected_server = -1;
}

static void RefreshServerBrowser() {
	ResetServerBrowser();

	for( const char * masterserver : MASTER_SERVERS ) {
		String< 256 > buf( "requestservers global {} {} full empty\n", masterserver, APPLICATION_NOSPACES );
		Cbuf_AddText( buf );
	}

	Cbuf_AddText( "requestservers local full empty\n" );
}

void UI_Init() {
	ResetServerBrowser();

	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;

	reset_video_settings = true;
}

static void SettingLabel( const char * label ) {
	ImGui::AlignTextToFramePadding();
	ImGui::Text( "%s", label );
	ImGui::SameLine( 200 );
}

template< size_t maxlen >
static void CvarTextbox( const char * label, const char * cvar_name, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	char buf[ maxlen + 1 ];
	Q_strncpyz( buf, cvar->string, sizeof( buf ) );

	String< 128 > unique( "##{}", cvar_name );
	ImGui::InputText( unique, buf, sizeof( buf ) );

	Cvar_Set( cvar_name, buf );
}

static void CvarCheckbox( const char * label, const char * cvar_name, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	String< 128 > unique( "##{}", cvar_name );
	bool val = cvar->integer != 0;
	ImGui::Checkbox( unique, &val );

	Cvar_Set( cvar_name, val ? "1" : "0" );
}

static void CvarSliderInt( const char * label, const char * cvar_name, int lo, int hi, const char * def, cvar_flag_t flags, const char * format = NULL ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	String< 128 > unique( "##{}", cvar_name );
	int val = cvar->integer;
	ImGui::SliderInt( unique, &val, lo, hi, format );

	String< 128 > buf( "{}", val );
	Cvar_Set( cvar_name, buf );
}

static void CvarSliderFloat( const char * label, const char * cvar_name, float lo, float hi, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	String< 128 > unique( "##{}", cvar_name );
	float val = cvar->value;
	ImGui::SliderFloat( unique, &val, lo, hi, "%.2f" );

	char buf[ 128 ];
	Q_snprintfz( buf, sizeof( buf ), "%f", val );
	RemoveTrailingZeroesFloat( buf );
	Cvar_Set( cvar_name, buf );
}

// TODO: put this somewhere else
void CG_GetBoundKeysString( const char *cmd, char *keys, size_t keysSize );

static void KeyBindButton( const char * label, const char * command ) {
	SettingLabel( label );
	ImGui::PushID( label );

	char keys[ 128 ];
	CG_GetBoundKeysString( command, keys, sizeof( keys ) );
	if( ImGui::Button( keys, ImVec2( 200, 0 ) ) ) {
		ImGui::OpenPopup( label );
	}

	if( ImGui::BeginPopupModal( label, NULL, ImGuiWindowFlags_NoDecoration ) ) {
		ImGui::Text( "Press a key to set a new bind, or press ESCAPE to cancel." );
		if( pressed_key != -1 ) {
			if( pressed_key != K_ESCAPE ) {
				Key_SetBinding( pressed_key, command );
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
}

static bool SelectableColor( const char * label, RGB8 rgb, bool selected ) {
	bool clicked = ImGui::Selectable( "", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_PressedOnRelease );

	ImGui::NextColumn();
	ImVec2 window_pos = ImGui::GetWindowPos();
	ImVec2 top_left = ImGui::GetCursorPos();
	top_left.x += window_pos.x;
	top_left.y += window_pos.y;
	ImVec2 bottom_right = top_left;
	bottom_right.x += ImGui::GetTextLineHeight() * 1.618f;
	bottom_right.y += ImGui::GetTextLineHeight();
	ImGui::GetWindowDrawList()->AddRectFilled( top_left, bottom_right, IM_COL32( rgb.r, rgb.g, rgb.b, 255 ) );
	ImGui::NextColumn();

	return clicked;
}

static void CvarTeamColorCombo( const char * label, const char * cvar_name, int def ) {
	SettingLabel( label );
	ImGui::PushItemWidth( 100 );

	String< 16 > def_str( "{}", def );
	cvar_t * cvar = Cvar_Get( cvar_name, def_str, CVAR_ARCHIVE );

	String< 128 > unique( "##{}", cvar_name );

	int selected = cvar->integer;
	if( selected >= int( ARRAY_COUNT( TEAM_COLORS ) ) )
		selected = def;

	if( ImGui::BeginCombo( unique, TEAM_COLORS[ selected ].name ) ) {
		ImGui::Columns( 2, cvar_name, false );
		ImGui::SetColumnWidth( 0, 0 );

		for( int i = 0; i < int( ARRAY_COUNT( TEAM_COLORS ) ); i++ ) {
			if( SelectableColor( TEAM_COLORS[ i ].name, TEAM_COLORS[ i ].rgb, i == selected ) )
				selected = i;
			if( i == selected )
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
		ImGui::Columns( 1 );
	}
	ImGui::PopItemWidth();

	String< 16 > buf( "{}", selected );
	Cvar_Set( cvar_name, buf );
}

static void SettingsGeneral() {
	ImGui::Text( "These settings are saved automatically" );

	CvarTextbox< MAX_NAME_BYTES >( "Name", "name", "Player", CVAR_USERINFO | CVAR_ARCHIVE );
	CvarSliderInt( "FOV", "fov", 60, 140, "100", CVAR_ARCHIVE );
	CvarTeamColorCombo( "Ally color", "cg_allyColor", 0 );
	CvarTeamColorCombo( "Enemy color", "cg_enemyColor", 1 );
	CvarCheckbox( "Show FPS", "cg_showFPS", "0", CVAR_ARCHIVE );
}

static void SettingsMouse() {
	ImGui::Text( "These settings are saved automatically" );

	CvarSliderFloat( "Sensitivity", "sensitivity", 1.0f, 10.0f, "3", CVAR_ARCHIVE );
	CvarSliderFloat( "Horizontal sensitivity", "horizontalsensscale", 0.5f, 2.0f, "1", CVAR_ARCHIVE );
	CvarSliderFloat( "Acceleration", "m_accel", 0.0f, 1.0f, "0", CVAR_ARCHIVE );
}

static void SettingsKeys() {
	ImGui::Text( "These settings are saved automatically" );

	ImGui::BeginChild( "binds" );

	ImGui::Separator();
	ImGui::Text( "Movement" );
	ImGui::Separator();

	KeyBindButton( "Forward", "+forward" );
	KeyBindButton( "Back", "+back" );
	KeyBindButton( "Left", "+left" );
	KeyBindButton( "Right", "+right" );
	KeyBindButton( "Jump", "+jump" );
	KeyBindButton( "Dash/walljump", "+special" );
	KeyBindButton( "Crouch", "+crouch" );
	KeyBindButton( "Walk", "+walk" );

	ImGui::Separator();
	ImGui::Text( "Actions" );
	ImGui::Separator();

	KeyBindButton( "Attack", "+attack" );
	KeyBindButton( "Drop bomb", "drop" );
	KeyBindButton( "Shop", "gametypemenu" );
	KeyBindButton( "Scoreboard", "+scores" );
	KeyBindButton( "Chat", "messagemode" );
	KeyBindButton( "Team chat", "messagemode2" );
	KeyBindButton( "Zoom", "+zoom" );

	ImGui::Separator();
	ImGui::Text( "Weapons" );
	ImGui::Separator();

	KeyBindButton( "Weapon 1", "weapon 1" );
	KeyBindButton( "Weapon 2", "weapon 2" );
	KeyBindButton( "Weapon 3", "weapon 3" );
	KeyBindButton( "Weapon 4", "weapon 4" );
	KeyBindButton( "Next weapon", "weapnext" );
	KeyBindButton( "Previous weapon", "weapprev" );
	KeyBindButton( "Last weapon", "weaplast" );

	ImGui::Separator();
	ImGui::Text( "Voice lines" );
	ImGui::Separator();

	KeyBindButton( "Yes", "vsay yes" );
	KeyBindButton( "No", "vsay no" );
	KeyBindButton( "Thanks", "vsay thanks" );
	KeyBindButton( "Good game", "vsay goodgame" );
	KeyBindButton( "Boomstick", "vsay boomstick" );
	KeyBindButton( "Shut up", "vsay shutup" );

	ImGui::Separator();
	ImGui::Text( "Specific weapons" );
	ImGui::Separator();

	KeyBindButton( "Gunblade", "use gb" );
	// KeyBindButton( "Machine Gun", "use mg" );
	KeyBindButton( "Disrespect Gun", "use rg" );
	KeyBindButton( "Grenade Launcher", "use gl" );
	KeyBindButton( "Rocket Launcher", "use rl" );
	KeyBindButton( "Plasma Gun", "use pg" );
	KeyBindButton( "Lasergun", "use lg" );
	KeyBindButton( "Electrobolt", "use eb" );

	ImGui::EndChild();
}

static const char * FullscreenModeToString( FullScreenMode mode ) {
	switch( mode ) {
		case FullScreenMode_Windowed: return "Windowed";
		case FullScreenMode_FullscreenBorderless: return "Borderless";
		case FullScreenMode_Fullscreen: return "Fullscreen";
	}
	return NULL;
}

static void SettingsVideo() {
	static WindowMode mode;

	if( reset_video_settings ) {
		mode = VID_GetWindowMode();
		reset_video_settings = false;
	}

	SettingLabel( "Window mode" );
	ImGui::PushItemWidth( 200 );
	if( ImGui::BeginCombo( "##fullscreen", FullscreenModeToString( mode.fullscreen ) ) ) {
		if( ImGui::Selectable( FullscreenModeToString( FullScreenMode_Windowed ), mode.fullscreen == FullScreenMode_Windowed ) ) {
			mode.fullscreen = FullScreenMode_Windowed;
		}
		if( ImGui::Selectable( FullscreenModeToString( FullScreenMode_FullscreenBorderless ), mode.fullscreen == FullScreenMode_FullscreenBorderless ) ) {
			mode.fullscreen = FullScreenMode_FullscreenBorderless;
		}
		if( ImGui::Selectable( FullscreenModeToString( FullScreenMode_Fullscreen ), mode.fullscreen == FullScreenMode_Fullscreen ) ) {
			mode.fullscreen = FullScreenMode_Fullscreen;
		}
		ImGui::EndCombo();
	}

	if( mode.fullscreen == FullScreenMode_Windowed ) {
		mode.video_mode.frequency = 0;
	}
	else if( mode.fullscreen == FullScreenMode_FullscreenBorderless ) {
		mode.video_mode.width = 0;
		mode.video_mode.height = 0;
		mode.video_mode.frequency = 0;
	}
	else if( mode.fullscreen == FullScreenMode_Fullscreen ) {
		SettingLabel( "Resolution" );
		ImGui::PushItemWidth( 200 );

		if( mode.video_mode.frequency == 0 ) {
			mode.video_mode = VID_GetVideoMode( 0 );
		}

		String< 128 > preview( "{}", mode.video_mode );

		if( ImGui::BeginCombo( "##resolution", preview ) ) {
			for( int i = 0; i < VID_GetNumVideoModes(); i++ ) {
				VideoMode video_mode = VID_GetVideoMode( i );

				String< 128 > buf( "{}", video_mode );
				bool is_selected = mode.video_mode.width == video_mode.width && mode.video_mode.height == video_mode.height && mode.video_mode.frequency == video_mode.frequency;
				if( ImGui::Selectable( buf, is_selected ) ) {
					mode.video_mode = video_mode;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}

	if( ImGui::Button( "Apply mode changes" ) ) {
		String< 128 > buf( "{}", mode );
		Cvar_Set( "vid_mode", buf );
	}

	ImGui::SameLine();
	if( ImGui::Button( "Discard mode changes" ) ) {
		reset_video_settings = true;
	}

	ImGui::Separator();

	ImGui::Text( "These settings are saved automatically" );

	{
		SettingLabel( "Anti-aliasing" );

		cvar_t * cvar = Cvar_Get( "r_samples", "0", CVAR_ARCHIVE );
		int samples = cvar->integer;

		String< 16 > current_samples;
		if( samples == 0 )
			current_samples.format( "Off" );
		else
			current_samples.format( "{}x", samples );

		ImGui::PushItemWidth( 100 );
		if( ImGui::BeginCombo( "##r_samples", current_samples ) ) {
			if( ImGui::Selectable( "Off", samples == 0 ) )
				samples = 0;
			if( samples == 0 )
				ImGui::SetItemDefaultFocus();

			for( int i = 2; i <= 16; i *= 2 ) {
				String< 16 > buf( "{}x", i );
				if( ImGui::Selectable( buf, i == samples ) )
					samples = i;
				if( i == samples )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		String< 16 > buf( "{}", samples );
		Cvar_Set( "r_samples", buf );
	}

	CvarCheckbox( "Vsync", "vid_vsync", "0", CVAR_ARCHIVE );

	{
		SettingLabel( "Max FPS" );

		constexpr int values[] = { 60, 75, 120, 144, 165, 180, 200, 240, 333, 500, 1000 };

		cvar_t * cvar = Cvar_Get( "cl_maxfps", "250", CVAR_ARCHIVE );
		int maxfps = cvar->integer;

		String< 16 > current( "{}", maxfps );

		ImGui::PushItemWidth( 100 );
		if( ImGui::BeginCombo( "##cl_maxfps", current ) ) {
			for( int value : values ) {
				String< 16 > buf( "{}", value );
				if( ImGui::Selectable( buf, maxfps == value ) )
					maxfps = value;
				if( value == maxfps )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		String< 16 > buf( "{}", maxfps );
		Cvar_Set( "cl_maxfps", buf );
	}
}

static void SettingsAudio() {
	ImGui::Text( "These settings are saved automatically" );

	CvarSliderFloat( "Master volume", "s_volume", 0.0f, 1.0f, "1", CVAR_ARCHIVE );
	CvarSliderFloat( "Music volume", "s_musicvolume", 0.0f, 1.0f, "1", CVAR_ARCHIVE );
	CvarCheckbox( "Mute when alt-tabbed", "s_muteinbackground", "1", CVAR_ARCHIVE );
}

static void Settings() {
	if( ImGui::Button( "GENERAL" ) ) {
		settings_state = SettingsState_General;
	}

	ImGui::SameLine();

	if( ImGui::Button( "MOUSE" ) ) {
		settings_state = SettingsState_Mouse;
	}

	ImGui::SameLine();

	if( ImGui::Button( "KEYS" ) ) {
		settings_state = SettingsState_Keys;
	}

	ImGui::SameLine();

	if( ImGui::Button( "VIDEO" ) ) {
		reset_video_settings = true;
		settings_state = SettingsState_Video;
	}

	ImGui::SameLine();

	if( ImGui::Button( "SOUND" ) ) {
		settings_state = SettingsState_Audio;
	}

	if( settings_state == SettingsState_General )
		SettingsGeneral();
	else if( settings_state == SettingsState_Mouse )
		SettingsMouse();
	else if( settings_state == SettingsState_Keys )
		SettingsKeys();
	else if( settings_state == SettingsState_Video )
		SettingsVideo();
	else if( settings_state == SettingsState_Audio )
		SettingsAudio();
}

static void ServerBrowser() {
	if( ImGui::Button( "Refresh" ) ) {
		RefreshServerBrowser();
	}

	ImGui::BeginChild( "servers" );
	ImGui::Columns( 2, "serverbrowser", false );
	ImGui::SetColumnWidth( 0, 200 );
	ImGui::Text( "Address" );
	ImGui::NextColumn();
	ImGui::Text( "Info" );
	ImGui::NextColumn();
	for( int i = 0; i < num_servers; i++ ) {
		if( ImGui::Selectable( servers[ i ].address, i == selected_server, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) ) {
			if( ImGui::IsMouseDoubleClicked( 0 ) ) {
				String< 256 > buf( "connect \"{}\"\n", servers[ i ].address );
				Cbuf_AddText( buf );
			}
			selected_server = i;
		}
		ImGui::NextColumn();
		ImGui::Text( "%s", servers[ i ].info );
		ImGui::NextColumn();
	}

	ImGui::Columns( 1 );
	ImGui::EndChild();
}

static void CreateServer() {
	CvarTextbox< 128 >( "Server name", "sv_hostname", APPLICATION " server", CVAR_SERVERINFO | CVAR_ARCHIVE );

	{
		cvar_t * cvar = Cvar_Get( "sv_maxclients", "16", CVAR_SERVERINFO | CVAR_LATCH );
		int maxclients = cvar->integer;

		SettingLabel( "Max players" );
		ImGui::PushItemWidth( 150 );
		ImGui::InputInt( "##sv_maxclients", &maxclients );
		ImGui::PopItemWidth();

		maxclients = max( maxclients, 1 );
		maxclients = min( maxclients, 64 );

		String< 128 > buf( "{}", maxclients );
		Cvar_Set( "sv_maxclients", buf );
	}

	{
		SettingLabel( "Map" );
		char selectedmapname[ 128 ];
		ML_GetMapByNum( selected_map, selectedmapname, sizeof( selectedmapname ) );

		ImGui::PushItemWidth( 200 );
		if( ImGui::BeginCombo( "##map", selectedmapname ) ) {
			for( int i = 0; true; i++ ) {
				char mapname[ 128 ];
				if( ML_GetMapByNum( i, mapname, sizeof( mapname ) ) == 0 )
					break;

				if( ImGui::Selectable( mapname, i == selected_map ) )
					selected_map = i;
				if( i == selected_map )
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}

	CvarCheckbox( "Public", "sv_public", "0", CVAR_LATCH );

	if( ImGui::Button( "Create server" ) ) {
		char mapname[ 128 ];
		if( ML_GetMapByNum( selected_map, mapname, sizeof( mapname ) ) != 0 ) {
			String< 256 > buf( "map \"{}\"\n", mapname );
			Cbuf_AddText( buf );
		}
	}
}

static void MainMenu() {
	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );
	ImGui::Begin( "mainmenu", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

	ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

	ImGui::BeginChild( "mainmenubody", ImVec2( 0, -ImGui::GetFrameHeightWithSpacing() + window_padding.y ) );

	ImGui::SetCursorPosX( 2 + 2 * sinf( cls.monotonicTime / 20.0f ) );
	ImGui::PushFont( cls.large_font );
	ImGui::Text( "COCAINE DIESEL" );
	ImGui::PopFont();

	if( ImGui::Button( "PLAY" ) ) {
		mainmenu_state = MainMenuState_ServerBrowser;
	}

	ImGui::SameLine();

	if( ImGui::Button( "CREATE SERVER" ) ) {
		mainmenu_state = MainMenuState_CreateServer;
		selected_map = 0;
	}

	ImGui::SameLine();

	if( ImGui::Button( "SETTINGS" ) ) {
		mainmenu_state = MainMenuState_Settings;
		settings_state = SettingsState_General;
	}

	ImGui::SameLine();

	if( ImGui::Button( "QUIT" ) ) {
		CL_Quit();
	}

	ImGui::Separator();

	if( mainmenu_state == MainMenuState_ServerBrowser ) {
		ServerBrowser();
	}
	else if( mainmenu_state == MainMenuState_CreateServer ) {
		CreateServer();
	}
	else {
		Settings();
	}

	ImGui::EndChild();

	{
		ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_Button, IM_COL32( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, IM_COL32( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonActive, IM_COL32( 0, 0, 0, 0 ) );

		const char * buf = APP_VERSION u8" \u00A9 AHA CHEERS";
		ImVec2 size = ImGui::CalcTextSize( buf );
		ImGui::SetCursorPosX( ImGui::GetWindowWidth() - size.x - window_padding.x - 1 - sinf( cls.monotonicTime / 29.0f ) );

		if( ImGui::Button( buf ) ) {
			ImGui::OpenPopup( "Credits" );
		}

		ImGui::PopStyleColor( 3 );
		ImGui::PopStyleVar();

		ImGuiWindowFlags flags = ( ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoTitleBar ) | ImGuiWindowFlags_NoMove;
		if( ImGui::BeginPopupModal( "Credits", NULL, flags ) ) {
			ImGui::Text( "goochie - art & programming" );
			ImGui::Text( "MikeJS - programming" );
			ImGui::Text( "Obani - music & fx & programming" );
			ImGui::Text( "Dexter - programming" );
			ImGui::Text( "Special thanks to MSC" );
			ImGui::Text( "Special thanks to the Warsow team except for slk and MWAGA" );
			ImGui::Spacing();

			if( ImGui::Button( "Close" ) )
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}

	ImGui::End();
}

static void GameMenuButton( const char * label, const char * command, bool * clicked = NULL, int column = -1 ) {
	ImVec2 size = ImVec2( -1, 0 );
	if( column != -1 ) {
		ImGuiStyle & style = ImGui::GetStyle();
		size = ImVec2( ImGui::GetColumnWidth( 0 ) - style.ItemSpacing.x, 0 );
	}

	if( ImGui::Button( label, size ) ) {
		String< 256 > buf( "{}\n", command );
		Cbuf_AddText( buf );
		if( clicked != NULL )
			*clicked = true;
	}
}

static void GameMenu() {
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
	bool should_close = false;

	if( gamemenu_state == GameMenuState_Menu ) {
		ImGui::SetNextWindowPos( ImGui::GetIO().DisplaySize * 0.5f, 0, Vec2( 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 300, 0 ) );
		ImGui::Begin( "gamemenu", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );
		ImGuiStyle & style = ImGui::GetStyle();
		const double half = ImGui::GetWindowWidth() / 2 - style.ItemSpacing.x - style.ItemInnerSpacing.x;

		if( is_spectating ) {
			if( GS_TeamBasedGametype() ) {
				ImGui::Columns( 2, NULL, false );
				ImGui::SetColumnWidth( 0, half );
				ImGui::SetColumnWidth( 1, half );

				GameMenuButton( "Join Cocaine", "join cocaine", &should_close, 0 );
				ImGui::NextColumn();
				GameMenuButton( "Join Diesel", "join diesel", &should_close, 1 );
				ImGui::NextColumn();
			} else {
				GameMenuButton( "Join Game", "join", &should_close );
			}
			ImGui::Columns( 1 );
		}
		else {
			if( ImGui::Checkbox( (is_ready ? " Ready !" : " Not ready"), &is_ready )) {
				String< 256 > buf( "{}\n", (is_ready ? "ready" : "unready"));
				Cbuf_AddText( buf );
			}

			GameMenuButton( "Spectate", "spec", &should_close );

			if( GS_TeamBasedGametype() )
				GameMenuButton( "Change loadout", "gametypemenu", &should_close );
		}

		if( ImGui::Button( "Settings", ImVec2( -1, 0 ) ) ) {
			gamemenu_state = GameMenuState_Settings;
			settings_state = SettingsState_General;
		}

		ImGui::Columns( 2, NULL, false );
		ImGui::SetColumnWidth( 0, half );
		ImGui::SetColumnWidth( 1, half );

		GameMenuButton( "Disconnect", "disconnect", &should_close, 0 );
		ImGui::NextColumn();
		GameMenuButton( "Exit game", "quit", &should_close, 1 );
		ImGui::NextColumn();

		ImGui::Columns( 1 );

		ImGui::End();
	}
	else if( gamemenu_state == GameMenuState_Loadout ) {
		ImGui::SetNextWindowPos( ImGui::GetIO().DisplaySize * 0.5f, 0, Vec2( 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 300, 0 ) );
		ImGui::Begin( "loadout", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		ImGui::Text( "Primary weapons" );

		// this has to match up with the order in player.as
		// TODO: should make this less fragile
		const char * primaries[]   = { "EB + RL", "RL + LG", "EB + LG" };
		const char * secondaries[] = { "PG", "RG", "GL" };

		ImGui::Columns( ARRAY_COUNT( primaries ), NULL, false );

		for( size_t i = 0; i < ARRAY_COUNT( primaries ); i++ ) {
			int key = '1' + int( i );
			String< 128 > buf( "{}: {}", char( key ), primaries[ i ] );
			ImVec2 size = ImVec2( ImGui::GetColumnWidth( i ), 0 );
			if( ImGui::Selectable( buf, i == selected_primary, 0, size ) || pressed_key == key ) {
				selected_primary = i;
			}
			ImGui::NextColumn();
		}

		ImGui::Spacing();
		ImGui::Columns( 1 );

		ImGui::Text( "Secondary weapon" );

		ImGui::Columns( ARRAY_COUNT( secondaries ), NULL, false );

		bool selected_with_number = false;
		for( size_t i = 0; i < ARRAY_COUNT( secondaries ); i++ ) {
			int key = '1' + int( i + ARRAY_COUNT( primaries ) );
			String< 128 > buf( "{}: {}", char( key ), secondaries[ i ] );
			ImVec2 size = ImVec2( ImGui::GetColumnWidth( i ), 0 );
			if( ImGui::Selectable( buf, i == selected_secondary, 0, size ) || pressed_key == key ) {
				selected_secondary = i;
				selected_with_number = pressed_key == key;
			}
			ImGui::NextColumn();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Columns( 1 );

		if( ImGui::Button( "OK", ImVec2( -1, 0 ) ) || selected_with_number ) {
			const char * primaries_weapselect[] = { "ebrl", "rllg", "eblg" };
			String< 128 > buf( "weapselect {} {}\n", primaries_weapselect[ selected_primary ], secondaries[ selected_secondary ] );
			Cbuf_AddText( buf );
			should_close = true;
		}

		if( pressed_key == K_ESCAPE || pressed_key == 'b' ) {
			should_close = true;
		}

		ImGui::End();
	}
	else if( gamemenu_state == GameMenuState_Settings ) {
		ImVec2 pos = ImGui::GetIO().DisplaySize;
		pos.x *= 0.5f;
		pos.y *= 0.5f;
		ImGui::SetNextWindowPos( pos, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 600, 500 ) );
		ImGui::Begin( "settings", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		Settings();

		ImGui::End();
	}

	if( pressed_key == K_ESCAPE || should_close ) {
		uistate = UIState_Hidden;
		CL_SetKeyDest( key_game );
	}

	ImGui::PopStyleColor();
}

static void DemoMenu() {
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
	bool should_close = false;

	ImVec2 pos = ImGui::GetIO().DisplaySize;
	pos.x *= 0.5f;
	pos.y *= 0.8f;
	ImGui::SetNextWindowPos( pos, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
	ImGui::SetNextWindowSize( ImVec2( 600, 0 ) );
	ImGui::Begin( "demomenu", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

	GameMenuButton( cls.demo.paused ? "Play" : "Pause", "demopause" );
	GameMenuButton( "Jump +15s", "demojump +15" );
	GameMenuButton( "Jump -15s", "demojump -15" );

	GameMenuButton( "Disconnect to main menu", "disconnect", &should_close );
	GameMenuButton( "Exit to desktop", "quit", &should_close );

	ImGui::End();

	if( pressed_key == K_ESCAPE || should_close ) {
		uistate = UIState_Hidden;
		CL_SetKeyDest( key_game );
	}

	ImGui::PopStyleColor();
}

void UI_Refresh() {
	ZoneScoped;

	if( uistate == UIState_Hidden && !Con_IsVisible() ) {
		pressed_key = -1;
		return;
	}

	if( uistate == UIState_MainMenu ) {
		MainMenu();
	}

	if( uistate == UIState_Connecting ) {
		ImGui::SetNextWindowPos( ImVec2() );
		ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );
		ImGui::Begin( "mainmenu", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		ImGui::Text( "Connecting..." );

		ImGui::End();
	}

	if( uistate == UIState_GameMenu ) {
		GameMenu();
	}

	if( uistate == UIState_DemoMenu ) {
		DemoMenu();
	}

	if( Con_IsVisible() ) {
		Con_Draw( pressed_key );
	}

	Cbuf_Execute();

	pressed_key = -1;
}

void UI_UpdateConnectScreen() {
	uistate = UIState_Connecting;
	UI_Refresh();
}

void UI_KeyEvent( bool mainContext, int key, bool down ) {
	if( down ) {
		pressed_key = key;
	}

	if( key != K_ESCAPE ) {
		if( key == K_MWHEELDOWN || key == K_MWHEELUP ) {
			if( down )
				ImGui::GetIO().MouseWheel += key == K_MWHEELDOWN ? -1 : 1;
		}
		else if( key == K_LCTRL || key == K_RCTRL ) {
			ImGui::GetIO().KeyCtrl = down;
		}
		else if( key == K_LSHIFT || key == K_RSHIFT ) {
			ImGui::GetIO().KeyShift = down;
		}
		else if( key == K_LALT || key == K_RALT ) {
			ImGui::GetIO().KeyAlt = down;
		}
		else {
			ImGui::GetIO().KeysDown[ key ] = down;
		}
	}
}

void UI_CharEvent( bool mainContext, wchar_t key ) {
	ImGui::GetIO().AddInputCharacter( key );
}

void UI_ShowMainMenu() {
	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;
	S_StartMenuMusic();
	RefreshServerBrowser();
}

void UI_ShowGameMenu( bool spectating, bool ready ) {
	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Menu;
	is_spectating = spectating;
	is_ready = ready;
	CL_SetKeyDest( key_menu );
}

void UI_ShowDemoMenu() {
	uistate = UIState_DemoMenu;
	CL_SetKeyDest( key_menu );
}

void UI_HideMenu() {
	uistate = UIState_Hidden;
}

void UI_AddToServerList( const char * address, const char *info ) {
	if( size_t( num_servers ) < ARRAY_COUNT( servers ) ) {
		servers[ num_servers ].address = strdup( address );
		servers[ num_servers ].info = strdup( info );
		num_servers++;
	}
}

void UI_MouseSet( bool mainContext, int mx, int my, bool showCursor ) {
}

void UI_ShowLoadoutMenu( int primary, int secondary ) {
	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Loadout;
	selected_primary = primary;
	selected_secondary = secondary;
	CL_SetKeyDest( key_menu );
}
