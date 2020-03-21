#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "client/client.h"
#include "qcommon/version.h"
#include "qcommon/string.h"

#include "cgame/cg_local.h"

#define GLFW_INCLUDE_NONE
#include "glfw3/GLFW/glfw3.h"

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

	MainMenuState_ParticleEditor,
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
	SettingsState_Controls,
	SettingsState_Video,
	SettingsState_Audio,
};

struct Server {
	const char * address;

	const char * name;
	const char * map;
	int ping;
	int num_players;
	int max_players;
};

static Server servers[ 1024 ];
static int num_servers = 0;

static UIState uistate;

static MainMenuState mainmenu_state;
static int selected_server;
static size_t selected_map;

static GameMenuState gamemenu_state;
static constexpr int MAX_CASH = 500;
static WeaponType selected_weapons[ MAX_WEAPONS ];
static int num_weapons = 0;

static SettingsState settings_state;
static bool reset_video_settings;

static void ResetServerBrowser() {
	for( int i = 0; i < num_servers; i++ ) {
		free( const_cast< char * >( servers[ i ].address ) );
		free( const_cast< char * >( servers[ i ].name ) );
		free( const_cast< char * >( servers[ i ].map ) );
	}

	memset( servers, 0, sizeof( servers ) );

	num_servers = 0;
	selected_server = -1;
}

static void RefreshServerBrowser() {
	TempAllocator temp = cls.frame_arena.temp();

	ResetServerBrowser();

	for( const char * masterserver : MASTER_SERVERS ) {
		Cbuf_AddText( temp( "requestservers global {} {} full empty\n", masterserver, APPLICATION_NOSPACES ) );
	}

	Cbuf_AddText( "requestservers local full empty\n" );
}

void UI_Init() {
	ResetServerBrowser();
	InitParticleMenuEffect();

	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;

	reset_video_settings = true;
}

void UI_Shutdown() {
	ResetServerBrowser();
	ShutdownParticleEditor();
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

	ImGui::PushID( cvar_name );
	ImGui::InputText( "", buf, sizeof( buf ) );
	ImGui::PopID();

	Cvar_Set( cvar_name, buf );
}

static void CvarCheckbox( const char * label, const char * cvar_name, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	bool val = cvar->integer != 0;
	ImGui::PushID( cvar_name );
	ImGui::Checkbox( "", &val );
	ImGui::PopID();

	Cvar_Set( cvar_name, val ? "1" : "0" );
}

static void CvarSliderInt( const char * label, const char * cvar_name, int lo, int hi, const char * def, cvar_flag_t flags, const char * format = NULL ) {
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	int val = cvar->integer;
	ImGui::PushID( cvar_name );
	ImGui::SliderInt( "", &val, lo, hi, format );
	ImGui::PopID();

	Cvar_Set( cvar_name, temp( "{}", val ) );
}

static void CvarSliderFloat( const char * label, const char * cvar_name, float lo, float hi, const char * def, cvar_flag_t flags ) {
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	float val = cvar->value;
	ImGui::PushID( cvar_name );
	ImGui::SliderFloat( "", &val, lo, hi, "%.2f" );
	ImGui::PopID();

	char * buf = temp( "{}", val );
	RemoveTrailingZeroesFloat( buf );
	Cvar_Set( cvar_name, buf );
}

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

		const ImGuiIO & io = ImGui::GetIO();
		for( size_t i = 0; i < ARRAY_COUNT( io.KeysDown ); i++ ) {
			if( ImGui::IsKeyPressed( i ) ) {
				if( i != K_ESCAPE ) {
					Key_SetBinding( i, command );
				}
				ImGui::CloseCurrentPopup();
			}
		}

		if( ImGui::IsKeyReleased( K_MWHEELUP ) || ImGui::IsKeyReleased( K_MWHEELDOWN ) ) {
			Key_SetBinding( ImGui::IsKeyReleased( K_MWHEELUP ) ? K_MWHEELUP : K_MWHEELDOWN, command );
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
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );
	ImGui::PushItemWidth( 100 );
	ImGui::PushID( cvar_name );

	cvar_t * cvar = Cvar_Get( cvar_name, temp( "{}", def ), CVAR_ARCHIVE );

	int selected = cvar->integer;
	if( selected >= int( ARRAY_COUNT( TEAM_COLORS ) ) )
		selected = def;

	if( ImGui::BeginCombo( "", TEAM_COLORS[ selected ].name ) ) {
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
	ImGui::PopID();
	ImGui::PopItemWidth();

	Cvar_Set( cvar_name, temp( "{}", selected ) );
}

static void SettingsGeneral() {
	TempAllocator temp = cls.frame_arena.temp();

	CvarTextbox< MAX_NAME_CHARS >( "Name", "name", "Player", CVAR_USERINFO | CVAR_ARCHIVE );
	CvarSliderInt( "FOV", "fov", MIN_FOV, MAX_FOV, temp( "{}", MIN_FOV ), CVAR_ARCHIVE );
	CvarTeamColorCombo( "Ally color", "cg_allyColor", 0 );
	CvarTeamColorCombo( "Enemy color", "cg_enemyColor", 1 );

	CvarCheckbox( "Show hotkeys", "cg_showHotkeys", "1", CVAR_ARCHIVE );
	CvarCheckbox( "Show FPS", "cg_showFPS", "0", CVAR_ARCHIVE );
}

static void SettingsControls() {
	TempAllocator temp = cls.frame_arena.temp();

	static bool advweap_keys = false;

	ImGui::BeginChild( "binds" );

	if( ImGui::BeginTabBar("##binds", ImGuiTabBarFlags_None ) ) {
		if( ImGui::BeginTabItem( "Game" ) ) {
			ImGui::Separator();
			ImGui::Text( "Movement" );
			ImGui::Separator();

			KeyBindButton( "Forward", "+forward" );
			KeyBindButton( "Back", "+back" );
			KeyBindButton( "Left", "+left" );
			KeyBindButton( "Right", "+right" );
			KeyBindButton( "Jump", "+jump" );
			KeyBindButton( "Dash/walljump/zoom", "+special" );
			KeyBindButton( "Crouch", "+crouch" );
			KeyBindButton( "Walk", "+walk" );

			ImGui::Separator();
			ImGui::Text( "Actions" );
			ImGui::Separator();

			KeyBindButton( "Attack", "+attack" );
			KeyBindButton( "Reload", "+reload" );
			KeyBindButton( "Drop bomb", "drop" );
			KeyBindButton( "Shop", "gametypemenu" );
			KeyBindButton( "Scoreboard", "+scores" );
			KeyBindButton( "Chat", "messagemode" );
			KeyBindButton( "Team chat", "messagemode2" );

			ImGui::EndTabItem();
		}


		if( ImGui::BeginTabItem( "Weapons" ) ) {
			KeyBindButton( "Next weapon", "weapnext" );
			KeyBindButton( "Previous weapon", "weapprev" );

			ImGui::Separator();

			KeyBindButton( "Weapon 1", "weapon 1" );
			KeyBindButton( "Weapon 2", "weapon 2" );
			KeyBindButton( "Weapon 3", "weapon 3" );
			KeyBindButton( "Weapon 4", "weapon 4" );
			KeyBindButton( "Weapon 5", "weapon 5" );
			KeyBindButton( "Weapon 6", "weapon 6" );

			ImGui::Checkbox( "Advanced weapon keys", &advweap_keys );

			if( advweap_keys ) {
				for( int i = 0; i < Weapon_Count; i++ ) {
					const WeaponDef * weapon = GS_GetWeaponDef( i );
					KeyBindButton( weapon->name, temp( "use {}", weapon->short_name ) );
				}
			}

			ImGui::EndTabItem();
		} else {
			advweap_keys = false;
		}


		if( ImGui::BeginTabItem( "Mouse" ) ) {
			CvarSliderFloat( "Sensitivity", "sensitivity", 1.0f, 10.0f, "3", CVAR_ARCHIVE );
			CvarSliderFloat( "Horizontal sensitivity", "horizontalsensscale", 0.5f, 2.0f, "1", CVAR_ARCHIVE );
			CvarSliderFloat( "Acceleration", "m_accel", 0.0f, 1.0f, "0", CVAR_ARCHIVE );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Voice lines" ) ) {
			KeyBindButton( "Yes", "vsay yes" );
			KeyBindButton( "No", "vsay no" );
			KeyBindButton( "Thanks", "vsay thanks" );
			KeyBindButton( "Good game", "vsay goodgame" );
			KeyBindButton( "Boomstick", "vsay boomstick" );
			KeyBindButton( "Shut up", "vsay shutup" );
			KeyBindButton( "Bruh", "vsay bruh" );
			KeyBindButton( "Cya", "vsay cya" );
			KeyBindButton( "Get good", "vsay getgood" );
			KeyBindButton( "Hit the showers", "vsay hittheshowers" );
			KeyBindButton( "Lads", "vsay lads" );
			KeyBindButton( "Shit son", "vsay shitson" );
			KeyBindButton( "Trash smash", "vsay trashsmash" );
			KeyBindButton( "Wow your terrible", "vsay wowyourterrible" );

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

static const char * FullscreenModeToString( FullscreenMode mode ) {
	switch( mode ) {
		case FullscreenMode_Windowed: return "Windowed";
		case FullscreenMode_Borderless: return "Borderless";
		case FullscreenMode_Fullscreen: return "Fullscreen";
	}
	return NULL;
}

static void SettingsVideo() {
	static WindowMode mode;

	TempAllocator temp = cls.frame_arena.temp();

	if( reset_video_settings ) {
		mode = GetWindowMode();
		reset_video_settings = false;
	}

	SettingLabel( "Window mode" );
	ImGui::PushItemWidth( 200 );

	if( ImGui::BeginCombo( "##fullscreen", FullscreenModeToString( mode.fullscreen ) ) ) {
		if( ImGui::Selectable( FullscreenModeToString( FullscreenMode_Windowed ), mode.fullscreen == FullscreenMode_Windowed ) ) {
			mode.fullscreen = FullscreenMode_Windowed;
		}
		if( ImGui::Selectable( FullscreenModeToString( FullscreenMode_Borderless ), mode.fullscreen == FullscreenMode_Borderless ) ) {
			mode.fullscreen = FullscreenMode_Borderless;
		}
		if( ImGui::Selectable( FullscreenModeToString( FullscreenMode_Fullscreen ), mode.fullscreen == FullscreenMode_Fullscreen ) ) {
			mode.fullscreen = FullscreenMode_Fullscreen;
		}
		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();

	if( mode.fullscreen != FullscreenMode_Fullscreen ) {
		mode.video_mode.frequency = 0;
	}

	if( mode.fullscreen != FullscreenMode_Windowed ) {
		int num_monitors;
		GLFWmonitor ** monitors = glfwGetMonitors( &num_monitors );

		if( num_monitors > 1 ) {
			SettingLabel( "Monitor" );
			ImGui::PushItemWidth( 400 );

			if( ImGui::BeginCombo( "##monitor", glfwGetMonitorName( monitors[ mode.monitor ] ) ) ) {
				for( int i = 0; i < num_monitors; i++ ) {
					ImGui::PushID( i );
					if( ImGui::Selectable( glfwGetMonitorName( monitors[ i ] ), mode.monitor == i ) ) {
						mode.monitor = i;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
		}

		if( mode.fullscreen == FullscreenMode_Fullscreen ) {
			SettingLabel( "Resolution" );
			ImGui::PushItemWidth( 200 );

			if( mode.video_mode.frequency == 0 ) {
				mode.video_mode = GetVideoMode( mode.monitor );
			}

			if( ImGui::BeginCombo( "##resolution", temp( "{}", mode.video_mode ) ) ) {
				int num_modes;
				const GLFWvidmode * modes = glfwGetVideoModes( monitors[ mode.monitor ], &num_modes );

				for( int i = 0; i < num_modes; i++ ) {
					int idx = num_modes - i - 1;

					VideoMode m = { };
					m.width = modes[ idx ].width;
					m.height = modes[ idx ].height;
					m.frequency = modes[ idx ].refreshRate;

					bool is_selected = mode.video_mode.width == m.width && mode.video_mode.height == m.height && mode.video_mode.frequency == m.frequency;
					if( ImGui::Selectable( temp( "{}", m ), is_selected ) ) {
						mode.video_mode = m;
					}
				}
				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
		}
	}

	if( mode != GetWindowMode() ) {
		if( ImGui::Button( "Apply changes" ) ) {
			if( !mode.fullscreen ) {
				const GLFWvidmode * primary_mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );
				mode.video_mode.width = primary_mode->width * 0.8f;
				mode.video_mode.height = primary_mode->height * 0.8f;
				mode.x = -1;
				mode.y = -1;
			}

			Cvar_Set( "vid_mode", temp( "{}", mode ) );
			reset_video_settings = true;
		}

		ImGui::SameLine();

		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.75f, 0.125f, 0.125f, 1.f ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.75f, 0.25f, 0.2f, 1.f ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.5f, 0.1f, 0.1f, 1.f ) );
		if( ImGui::Button( "Discard changes" ) ) {
			reset_video_settings = true;
		}
		ImGui::PopStyleColor( 3 );
	}

	ImGui::Separator();

	{
		SettingLabel( "Anti-aliasing" );

		cvar_t * cvar = Cvar_Get( "r_samples", "0", CVAR_ARCHIVE );
		int samples = cvar->integer;

		ImGui::PushItemWidth( 100 );
		if( ImGui::BeginCombo( "##r_samples", samples == 0 ? "Off" : temp( "{}x", samples ) ) ) {
			if( ImGui::Selectable( "Off", samples == 0 ) )
				samples = 0;
			if( samples == 0 )
				ImGui::SetItemDefaultFocus();

			for( int i = 2; i <= 8; i *= 2 ) {
				if( ImGui::Selectable( temp( "{}x", i ), i == samples ) )
					samples = i;
				if( i == samples )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		if( samples > 1 ) {
			ImGui::SameLine();
			ImGui::Text( S_COLOR_RED "Enabling anti-aliasing can cause significant FPS drops!" );
		}

		Cvar_Set( "r_samples", temp( "{}", samples ) );
	}

	{
		SettingLabel( "Max FPS" );

		constexpr int values[] = { 60, 75, 120, 144, 165, 180, 200, 240, 333, 500, 1000 };

		cvar_t * cvar = Cvar_Get( "cl_maxfps", "250", CVAR_ARCHIVE );
		int maxfps = cvar->integer;

		ImGui::PushItemWidth( 100 );
		if( ImGui::BeginCombo( "##cl_maxfps", temp( "{}", maxfps ) ) ) {
			for( int value : values ) {
				if( ImGui::Selectable( temp( "{}", value ), maxfps == value ) )
					maxfps = value;
				if( value == maxfps )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		Cvar_Set( "cl_maxfps", temp( "{}", maxfps ) );
	}

	CvarCheckbox( "Vsync", "vid_vsync", "0", CVAR_ARCHIVE );
}

static void SettingsAudio() {
	CvarSliderFloat( "Master volume", "s_volume", 0.0f, 1.0f, "1", CVAR_ARCHIVE );
	CvarSliderFloat( "Music volume", "s_musicvolume", 0.0f, 1.0f, "1", CVAR_ARCHIVE );
	CvarCheckbox( "Mute when alt-tabbed", "s_muteinbackground", "1", CVAR_ARCHIVE );
}

static void Settings() {
	if( ImGui::Button( "GENERAL" ) ) {
		settings_state = SettingsState_General;
	}

	ImGui::SameLine();

	if( ImGui::Button( "CONTROLS" ) ) {
		settings_state = SettingsState_Controls;
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
	else if( settings_state == SettingsState_Controls )
		SettingsControls();
	else if( settings_state == SettingsState_Video )
		SettingsVideo();
	else if( settings_state == SettingsState_Audio )
		SettingsAudio();
}

static void ServerBrowser() {
	TempAllocator temp = cls.frame_arena.temp();

	ImGui::TextWrapped( "This game is very pre-alpha so there are probably 0 players online. Join the Discord to find games!" );
	if( ImGui::Button( "discord.gg/5ZbV4mF" ) ) {
		Sys_OpenInWebBrowser( "https://discord.gg/5ZbV4mF" );
	}
	ImGui::Separator();

	if( ImGui::Button( "Refresh" ) ) {
		RefreshServerBrowser();
	}

	ImGui::BeginChild( "servers" );
	ImGui::Columns( 4, "serverbrowser", false );

	ImGui::Text( "Server" );
	ImGui::NextColumn();
	ImGui::Text( "Map" );
	ImGui::NextColumn();
	ImGui::Text( "Players" );
	ImGui::NextColumn();
	ImGui::Text( "Ping" );
	ImGui::NextColumn();

	for( int i = 0; i < num_servers; i++ ) {
		const char * name = servers[ i ].name != NULL ? servers[ i ].name : servers[ i ].address;
		if( ImGui::Selectable( name, i == selected_server, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) ) {
			if( ImGui::IsMouseDoubleClicked( 0 ) ) {
				Cbuf_AddText( temp( "connect \"{}\"\n", servers[ i ].address ) );
			}
			selected_server = i;
		}
		ImGui::NextColumn();

		if( servers[ i ].name == NULL ) {
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
		}
		else {
			ImGui::Text( "%s", servers[ i ].map );
			ImGui::NextColumn();
			ImGui::Text( "%d/%d", servers[ i ].num_players, servers[ i ].max_players );
			ImGui::NextColumn();
			ImGui::Text( "%d", servers[ i ].ping );
			ImGui::NextColumn();
		}
	}

	ImGui::Columns( 1 );
	ImGui::EndChild();
}

static void CreateServer() {
	TempAllocator temp = cls.frame_arena.temp();

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

		Cvar_Set( "sv_maxclients", temp( "{}", maxclients ) );
	}

	{
		SettingLabel( "Map" );

		Span< const char * > maps = GetMapList();

		ImGui::PushItemWidth( 200 );
		if( ImGui::BeginCombo( "##map", maps[ selected_map ] ) ) {
			for( size_t i = 0; i < maps.n; i++ ) {
				if( ImGui::Selectable( maps[ i ], i == selected_map ) )
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
		Span< const char * > maps = GetMapList();
		if( selected_map < maps.n ) {
			Cbuf_AddText( temp( "map \"{}\"\n", maps[ selected_map ] ) );
		}
	}
}

static void MainMenu() {
	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );

	bool parteditor_wason = mainmenu_state == MainMenuState_ParticleEditor;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

	ImGui::Begin( "mainmenu", WindowZOrder_Menu, flags );

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

	if( cl_devtools->integer != 0 ) {
		ImGui::SameLine( 0, 50 );
		ImGui::AlignTextToFramePadding();
		ImGui::Text( "Dev tools:" );

		ImGui::SameLine();

		if( ImGui::Button( "Particle editor" ) ) {
			mainmenu_state = MainMenuState_ParticleEditor;
			ResetParticleEditor();
		}
	}

	if( parteditor_wason && mainmenu_state != MainMenuState_ParticleEditor ) {
		ResetParticleMenuEffect();
	}

	ImGui::Separator();

	switch( mainmenu_state ) {
		case MainMenuState_ServerBrowser: ServerBrowser(); break;
		case MainMenuState_CreateServer: CreateServer(); break;
		case MainMenuState_Settings: Settings(); break;
		case MainMenuState_ParticleEditor: DrawParticleEditor(); break;
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

		ImGuiWindowFlags credits_flags = ( ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoTitleBar ) | ImGuiWindowFlags_NoMove;
		if( ImGui::BeginPopupModal( "Credits", NULL, credits_flags ) ) {
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
	TempAllocator temp = cls.frame_arena.temp();

	ImVec2 size = ImVec2( -1, 0 );
	if( column != -1 ) {
		ImGuiStyle & style = ImGui::GetStyle();
		size = ImVec2( ImGui::GetColumnWidth( 0 ) - style.ItemSpacing.x, 0 );
	}

	if( ImGui::Button( label, size ) ) {
		Cbuf_AddText( temp( "{}\n", command ) );
		if( clicked != NULL )
			*clicked = true;
	}
}

static int SelectedWeaponIndex( WeaponType weapon ) {
	for( int i = 0; i < MAX_WEAPONS; i++ ) {
		if( selected_weapons[ i ] == weapon ) {
			return i;
		}
	}
	
	return -1;
}

static bool WeaponButton( int cash, WeaponType weapon, ImVec2 size, Vec4 * tint ) {
	ImGui::PushStyleColor( ImGuiCol_Button, Vec4( 0 ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, Vec4( 0 ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonActive, Vec4( 0 ) );
	defer { ImGui::PopStyleColor( 3 ); };

	const Material * icon = cgs.media.shaderWeaponIcon[ weapon ];
	Vec2 half_pixel = 0.5f / Vec2( icon->texture->width, icon->texture->height );

	const WeaponDef * weap_def = GS_GetWeaponDef( weapon );
	bool selected = SelectedWeaponIndex( weapon ) != -1;

	if( !selected && weap_def->cost > cash ) {
		*tint = Vec4( 1.0f, 1.0f, 1.0f, 0.125f );
		ImGui::Image( icon, size, half_pixel, 1.0f - half_pixel, *tint );
		return false;
	}

	*tint = vec4_white;
	if( !selected ) {
		tint->w = 0.5f;
	}

	int weaponBinds[ 2 ] = { -1, -1 };
	CG_GetBoundKeycodes( va( "use %s", weap_def->short_name ), weaponBinds );
	return ImGui::ImageButton( icon, size, half_pixel, 1.0f - half_pixel, 0, Vec4( 0 ), *tint ) || ImGui::IsKeyPressed( weaponBinds[ 0 ], false ) || ImGui::IsKeyPressed( weaponBinds[ 1 ], false );
}


static void GameMenu() {
	bool spectating = cg.predictedPlayerState.real_team == TEAM_SPECTATOR;
	bool ready = false;

	if( GS_MatchState( &client_gs ) <= MATCH_STATE_WARMUP ) {
		ready = cg.predictedPlayerState.ready;
	} else if( GS_MatchState( &client_gs ) == MATCH_STATE_COUNTDOWN ) {
		ready = true;
	}

	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
	bool should_close = false;

	if( gamemenu_state == GameMenuState_Menu ) {
		ImGui::SetNextWindowPos( ImGui::GetIO().DisplaySize * 0.5f, 0, Vec2( 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 300, 0 ) );
		ImGui::Begin( "gamemenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );
		ImGuiStyle & style = ImGui::GetStyle();
		const double half = ImGui::GetWindowWidth() / 2 - style.ItemSpacing.x - style.ItemInnerSpacing.x;

		if( spectating ) {
			if( GS_TeamBasedGametype( &client_gs ) ) {
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
			if( GS_MatchState( &client_gs ) <= MATCH_STATE_COUNTDOWN ) {
				if( ImGui::Checkbox( ready ? "Ready!" : "Not ready", &ready ) ) {
					Cbuf_AddText( "toggleready\n" );
				}
			}

			GameMenuButton( "Spectate", "spec", &should_close );

			if( GS_TeamBasedGametype( &client_gs ) ) {
				GameMenuButton( "Change loadout", "gametypemenu", &should_close );
			}
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
	}
	else if( gamemenu_state == GameMenuState_Loadout ) {
		ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 255 ) );
		ImGui::SetNextWindowPos( Vec2( 0, 0 ) );
		ImGui::SetNextWindowSize( ImGui::GetIO().DisplaySize );
		ImGui::Begin( "Loadout", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		int hovered = Weapon_Count;

		int cash = MAX_CASH;
		for( int i = 0; i < MAX_WEAPONS; i++ ) {
			cash -= GS_GetWeaponDef( selected_weapons[ i ] )->cost;
		}

		// this has to match up with the order in player.as
		// TODO: should make this less fragile

		Vec2 window_size = ImGui::GetWindowSize();

		TempAllocator temp = cls.frame_arena.temp();
		const Vec2 icon_size = Vec2( window_size.x * 0.1f );
		const int desc_width = window_size.x * 0.375f;
		int desc_height;
		const bool bigger_font = window_size.y >= 864;

		{
			ImGui::Columns( 10, NULL, false );
			ImGui::SetColumnWidth( 0, window_size.x * 0.075f );
			ImGui::SetColumnWidth( 1, icon_size.x );
			ImGui::SetColumnWidth( 2, window_size.x * 0.05f );
			ImGui::SetColumnWidth( 3, icon_size.x );
			ImGui::SetColumnWidth( 4, window_size.x * 0.05f );
			ImGui::SetColumnWidth( 5, icon_size.x );
			ImGui::SetColumnWidth( 4, window_size.x * 0.05f );
			ImGui::SetColumnWidth( 5, icon_size.x );
			ImGui::SetColumnWidth( 6, window_size.x * 0.075f );
			ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, ImGui::GetStyle().ItemSpacing.y ) );
			ImGui::NextColumn();

			constexpr WeaponType weapon_order[] = {
				Weapon_Railgun,
				Weapon_RocketLauncher,
				Weapon_Laser,
				Weapon_Sniper,
				Weapon_AssaultRifle,
				Weapon_Deagle,
				Weapon_MachineGun,
				Weapon_Shotgun,
				Weapon_Plasma,
				Weapon_GrenadeLauncher,
				Weapon_Pistol,
				Weapon_Rifle,
			};

			// weapon grid
			{

				if( bigger_font ) ImGui::PushFont( cls.medium_font );
				for( size_t i = 0; i < ARRAY_COUNT( weapon_order ); i++ ) {
					WeaponType weapon = weapon_order[ i ];

					Vec4 tint;
					if( WeaponButton( cash, weapon, icon_size, &tint ) ) {
						int weap_pos = SelectedWeaponIndex( weapon );
						if( weap_pos == -1 ) {
							selected_weapons[ num_weapons ] = weapon;
							num_weapons++;
						} else {
							num_weapons--;

							if( weap_pos == MAX_WEAPONS - 1 ) {
								selected_weapons[ weap_pos ] = Weapon_None;
							} else {
								for( int i = weap_pos; i < num_weapons; i++ ) {
									selected_weapons[ i ] = selected_weapons[ i + 1 ];
								}
								selected_weapons[ num_weapons ] = Weapon_None;
							}

						}
					}

					if( ImGui::IsItemHovered() ) {
						hovered = weapon;
					}

					int cost = GS_GetWeaponDef( weapon )->cost;
					ImGuiColorToken token = ImGuiColorToken( 255 * tint.x, 255 * tint.y, 255 * tint.z, 255 * tint.w );
					ColumnCenterText( temp( "{}{}: {}", token, i + 1, GS_GetWeaponDef( weapon )->name ) );
					ColumnCenterText( temp( "{}${}.{02}", token, cost / 100, cost % 100 ) );

					if( i % 3 == 2 ) {
						ImGui::NextColumn();
						ImGui::NextColumn();
					}
				}

				{
					const Material * icon = FindMaterial( "weapons/weap_none" );
					Vec2 half_pixel = 0.5f / Vec2( icon->texture->width, icon->texture->height );
					ImGuiColorToken pink = ImGuiColorToken( 255, 53, 255, 64 );

					ImGui::Image( icon, icon_size, half_pixel, 1.0f - half_pixel, Vec4( 1.0f, 1.0f, 1.0f, 0.25f ) );
					ColumnCenterText( temp( "{}Dud bomb", pink ) );
					ColumnCenterText( temp( "{}$13.37", pink ) );

					desc_height = ImGui::GetCursorPosY();
				}

				if( bigger_font ) ImGui::PopFont();

				if( ARRAY_COUNT( weapon_order ) % 3 != 0 ) {
					ImGui::NextColumn();
					ImGui::NextColumn();
				}
			}

			ImGui::PopStyleVar();

			{
				// Weapon description
				if( hovered != Weapon_Count ) {
					ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 225 ) );

					const Material * icon = cgs.media.shaderWeaponIcon[ hovered ];
					Vec2 half_pixel = 0.5f / Vec2( icon->texture->width, icon->texture->height );
					const WeaponDef * weapon = GS_GetWeaponDef( hovered );

					ImGui::PushStyleVar( ImGuiStyleVar_ChildBorderSize, 4 );
					ImGui::BeginChild( "weapondescription", ImVec2( desc_width, desc_height - ImGui::GetStyle().WindowPadding.y*2 ), true );

					ImGui::Columns( 2, NULL, false );
					ImGui::SetColumnWidth( 0, icon_size.x * 0.5f + ImGui::GetStyle().WindowPadding.x*2 );

					ImGui::Image( icon, icon_size * 0.5f, half_pixel, 1.0f - half_pixel );
					ImGui::NextColumn();

					if( bigger_font ) ImGui::PushFont( cls.big_font );
					ImGui::Text( "%s", weapon->name );
					if( bigger_font ) ImGui::PopFont();
					if( !bigger_font ) ImGui::PushFont( cls.console_font );
					ImGui::TextWrapped( "%s", temp( "{}{}", ImGuiColorToken( 150, 150, 150, 255 ), weapon->description ) );
					if( !bigger_font ) ImGui::PopFont();

					ImGui::NextColumn();
					ImGui::Columns( 1 );

					ImGui::Separator();

					int pos_y = ImGui::GetCursorPosY();
					if( bigger_font ) ImGui::PushFont( cls.medium_font );

					const float val_spacing = ( desc_height - pos_y )*0.075f;
					const float txt_spacing = ImGui::GetTextLineHeight() + 10;

					pos_y +=  val_spacing;
					ImGui::SetCursorPosY( pos_y );
					ColumnCenterText( temp( "{}Type", ImGuiColorToken( 255, 200, 0, 255 ) ) );
					ImGui::SetCursorPosY(pos_y + txt_spacing );
					ColumnCenterText( weapon->speed == 0 ? "Hitscan" : "Projectile" );

					pos_y = ImGui::GetCursorPosY() + val_spacing;
					ImGui::SetCursorPosY( pos_y );
					ColumnCenterText( temp( "{}Damage", ImGuiColorToken( 255, 200, 0, 255 ) ) );
					ImGui::SetCursorPosY( pos_y + txt_spacing );
					ColumnCenterText( temp( "{}", int( weapon->damage ) ) );

					pos_y = ImGui::GetCursorPosY() + val_spacing;
					ImGui::SetCursorPosY( pos_y );
					ColumnCenterText( temp("{}Reload", ImGuiColorToken( 255, 200, 0, 255 ) ) );
					ImGui::SetCursorPosY( pos_y + txt_spacing );

					char * reload = temp( "{.1}s", weapon->refire_time / 1000.f );
					RemoveTrailingZeroesFloat( reload );
					ColumnCenterText( reload );

					pos_y = ImGui::GetCursorPosY() + val_spacing;
					ImGui::SetCursorPosY( pos_y );
					ColumnCenterText( temp( "{}Cost", ImGuiColorToken( 255, 200, 0, 255 ) ) );

					ImGui::SetCursorPosY(pos_y + txt_spacing );
					ColumnCenterText( temp( "${}.{02}", weapon->cost / 100, weapon->cost % 100 ) );

					if( bigger_font ) ImGui::PopFont();
					ImGui::EndChild();
					ImGui::PopStyleVar();
					ImGui::PopStyleColor();
				}

				ImGui::SetCursorPosY( desc_height + ImGui::GetStyle().WindowPadding.y*2 );
				if( bigger_font ) ImGui::PushFont( cls.medium_font );
				ImGuiColorToken c = cash == 0 ? ImGuiColorToken( 255, 255, 255, 255 ) : ImGuiColorToken( RGBA8( AttentionGettingColor() ) );
				ImGui::Text( "%s", temp( "{}CASH: {}${}.{02}{}{}", c, S_COLOR_GREEN, cash / 100, cash % 100, c, cash == 0 ? "" : "!!!" ) );
				if( bigger_font ) ImGui::PopFont();
			}


			if( bigger_font ) ImGui::PushFont( cls.large_font );
			const int button_height = ImGui::GetTextLineHeight()*1.5f;

			ImGui::SetCursorPosY( window_size.y - ImGui::GetTextLineHeight()*2 );
			ImGui::Columns( 6, NULL, false );

			ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.75f, 0.125f, 0.125f, 1.f ) );
			ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.75f, 0.25f, 0.2f, 1.f ) );
			ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.5f, 0.1f, 0.1f, 1.f ) );

			if( ImGui::Button( "Clear", ImVec2( -1, button_height ) ) ) {
				for( WeaponType &w : selected_weapons ) {
					w = Weapon_None;
				}

				num_weapons = 0;
			} ImGui::PopStyleColor( 3 );

			ImGui::NextColumn();
			ImGui::NextColumn();

			if( ImGui::Button( "OK", ImVec2( -1, button_height ) ) || ImGui::CloseKey( K_ENTER ) ) {
				DynamicString loadout( &temp, "weapselect" );
				for( size_t i = 0; i < ARRAY_COUNT( selected_weapons ); i++ ) {
					if( selected_weapons[ i ] != Weapon_None ) {
						loadout.append( " {}", selected_weapons[ i ] );
					}
				}
				loadout += "\n";

				Cbuf_AddText( loadout.c_str() );
				should_close = true;
			}

			ImGui::NextColumn();

			if( ImGui::Button( "Cancel", ImVec2( -1, button_height ) ) ) {
				should_close = true;
			} if( bigger_font ) ImGui::PopFont();
		}

		ImGui::PopStyleColor();
	}
	else if( gamemenu_state == GameMenuState_Settings ) {
		ImVec2 pos = ImGui::GetIO().DisplaySize;
		pos.x *= 0.5f;
		pos.y *= 0.5f;
		ImGui::SetNextWindowPos( pos, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 600, 500 ) );
		ImGui::Begin( "settings", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		Settings();
	}

	if( ImGui::CloseKey( K_ESCAPE ) || should_close ) {
		uistate = UIState_Hidden;
		CL_SetKeyDest( key_game );
	}

	ImGui::End();

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
	ImGui::Begin( "demomenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

	GameMenuButton( cls.demo.paused ? "Play" : "Pause", "demopause" );
	GameMenuButton( "Jump +15s", "demojump +15" );
	GameMenuButton( "Jump -15s", "demojump -15" );

	GameMenuButton( "Disconnect to main menu", "disconnect", &should_close );
	GameMenuButton( "Exit to desktop", "quit", &should_close );

	if( ImGui::CloseKey( K_ESCAPE ) || should_close ) {
		uistate = UIState_Hidden;
		CL_SetKeyDest( key_game );
	}

	ImGui::End();

	ImGui::PopStyleColor();
}

void UI_Refresh() {
	ZoneScoped;

	if( uistate == UIState_Hidden && !Con_IsVisible() ) {
		return;
	}

	if( uistate == UIState_GameMenu ) {
		GameMenu();
	}

	if( uistate == UIState_DemoMenu ) {
		DemoMenu();
	}

	if( uistate == UIState_MainMenu ) {
		if( mainmenu_state != MainMenuState_ParticleEditor ) {
			DrawParticleMenuEffect();
		}

		MainMenu();
	}

	if( uistate == UIState_Connecting ) {
		if( mainmenu_state != MainMenuState_ParticleEditor ) {
			DrawParticleMenuEffect();
		}

		const char * connecting = "Connecting...";
		ImGui::SetNextWindowPos( ImVec2() );
		ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );
		ImGui::Begin( "mainmenu", WindowZOrder_Menu, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		ImGui::PushFont( cls.large_font );
		ImGui::SetCursorPos( ( ImGui::GetWindowSize() - ImGui::CalcTextSize( connecting ) )/2 );
		ImGui::Text( "%s", connecting );
		ImGui::PopFont();

		ImGui::End();
	}


	if( Con_IsVisible() ) {
		Con_Draw();
	}

	Cbuf_Execute();
}

void UI_ShowConnectingScreen() {
	uistate = UIState_Connecting;
}

void UI_ShowMainMenu() {
	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;
	S_StartMenuMusic();
	RefreshServerBrowser();
}

void UI_ShowGameMenu() {
	// so the menu doesn't instantly close
	ImGui::GetIO().KeysDown[ K_ESCAPE ] = false;

	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Menu;
	CL_SetKeyDest( key_menu );
}

void UI_ShowDemoMenu() {
	// so the menu doesn't instantly close
	ImGui::GetIO().KeysDown[ K_ESCAPE ] = false;

	uistate = UIState_DemoMenu;
	CL_SetKeyDest( key_menu );
}

void UI_HideMenu() {
	uistate = UIState_Hidden;
}

void UI_AddToServerList( const char * address, const char * info ) {
	for( int i = 0; i < num_servers; i++ ) {
		if( strcmp( address, servers[ i ].address ) == 0 ) {
			char name[ 128 ];
			char map[ 32 ];
			int parsed = sscanf( info, "\\\\ping\\\\%d\\\\n\\\\%127[^\\]\\\\m\\\\ %31[^\\]\\\\u\\\\%d/%d\\\\EOT", &servers[ i ].ping, name, map, &servers[ i ].num_players, &servers[ i ].max_players );

			if( parsed == 5 ) {
				servers[ i ].name = strdup( name );
				servers[ i ].map = strdup( map );
			}

			return;
		}
	}

	if( size_t( num_servers ) < ARRAY_COUNT( servers ) ) {
		servers[ num_servers ].address = strdup( address );
		num_servers++;

		if( strcmp( info, "\\\\EOT" ) == 0 ) {
			TempAllocator temp = cls.frame_arena.temp();
			Cbuf_AddText( temp( "pingserver {}\n", address ) );
		}
	}
}

void UI_ShowLoadoutMenu( Span< int > weapons ) {
	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Loadout;

	for( WeaponType & w : selected_weapons ) {
		w = Weapon_None;
	}

	num_weapons = 0;
	for( int w : weapons ) {
		selected_weapons[ num_weapons ] = w;
		num_weapons++;
	}

	CL_SetKeyDest( key_menu );
}
