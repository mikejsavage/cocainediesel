#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "qcommon/string.h"

#include "imgui/imgui.h"

constexpr size_t CHAT_MESSAGE_SIZE = 512;
constexpr size_t CHAT_HISTORY_SIZE = 64;

enum ChatMode {
	ChatMode_None,
	ChatMode_Say,
	ChatMode_SayTeam,
};

struct ChatMessage {
	s64 time;
	char text[ CHAT_MESSAGE_SIZE ];
};

struct Chat {
	ChatMode mode;
	char input[ CHAT_MESSAGE_SIZE ];

	ChatMessage history[ CHAT_HISTORY_SIZE ];
	size_t history_head;
	size_t history_len;

	bool at_bottom;
	bool scroll_to_bottom;

	s64 lastHighlightTime;
};

static Chat chat;

static void OpenChat() {
	if( !cls.demo.playing ) {
		bool team = Q_stricmp( Cmd_Argv( 0 ), "messagemode2" ) == 0;
		chat.mode = team ? ChatMode_SayTeam : ChatMode_Say;
		chat.input[ 0 ] = '\0';
		chat.scroll_to_bottom = true;
		CL_SetKeyDest( key_message );
	}
}

static void CloseChat() {
	chat.mode = ChatMode_None;
	CL_SetKeyDest( key_game );
}

void CG_InitChat() {
	chat = { };

	AddCommand( "messagemode", OpenChat );
	AddCommand( "messagemode2", OpenChat );
}

void CG_ShutdownChat() {
	RemoveCommand( "messagemode" );
	RemoveCommand( "messagemode2" );
}

void CG_AddChat( const char * str ) {
	size_t idx = ( chat.history_head + chat.history_len ) % ARRAY_COUNT( chat.history );
	chat.history[ idx ].time = cls.monotonicTime;
	Q_strncpyz( chat.history[ idx ].text, str, sizeof( chat.history[ idx ].text ) );

	if( chat.history_len < ARRAY_COUNT( chat.history ) ) {
		chat.history_len++;
	}
	else {
		chat.history_head = ( chat.history_head + 1 ) % CHAT_HISTORY_SIZE;
	}

	if( chat.at_bottom ) {
		chat.scroll_to_bottom = true;
	}
}

#define GAMECHAT_NOTIFY_TIME        5000
#define GAMECHAT_WAIT_OUT_TIME      4000
#define GAMECHAT_HIGHLIGHT_TIME     4000
#define GAMECHAT_FADE_OUT_TIME      ( GAMECHAT_NOTIFY_TIME - GAMECHAT_WAIT_OUT_TIME )

static void SendChat() {
	if( strlen( chat.input ) > 0 ) {
		TempAllocator temp = cls.frame_arena.temp();

		const char * cmd = chat.mode == ChatMode_SayTeam ? "say_team" : "say";
		Cbuf_Add( "{} {}", cmd, chat.input );

		S_StartGlobalSound( "sounds/typewriter/return", CHAN_AUTO, 1.0f, 1.0f );
	}

	CloseChat();
}

static int InputCallback( ImGuiInputTextCallbackData * data ) {
	if( data->EventChar == ' ' ) {
		S_StartGlobalSound( "sounds/typewriter/space", CHAN_AUTO, 1.0f, 1.0f );
		Cbuf_Add( "typewriterspace" );
	}
	else {
		S_StartGlobalSound( "sounds/typewriter/clack", CHAN_AUTO, 1.0f, 1.0f );
		Cbuf_Add( "typewriterclack" );
	}
	return 0;
}

void CG_DrawChat() {
	TempAllocator temp = cls.frame_arena.temp();

	float width_frac = Lerp( 0.5f, Unlerp01( 1024.0f, float( frame_static.viewport_width ), 1920.0f ), 0.3f );
	Vec2 size = frame_static.viewport * Vec2( width_frac, 0.25f );

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
	ImGuiWindowFlags log_flags = ImGuiWindowFlags_AlwaysUseWindowPadding;
	if( chat.mode == ChatMode_None ) {
		flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
		log_flags |= ImGuiWindowFlags_NoScrollbar;
	}

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 0.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 8.0f, 8.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8.0f, 8.0f ) );

	// round size down to integer number of lines
	float extra_height = ImGui::GetFrameHeightWithSpacing() + 3 * ImGui::GetStyle().WindowPadding.y;
	size.y -= extra_height;
	size.y -= fmodf( size.y, ImGui::GetTextLineHeightWithSpacing() );
	size.y += extra_height;

	ImGui::SetNextWindowSize( ImVec2( size.x, size.y ) );
	ImGui::SetNextWindowPos( ImVec2( 8, size.y * 3 ), ImGuiCond_Always, ImVec2( 0, 1.0f ) );
	ImGui::Begin( "chat", WindowZOrder_Chat, flags );

	ImGui::BeginChild( "chatlog", ImVec2( 0, -ImGui::GetFrameHeight() ), false, log_flags );

	float wrap_width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

	for( size_t i = 0; i < chat.history_len; i++ ) {
		size_t idx = ( chat.history_head + i ) % ARRAY_COUNT( chat.history );
		const ChatMessage * msg = &chat.history[ idx ];

		if( chat.mode == ChatMode_None && cls.monotonicTime > msg->time + GAMECHAT_NOTIFY_TIME ) {
			ImGui::Dummy( ImGui::CalcTextSize( msg->text, NULL, false, wrap_width ) );
		}
		else {
			ImGui::TextWrapped( "%s", msg->text );
		}
	}

	if( chat.scroll_to_bottom ) {
		ImGui::SetScrollHereY( 1.0f );
		chat.scroll_to_bottom = false;
	}

	if( ImGui::IsKeyPressed( K_PGUP ) || ImGui::IsKeyPressed( K_PGDN ) ) {
		float scroll = ImGui::GetScrollY();
		float page = ImGui::GetWindowSize().y - ImGui::GetTextLineHeight();
		scroll += page * ( ImGui::IsKeyPressed( K_PGUP ) ? -1 : 1 );
		scroll = Clamp( 0.0f, scroll, ImGui::GetScrollMaxY() );
		ImGui::SetScrollY( scroll );
	}

	chat.at_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();

	ImGui::EndChild();

	if( chat.mode != ChatMode_None ) {
		RGB8 color = { 50, 50, 50 };
		if( chat.mode == ChatMode_SayTeam ) {
			color = CG_TeamColor( TEAM_ALLY );
		}

		ImGui::PushStyleColor( ImGuiCol_FrameBg, IM_COL32( color.r, color.g, color.b, 50 ) );

		ImGuiInputTextFlags input_flags = 0;
		input_flags |= ImGuiInputTextFlags_CallbackCharFilter;
		input_flags |= ImGuiInputTextFlags_EnterReturnsTrue;

		ImGui::PushItemWidth( ImGui::GetWindowWidth() );
		ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText( "##chatinput", chat.input, sizeof( chat.input ), input_flags, InputCallback );

		if( enter ) {
			SendChat();
		}

		ImGui::PopStyleColor();
	}

	if( ImGui::Hotkey( K_ESCAPE ) ) {
		CloseChat();
	}

	ImGui::End();
	ImGui::PopStyleVar( 3 );
}
