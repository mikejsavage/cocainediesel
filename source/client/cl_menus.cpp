#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "client/client.h"
#include "client/renderer/renderer.h"
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
	MainMenuState_RagdollEditor,
};

enum GameMenuState {
	GameMenuState_Menu,
	GameMenuState_Loadout,
	GameMenuState_Settings,
	GameMenuState_Vote,
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

static GameMenuState gamemenu_state;
static WeaponType selected_weapons[ WeaponCategory_Count ];

static SettingsState settings_state;
static bool reset_video_settings;

static void PushButtonColor( ImVec4 color ) {
	ImGui::PushStyleColor( ImGuiCol_Button, color );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( color.x + 0.125f, color.y + 0.125f, color.z + 0.125f, color.w ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( color.x - 0.125f, color.y - 0.125f, color.z - 0.125f, color.w ) );
}

static void ResetServerBrowser() {
	for( int i = 0; i < num_servers; i++ ) {
		FREE( sys_allocator, const_cast< char * >( servers[ i ].address ) );
		FREE( sys_allocator, const_cast< char * >( servers[ i ].name ) );
		FREE( sys_allocator, const_cast< char * >( servers[ i ].map ) );
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
	InitRagdollEditor();
	// InitParticleMenuEffect();

	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_RagdollEditor;

	reset_video_settings = true;
}

void UI_Shutdown() {
	ResetServerBrowser();
	// ShutdownParticleEditor();
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

static const char * SelectableMapList() {
	Span< const char * > maps = GetMapList();
	static size_t selected_map = 0;

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

	return ( selected_map < maps.n ? maps[ selected_map ] : "" );
}

static void SettingsGeneral() {
	TempAllocator temp = cls.frame_arena.temp();

	CvarTextbox< MAX_NAME_CHARS >( "Name", "name", "Player", CVAR_USERINFO | CVAR_ARCHIVE );

	CvarCheckbox( "Show chat", "cg_chat", "1", CVAR_ARCHIVE );
	CvarCheckbox( "Show hotkeys", "cg_showHotkeys", "1", CVAR_ARCHIVE );
	CvarCheckbox( "Show FPS", "cg_showFPS", "0", CVAR_ARCHIVE );
	CvarCheckbox( "Show speed", "cg_showSpeed", "0", CVAR_ARCHIVE );
}

static void SettingsControls() {
	TempAllocator temp = cls.frame_arena.temp();

	ImGui::BeginChild( "binds" );

	if( ImGui::BeginTabBar( "##binds", ImGuiTabBarFlags_None ) ) {
		if( ImGui::BeginTabItem( "Game" ) ) {
			KeyBindButton( "Forward", "+forward" );
			KeyBindButton( "Back", "+back" );
			KeyBindButton( "Left", "+left" );
			KeyBindButton( "Right", "+right" );
			KeyBindButton( "Jump", "+jump" );
			KeyBindButton( "Dash", "+special" );

			ImGui::Separator();

			KeyBindButton( "Attack", "+attack" );
			KeyBindButton( "Reload", "+reload" );
			KeyBindButton( "Plant bomb", "+crouch" );
			KeyBindButton( "Drop bomb", "drop" );
			KeyBindButton( "Shop", "gametypemenu" );
			KeyBindButton( "Scoreboard", "+scores" );

			ImGui::Separator();

			KeyBindButton( "Chat", "messagemode" );
			KeyBindButton( "Team chat", "messagemode2" );
			KeyBindButton( "Spray", "spray" );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Weapons" ) ) {
			KeyBindButton( "Melee", "weapon 1" );
			KeyBindButton( "Primary", "weapon 2" );
			KeyBindButton( "Secondary", "weapon 3" );
			KeyBindButton( "Backup", "weapon 4" );
			KeyBindButton( "Next weapon", "weapnext" );
			KeyBindButton( "Previous weapon", "weapprev" );

			ImGui::BeginChild( "weapon", ImVec2( 400, -1 ) );
			if( ImGui::CollapsingHeader( "Advanced" ) ) {
				for( int i = Weapon_Knife; i < Weapon_Count; i++ ) {
					const WeaponDef * weapon = GS_GetWeaponDef( i );
					KeyBindButton( weapon->name, temp( "use {}", weapon->short_name ) );
				}
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Mouse" ) ) {
			CvarSliderFloat( "Sensitivity", "sensitivity", 1.0f, 10.0f, "3", CVAR_ARCHIVE );
			CvarSliderFloat( "Horizontal sensitivity", "horizontalsensscale", 0.5f, 2.0f, "1", CVAR_ARCHIVE );
			CvarSliderFloat( "Acceleration", "m_accel", 0.0f, 1.0f, "0", CVAR_ARCHIVE );
			CvarCheckbox( "Invert Y axis", "m_invertY", "0", CVAR_ARCHIVE );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Voice lines" ) ) {
			KeyBindButton( "Acne pack", "vsay acne" );
			KeyBindButton( "Valley pack", "vsay valley" );
			KeyBindButton( "Mike pack", "vsay mike" );
			KeyBindButton( "User pack", "vsay user" );
			KeyBindButton( "Guyman pack", "vsay guyman" );
			KeyBindButton( "Helena pack", "vsay helena" );

			ImGui::BeginChild( "voice", ImVec2( 400, -1 ) );
			if( ImGui::CollapsingHeader( "Advanced" ) ) {
				KeyBindButton( "Sorry", "vsay sorry" );
				KeyBindButton( "Thanks", "vsay thanks" );
				KeyBindButton( "Good game", "vsay goodgame" );
				KeyBindButton( "Boomstick", "vsay boomstick" );
				KeyBindButton( "Shut up", "vsay shutup" );
				KeyBindButton( "Bruh", "vsay bruh" );
				KeyBindButton( "Cya", "vsay cya" );
				KeyBindButton( "Get good", "vsay getgood" );
				KeyBindButton( "Hit the showers", "vsay hittheshowers" );
				KeyBindButton( "Lads", "vsay lads" );
				KeyBindButton( "She doesn't even", "vsay shedoesnteven" );
				KeyBindButton( "Shit son", "vsay shitson" );
				KeyBindButton( "Trash smash", "vsay trashsmash" );
				KeyBindButton( "What the shit", "vsay whattheshit" );
				KeyBindButton( "Wow your terrible", "vsay wowyourterrible" );
			} ImGui::EndChild();

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Misc" ) ) {
			KeyBindButton( "Vote yes", "vote yes" );
			KeyBindButton( "Vote no", "vote no" );
			KeyBindButton( "Join team", "join" );
			KeyBindButton( "Ready", "toggleready" );
			KeyBindButton( "Spectate", "chase" );
			KeyBindButton( "Screenshot", "screenshot" );

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

		PushButtonColor( ImVec4( 0.5f, 0.5f, 0.5f, 1.f ) );
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
			ImGui::Text( S_COLOR_WHITE "Enabling anti-aliasing can cause significant FPS drops!" );
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

static const char * CleanAudioDeviceName( const char * name ) {
	const char * openal_prefix = "OpenAL Soft on ";
	if( StartsWith( name, openal_prefix ) ) {
		return name + strlen( openal_prefix );
	}
	return name;
}

static void SettingsAudio() {
	SettingLabel( "Audio device" );
	ImGui::PushItemWidth( 400 );

	const char * current = strcmp( s_device->string, "" ) == 0 ? "Default" : s_device->string;
	if( ImGui::BeginCombo( "##audio_device", CleanAudioDeviceName( current ) ) ) {
		if( ImGui::Selectable( "Default", strcmp( s_device->string, "" ) == 0 ) ) {
			Cvar_Set( "s_device", "" );
		}

		const char * device = GetAudioDevicesAsSequentialStrings();
		while( strcmp( device, "" ) != 0 ) {
			if( ImGui::Selectable( CleanAudioDeviceName( device ), strcmp( device, s_device->string ) == 0 ) ) {
				Cvar_Set( "s_device", device );
			}
			device += strlen( device ) + 1;
		}
		ImGui::EndCombo();
	}

	if( ImGui::Button( "Test" ) ) {
		S_StartLocalSound( FindSoundEffect( "sounds/announcer/bomb/ace" ), CHAN_AUTO, 1.0f );
	}

	ImGui::Separator();

	CvarSliderFloat( "Master volume", "s_volume", 0.0f, 1.0f, "1", CVAR_ARCHIVE );
	CvarSliderFloat( "Music volume", "s_musicvolume", 0.0f, 1.0f, "0.5", CVAR_ARCHIVE );
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

	if( ImGui::Button( "discord.gg/5ZbV4mF" ) ) {
		Sys_OpenInWebBrowser( "https://discord.gg/5ZbV4mF" );
	}
	ImGui::SameLine();
	ImGui::TextWrapped( "This game is very pre-alpha so there are probably 0 players online. Join the Discord to find games!" );
	ImGui::Separator();

	char server_filter[ 256 ] = { };
	if( ImGui::Button( "Refresh" ) ) {
		RefreshServerBrowser();
	}
	ImGui::AlignTextToFramePadding();
	ImGui::SameLine(); ImGui::Text( "Search");
	ImGui::SameLine(); ImGui::InputText( "", server_filter, sizeof( server_filter ) );


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
		if( strstr( name, server_filter ) != NULL ) {
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

		maxclients = Clamp( 1, maxclients, 64 );

		Cvar_Set( "sv_maxclients", temp( "{}", maxclients ) );
	}


	SettingLabel( "Map" );

	const char * map_name = SelectableMapList();

	CvarCheckbox( "Public", "sv_public", "0", CVAR_LATCH );

	if( ImGui::Button( "Create server" ) ) {
		Cbuf_AddText( temp( "map \"{}\"\n", map_name ) );
	}
}

static void MainMenu() {
	static bool change_name_popup = false;
	static const char * player_name = Cvar_Get( "name", "Player", CVAR_USERINFO | CVAR_ARCHIVE )->string;

	TempAllocator temp = cls.frame_arena.temp();

	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );

	bool parteditor_wason = mainmenu_state == MainMenuState_ParticleEditor;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

	ImGui::Begin( "mainmenu", WindowZOrder_Menu, flags );

	//Set your nickname if its the default one
	if( !change_name_popup && strcmp( "Player", player_name ) == 0 ) {
		ImGui::OpenPopup( "change name" );
	} else {
		change_name_popup = true;
	}

	if( ImGui::BeginPopupModal( "change name", NULL, ImGuiWindowFlags_NoDecoration ) ) {
		ImGui::BeginChild( "nameset", ImVec2( 500, 150 ) );
		ImGui::Text( "Change your nickname" );

		CvarTextbox< MAX_NAME_CHARS >( "Name", "name", "Player", CVAR_USERINFO | CVAR_ARCHIVE );

		if( ImGui::Button( "Ok", ImVec2( -1, 0 ) ) || ImGui::Hotkey( K_ESCAPE ) ) {
			change_name_popup = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndChild();
		ImGui::EndPopup();
	}



	ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

	ImGui::BeginChild( "mainmenubody", ImVec2( 0, -ImGui::GetFrameHeightWithSpacing() + window_padding.y ) );

	auto triangel = []( s64 x, s64 period ) {
		float normalized = float( x % period ) / period;
		return normalized < 0.5f ? Unlerp( 0.0f, normalized, 0.5f ) : Unlerp( 1.0f, normalized, 0.5f );
	};

	auto glitch = []( s64 x ) {
		s64 a = x % 697;
		s64 b = x % 531;
		return a + b < 300;
	};

	ImGui::SetCursorPosX( 40.0f * triangel( cls.monotonicTime, 631 ) );
	ImGui::PushFont( cls.large_font );
	ImGui::PushStyleColor( ImGuiCol_Text, glitch( cls.monotonicTime ) ? IM_COL32( 255, 255, 255, 255 ) : IM_COL32( 32, 182, 252, 255 ) );
	ImGui::Text( "VACCAINE PFIZEL" );
	ImGui::PopStyleColor();
	ImGui::PopFont();

	if( ImGui::Button( "FIND SERVERS" ) ) {
		mainmenu_state = MainMenuState_ServerBrowser;
	}

	ImGui::SameLine();

	if( ImGui::Button( "CREATE SERVER" ) ) {
		mainmenu_state = MainMenuState_CreateServer;
	}

	ImGui::SameLine();

	if( ImGui::Button( "SETTINGS" ) ) {
		mainmenu_state = MainMenuState_Settings;
		settings_state = SettingsState_General;
	}

	ImGui::SameLine();

	PushButtonColor( ImVec4( 0.375f, 0.f, 0.f, 0.75f ) );
	if( ImGui::Button( "QUIT" ) ) {
		CL_Quit();
	} ImGui::PopStyleColor( 3 );

	if( cl_devtools->integer != 0 ) {
		ImGui::SameLine();

		if( DevToolButton( "Particle editor" ) ) {
			mainmenu_state = MainMenuState_ParticleEditor;
			// ResetParticleEditor();
		}

		ImGui::SameLine();

		if( DevToolButton( "Ragdoll editor" ) ) {
			mainmenu_state = MainMenuState_RagdollEditor;
			ResetRagdollEditor();
		}
	}

	if( parteditor_wason && mainmenu_state != MainMenuState_ParticleEditor ) {
		// ResetParticleMenuEffect();
	}

	ImGui::Separator();

	switch( mainmenu_state ) {
		case MainMenuState_ServerBrowser: ServerBrowser(); break;
		case MainMenuState_CreateServer: CreateServer(); break;
		case MainMenuState_Settings: Settings(); break;
		// case MainMenuState_ParticleEditor: DrawParticleEditor(); break;
		case MainMenuState_RagdollEditor: DrawRagdollEditor(); break;
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
			ImGui::Text( "Dexter - programming" );
			ImGui::Text( "general adnic - voice acting" );
			ImGui::Text( "goochie - art & programming" );
			ImGui::Text( "MSC - programming" );
			ImGui::Text( "MikeJS - programming" );
			ImGui::Text( "Obani - music & fx & programming" );
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

static void WeaponTooltip( const WeaponDef * def ) {
	if( ImGui::IsItemHovered() ) {
		ImGui::BeginTooltip();

		TempAllocator temp = cls.frame_arena.temp();

		ImGui::Text( "%s", temp( "{}Weapon: {}{}", ImGuiColorToken( 255, 200, 0, 255 ), ImGuiColorToken( 255, 255, 255, 255 ), def->name ) );
		ImGui::Text( "%s", temp( "{}Type: {}{}", ImGuiColorToken( 255, 200, 0, 255 ), ImGuiColorToken( 255, 255, 255, 255 ), def->speed == 0 ? "Hitscan" : "Projectile" ) );
		ImGui::Text( "%s", temp( "{}Damage: {}{}", ImGuiColorToken( 255, 200, 0, 255 ), ImGuiColorToken( 255, 255, 255, 255 ), int( def->damage ) ) );
		char * reload = temp( "{.1}s", def->refire_time / 1000.f );
		RemoveTrailingZeroesFloat( reload );
		ImGui::Text( "%s", temp( "{}Reload: {}{}", ImGuiColorToken( 255, 200, 0, 255 ), ImGuiColorToken( 255, 255, 255, 255 ), reload ) );

		ImGui::EndTooltip();
	}
}

static void SendLoadout() {
	TempAllocator temp = cls.frame_arena.temp();

	DynamicString loadout( &temp, "weapselect" );

	for( size_t i = 0; i < ARRAY_COUNT( selected_weapons ); i++ ) {
		if( selected_weapons[ i ] != Weapon_None ) {
			loadout.append( " {}", selected_weapons[ i ] );
		}
	}
	loadout += "\n";

	Cbuf_AddText( loadout.c_str() );
}

static void WeaponButton( WeaponType weapon, Vec2 size ) {
	ImGui::PushStyleColor( ImGuiCol_Button, vec4_black );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, Vec4( 0.1f, 0.1f, 0.1f, 1.0f ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonActive, Vec4( 0.2f, 0.2f, 0.2f, 1.0f ) );
	defer { ImGui::PopStyleColor( 3 ); };

	ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 2 );
	ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 0 );
	defer { ImGui::PopStyleVar( 2 ); };

	const WeaponDef * def = GS_GetWeaponDef( weapon );
	bool selected = selected_weapons[ def->category ] == weapon;

	const Material * icon = cgs.media.shaderWeaponIcon[ weapon ];
	Vec2 half_pixel = HalfPixelSize( icon );
	Vec4 color = selected ? vec4_green : vec4_white;

	ImGui::PushStyleColor( ImGuiCol_Border, color );
	defer { ImGui::PopStyleColor(); };

	bool clicked = ImGui::ImageButton( icon, size, half_pixel, 1.0f - half_pixel, 5, Vec4( 0.0f ), color );

	WeaponTooltip( def );

	ImGui::SameLine();
	ImGui::Dummy( Vec2( 16, 0 ) );
	ImGui::SameLine();

	int weaponBinds[ 2 ] = { -1, -1 };
	CG_GetBoundKeycodes( va( "use %s", def->short_name ), weaponBinds );

	if( clicked || ImGui::Hotkey( weaponBinds[ 0 ] ) || ImGui::Hotkey( weaponBinds[ 1 ] ) ) {
		selected_weapons[ def->category ] = selected ? WeaponType( Weapon_None ) : weapon;
		SendLoadout();
	}
}

static void LoadoutCategory( const char * label, WeaponCategory category, Vec2 icon_size ) {
	ImGui::Text( "%s", label );
	ImGui::Dummy( ImVec2( 0, icon_size.y * 1.5f ) );
	ImGui::NextColumn();

	for( WeaponType i = 0; i < Weapon_Count; i++ ) {
		const WeaponDef * def = GS_GetWeaponDef( i );
		if( def->category == category ) {
			WeaponButton( i, icon_size );
		}
	}

	ImGui::NextColumn();
}

static bool LoadoutMenu( Vec2 displaySize ) {
	ImGui::PushFont( cls.medium_font );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 255 ) );
	ImGui::SetNextWindowPos( Vec2( 0, 0 ) );
	ImGui::SetNextWindowSize( displaySize );
	ImGui::Begin( "Loadout", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

	const Vec2 icon_size = Vec2( displaySize.x * 0.075f );

	ImGui::Columns( 2, NULL, false );
	ImGui::SetColumnWidth( 0, 300 );

	LoadoutCategory( "Primary", WeaponCategory_Primary, icon_size );
	LoadoutCategory( "Secondary", WeaponCategory_Secondary, icon_size );
	LoadoutCategory( "Backup", WeaponCategory_Backup, icon_size );

	ImGui::EndColumns();

	int loadoutKeys[ 2 ] = { };
	CG_GetBoundKeycodes( "gametypemenu", loadoutKeys );

	bool should_close = false;
	if( ImGui::Hotkey( loadoutKeys[ 0 ] ) || ImGui::Hotkey( loadoutKeys[ 1 ] ) ) {
		should_close = true;
	}

	ImGui::PopStyleColor();
	ImGui::PopFont();

	return should_close;
}

static void GameMenu() {
	bool spectating = cg.predictedPlayerState.real_team == TEAM_SPECTATOR;
	bool ready = false;

	if( GS_MatchState( &client_gs ) <= MATCH_STATE_WARMUP ) {
		ready = cg.predictedPlayerState.ready;
	}
	else if( GS_MatchState( &client_gs ) == MATCH_STATE_COUNTDOWN ) {
		ready = true;
	}

	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
	bool should_close = false;

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	if( gamemenu_state == GameMenuState_Menu ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, 0, Vec2( 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 500, 0 ) );
		ImGui::Begin( "gamemenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );
		ImGuiStyle & style = ImGui::GetStyle();
		const double half = ImGui::GetWindowWidth() / 2 - style.ItemSpacing.x - style.ItemInnerSpacing.x;
		bool team_based = GS_TeamBasedGametype( &client_gs );

		if( spectating ) {
			if( team_based ) {
				GameMenuButton( "Auto-join", "join", &should_close );

				ImGui::Columns( 2, NULL, false );
				ImGui::SetColumnWidth( 0, half );
				ImGui::SetColumnWidth( 1, half );


				PushButtonColor( CG_TeamColorVec4( TEAM_ALPHA ) * 0.5f );
				GameMenuButton( "Join Corona", "join cocaine", &should_close, 0 );
				ImGui::PopStyleColor( 3 );

				ImGui::NextColumn();

				PushButtonColor( CG_TeamColorVec4( TEAM_BETA ) * 0.5f );
				GameMenuButton( "Join Diesel", "join diesel", &should_close, 1 );
				ImGui::PopStyleColor( 3 );

				ImGui::NextColumn();
			}
			else {
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


			if( team_based ) {
				PushButtonColor( CG_TeamColorVec4( TEAM_ENEMY ) * 0.5f );
				GameMenuButton( "Switch team", "join", &should_close );
				ImGui::PopStyleColor( 3 );
			}


			GameMenuButton( "Spectate", "chase", &should_close );

			if( team_based ) {
				GameMenuButton( "Change weapons", "gametypemenu", &should_close );
			}
		}

		if( ImGui::Button( "Start a vote", ImVec2( -1, 0 ) ) ) {
			gamemenu_state = GameMenuState_Vote;
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
		if( LoadoutMenu( displaySize ) ) {
			should_close = true;
		}
	}
	else if( gamemenu_state == GameMenuState_Vote ) {
		TempAllocator temp = cls.frame_arena.temp();
		ImGui::SetNextWindowPos( displaySize * 0.5f, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( -1, -1 ) );
		ImGui::Begin( "votemap", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		static int e = 0;
		ImGui::RadioButton( "Start match", &e, 0 ); ImGui::SameLine();
		ImGui::RadioButton( "Change map", &e, 1 );

		if( e == 0 ) {
			GameMenuButton( "Start vote", "callvote allready", &should_close );
		}

		if( e == 1 ) {
			const char * map_name = SelectableMapList();
			GameMenuButton( "Start vote", temp( "callvote map {}", map_name ), &should_close );
		}
	}
	else if( gamemenu_state == GameMenuState_Settings ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( Max2( 800.f, displaySize.x * 0.65f ), Max2( 600.f, displaySize.y * 0.65f ) ) );
		ImGui::Begin( "settings", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );

		Settings();
	}

	if( ImGui::Hotkey( K_ESCAPE ) || should_close ) {
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

	if( ImGui::Hotkey( K_ESCAPE ) || should_close ) {
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
				servers[ i ].name = CopyString( sys_allocator, name );
				servers[ i ].map = CopyString( sys_allocator, map );
			}

			return;
		}
	}

	if( size_t( num_servers ) < ARRAY_COUNT( servers ) ) {
		servers[ num_servers ].address = CopyString( sys_allocator, address );
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

	for( int w : weapons ) {
		if( w <= Weapon_None || w >= Weapon_Count )
			return;

		WeaponCategory category = GS_GetWeaponDef( w )->category;
		if( category == WeaponCategory_Count || selected_weapons[ category ] != Weapon_None )
			return;

		selected_weapons[ category ] = w;
	}

	CL_SetKeyDest( key_menu );
}
