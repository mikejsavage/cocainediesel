#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "client/assets.h"
#include "client/client.h"
#include "client/demo_browser.h"
#include "client/server_browser.h"
#include "client/renderer/renderer.h"
#include "qcommon/array.h"
#include "qcommon/maplist.h"
#include "qcommon/time.h"
#include "qcommon/version.h"

#include "cgame/cg_local.h"

#define GLFW_INCLUDE_NONE
#include "glfw3/GLFW/glfw3.h"

#include "glad/glad.h"

enum UIState {
	UIState_Hidden,
	UIState_MainMenu,
	UIState_Connecting,
	UIState_GameMenu,
	UIState_DemoMenu,
};

enum MainMenuState {
	MainMenuState_ServerBrowser,
	MainMenuState_DemoBrowser,
	MainMenuState_CreateServer,
	MainMenuState_Settings,

	MainMenuState_ParticleEditor,
};

enum GameMenuState {
	GameMenuState_Menu,
	GameMenuState_Loadout,
	GameMenuState_Settings,
	GameMenuState_Vote,
};

enum DemoMenuState {
	DemoMenuState_Menu,
	DemoMenuState_Settings,
};

enum SettingsState {
	SettingsState_General,
	SettingsState_Controls,
	SettingsState_Video,
	SettingsState_Audio,
};

static UIState uistate;

static MainMenuState mainmenu_state;
static GameMenuState gamemenu_state;
static DemoMenuState demomenu_state;

static int selected_server;

static bool yolodemo;

static Loadout loadout;

static SettingsState settings_state;
static bool reset_video_settings;
static float sensivity_range[] = { 0.25f, 10.f };

static size_t selected_mask = 0;
static NonRAIIDynamicArray< char * > masks;
static const char * masks_folder = "models/masks/";


static void PushButtonColor( ImVec4 color ) {
	ImGui::PushStyleColor( ImGuiCol_Button, color );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( color.x + 0.125f, color.y + 0.125f, color.z + 0.125f, color.w ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( color.x - 0.125f, color.y - 0.125f, color.z - 0.125f, color.w ) );
}

static void ResetServerBrowser() {
	selected_server = -1;
}

static void ClearMasksList() {
	for( char * mask : masks ) {
		FREE( sys_allocator, mask );
	}
	masks.clear();
}

static void SetMask( const char * mask_name ) {
	TempAllocator temp = cls.frame_arena.temp();
	char * path = temp( "{}{}", masks_folder, mask_name );
	Cvar_Set( "cg_mask", path );
}

static void RefreshMasksList() {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();
	ClearMasksList();

	masks.add( CopyString( sys_allocator, "None" ) );
	for( const char * path : AssetPaths() ) {
		if( !StartsWith( path, masks_folder ) || !EndsWith( path, ".glb" ) )
			continue;

		masks.add( ( *sys_allocator )( "{}", StripPrefix( StripExtension( path ), masks_folder ) ) );
	}


	const char * mask = Cvar_String( "cg_mask" );
	for( size_t i = 0; i < masks.size(); i++ ) {
		if( StrEqual( mask, temp( "{}{}", masks_folder, masks[ i ] ) ) ) {
			selected_mask = i;
			return;
		}
	}

	SetMask( masks[ 0 ] );
}

static void Refresh() {
	ResetServerBrowser();
	RefreshServerBrowser();
}

void UI_Init() {
	masks.init( sys_allocator );
	ResetServerBrowser();
	RefreshMasksList();
	yolodemo = false;
	// InitParticleMenuEffect();

	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;

	reset_video_settings = true;
}

void UI_Shutdown() {
	ClearMasksList();
	masks.shutdown();
	// ShutdownParticleEditor();
}

static void SettingLabel( const char * label ) {
	ImGui::AlignTextToFramePadding();
	ImGui::Text( "%s", label );
	ImGui::SameLine( 200 );
}

template< size_t maxlen >
static void CvarTextbox( const char * label, const char * cvar_name ) {
	SettingLabel( label );

	char buf[ maxlen + 1 ];
	Q_strncpyz( buf, Cvar_String( cvar_name ), sizeof( buf ) );

	ImGui::PushID( cvar_name );
	ImGui::InputText( "", buf, sizeof( buf ) );
	ImGui::PopID();

	Cvar_Set( cvar_name, buf );
}

static void CvarCheckbox( const char * label, const char * cvar_name ) {
	SettingLabel( label );

	bool val = Cvar_Bool( cvar_name );
	ImGui::PushID( cvar_name );
	ImGui::Checkbox( "", &val );
	ImGui::PopID();

	Cvar_Set( cvar_name, val ? "1" : "0" );
}

static void CvarSliderInt( const char * label, const char * cvar_name, int lo, int hi ) {
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );

	int val = Cvar_Integer( cvar_name );
	ImGui::PushID( cvar_name );
	ImGui::SliderInt( "", &val, lo, hi, NULL );
	ImGui::PopID();

	char * buf = temp( "{}", val );
	Cvar_Set( cvar_name, buf );
}

static void CvarSliderFloat( const char * label, const char * cvar_name, float lo, float hi ) {
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );

	float val = Cvar_Float( cvar_name );
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
		ImGui::Text( "Press a key to set a new bind, or press DEL to delete it (ESCAPE to cancel)" );

		ImGuiIO & io = ImGui::GetIO();
		for( size_t i = 0; i < ARRAY_COUNT( io.KeysDown ); i++ ) {
			if( ImGui::IsKeyPressed( i ) ) {
				if( i == K_DEL ) {
					int binds[ 2 ];
					int num_binds = CG_GetBoundKeycodes( command, binds );
					for( int j = 0; j < num_binds; j++ ) {
						Key_SetBinding( binds[ j ], NULL );
					}
				} else if( i != K_ESCAPE ) {
					Key_SetBinding( i, command );
				}
				ImGui::CloseCurrentPopup();

				// consume the escape so we don't close the ingame menu
				io.KeysDown[ K_ESCAPE ] = false;
				io.KeysDownDuration[ K_ESCAPE ] = -1.0f;
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

static const char * SelectablePlayerList() {
	TempAllocator temp = cls.frame_arena.temp();
	DynamicArray< const char * > players( &temp );

	for( int i = 0; i < client_gs.maxclients; i++ ) {
		const char * name = PlayerName( i );
		if( strlen( name ) != 0 && !ISVIEWERENTITY( i + 1 ) ) {
			players.add( name );
		}
	}

	static size_t selected_player = 0;

	ImGui::PushItemWidth( 200 );
	if( ImGui::BeginCombo( "##players", players[ selected_player ] ) ) {
		for( size_t i = 0; i < players.size(); i++ ) {
			if( ImGui::Selectable( players[ i ], i == selected_player ) )
				selected_player = i;
			if( i == selected_player )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	return ( selected_player < players.size() ? players[ selected_player ] : "" );
}

static void MasksList() {
	SettingLabel( "Mask" );

	ImGui::PushItemWidth( 200 );
	if( ImGui::BeginCombo( "##masks", masks[ selected_mask ] ) ) {
		for( size_t i = 0; i < masks.size(); i++ ) {
			if( ImGui::Selectable( masks[ i ], i == selected_mask ) ) {
				selected_mask = i;
				SetMask( masks[ i ] );
			}

			if( i == selected_mask ) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
}

static void SettingsGeneral() {
	TempAllocator temp = cls.frame_arena.temp();

	CvarTextbox< MAX_NAME_CHARS >( "Name", "name" );

	CvarSliderInt( "Crosshair size", "cg_crosshair_size", 1, 50 );
	CvarSliderInt( "Crosshair gap", "cg_crosshair_gap", 0, 50 );
	CvarCheckbox( "Dynamic crosshair", "cg_crosshair_dynamic" );

	CvarCheckbox( "Show chat", "cg_chat" );
	CvarCheckbox( "Show hotkeys", "cg_showHotkeys" );
	CvarCheckbox( "Show FPS", "cg_showFPS" );

	MasksList();
}

static void SettingsControls() {
	TempAllocator temp = cls.frame_arena.temp();

	ImGui::BeginChild( "binds" );

	PushButtonColor( ImVec4( 0.375f, 0.f, 0.f, 0.75f ) );
	if( ImGui::Button("Reset to default") ) {
		Key_Unbindall();
		ExecDefaultCfg();
	} ImGui::PopStyleColor( 3 );

	if( ImGui::BeginTabBar( "##binds", ImGuiTabBarFlags_None ) ) {
		if( ImGui::BeginTabItem( "Game" ) ) {
			KeyBindButton( "Forward", "+forward" );
			KeyBindButton( "Back", "+back" );
			KeyBindButton( "Left", "+left" );
			KeyBindButton( "Right", "+right" );
			KeyBindButton( "Movement ability 1", "+ability1" );
			KeyBindButton( "Movement ability 2", "+ability2" );

			ImGui::Separator();

			KeyBindButton( "Attack", "+attack1" );
			KeyBindButton( "Scope (weapon specific)", "+attack2" ); //will be changed to secondary when it makes sense
			KeyBindButton( "Reload", "+reload" );
			KeyBindButton( "Use gadget", "+gadget" );
			KeyBindButton( "Plant bomb", "+plant" );
			KeyBindButton( "Drop bomb", "drop" );
			KeyBindButton( "Shop", "loadoutmenu" );
			KeyBindButton( "Scoreboard", "+scores" );

			ImGui::Separator();

			KeyBindButton( "Chat", "chat" );
			KeyBindButton( "Team chat", "teamchat" );
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
			KeyBindButton( "Last weapon", "lastweapon" );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Mouse" ) ) {
			CvarSliderFloat( "Sensitivity", "sensitivity", sensivity_range[ 0 ], sensivity_range[ 1 ] );
			CvarSliderFloat( "Horizontal sensitivity", "horizontalsensscale", 0.5f, 2.0f );
			CvarCheckbox( "Invert Y axis", "m_invertY" );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Voices" ) ) {
			KeyBindButton( "Acne pack", "vsay acne" );
			KeyBindButton( "Fart pack", "vsay fart" );
			KeyBindButton( "Guyman pack", "vsay guyman" );
			KeyBindButton( "Dodonga pack", "vsay dodonga" );
			KeyBindButton( "Helena pack", "vsay helena" );
			KeyBindButton( "Larp pack", "vsay larp" );
			KeyBindButton( "Fam pack", "vsay fam" );
			KeyBindButton( "Mike pack", "vsay mike" );
			KeyBindButton( "User pack", "vsay user" );
			KeyBindButton( "Valley pack", "vsay valley" );
			KeyBindButton( "Zombie pack", "vsay zombie" );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Misc" ) ) {
			KeyBindButton( "Vote yes", "vote_yes" );
			KeyBindButton( "Vote no", "vote_no" );
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

		int samples = Cvar_Integer( "r_samples" );

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
		SettingLabel( "Shadow Quality" );

		ShadowQuality quality = ShadowQuality( Cvar_Integer( "r_shadow_quality" ) );

		ImGui::PushItemWidth( 150 );
		if( ImGui::BeginCombo( "##r_shadow_quality", ShadowQualityToString( quality ) ) ) {
			for( int s = ShadowQuality_Low; s <= ShadowQuality_Ultra; s++ ) {
				if( ImGui::Selectable( ShadowQualityToString( ShadowQuality( s ) ), quality == s ) )
					quality = ShadowQuality( s );
				if( s == quality )
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		Cvar_Set( "r_shadow_quality", temp( "{}", quality ) );
	}

	{
		SettingLabel( "Max FPS" );

		constexpr int values[] = { 60, 75, 120, 144, 165, 180, 200, 240, 333, 500, 1000 };

		int maxfps = Cvar_Integer( "cl_maxfps" );

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

	CvarCheckbox( "Vsync", "vid_vsync" );

	CvarCheckbox( "Colorblind mode", "cg_colorBlind" );
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

	const char * current = StrEqual( s_device->value, "" ) ? "Default" : s_device->value;
	if( ImGui::BeginCombo( "##audio_device", CleanAudioDeviceName( current ) ) ) {
		if( ImGui::Selectable( "Default", StrEqual( s_device->value, "" ) ) ) {
			Cvar_Set( "s_device", "" );
		}

		TempAllocator temp = cls.frame_arena.temp();
		for( const char * device : GetAudioDevices( &temp ) ) {
			if( ImGui::Selectable( CleanAudioDeviceName( device ), StrEqual( device, s_device->value ) ) ) {
				Cvar_Set( "s_device", device );
			}
		}
		ImGui::EndCombo();
	}

	if( ImGui::Button( "Test" ) ) {
		PlaySFX( "sounds/announcer/bomb/ace" );
	}

	ImGui::Separator();

	CvarSliderFloat( "Master volume", "s_volume", 0.0f, 1.0f );
	CvarSliderFloat( "Music volume", "s_musicvolume", 0.0f, 1.0f );
	CvarCheckbox( "Mute when alt-tabbed", "s_muteinbackground" );
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

	if( ImGui::Button( "Refresh" ) ) {
		Refresh();
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

	Span< const ServerBrowserEntry > servers = GetServerBrowserEntries();
	for( size_t i = 0; i < servers.n; i++ ) {
		if( ImGui::Selectable( servers[ i ].name, i == selected_server, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) ) {
			if( ImGui::IsMouseDoubleClicked( 0 ) ) {
				CL_Connect( servers[ i ].address );
			}
			selected_server = i;
		}
		ImGui::NextColumn();

		ImGui::Text( "%s", servers[ i ].map );
		ImGui::NextColumn();
		ImGui::Text( "%d/%d", servers[ i ].num_players, servers[ i ].max_players );
		ImGui::NextColumn();
		ImGui::Text( "%d", servers[ i ].ping );
		ImGui::NextColumn();
	}

	ImGui::Columns( 1 );
	ImGui::EndChild();
}

static void DemoBrowser() {
	TempAllocator temp = cls.frame_arena.temp();

	DemoBrowserFrame();

	ImGui::Checkbox( "Try to force load demos from old versions. Comes with no warranty", &yolodemo );

	ImGui::Columns( 5, "demobrowser", false );

	ImGui::Text( "Filename" );
	ImGui::NextColumn();
	ImGui::Text( "Server" );
	ImGui::NextColumn();
	ImGui::Text( "Map" );
	ImGui::NextColumn();
	ImGui::Text( "Date" );
	ImGui::NextColumn();
	ImGui::Text( "Game version" );
	ImGui::NextColumn();

	ImGui::Columns( 1 );
	ImGui::BeginChild( "demos" );
	ImGui::Columns( 5 );

	for( const DemoBrowserEntry & demo : GetDemoBrowserEntries() ) {
		bool clicked = ImGui::Selectable( demo.path, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick );
		ImGui::NextColumn();
		ImGui::Text( "%s", demo.server );
		ImGui::NextColumn();
		ImGui::Text( "%s", demo.map );
		ImGui::NextColumn();
		ImGui::Text( "%s", demo.date );
		ImGui::NextColumn();

		bool old_version = !StrEqual( demo.version, APP_VERSION );
		ImGui::PushStyleColor( ImGuiCol_Text, old_version ? vec4_red : vec4_green );
		ImGui::Text( "%s", demo.version );
		ImGui::NextColumn();
		ImGui::PopStyleColor();

		if( clicked && ImGui::IsMouseDoubleClicked( 0 ) ) {
			const char * cmd = yolodemo ? "yolodemo" : "demo";
			Cbuf_Add( "{} \"{}\"", cmd, demo.path );
		}
	}

	ImGui::Columns( 1 );
	ImGui::EndChild();
}

static void CreateServer() {
	TempAllocator temp = cls.frame_arena.temp();

	CvarTextbox< 128 >( "Server name", "sv_hostname" );

	{
		int maxclients = Cvar_Integer( "sv_maxclients" );

		SettingLabel( "Max players" );
		ImGui::PushItemWidth( 150 );
		ImGui::InputInt( "##sv_maxclients", &maxclients );
		ImGui::PopItemWidth();

		maxclients = Clamp( 1, maxclients, 64 );

		Cvar_Set( "sv_maxclients", temp( "{}", maxclients ) );
	}


	SettingLabel( "Map" );

	const char * map_name = SelectableMapList();

	CvarCheckbox( "Public", "sv_public" );

	if( ImGui::Button( "Create server" ) ) {
		Cbuf_Add( "map \"{}\"", map_name );
	}
}

static void MainMenu() {
	TempAllocator temp = cls.frame_arena.temp();

	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );

	bool parteditor_wason = mainmenu_state == MainMenuState_ParticleEditor;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_Interactive;

	ImGui::Begin( "mainmenu", WindowZOrder_Menu, flags );

	ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

	ImGui::BeginChild( "mainmenubody", ImVec2( 0, -ImGui::GetFrameHeightWithSpacing() + window_padding.y ) );

	u32 ukraine_blue = IM_COL32( 0, 87, 183, 255 );
	u32 ukraine_yellow = IM_COL32( 255, 215, 0, 255 );
	ImGui::PushFont( cls.large_font );
	ImGui::PushStyleColor( ImGuiCol_Text, ukraine_blue );
	ImGui::Text( "UKRAINE" );
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor( ImGuiCol_Text, ukraine_yellow );
	ImGui::Text( "DIESEL" );
	ImGui::PopStyleColor();
	ImGui::PopFont();

	ImGui::SetCursorPosX( -1000.0f + 500.0f * Sin( cls.monotonicTime, Milliseconds( 6029 ) ) );
	ImGui::PushFont( cls.idi_nahui_font );
	constexpr const char * idi_nahui = u8"\u0418\u0434\u0438 \u043d\u0430 \u0445\u0443\u0439";
	for( int i = 0; i < 100; i++ ) {
		ImGui::PushStyleColor( ImGuiCol_Text, i % 2 == 0 ? ukraine_blue : ukraine_yellow );
		ImGui::Text( idi_nahui );
		ImGui::PopStyleColor();
		if( i < 99 ) {
			ImGui::SameLine();
		}
	}
	ImGui::PopFont();

	if( ImGui::Button( "FIND SERVERS" ) ) {
		mainmenu_state = MainMenuState_ServerBrowser;
		ResetServerBrowser();
	}

	ImGui::SameLine();

	if( ImGui::Button( "REPLAYS" ) ) {
		mainmenu_state = MainMenuState_DemoBrowser;
		RefreshDemoBrowser();
		yolodemo = false;
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
		Com_DeferQuit();
	} ImGui::PopStyleColor( 3 );

	if( cl_devtools->integer != 0 ) {
		ImGui::SameLine( 0, 50 );
		ImGui::AlignTextToFramePadding();
		ImGui::Text( "Dev tools:" );

		ImGui::SameLine();

		if( ImGui::Button( "Particle editor" ) ) {
			mainmenu_state = MainMenuState_ParticleEditor;
			// ResetParticleEditor();
		}
	}

	if( parteditor_wason && mainmenu_state != MainMenuState_ParticleEditor ) {
		// ResetParticleMenuEffect();
	}

	ImGui::Separator();

	switch( mainmenu_state ) {
		case MainMenuState_ServerBrowser: ServerBrowser(); break;
		case MainMenuState_DemoBrowser: DemoBrowser(); break;
		case MainMenuState_CreateServer: CreateServer(); break;
		case MainMenuState_Settings: Settings(); break;
		// case MainMenuState_ParticleEditor: DrawParticleEditor(); break;
	}

	ImGui::EndChild();

	{
		ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_Button, IM_COL32( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, IM_COL32( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonActive, IM_COL32( 0, 0, 0, 0 ) );

		const char * buf = APP_VERSION u8" \u00A9 AHA CHEERS";
		ImVec2 size = ImGui::CalcTextSize( buf );
		ImGui::SetCursorPosX( ImGui::GetWindowWidth() - size.x - window_padding.x - 1.0f - Sin( cls.monotonicTime, Milliseconds( 182 ) ) );

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
			ImGui::Text( "MikeJS - programming" );
			ImGui::Text( "MSC - programming & art" );
			ImGui::Text( "Obani - music & fx & programming" );
			ImGui::Text( "Rhodanathema - art" );
			ImGui::Separator();
			ImGui::Text( "jwzr - medical research" );
			ImGui::Text( "naxeron - chief propagandist" );
			ImGui::Text( "zmiles - american cultural advisor" );
			ImGui::Separator();
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
		Cbuf_Add( "{}", command );
		if( clicked != NULL )
			*clicked = true;
	}
}

static void SendLoadout() {
	TempAllocator temp = cls.frame_arena.temp();
	Cbuf_Add( "setloadout {}", loadout );
}

static Vec4 RGBA8ToVec4NosRGB( RGBA8 rgba ) {
	return Vec4( rgba.r / 255.0f, rgba.g / 255.0f, rgba.b / 255.0f, rgba.a / 255.0f );
}

static bool LoadoutButton( const char * label, Vec2 icon_size, const Material * icon, bool selected ) {
	ImGui::TableNextColumn();

	ImGui::PushStyleColor( ImGuiCol_Button, vec4_black );
	ImGui::PushStyleColor( ImGuiCol_ButtonHovered, Vec4( 0.1f, 0.1f, 0.1f, 1.0f ) );
	ImGui::PushStyleColor( ImGuiCol_ButtonActive, Vec4( 0.2f, 0.2f, 0.2f, 1.0f ) );
	defer { ImGui::PopStyleColor( 3 ); };

	ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1 );
	ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 0 );
	defer { ImGui::PopStyleVar( 2 ); };

	Vec2 half_pixel = HalfPixelSize( icon );
	Vec4 color = RGBA8ToVec4NosRGB( selected ? rgba8_diesel_yellow : rgba8_white ); // TODO...

	ImGui::PushStyleColor( ImGuiCol_Text, color );
	ImGui::PushStyleColor( ImGuiCol_Border, color );
	defer { ImGui::PopStyleColor( 2 ); };

	ImGui::PushID( label );
	CellCenter( icon_size.x );
	bool clicked = ImGui::ImageButton( icon, icon_size, half_pixel, 1.0f - half_pixel, 5, Vec4( 0.0f ), color );
	ImGui::PopID();

	CellCenterText( label );

	return clicked;
}

static void Perks( Vec2 icon_size ) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex( 0 );
	ImGui::Text( "CLASS" );
	ImGui::Dummy( ImVec2( 0, icon_size.y * 1.5f ) );

	for( PerkType i = PerkType( Perk_None + 1 ); i < Perk_Count; i++ ) {
		if( !GetPerkDef( i )->enabled )
			continue;

		const Material * icon = FindMaterial( cgs.media.shaderPerkIcon[ i ] );
		if( LoadoutButton( GetPerkDef( i )->name, icon_size, icon, loadout.perk == i ) ) {
			loadout.perk = i;
			SendLoadout();
		}
	}
}

static int CountWeaponCategory( WeaponCategory category ) {
	int n = 0;
	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		const WeaponDef * def = GS_GetWeaponDef( i );
		if( def->category == category ) {
			n++;
		}
	}
	return n;
}

static void LoadoutCategory( const char * label, WeaponCategory category, Vec2 icon_size ) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex( 0 );
	ImGui::Text( "%s", label );
	ImGui::Dummy( ImVec2( 0, icon_size.y * 1.5f ) );

	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		const WeaponDef * def = GS_GetWeaponDef( i );
		if( def->category == category ) {
			const Material * icon = FindMaterial( cgs.media.shaderWeaponIcon[ i ] );
			if( LoadoutButton( def->name, icon_size, icon, loadout.weapons[ def->category ] == i ) ) {
				loadout.weapons[ def->category ] = i;
				SendLoadout();
			}
		}
	}
}

static void Gadgets( Vec2 icon_size ) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex( 0 );
	ImGui::Text( "GADGET" );
	ImGui::Dummy( ImVec2( 0, icon_size.y * 1.5f ) );

	for( GadgetType i = GadgetType( Gadget_None + 1 ); i < Gadget_Count; i++ ) {
		const GadgetDef * def = GetGadgetDef( i );
		const Material * icon = FindMaterial( cgs.media.shaderGadgetIcon[ i ] );
		if( LoadoutButton( def->name, icon_size, icon, loadout.gadget == i ) ) {
			loadout.gadget = GadgetType( i );
			SendLoadout();
		}
	}
}

static bool LoadoutMenu( Vec2 displaySize ) {
	ImGui::PushFont( cls.medium_italic_font );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 255 ) );
	ImGui::SetNextWindowPos( Vec2( 0, 0 ) );
	ImGui::SetNextWindowSize( displaySize );
	ImGui::Begin( "Loadout", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, Vec2( 0, displaySize.y * 0.02 ) );
	Vec2 icon_size = Vec2( displaySize.x * 0.04f );

	int cols = 0;
	cols = Max2( CountWeaponCategory( WeaponCategory_Primary ), cols );
	cols = Max2( CountWeaponCategory( WeaponCategory_Secondary ), cols );
	cols = Max2( CountWeaponCategory( WeaponCategory_Backup ), cols );
	cols = Max2( CountWeaponCategory( WeaponCategory_Melee ), cols );
	cols = Max2( int( Gadget_Count ) - 1, cols );
	cols = Max2( int( Perk_Count ) - 1, cols );

	ImGui::BeginTable( "loadoutmenu", cols + 1 );

	Perks( icon_size );
	LoadoutCategory( "PRIMARY", WeaponCategory_Primary, icon_size );
	LoadoutCategory( "SECONDARY", WeaponCategory_Secondary, icon_size );
	LoadoutCategory( "BACKUP", WeaponCategory_Backup, icon_size );
	LoadoutCategory( "MELEE", WeaponCategory_Melee, icon_size );
	Gadgets( icon_size );

	ImGui::EndTable();

	int loadoutKeys[ 2 ] = { };
	CG_GetBoundKeycodes( "loadoutmenu", loadoutKeys );

	bool should_close = false;
	if( ImGui::Hotkey( loadoutKeys[ 0 ] ) || ImGui::Hotkey( loadoutKeys[ 1 ] ) ) {
		should_close = true;
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	ImGui::PopFont();

	return should_close;
}

static void GameMenu() {
	bool spectating = cg.predictedPlayerState.real_team == Team_None;
	bool ready = false;

	if( client_gs.gameState.match_state <= MatchState_Warmup ) {
		ready = cg.predictedPlayerState.ready;
	}
	else if( client_gs.gameState.match_state == MatchState_Countdown ) {
		ready = true;
	}

	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
	bool should_close = false;

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	if( gamemenu_state == GameMenuState_Menu ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, 0, Vec2( 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 500, 0 ) );
		ImGui::Begin( "gamemenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );
		ImGuiStyle & style = ImGui::GetStyle();
		const double half = ImGui::GetWindowWidth() / 2 - style.ItemSpacing.x - style.ItemInnerSpacing.x;

		if( spectating ) {
			GameMenuButton( "Join Game", "join", &should_close );
			ImGui::Columns( 1 );
		}
		else {
			if( client_gs.gameState.match_state <= MatchState_Countdown ) {
				if( ImGui::Checkbox( ready ? "Ready!" : "Not ready", &ready ) ) {
					Cbuf_Add( "toggleready" );
				}
			}

			GameMenuButton( "Spectate", "spectate", &should_close );

			// TODO
			if( true ) {
				GameMenuButton( "Change weapons", "loadoutmenu", &should_close );
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
		ImGui::SetNextWindowSize( ImVec2( displaySize.x * 0.5f, -1 ) );
		ImGui::Begin( "votemap", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

		static int e = 0;
		ImGui::Columns( 2, NULL, false );
		ImGui::RadioButton( "Start match", &e, 0 );
		ImGui::RadioButton( "Change map", &e, 1 );
		ImGui::RadioButton( "Spectate", &e, 2 );
		ImGui::RadioButton( "Kick", &e, 3 );

		ImGui::NextColumn();

		const char * vote;
		const char * arg;
		if( e == 0 ) {
			vote = "start";
			arg = "";
		} else if( e == 1 ) {
			vote = "map";
			arg = SelectableMapList();
		} else {
			vote = e == 2 ? "spectate" : "kick";
			arg = SelectablePlayerList();
		}

		ImGui::Columns( 1 );
		GameMenuButton( "Start vote", temp( "callvote {} {}", vote, arg ), &should_close );
	}
	else if( gamemenu_state == GameMenuState_Settings ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( Max2( 800.f, displaySize.x * 0.65f ), Max2( 600.f, displaySize.y * 0.65f ) ) );
		ImGui::Begin( "settings", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

		Settings();
	}
	
	ImGui::SetWindowFocus();

	if( ImGui::Hotkey( K_ESCAPE ) || should_close ) {
		uistate = UIState_Hidden;
	}

	ImGui::End();

	ImGui::PopStyleColor();
}

static void DemoMenu() {
	ImGui::PushStyleColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
	bool should_close = false;

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImVec2 pos = ImGui::GetIO().DisplaySize;
	pos.x *= 0.5f;
	pos.y *= 0.8f;
	if( demomenu_state == DemoMenuState_Menu ) {
		ImGui::SetNextWindowPos( pos, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 600, 0 ) );
		ImGui::Begin( "demomenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

		ImGuiStyle & style = ImGui::GetStyle();
		const double half = ImGui::GetWindowWidth() / 2 - style.ItemSpacing.x - style.ItemInnerSpacing.x;

		GameMenuButton( CL_DemoPaused() ? "Play" : "Pause", "demopause" );

		ImGui::Columns( 2, NULL, false );
		ImGui::SetColumnWidth( 0, half );
		ImGui::SetColumnWidth( 1, half );

		GameMenuButton( "-15s", "demojump -15", NULL, 0 );
		ImGui::NextColumn();
		GameMenuButton( "+15s", "demojump +15", NULL, 1 );
		ImGui::NextColumn();
		if( Cvar_Bool( "cg_draw2D" ) ) {
			GameMenuButton( "Show HUD", "cg_draw2D 1", NULL, 1 );
		} else {
			GameMenuButton( "Hide HUD", "cg_draw2D 0", NULL, 1 );
		}


		ImGui::Columns( 1, NULL, false );

		if( ImGui::Button( "Settings", ImVec2( -1, 0 ) ) ) {
			demomenu_state = DemoMenuState_Settings;
		}

		GameMenuButton( "Disconnect to main menu", "disconnect", &should_close );
		GameMenuButton( "Exit to desktop", "quit", &should_close );
	} else if( demomenu_state == DemoMenuState_Settings ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( Max2( 800.f, displaySize.x * 0.65f ), Max2( 600.f, displaySize.y * 0.65f ) ) );
		ImGui::Begin( "settings", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

		Settings();
	}

	if( ImGui::Hotkey( K_ESCAPE ) || should_close ) {
		uistate = UIState_Hidden;
	}

	ImGui::End();

	ImGui::PopStyleColor();
}

void UI_Refresh() {
	TracyZoneScoped;

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
		ImGui::Begin( "mainmenu", WindowZOrder_Menu, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

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
	StartMenuMusic();
	Refresh();
}

void UI_ShowGameMenu() {
	// so the menu doesn't instantly close
	ImGui::GetIO().KeysDown[ K_ESCAPE ] = false;

	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Menu;
}

void UI_ShowDemoMenu() {
	// so the menu doesn't instantly close
	ImGui::GetIO().KeysDown[ K_ESCAPE ] = false;

	uistate = UIState_DemoMenu;
	demomenu_state = DemoMenuState_Menu;
}

void UI_HideMenu() {
	uistate = UIState_Hidden;
}

void UI_ShowLoadoutMenu( Loadout new_loadout ) {
	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Loadout;
	loadout = new_loadout;
}
