#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "client/assets.h"
#include "client/audio/api.h"
#include "client/client.h"
#include "client/clay.h"
#include "client/dev_tools.h"
#include "client/keys.h"
#include "client/renderer/renderer.h"
#include "client/demo_browser.h"
#include "client/server_browser.h"
#include "client/renderer/renderer.h"
#include "client/platform/browser.h"
#include "qcommon/array.h"
#include "qcommon/maplist.h"
#include "qcommon/time.h"
#include "qcommon/version.h"
#include "gameshared/vsays.h"

#include "cgame/cg_local.h"

#include "clay/clay.h"

#include "sdl/SDL3/SDL_video.h"

enum UIState {
	UIState_Hidden,
	UIState_MainMenu,
	UIState_Connecting,
	UIState_GameMenu,
	UIState_DemoMenu,
	UIState_DevTool,
};

enum MainMenuState {
	MainMenuState_Main,
	MainMenuState_License,
	MainMenuState_Locker,
	MainMenuState_Replays,
	MainMenuState_ServerBrowser,
	MainMenuState_Ranked,
	MainMenuState_CreateServerGladiator,
	MainMenuState_CreateServerBomb,
	MainMenuState_Settings,
	MainMenuState_Extras,
	MainMenuState_Career,
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

static DevToolRenderCallback devtool_render_callback;
static DevToolCleanupCallback devtool_cleanup_callback;

static Optional< size_t > selected_server;

static bool yolodemo;

static Loadout loadout;

static SettingsState settings_state;
static bool reset_video_settings;
static float sensivity_range[] = { 0.25f, 10.f };

static size_t selected_mask = 0;
static NonRAIIDynamicArray< char * > masks;
static Span< const char > MASKS_DIR = "models/masks/";

static void ResetServerBrowser() {
	selected_server = NONE;
}

static void ClearMasksList() {
	for( char * mask : masks ) {
		Free( sys_allocator, mask );
	}
	masks.clear();
}

static void SetMask( const char * mask_name ) {
	Cvar_Set( "cg_mask", MakeSpan( mask_name ) );
}

static void RefreshMasksList() {
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();
	ClearMasksList();

	masks.add( CopyString( sys_allocator, "None" ) );
	for( Span< const char > path : AssetPaths() ) {
		if( !StartsWith( path, MASKS_DIR ) || !EndsWith( path, ".glb" ) )
			continue;

		masks.add( ( *sys_allocator )( "{}", StripPrefix( StripExtension( path ), MASKS_DIR ) ) );
	}

	Span< const char > mask = Cvar_String( "cg_mask" );
	for( size_t i = 0; i < masks.size(); i++ ) {
		if( StrEqual( mask, temp( "{}{}", MASKS_DIR, masks[ i ] ) ) ) {
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
	RefreshMasksList();
	yolodemo = false;

	UI_ShowMainMenu();
	reset_video_settings = true;

	devtool_render_callback = NULL;
	devtool_cleanup_callback = NULL;
}

void UI_Shutdown() {
	ClearMasksList();
	masks.shutdown();

	if( devtool_cleanup_callback ) {
		devtool_cleanup_callback();
	}
}

static void SettingLabel( Span< const char > label ) {
	ImGui::AlignTextToFramePadding();
	ImGui::Text( label );
	ImGui::SameLine( 200 * GetContentScale() );
}

static bool ColorButton( const char * label, ImVec4 color ) {
	ScopedColor( ImGuiCol_Button, color );
	ScopedColor( ImGuiCol_ButtonHovered, ImVec4( color.x + 0.125f, color.y + 0.125f, color.z + 0.125f, color.w ) );
	ScopedColor( ImGuiCol_ButtonActive, ImVec4( color.x - 0.125f, color.y - 0.125f, color.z - 0.125f, color.w ) );
	return ImGui::Button( label );
}

static void CvarTextbox( Span< const char > label, Span< const char > cvar_name, size_t max_len ) {
	SettingLabel( label );

	TempAllocator temp = cls.frame_arena.temp();
	char * buf = AllocMany< char >( &temp, max_len + 1 );
	ggformat( buf, max_len + 1, "{}", Cvar_String( cvar_name ) );

	ImGui::PushID( cvar_name );
	ImGui::InputText( "", buf, max_len + 1 );
	ImGui::PopID();

	Cvar_Set( cvar_name, MakeSpan( buf ) );
}

static void CvarCheckbox( Span< const char > label, Span< const char > cvar_name ) {
	SettingLabel( label );

	bool val = Cvar_Bool( cvar_name );
	ImGui::PushID( cvar_name );
	ImGui::Checkbox( "", &val );
	ImGui::PopID();

	Cvar_Set( cvar_name, val ? "1" : "0" );
}

static void CvarSliderInt( Span< const char > label, Span< const char > cvar_name, int lo, int hi ) {
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );

	int val = Cvar_Integer( cvar_name );
	ImGui::PushID( cvar_name );
	ImGui::SliderInt( "", &val, lo, hi, NULL );
	ImGui::PopID();

	Cvar_Set( cvar_name, temp.sv( "{}", val ) );
}

static void CvarSliderFloat( Span< const char > label, Span< const char > cvar_name, float lo, float hi ) {
	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );

	float val = Cvar_Float( cvar_name );
	ImGui::PushID( cvar_name );
	ImGui::SliderFloat( "", &val, lo, hi, "%.2f" );
	ImGui::PopID();

	Cvar_Set( cvar_name, RemoveTrailingZeroesFloat( temp.sv( "{}", val ) ) );
}

Optional< Key > KeyFromImGui( ImGuiKey imgui );

static void KeyBindButton( Span< const char > label, Span< const char > command ) {
	static Key rebinding_key = Key_Count;
	auto CloseRebindingPopup = []() {
		rebinding_key = Key_Count;
		ImGui::CloseCurrentPopup();
	};

	TempAllocator temp = cls.frame_arena.temp();

	SettingLabel( label );
	ImGui::PushID( label );

	Optional< Key > key1, key2;
	GetKeyBindsForCommand( command, &key1, &key2 );

	bool rebinding = false;
	if( ImGui::Button( key1.exists ? temp( "{}", KeyName( key1.value ) ) : "N/A", ImVec2( 200 * GetContentScale(), 0 ) ) ) {
		rebinding = true;
		if( key1.exists ) rebinding_key = key1.value;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled( !key1.exists );
	if( ColorButton( "X", diesel_red.linear ) ) {
		UnbindKey( key1.value );
	}
	ImGui::EndDisabled();

	ImGui::SameLine( 0.0f, 50.0f );

	ImGui::PushID( "key2" );
	ImGui::BeginDisabled( !key1.exists );
	if( ImGui::Button( key2.exists ? temp( "{}", KeyName( key2.value ) ) : "N/A", ImVec2( 200 * GetContentScale(), 0 ) ) ) {
		rebinding = true;
		if( key2.exists ) rebinding_key = key2.value;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled( !key2.exists );
	if( ColorButton( "X", diesel_red.linear ) ) {
		UnbindKey( key2.value );
	}
	ImGui::PopID();
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	if( rebinding ) {
		ImGui::OpenPopup( "modal" );
	}

	if( ImGui::BeginPopupModal( "modal", NULL, ImGuiWindowFlags_NoDecoration ) ) {
		ImGui::Text( "Press a key to set a new bind, or Escape to cancel" );

		ImGuiIO & io = ImGui::GetIO();
		if( ImGui::Shortcut( ImGuiKey_Escape ) ) {
			CloseRebindingPopup();
		}
		else {
			for( ImGuiKey i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; i++ ) {
				if( ImGui::IsKeyPressed( i ) ) {
					Optional< Key > key = KeyFromImGui( i );
					if( key.exists ) {
						if( rebinding_key != Key_Count ) {
							UnbindKey( rebinding_key );
						}

						SetKeyBind( key.value, command );
						CloseRebindingPopup();
					}
				}
			}

			if( io.MouseWheel != 0.0f ) {
				SetKeyBind( io.MouseWheel > 0.0f ? Key_MouseWheelUp : Key_MouseWheelDown, command );
				CloseRebindingPopup();
			}
		}

		ImGui::EndPopup();
	}

	ImGui::PopID();
}

static Span< const char > SelectableMapList( bool include_gladiator ) {
	TempAllocator temp = cls.frame_arena.temp();
	Span< Span< const char > > maps = GetMapList();
	static size_t selected_map = 0;

	// Don't select the gladiator map if it was selected
	if ( StrEqual( maps[ selected_map ], "gladiator" ) ) {
		selected_map = (selected_map + 1) % maps.n;
	}

	ImGui::PushItemWidth( 200 * GetContentScale() );
	if( ImGui::BeginCombo( "##map", temp( "{}", maps[ selected_map ] ) ) ) {
		for( size_t i = 0; i < maps.n; i++ ) {
			if( !include_gladiator && StrEqual( maps[ i ], "gladiator" ) )
				continue;

			if( ImGui::Selectable( temp( "{}", maps[ i ] ), i == selected_map ) )
				selected_map = i;
			if( i == selected_map )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	return ( selected_map < maps.n ? maps[ selected_map ] : "" );
}

static Span< const char > SelectablePlayerList() {
	TempAllocator temp = cls.frame_arena.temp();
	DynamicArray< const char * > players( &temp );

	for( int i = 0; i < client_gs.maxclients; i++ ) {
		Span< const char > name = PlayerName( i );
		if( name != "" && !ISVIEWERENTITY( i + 1 ) ) {
			players.add( temp( "{}", name ) );
		}
	}

	if( players.size() == 0 ) {
		return "";
	}

	static size_t selected_player = 0;

	ImGui::PushItemWidth( 200 * GetContentScale() );
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

	return selected_player < players.size() ? MakeSpan( players[ selected_player ] ) : "";
}

static void Locker() {
	SettingLabel( "Mask" );

	ImGui::PushItemWidth( 200 * GetContentScale() );
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

	CvarTextbox( "Name", "name", MAX_NAME_CHARS );

	CvarSliderInt( "Crosshair size", "cg_crosshair_size", 1, 50 );
	CvarSliderInt( "Crosshair gap", "cg_crosshair_gap", 0, 50 );
	CvarCheckbox( "Dynamic crosshair", "cg_crosshair_dynamic" );

	CvarCheckbox( "Show chat", "cg_chat" );
	CvarCheckbox( "Show help", "cg_showHotkeys" );
	CvarCheckbox( "Show FPS", "cg_showFPS" );
}

static void SettingsControls() {
	TempAllocator temp = cls.frame_arena.temp();

	ImGui::BeginChild( "binds" );

	if( ColorButton( "Reset to default", diesel_red.linear ) ) {
		UnbindAllKeys();
		ExecDefaultCfg();
	}

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
			KeyBindButton( "Pick lifestyle", "loadoutmenu" );
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
			ImGui::Text( "Sensitivity is the same as CSGO/TF2/etc" );
			CvarSliderFloat( "Horizontal sensitivity", "horizontalsensscale", 0.5f, 2.0f );
			CvarCheckbox( "Invert Y axis", "m_invertY" );

			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Voices" ) ) {
			for( Vsay vsay : vsays ) {
				KeyBindButton( vsay.description, temp.sv( "vsay {}", vsay.short_name ) );
			}
			ImGui::EndTabItem();
		}

		if( ImGui::BeginTabItem( "Misc" ) ) {
			KeyBindButton( "Vote yes", "vote_yes" );
			KeyBindButton( "Vote no", "vote_no" );
			KeyBindButton( "Join/Switch team", "join" );
			KeyBindButton( "Ready up", "toggleready" );
			KeyBindButton( "Sit out", "spectate" );
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
	ImGui::PushItemWidth( 200 * GetContentScale() );

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
		mode.video_mode.refresh_rate = 0.0f;
	}

	if( mode.fullscreen != FullscreenMode_Windowed ) {
		int num_monitors;
		SDL_DisplayID * monitors = SDL_GetDisplays( &num_monitors );
		defer { SDL_free( monitors ); };

		if( num_monitors > 1 ) {
			SettingLabel( "Monitor" );
			ImGui::PushItemWidth( 400 * GetContentScale() );

			if( ImGui::BeginCombo( "##monitor", SDL_GetDisplayName( monitors[ mode.monitor ] ) ) ) {
				for( int i = 0; i < num_monitors; i++ ) {
					ImGui::PushID( i );
					if( ImGui::Selectable( SDL_GetDisplayName( monitors[ i ] ), mode.monitor == i ) ) {
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
			ImGui::PushItemWidth( 200 * GetContentScale() );

			if( mode.video_mode.refresh_rate == 0.0f ) {
				mode.video_mode = GetVideoMode( mode.monitor );
			}

			if( ImGui::BeginCombo( "##resolution", temp( "{}", mode.video_mode ) ) ) {
				int num_modes;
				SDL_DisplayMode ** modes = SDL_GetFullscreenDisplayModes( monitors[ mode.monitor ], &num_modes );
				defer { SDL_free( modes ); };

				for( int i = 0; i < num_modes; i++ ) {
					int idx = num_modes - i - 1;

					VideoMode m = { };
					m.width = modes[ idx ]->w;
					m.height = modes[ idx ]->h;
					m.refresh_rate = modes[ idx ]->refresh_rate;

					bool is_selected = mode.video_mode.width == m.width && mode.video_mode.height == m.height && mode.video_mode.refresh_rate == m.refresh_rate;
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
			if( mode.fullscreen == FullscreenMode_Windowed ) {
				const SDL_DisplayMode * primary_mode = SDL_GetDesktopDisplayMode( SDL_GetPrimaryDisplay() );
				mode.video_mode.width = primary_mode->w * 0.8f;
				mode.video_mode.height = primary_mode->h * 0.8f;
				mode.x = -1;
				mode.y = -1;
			}

			Cvar_Set( "vid_mode", temp.sv( "{}", mode ) );
			reset_video_settings = true;
		}

		ImGui::SameLine();

		if( ColorButton( "Discard changes", Vec4( Vec3( 0.5f ), 1.0f ) ) ) {
			reset_video_settings = true;
		}
	}

	ImGui::Separator();

	{
		SettingLabel( "Anti-aliasing" );

		int samples = Cvar_Integer( "r_samples" );

		ImGui::PushItemWidth( 100 * GetContentScale() );
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

		Cvar_Set( "r_samples", temp.sv( "{}", samples ) );
	}

	{
		SettingLabel( "Shadow Quality" );

		ShadowQuality quality = ShadowQuality( Cvar_Integer( "r_shadow_quality" ) );

		ImGui::PushItemWidth( 150 * GetContentScale() );
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

		Cvar_Set( "r_shadow_quality", temp.sv( "{}", quality ) );
	}

	{
		SettingLabel( "Max FPS" );

		constexpr int values[] = { 60, 75, 120, 144, 165, 180, 200, 240, 333, 500, 1000 };

		int maxfps = Cvar_Integer( "cl_maxfps" );

		ImGui::PushItemWidth( 100 * GetContentScale() );
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

		Cvar_Set( "cl_maxfps", temp.sv( "{}", maxfps ) );
	}

	CvarCheckbox( "Vsync", "vid_vsync" );

	CvarCheckbox( "Colorblind mode", "cg_colorBlind" );
	for( Team i = Team_One; i < Team_Count; i++ ) {
		ImGui::SameLine();
		ImGui::Text( "%s", temp( "{}[Team {}]", ImGuiColorToken( CG_RealTeamColor( i ) ), i ) );
	}
}

static void SettingsAudio() {
	SettingLabel( "Audio device" );
	ImGui::PushItemWidth( 400 * GetContentScale() );

	const char * current = StrEqual( s_device->value, "" ) ? "Default" : s_device->value;
	if( ImGui::BeginCombo( "##audio_device", current ) ) {
		if( ImGui::Selectable( "Default", StrEqual( s_device->value, "" ) ) ) {
			Cvar_Set( "s_device", "" );
		}

		TempAllocator temp = cls.frame_arena.temp();

		for( Span< const char > device : GetAudioDeviceNames( &temp ) ) {
			if( ImGui::Selectable( temp( "{}", device ), StrEqual( device, s_device->value ) ) ) {
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

// static bool LicenseCategory( TempAllocator& temp, char c, const ImU32& color, const Vec2& size, const Vec2& pos ) {
// 	constexpr auto draw_text = [] (ImDrawList * draw_list, ImFont * font, float font_size, const char * text, const Vec2& pos, const Vec2& size) {
// 		ImGui::PushFont( font );
//
// 		const Vec2 text_size = ImGui::CalcTextSize( text );
// 		ImGui::SetCursorPos( Vec2( pos.x + (size.x - text_size.x) * 0.5f, pos.y + (size.y - text_size.y) * 0.5f ) );
// 		const Vec2 global_pos = ImGui::GetCursorScreenPos();
// 		draw_list->AddText( font, font_size, global_pos, IM_COL32( 255, 255, 255, 255 ), text );
//
// 		ImGui::PopFont();
// 	};
//
// 	ImGui::SetCursorPos( pos );
//
// 	Vec2 size2 = Vec2( size.x, size.y / 3.f );
// 	Vec2 size1 = Vec2( size.x, size.y - size2.y );
//
// 	Vec2 global_pos = ImGui::GetCursorScreenPos();
// 	ImDrawList* draw_list = ImGui::GetForegroundDrawList();
// 	draw_list->AddRectFilledMultiColor( global_pos, global_pos + size1, color, color, IM_COL32( 0, 0, 0, 255 ), IM_COL32( 0, 0, 0, 255 ) );
// 	draw_list->AddRectFilled( global_pos + Vec2( 0.f, size1.y ), global_pos + size, color );
//
// 	const char * text1 = temp( "{}", c );
// 	draw_text( draw_list, cls.license_italic_font, 128.f, text1, pos, size1 );
// 	draw_text( draw_list, cls.large_font, 64.f, temp( "{} LICENSE", c ), pos + Vec2( 0.f, size1.y ), size2 );
//
// 	ImGui::SetCursorPos( pos );
// 	ImGui::PushID( text1 );
// 	bool pressed = ImGui::InvisibleButton( "", size );
// 	ImGui::PopID();
//
// 	return pressed;
// }
//
// static void License( const Vec2& size ) {
// 	TempAllocator temp = cls.frame_arena.temp();
//
// 	const float CATEGORY_X = 32.f;
// 	const float CATEGORY_Y = 32.f;
// 	const Vec2 LICENSE_CATEGORY_SIZE = Vec2( (size.x - CATEGORY_X) * 0.3f, (size.y - CATEGORY_Y * 4.f) / 3.f );
//
// 	LicenseCategory( temp, 'B', IM_COL32( 125, 140, 255, 255 ), LICENSE_CATEGORY_SIZE, Vec2( CATEGORY_X, CATEGORY_Y ) );
// 	LicenseCategory( temp, 'A', IM_COL32( 255, 140, 16, 255 ), LICENSE_CATEGORY_SIZE, Vec2( CATEGORY_X, CATEGORY_Y + (LICENSE_CATEGORY_SIZE.y + CATEGORY_Y) ) );
// 	LicenseCategory( temp, 'S', IM_COL32( 255, 48, 48, 255 ), LICENSE_CATEGORY_SIZE, Vec2( CATEGORY_X, CATEGORY_Y + (LICENSE_CATEGORY_SIZE.y + CATEGORY_Y) * 2.f ) );
// }

static void ServerBrowser() {
	TempAllocator temp = cls.frame_arena.temp();

	if( ImGui::Button( "discord.gg/5ZbV4mF" ) ) {
		OpenInWebBrowser( "https://discord.gg/5ZbV4mF" );
	}
	ImGui::SameLine();
	ImGui::TextWrapped( "This game is very pre-alpha so there are probably 0 players online. Join the Discord to find games!" );

	if( ImGui::Button( "Refresh" ) ) {
		Refresh();
	}

	ImGui::SameLine();
	if( ImGui::Button( "Host Bomb match" ) ) {
		mainmenu_state = MainMenuState_CreateServerBomb;
	}

	ImGui::SameLine();
	if( ImGui::Button( "Host Gladiator match" ) ) {
		mainmenu_state = MainMenuState_CreateServerGladiator;
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
		ImGui::PushStyleColor( ImGuiCol_Text, old_version ? diesel_red.linear : diesel_green.linear );
		ImGui::Text( "%s", demo.version );
		ImGui::NextColumn();
		ImGui::PopStyleColor();

		if( clicked && ImGui::IsMouseDoubleClicked( 0 ) ) {
			const char * cmd = yolodemo ? "yolodemo" : "demo";
			Cmd_Execute( &temp, "{} \"{}\"", cmd, demo.path );
		}
	}

	ImGui::Columns( 1 );
	ImGui::EndChild();
}

static void CreateServer( bool gladiator ) {
	TempAllocator temp = cls.frame_arena.temp();

	CvarTextbox( "Server name", "sv_hostname", 128 );
	if( !gladiator ) {
		SettingLabel( "Map" );
	}
	Span< const char > map_name = gladiator ? "gladiator" : SelectableMapList( false );
	CvarCheckbox( "Public", "sv_public" );

	if( ImGui::Button( "Create server" ) ) {
		Cmd_Execute( &temp, "map \"{}\"", map_name );
	}
	else if( ImGui::Shortcut( ImGuiKey_Escape ) ) {
		mainmenu_state = MainMenuState_ServerBrowser;
	}
}

static void ShadowedText( Span< const char > text, float shadow_size ) {
	const Vec2 pos = ImGui::GetCursorPos();
	ImGui::SetCursorPos( pos + shadow_size );

	ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, 255 ) );
	ImGui::Text( text );
	ImGui::PopStyleColor();

	ImGui::SetCursorPos( pos );
	ImGui::Text( text );
}

template< bool BUTTON >
static bool MainSectionButton( const ImVec2& pos, const Material * icon, const Vec2& size, Span<const char> name, const Vec4& bg_color, const bool is_enabled ) {
	const Vec2 half_pixel = HalfPixelSize( icon );
	const Vec2 SQUARE_SIZE = size + 8.0f;
	const ImVec2 text_size = ImGui::CalcTextSize( name );
	const Vec4 text_color = is_enabled ? white.linear : diesel_grey.linear;

	bool pressed = false;
	bool hovered = false;
	if( BUTTON ) {
		ImGui::SetCursorPos( pos );
		ImGui::PushID( name );
		pressed = ImGui::InvisibleButton( "", size );
		hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		if( ImGui::IsItemHoveredThisFrame() ) {
			PlaySFX( "ui/sounds/hover" );
		}

		if( pressed ) {
			PlaySFX( "ui/sounds/click" );
		}
	}

	constexpr float period = 0.5f;
	float offset = !hovered ? 0.0f : -cosf( ( ImGui::GetCurrentContext()->HoverItemDelayTimer / period ) * PI * 2.0f ) * 2.0f + 2.0f;

	ImGui::SetCursorPos( pos - 4.0f - offset );
	ImGui::Image( cls.white_material, SQUARE_SIZE, Vec2( 0.f ), Vec2( 0.f ), dark.linear, Vec4( 0.0f ) );
	ImGui::SetCursorPos( pos + 4.0f );
	ImGui::Image( cls.white_material, SQUARE_SIZE + offset, Vec2( 0.f ), Vec2( 0.f ), dark.linear, Vec4( 0.0f ) );

	ImGui::SetCursorPos( pos - offset );
	ImGui::Image( cls.white_material, size, Vec2( 0.f ), Vec2( 0.f ), bg_color, Vec4( 0.0f ) );

	ImGui::SetCursorPos( pos - offset * 2.f );
	ImGui::Image( icon, size + offset, half_pixel, 1.0f - half_pixel, Vec4( 0.f, 0.f, 0.f, 1.f ), Vec4( 0.0f ) );

	if( BUTTON ) {
		ImGui::SetCursorPos( ImVec2( pos.x + (size.x - text_size.x) * 0.5f + 4.f, pos.y + size.y + 16.f ) );
	}
	else {
		ImGui::SetCursorPos( ImVec2( pos.x + size.x + 32.f, pos.y + (size.y - text_size.y) * 0.5f + 4.f ) );
	}
	ImGui::PushStyleColor( ImGuiCol_Text, text_color );
	ShadowedText( name, 4.f );
	ImGui::PopStyleColor();

	return pressed;
}

struct MainMenuCategory {
	StringHash icon_path;
	Span< const char > name;
	MainMenuState state;
	Vec4 bg_color;
	bool is_enabled;
};

static void NotImplemented() {
	ImGui::PushFont( cls.large_font );
	ImGui::Text( "FEATURE NOT IMPLEMENTED YET" );
	ImGui::PopFont();
}

static void ClayVerticalSpacing( Clay_ElementId id, Clay_SizingAxis spacing ) {
	CLAY( {
		.id = id,
		.layout = { .sizing = { .height = spacing } },
	} ) { }
}

struct ClayCallbackAndUserdata {
	ClayCustomElementCallback callback;
	void * userdata;
};

static ClayCustomElementConfig * ClayImGui( ClayCustomElementCallback callback, void * userdata = NULL ) {
	return Clone( ClayAllocator(), ClayCustomElementConfig {
		.type = ClayCustomElementType_Callback,
		.callback = {
			.f = []( const Clay_BoundingBox & bounds, void * userdata ) {
				ImGui::Begin( "mainmenu" );
				ImGui::SetCursorPos( Vec2( bounds.x, bounds.y ) );

				const ClayCallbackAndUserdata * cb = ( const ClayCallbackAndUserdata * ) userdata;
				cb->callback( bounds, cb->userdata );

				ImGui::End();
			},
			.userdata = Clone( ClayAllocator(), ClayCallbackAndUserdata { callback, userdata } ),
		},
	} );
}

static void MainMenu() {
	constexpr MainMenuCategory categories[] = {
		{ "hud/license", "LICENSE", MainMenuState_License, diesel_grey.linear, false },
		{ "hud/locker", "LOCKER", MainMenuState_Locker, diesel_grey.linear, false },
		{ "hud/gladiator", "RANKED", MainMenuState_Ranked, diesel_grey.linear, false },
		{ "hud/replays", "REPLAYS", MainMenuState_Replays, diesel_yellow.linear, true },
		{ "hud/bomb", "PLAY", MainMenuState_ServerBrowser, diesel_green.linear, true },
		{ "hud/settings", "SETTINGS", MainMenuState_Settings, diesel_yellow.linear, true },
		{ "hud/extras", "EXTRAS", MainMenuState_Extras, diesel_grey.linear, false },
		{ "hud/career", "CAREER", MainMenuState_Career, diesel_grey.linear, false },
	};

	TempAllocator temp = cls.frame_arena.temp();

	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( frame_static.viewport );
	ScopedStyle( ImGuiStyleVar_WindowPadding, Vec2( 0.0f ) );

	ImGui::Begin( "mainmenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_Interactive );
	ImGui::Dummy( frame_static.viewport ); // NOTE(mike): needed to fix the ErrorCheckUsingSetCursorPosToExtendParentBoundaries assert

	FittedTextShadow shadow = FittedTextShadow {
		.color = black.linear,
		.offset = 8.0f * GetContentScale(),
		.angle = 40.0f,
	};

	CLAY( {
		.id = CLAY_ID_LOCAL( "Background" ),
		.layout = {
			.sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 1.0f ) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
	} ) {
		auto scrolling_hazard_stripes = []( const Clay_BoundingBox & bounds, void * userdata ) {
			const Material * hazard = FindMaterial( "hud/tape_hazard_yellow" );
			Vec2 half_pixel = HalfPixelSize( hazard );
			float repetitions = ( bounds.width / bounds.height ) / ( hazard->texture->width / hazard->texture->height );
			float scroll = Sawtooth01( cls.monotonicTime, Seconds( 4 ) );
			float scale = bit_cast< uintptr_t >( userdata ) == 0 ? 1.0f : -1.0f;
			Vec2 tl = half_pixel + Vec2( scroll * scale, 0.0f );
			Vec2 br = ( 1.0f - half_pixel ) + Vec2( repetitions + scroll * scale, 0.0f );
			Draw2DBoxUV( bounds.x, bounds.y, bounds.width, bounds.height, tl, br, hazard );
		};

		CLAY( {
			.id = CLAY_ID_LOCAL( "Top bar" ),
			.layout = {
				.sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.1f ) },
				.padding = CLAY_PADDING_ALL( u16( 8 * GetContentScale() ) ),
			},
			.backgroundColor = { 0, 0, 0, 255 },
		} ) {
			if( cl_devtools->integer ) {
				CLAY( {
					.id = CLAY_ID_LOCAL( "Dev tools" ),
					.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 1.0f ) } },
					.custom = { ClayImGui( []( const Clay_BoundingBox & bounds, void * userdata ) {
						if( ImGui::Button( "Model viewer" ) ) {
							uistate = UIState_DevTool;
							devtool_render_callback = DrawModelViewer;
						}
					} ) },
				} ) { }
			}
		}

		CLAY( {
			.id = CLAY_ID_LOCAL( "Top hazard stripes" ),
			.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.03f ) } },
			.custom = { ClayImGui( scrolling_hazard_stripes, bit_cast< void * >( uintptr_t( 0 ) ) ) },
		} ) { }

		ClayCustomElementConfig * nk_custom = ClayImGui( []( const Clay_BoundingBox & bounds, void * userdata ) {
			Draw2DBox( bounds.x, bounds.y, bounds.width, bounds.height, FindMaterial( "hud/nk" ) );
		} );

		CLAY( {
			.id = CLAY_ID_LOCAL( "Main" ),
			.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_GROW() } },
			.custom = { nk_custom },
		} ) {
			CLAY( {
				.id = CLAY_ID_LOCAL( "Sidebar" ),
				.layout = {
					.sizing = { .width = CLAY_SIZING_PERCENT( 0.25f ), .height = CLAY_SIZING_PERCENT( 1.0f ) },
					.padding = CLAY_PADDING_ALL( u16( 24.0f * GetContentScale() ) ),
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
			} ) {
				ClayCustomElementConfig * cocaine_custom = Clone( ClayAllocator(), ClayCustomElementConfig {
					.type = ClayCustomElementType_FittedText,
					.fitted_text = {
						.text = CLAY_STRING( "COCAINE" ),
						.config = {
							.textColor = { 255, 255, 255, 255 },
							.fontId = ClayFont_BoldItalic,
						},
						.shadow = shadow,
					},
				} );

				ClayCustomElementConfig * diesel_custom = Clone( ClayAllocator(), ClayCustomElementConfig {
					.type = ClayCustomElementType_FittedText,
					.fitted_text = {
						.text = CLAY_STRING( "DIESEL" ),
						.config = {
							.textColor = { 255, 255, 255, 255 },
							.fontId = ClayFont_BoldItalic,
						},
						.shadow = shadow,
					},
				} );

				CLAY( {
					.id = CLAY_ID_LOCAL( "COCAINE" ),
					.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.1f ) } },
					.custom = { cocaine_custom },
				} ) { }

				ClayVerticalSpacing( CLAY_ID_LOCAL( "Inter-title spacing" ), CLAY_SIZING_FIXED( 2.0f * GetContentScale() ) );

				CLAY( {
					.id = CLAY_ID_LOCAL( "DIESEL" ),
					.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.1f ) } },
					.custom = { diesel_custom },
				} ) { }

				ClayVerticalSpacing( CLAY_ID_LOCAL( "Title-subtitle spacing" ), CLAY_SIZING_FIXED( 8.0f * GetContentScale() ) );

				CLAY( {
					.id = CLAY_ID_LOCAL( "Subtitle" ),
					.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_FIT() } },
				} ) {
					constexpr Span< const char > subtitles[] = {
						#include "subtitles.h"
					};

					RNG rng = NewRNG( cls.launch_day, 0 );
					Span< const char > subtitle = RandomElement( &rng, subtitles );
					Span< const char > full_subtitle = temp.sv( "THE{}{} GAME", subtitle == "" ? "" : " ", subtitle );

					CLAY_TEXT( AllocateClayString( full_subtitle ), CLAY_TEXT_CONFIG( {
						.textColor = { 255, 255, 255, 255 },
						.fontId = ClayFont_Regular,
						.fontSize = u16( 16.0f * GetContentScale() ),
						.hashStringContents = true,
						.userData = Clone( ClayAllocator(), Optional< Vec4 >( NONE ) ),
					} ) );
				}

				if( mainmenu_state != MainMenuState_Main ) {
					ClayVerticalSpacing( CLAY_ID_LOCAL( "Bottom-align everything below this" ), CLAY_SIZING_GROW() );

					ClayCustomElementConfig * back_custom = ClayImGui( []( const Clay_BoundingBox & bounds, void * userdata ) {
						if( ImGui::Button( "cya" ) ) {
							mainmenu_state = MainMenuState_Main;
						}
					} );

					CLAY( {
						.id = CLAY_ID_LOCAL( "Back" ),
						.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.10f ) } },
						.custom = { back_custom },
					} ) { }
				}
			}
		}

		CLAY( {
			.id = CLAY_ID_LOCAL( "Bottom hazard stripes" ),
			.layout = { .sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.03f ) } },
			.custom = { ClayImGui( scrolling_hazard_stripes, bit_cast< void * >( uintptr_t( 1 ) ) ) },
		} ) { }

		CLAY( {
			.id = CLAY_ID_LOCAL( "Bottom bar" ),
			.layout = {
				.sizing = { .width = CLAY_SIZING_PERCENT( 1.0f ), .height = CLAY_SIZING_PERCENT( 0.1f ) },
				.padding = {
					.right = u16( 8 + Sin( cls.monotonicTime, Milliseconds( 182 ) ) * GetContentScale() ),
					.bottom = 8,
				},
				.childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_BOTTOM },
			},
			.backgroundColor = { 0, 0, 0, 255 },
		} ) {
			CLAY( {
				.id = CLAY_ID_LOCAL( "Version" ),
			} ) {
				const char * buf = ( const char * ) APP_VERSION u8" \u00A9 AHA CHEERS";
				CLAY_TEXT( AllocateClayString( MakeSpan( buf ) ), CLAY_TEXT_CONFIG( {
					.textColor = { 255, 255, 255, 255 },
					.fontId = ClayFont_Bold,
					.fontSize = u16( 14.0f * GetContentScale() ),
					.userData = Clone( ClayAllocator(), Optional< Vec4 >( NONE ) ),
				} ) );
			}
		}
	}

	const float OFFSET = frame_static.viewport_height * 0.1f;
	const Vec2 icon_size = Vec2( frame_static.viewport_height * 0.10f, frame_static.viewport_height * 0.10f );

	if( mainmenu_state == MainMenuState_Main ) {
		const float BASE_COLUMN = frame_static.viewport_width * 0.4f;
		const float COLUMN_OFFSET = frame_static.viewport_width * 0.2f;
		const float COLUMN_LINE_OFFSET = frame_static.viewport_width * 0.05f;

		const float BASE_LINE = frame_static.viewport_height * 0.25f - icon_size.y * 0.5f;
		const float LINE_OFFSET = frame_static.viewport_height * 0.48f - icon_size.y * 0.5f - BASE_LINE;

		//CATEGORIES
		ScopedFont( cls.big_font );
		for( size_t i = 0; i < ARRAY_COUNT( categories ); i++ ) {
			if( MainSectionButton< true >( ImVec2( BASE_COLUMN + (COLUMN_OFFSET * (i % 3)) + COLUMN_LINE_OFFSET * (i/3), BASE_LINE + LINE_OFFSET * (i/3) ),
				FindMaterial( categories[ i ].icon_path ), icon_size, categories[ i ].name, categories[ i ].bg_color, categories[ i ].is_enabled ) ) {
				mainmenu_state = categories[ i ].state;
			}
		}

		if( MainSectionButton<true>( ImVec2( BASE_COLUMN + COLUMN_OFFSET * 2.f + COLUMN_LINE_OFFSET * 2.f, BASE_LINE + LINE_OFFSET * 2.f ),
			FindMaterial( "hud/exit" ), icon_size, "EXIT", diesel_red.linear, true ) ) {
			Cmd_Execute( &temp, "quit" );
		}
	}
	else {
		const ImVec2 submenus_offset = ImVec2( frame_static.viewport_width * 0.275f, OFFSET + 128.f );
		const ImVec2 submenus_size = ImVec2( frame_static.viewport_width - submenus_offset.x - 512.f, frame_static.viewport_height - OFFSET * 2.f - 256.f );

		ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
		constexpr ImU32 DARK_COL32 = IM_COL32( 25, 25, 25, 255 );
		constexpr ImU32 GRAY_COL32 = IM_COL32( 100, 100, 100, 255 );
		draw_list->AddRectFilledMultiColor( submenus_offset - Vec2( 0.f, 32.f ), submenus_offset + Vec2( submenus_size.x * 0.5f, 0.f ), DARK_COL32, GRAY_COL32, GRAY_COL32, DARK_COL32 );
		draw_list->AddRectFilledMultiColor( submenus_offset + Vec2( submenus_size.x * 0.5f, -32.f ), submenus_offset + Vec2( submenus_size.x, 0.f ), GRAY_COL32, DARK_COL32, DARK_COL32, GRAY_COL32 );
		draw_list->AddRectFilledMultiColor( submenus_offset + Vec2( 0.f, submenus_size.y ), submenus_offset + Vec2( submenus_size.x * 0.5f, submenus_size.y + 32.f ), DARK_COL32, GRAY_COL32, GRAY_COL32, DARK_COL32 );
		draw_list->AddRectFilledMultiColor( submenus_offset + Vec2( submenus_size.x * 0.5f, submenus_size.y ), submenus_offset + Vec2( submenus_size.x, submenus_size.y + 32.f ), GRAY_COL32, DARK_COL32, DARK_COL32, GRAY_COL32 );

		Draw2DBox( submenus_offset.x, submenus_offset.y, submenus_size.x, submenus_size.y, cls.white_material, dark.linear );
		Draw2DBoxUV( submenus_offset.x, submenus_offset.y, submenus_size.x, submenus_size.y, Vec2( 0.f, 0.f ), submenus_size/8.f, FindMaterial( "hud/diagonal_pattern" ), Vec4( 1.f, 1.f, 1.f, 0.025f ) );

		ImGui::SetCursorPos( submenus_offset );

		ScopedColor( ImGuiCol_Border, Vec4( 0.0f ) );
		ScopedColor( ImGuiCol_ChildBg, Vec4( 0.0f ) );
		ImGui::BeginChild( "sub main menus", submenus_size, true );

		if( mainmenu_state == MainMenuState_License ) {
			NotImplemented();
			//License( submenus_size );
		}
		else if( mainmenu_state == MainMenuState_Locker ) {
			Locker();
		}
		else if( mainmenu_state == MainMenuState_Replays ) {
			DemoBrowser();
		}
		else if( mainmenu_state == MainMenuState_ServerBrowser ) {
			ServerBrowser();
		}
		else if( mainmenu_state == MainMenuState_Ranked ) {
			NotImplemented();
		}
		else if( mainmenu_state == MainMenuState_CreateServerGladiator ) {
			CreateServer( true );
		}
		else if( mainmenu_state == MainMenuState_CreateServerBomb ) {
			CreateServer( false );
		}
		else if( mainmenu_state == MainMenuState_Settings ) {
			Settings();
		}
		else if( mainmenu_state == MainMenuState_Extras ) {
			NotImplemented();
		}
		else if( mainmenu_state == MainMenuState_Career ) {
			NotImplemented();
		}

		ImGui::EndChild();
	}

	// {
	// 	{
	// 		ScopedStyle( ImGuiStyleVar_FramePadding, Vec2( 0.0f ) );
	// 		ScopedColor( ImGuiCol_Button, Vec4( 0.0f ) );
	// 		ScopedColor( ImGuiCol_ButtonHovered, Vec4( 0.0f ) );
	// 		ScopedColor( ImGuiCol_ButtonActive, Vec4( 0.0f ) );
    //
	// 		const char * buf = ( const char * ) APP_VERSION u8" \u00A9 AHA CHEERS";
	// 		ImVec2 size = ImGui::CalcTextSize( buf );
	// 		ImGui::SetCursorPosY( ImGui::GetWindowHeight() - size.y - 8.0f );
	// 		ImGui::SetCursorPosX( ImGui::GetWindowWidth() - size.x - 8.0f - Sin( cls.monotonicTime, Milliseconds( 182 ) ) );
	// 		ImGui::Text( "%s", buf );
    //
	// 		// if( ImGui::Button( buf ) ) {
	// 		// 	ImGui::OpenPopup( "Credits" );
	// 		// }
	// 	}
    //
	// 	// ImGuiWindowFlags credits_flags = ( ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoTitleBar ) | ImGuiWindowFlags_NoMove;
	// 	// if( ImGui::BeginPopupModal( "Credits", NULL, credits_flags ) ) {
	// 	// 	ImGui::Text( "Dexter - programming" );
	// 	// 	ImGui::Text( "general adnic - voice acting" );
	// 	// 	ImGui::Text( "goochie - art & programming" );
	// 	// 	ImGui::Text( "MikeJS - programming" );
	// 	// 	ImGui::Text( "MSC - programming & art" );
	// 	// 	ImGui::Text( "Obani - music & fx & programming" );
	// 	// 	ImGui::Text( "Rhodanathema - art" );
	// 	// 	ImGui::Separator();
	// 	// 	ImGui::Text( "jwzr - medical research" );
	// 	// 	ImGui::Text( "naxeron - chief propagandist" );
	// 	// 	ImGui::Text( "zmiles - american cultural advisor" );
	// 	// 	ImGui::Separator();
	// 	// 	ImGui::Text( "Special thanks to the Warsow team except for slk and MWAGA" );
	// 	// 	ImGui::Spacing();
    //     //
	// 	// 	if( ImGui::Button( "Close" ) ) {
	// 	// 		ImGui::CloseCurrentPopup();
	// 	// 	}
    //     //
	// 	// 	ImGui::EndPopup();
	// 	// }
	// }

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
		Cmd_Execute( &temp, "{}", command );
		if( clicked != NULL )
			*clicked = true;
	}
}

static void SendLoadout() {
	TempAllocator temp = cls.frame_arena.temp();
	Cmd_Execute( &temp, "setloadout {}", loadout );
}

static bool LoadoutButton( Span< const char > label, Vec2 icon_size, const Material * icon, bool selected ) {
	constexpr RGBA8 button_gray = RGBA8( 200, 200, 200, 255 );
	Vec2 start_pos = ImGui::GetCursorPos();
	ImGui::GetCursorPos();

	ImGui::PushID( label.begin(), label.end() );
	bool clicked = ImGui::InvisibleButton( "", ImVec2( -1, icon_size.y ) );
	ImGui::PopID();

	Vec2 half_pixel = HalfPixelSize( icon );
	Vec4 color = selected ? diesel_yellow.linear : ImGui::IsItemHovered() ? sRGBToLinear( button_gray ) : white.linear;

	ImGui::SetCursorPos( start_pos );
	ImGui::Image( icon, icon_size, half_pixel, 1.0f - half_pixel, color, Vec4( 0.0f ) );
	ImGui::SameLine();
	ImGui::Dummy( ImVec2( icon_size.x * 0.2f, 0 ) );
	ImGui::SameLine();

	ImGui::PushStyleColor( ImGuiCol_Text, color );
	CenterTextY( label, icon_size.y );
	ImGui::PopStyleColor();

	return clicked;
}

static void InitCategory( const char * category_name, float padding ) {
	ImGui::TableNextColumn();

	ScopedColor( ImGuiCol_Text, diesel_yellow.linear );
	ScopedFont( cls.big_italic_font );
	ImGui::Text( "%s", category_name );
	ImGui::Dummy( ImVec2( 0, padding ) );
}

static void LoadoutCategory( const char * label, WeaponCategory category, Vec2 icon_size ) {
	TempAllocator temp = cls.frame_arena.temp();

	InitCategory( label, icon_size.y * 0.5 );

	for( WeaponType i = Weapon_None; i < Weapon_Count; i++ ) {
		const WeaponDef * def = GS_GetWeaponDef( i );
		if( def->category == category ) {
			const Material * icon = FindMaterial( cgs.media.shaderWeaponIcon[ i ] );
			if( LoadoutButton( ToUpperASCII( &temp, def->name ), icon_size, icon, loadout.weapons[ def->category ] == i ) ) {
				loadout.weapons[ def->category ] = i;
				SendLoadout();
			}
		}
	}
}

static void Perks( Vec2 icon_size ) {
	TempAllocator temp = cls.frame_arena.temp();

	InitCategory( "MINDSET", icon_size.y * 0.5 );

	for( PerkType i = PerkType( Perk_None + 1 ); i < Perk_Count; i++ ) {
		if( GetPerkDef( i )->disabled )
			continue;

		const Material * icon = FindMaterial( cgs.media.shaderPerkIcon[ i ] );
		if( LoadoutButton( ToUpperASCII( &temp, GetPerkDef( i )->name ), icon_size, icon, loadout.perk == i ) ) {
			loadout.perk = i;
			SendLoadout();
		}
	}
}

static void Gadgets( Vec2 icon_size ) {
	TempAllocator temp = cls.frame_arena.temp();

	InitCategory( "GADGET", icon_size.y * 0.5 );

	for( GadgetType i = GadgetType( Gadget_None + 1 ); i < Gadget_Count; i++ ) {
		const GadgetDef * def = GetGadgetDef( i );
		const Material * icon = FindMaterial( cgs.media.shaderGadgetIcon[ i ] );
		if( LoadoutButton( ToUpperASCII( &temp, def->name ), icon_size, icon, loadout.gadget == i ) ) {
			loadout.gadget = GadgetType( i );
			SendLoadout();
		}
	}
}

ImGuiKey KeyToImGui( Key key );
static bool LoadoutMenu() {
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	ScopedFont( cls.medium_italic_font );
	ScopedColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 255 ) );
	ScopedStyle( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
	ScopedStyle( ImGuiStyleVar_FramePadding, ImVec2( 0.0f, 0.0f ) );

	ImGui::SetNextWindowPos( Vec2( 0, 0 ) );
	ImGui::SetNextWindowSize( displaySize );
	ImGui::Begin( "Loadout", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

	Vec2 icon_size = Vec2( displaySize.y * 0.065f );
	size_t title_height = displaySize.y * 0.075f;

	{
		ScopedColor( ImGuiCol_ChildBg, Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		ScopedChild( "loadout title", ImVec2( -1, title_height ) );

		ImGui::Dummy( ImVec2( displaySize.x * 0.02f, 0.0f ) );
		ImGui::SameLine();

		ImGui::PushFont( cls.large_italic_font );
		CenterTextY( "PICK A LIFESTYLE...", title_height );
		ImGui::PopFont();

		ImGui::SameLine();

		ScopedColor( ImGuiCol_Button, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
		ScopedColor( ImGuiCol_ButtonHovered, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
		ScopedColor( ImGuiCol_ButtonActive, ImVec4( 0.f, 0.f, 0.f, 0.f ) );

		ImGui::SetCursorPos( ImVec2( displaySize.x - title_height, 0.f ) );

		const Material * icon = FindMaterial( "textures/sprays/peekatyou" );
		Vec2 half_pixel = HalfPixelSize( icon );
		if( ImGui::ImageButton( "random", icon, ImVec2( title_height, title_height ), half_pixel, 1.0f - half_pixel, Vec4( 0.f ), Vec4( 1.0f ) ) ) {
			for( WeaponCategory category = WeaponCategory( 0 ); category < WeaponCategory_Count; category++ ) {
				do {
					loadout.weapons[ category ] = WeaponType( RandomUniform( &cls.rng, Weapon_None + 1, Weapon_Count ) );
				} while( GS_GetWeaponDef( loadout.weapons[ category ] )->category != category );
			}

			loadout.gadget = GadgetType( RandomUniform( &cls.rng, Gadget_None + 1, Gadget_Count ) );

			do {
				loadout.perk = PerkType( RandomUniform( &cls.rng, Perk_None + 1, Perk_Count ) );
			} while( GetPerkDef( loadout.perk )->disabled );

			SendLoadout();
		}
	}

	ScopedStyle( ImGuiStyleVar_ItemSpacing, Vec2( 0.0f ) );

	ImGui::Dummy( ImVec2( 0.0f, displaySize.x * 0.01f ) );
	ImGui::Dummy( ImVec2( displaySize.x * 0.06f, 0.0f ) );
	ImGui::SameLine();

	ImGui::BeginTable( "loadoutmenu", 6 );
	ImGui::NextColumn();

	Perks( icon_size );
	LoadoutCategory( "PRIMARY", WeaponCategory_Primary, icon_size );
	LoadoutCategory( "SECONDARY", WeaponCategory_Secondary, icon_size );
	LoadoutCategory( "BACKUP", WeaponCategory_Backup, icon_size );
	Gadgets( icon_size );
	LoadoutCategory( "MELEE", WeaponCategory_Melee, icon_size );

	ImGui::EndTable();

	bool should_close = false;
	{
		constexpr auto PrintMoveText = [] ( Span< const char > text, ImVec2 & textPos ) {
			textPos.x -= ImGui::CalcTextSize( text ).x;
			ImGui::SetCursorPos( textPos );
			ImGui::Text( text );
		};

		constexpr auto PrintMoveImage = [] ( const Material * icon, size_t icon_size, ImVec2 & imgPos, ImVec4 color ) {
			Vec2 half_pixel = HalfPixelSize( icon );
			imgPos.x -= icon_size;
			ImGui::SetCursorPos( imgPos );
			ImGui::Image( icon, ImVec2(icon_size, icon_size), half_pixel, 1.0f - half_pixel, color, Vec4( 0.0f ) );
		};

		ImGui::SetNextWindowPos( ImVec2( 0, displaySize.y - title_height ) );
		ScopedColor( ImGuiCol_ChildBg, Vec4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		ScopedChild( "loadout bottom", ImVec2( -1, title_height ) );
		ScopedFont( cls.large_italic_font );

		ScopedColor( ImGuiCol_Button, Vec4( 0.f ) );
		ScopedColor( ImGuiCol_ButtonActive, Vec4( 0.f ) );
		ScopedColor( ImGuiCol_ButtonHovered, Vec4( 0.f ) );

		const char * imdone = " I'M DONE";
		ImVec2 textSize = ImGui::CalcTextSize( imdone );
		ImVec2 textPos = ImVec2( displaySize.x - textSize.x * 1.1f, (title_height - textSize.y) * 0.5f );
		ImGui::SetCursorPos( textPos );

		ImGui::PushID( imdone );
		should_close = ImGui::InvisibleButton( "", textSize );
		ImGui::PopID();

		textPos.x += textSize.x;
		ImGui::PushStyleColor( ImGuiCol_Text, ImGui::IsItemHovered() ? diesel_yellow.linear : white.linear );
		PrintMoveText( MakeSpan( imdone ), textPos );
		ImGui::PopStyleColor();

		textPos.y = 0;
		PrintMoveImage( FindMaterial( "hud/icons/clickme" ), title_height, textPos, white.linear );
		textPos.y = (title_height - textSize.y) * 0.5f;
		PrintMoveText( "AND ", textPos );

		textPos.y = (title_height - icon_size.y) * 0.5f;
		PrintMoveImage( FindMaterial( cgs.media.shaderWeaponIcon[ loadout.weapons[ WeaponCategory_Melee ] ] ), icon_size.y, textPos, diesel_yellow.linear );
		PrintMoveImage( FindMaterial( cgs.media.shaderGadgetIcon[ loadout.gadget ] ), icon_size.y, textPos, diesel_yellow.linear );
		PrintMoveImage( FindMaterial( cgs.media.shaderWeaponIcon[ loadout.weapons[ WeaponCategory_Backup ] ] ), icon_size.y, textPos, diesel_yellow.linear );
		PrintMoveImage( FindMaterial( cgs.media.shaderWeaponIcon[ loadout.weapons[ WeaponCategory_Secondary ] ] ), icon_size.y, textPos, diesel_yellow.linear );
		PrintMoveImage( FindMaterial( cgs.media.shaderWeaponIcon[ loadout.weapons[ WeaponCategory_Primary ] ] ), icon_size.y, textPos, diesel_yellow.linear );
		PrintMoveImage( FindMaterial( cgs.media.shaderPerkIcon[ loadout.perk ] ), icon_size.y, textPos, diesel_yellow.linear );
		textPos.y = (title_height - textSize.y) * 0.5f;

		Span< const char > playerName = PlayerName( cg.predictedPlayerState.playerNum );
		ImVec2 playerTextSize = ImGui::CalcTextSize( playerName );

		float predictedPos = textPos.x - ImGui::CalcTextSize( " AND I'M " ).x - ImGui::CalcTextSize( "I AM " ).x * 2.f - playerTextSize.x;
		float letterWidth = playerTextSize.x / playerName.n;
		int excessLetters = Max2(3.f, -predictedPos / letterWidth); //the excess is the negative part

		if( predictedPos > 0.f || excessLetters < playerName.n ) { //don't print name if the window is too small
			PrintMoveText( " AND I'M ", textPos );

			ScopedColor( ImGuiCol_Text, diesel_yellow.linear );
			if( predictedPos <= 0.f ) {
				PrintMoveText( "...", textPos );
				PrintMoveText( playerName.slice( 0, playerName.n - excessLetters ), textPos );
			} else {
				PrintMoveText( playerName, textPos );
			}
		}

		PrintMoveText( "I AM ", textPos );
	}

	Optional< Key > key1, key2;
	GetKeyBindsForCommand( "loadoutmenu", &key1, &key2 );

	return should_close || ( key1.exists && ImGui::Shortcut( KeyToImGui( key1.value ) ) ) || ( key2.exists && ImGui::Shortcut( KeyToImGui( key2.value ) ) );
}

static void GameMenu() {
	TempAllocator temp = cls.frame_arena.temp();

	bool spectating = cg.predictedPlayerState.real_team == Team_None;
	bool ready = false;

	if( client_gs.gameState.match_state <= MatchState_Warmup ) {
		ready = cg.predictedPlayerState.ready;
	}
	else if( client_gs.gameState.match_state == MatchState_Countdown ) {
		ready = true;
	}

	ScopedColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 225 ) );
	bool should_close = false;

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	/*
	 * bad hack to fix being able to click outside of the ingame menus and
	 * breaking the escape to close hotkey
	 *
	 * unconditionally setting focus breaks stuff like dropdown menus so do
	 * this instead
	 */
	if( !ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) ) {
		ImGui::SetNextWindowFocus();
	}

	if( gamemenu_state == GameMenuState_Menu ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, 0, Vec2( 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 500, 0 ) * GetContentScale() );
		ImGui::Begin( "gamemenu", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );
		ImGuiStyle & style = ImGui::GetStyle();
		const double half = ImGui::GetWindowWidth() / 2 - style.ItemSpacing.x - style.ItemInnerSpacing.x;

		if( spectating ) {
			GameMenuButton( "Join Game", "join", &should_close );
			ImGui::Columns( 1 );
		}
		else {
			if( client_gs.gameState.match_state <= MatchState_Countdown ) {
				ScopedColor( ImGuiCol_Text, ready ? diesel_red.linear : diesel_green.linear );
				GameMenuButton( ready ? "Unready" : "Ready", "toggleready", &should_close );
			}

			GameMenuButton( "Spectate", "spectate", &should_close );
			GameMenuButton( "Change weapons", "loadoutmenu", &should_close );
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

		GameMenuButton( "Disconnect", "disconnect", NULL, 0 );
		ImGui::NextColumn();
		GameMenuButton( "Exit game", "quit", &should_close, 1 );
		ImGui::NextColumn();

		ImGui::Columns( 1 );
	}
	else if( gamemenu_state == GameMenuState_Loadout ) {
		if( LoadoutMenu() ) {
			should_close = true;
		}
	}
	else if( gamemenu_state == GameMenuState_Vote ) {
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

		Span< const char > vote;
		Span< const char > arg;
		if( e == 0 ) {
			vote = "start";
			arg = "";
		} else if( e == 1 ) {
			vote = "map";
			arg = SelectableMapList( true );
		} else {
			vote = e == 2 ? "spectate"_sp : "kick"_sp;
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

	if( ImGui::Shortcut( ImGuiKey_Escape ) || should_close ) {
		uistate = UIState_Hidden;
	}

	ImGui::End();
}

static void DemoMenu() {
	ScopedColor( ImGuiCol_WindowBg, IM_COL32( 0x1a, 0x1a, 0x1a, 192 ) );
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

		GameMenuButton( "Disconnect to main menu", "disconnect", NULL );
		GameMenuButton( "Exit to desktop", "quit", &should_close );
	} else if( demomenu_state == DemoMenuState_Settings ) {
		ImGui::SetNextWindowPos( displaySize * 0.5f, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( Max2( 800.f, displaySize.x * 0.65f ), Max2( 600.f, displaySize.y * 0.65f ) ) );
		ImGui::Begin( "settings", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

		Settings();
	}

	if( ImGui::Shortcut( ImGuiKey_Escape ) || should_close ) {
		uistate = UIState_Hidden;
	}

	ImGui::End();
}

void UI_Refresh() {
	TracyZoneScoped;

	if( uistate == UIState_DevTool ) {
		devtool_cleanup_callback = devtool_render_callback();
	}
	else if( devtool_cleanup_callback != NULL ) {
		devtool_cleanup_callback();
		devtool_render_callback = NULL;
		devtool_cleanup_callback = NULL;
	}

	if( uistate == UIState_GameMenu ) {
		GameMenu();
	}

	if( uistate == UIState_DemoMenu ) {
		DemoMenu();
	}

	if( uistate == UIState_MainMenu ) {
		MainMenu();
	}

	if( uistate == UIState_Connecting ) {
		ImGui::SetNextWindowPos( ImVec2() );
		ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width, frame_static.viewport_height ) );
		ImGui::Begin( "mainmenu", WindowZOrder_Menu, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration );

		constexpr Span< const char > connecting = "Connecting...";
		ImGui::PushFont( cls.large_font );
		ImGui::SetCursorPos( 0.5f * ( ImGui::GetContentRegionAvail() - ImGui::CalcTextSize( connecting ) ) );
		ImGui::Text( connecting );
		ImGui::PopFont();

		ImGui::End();
	}

	if( Con_IsVisible() ) {
		Con_Draw();
	}
}

void UI_ShowConnectingScreen() {
	uistate = UIState_Connecting;
}

void UI_ShowMainMenu() {
	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_Main;
	StartMenuMusic();
	Refresh();
}

void UI_ShowGameMenu() {
	// so the menu doesn't instantly close
	ImGui::GetIO().AddKeyEvent( ImGuiKey_Escape, false );

	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Menu;
}

void UI_ShowDemoMenu() {
	// so the menu doesn't instantly close
	ImGui::GetIO().AddKeyEvent( ImGuiKey_Escape, false );

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
