#include "cgame/cg_local.h"
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

	s64 lastHighlightTime;
};

static Chat chat;

static void OpenChat() {
	if( !cls.demo.playing ) {
		chat.mode = ChatMode_Say;
		chat.input[ 0 ] = '\0';
		CL_SetKeyDest( key_message );
	}
}

static void OpenTeamChat() {
	if( !cls.demo.playing ) {
		chat.mode = Cmd_Exists( "say_team" ) ? ChatMode_SayTeam : ChatMode_Say;
		chat.input[ 0 ] = '\0';
		CL_SetKeyDest( key_message );
	}
}

static void CloseChat() {
	chat.mode = ChatMode_None;
	CL_SetKeyDest( key_game );
}

void CG_InitChat() {
	chat = { };

	Cmd_AddCommand( "messagemode", OpenChat );
	Cmd_AddCommand( "messagemode2", OpenTeamChat );
}

void CG_ShutdownChat() {
	Cmd_RemoveCommand( "messagemode" );
	Cmd_RemoveCommand( "messagemode2" );
}

void CG_AddChat( const char * str ) {
	size_t idx = ( chat.history_head + chat.history_len ) % ARRAY_COUNT( chat.history );
	chat.history[ idx ].time = cls.monotonicTime;
	Q_strncpyz( chat.history[ idx ].text, str, sizeof( chat.history[ idx ].text ) );

	if( chat.history_len < ARRAY_COUNT( chat.history ) ) {
		chat.history_len++;
	}
	else {
		chat.history_head = ( chat.history_head + 1 ) % GAMECHAT_STACK_SIZE;
	}
}

#define GAMECHAT_NOTIFY_TIME        5000
#define GAMECHAT_WAIT_OUT_TIME      4000
#define GAMECHAT_HIGHLIGHT_TIME     4000
#define GAMECHAT_FADE_OUT_TIME      ( GAMECHAT_NOTIFY_TIME - GAMECHAT_WAIT_OUT_TIME )

static void SendChat() {
	if( strlen( chat.input ) > 0 ) {
		// convert double quotes to single quotes
		for( char * p = chat.input; *p != '\0'; p++ ) {
			if( *p == '"' ) {
				*p = '\'';
			}
		}

		TempAllocator temp = cls.frame_arena.temp();

		const char * cmd = chat.mode == ChatMode_SayTeam && Cmd_Exists( "say_team" ) ? "say_team" : "say";
		Cbuf_AddText( temp( "{} \"{}\"\n", cmd, chat.input ) );
	}

	CloseChat();
}

void CG_DrawChat() {
	TempAllocator temp = cls.frame_arena.temp();

	const ImGuiIO & io = ImGui::GetIO();
	float width_frac = Lerp( 0.5f, Clamp01( Unlerp( 1024.0f, io.DisplaySize.x, 1920.0f ) ), 0.25f );
	Vec2 size = io.DisplaySize * Vec2( width_frac, 0.25f );

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
	ImGuiWindowFlags log_flags = 0;
	if( chat.mode == ChatMode_None ) {
		flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
		log_flags |= ImGuiWindowFlags_NoScrollbar;
	}

	ImGui::SetNextWindowSize( ImVec2( size.x, size.y ) );
	ImGui::SetNextWindowPos( ImVec2( 0, size.y * 3 ), ImGuiCond_Always, ImVec2( 0, 0.5f ) );
	ImGui::Begin( "chat", WindowZOrder_Chat, flags );

	ImGui::BeginChild( "chatlog", ImVec2( 0, -ImGui::GetFrameHeight() ), false, log_flags );
	for( size_t i = 0; i < chat.history_len; i++ ) {
		size_t idx = ( chat.history_head + i ) % ARRAY_COUNT( chat.history );
		const ChatMessage * msg = &chat.history[ idx ];

		if( chat.mode == ChatMode_None && cls.monotonicTime > msg->time + GAMECHAT_NOTIFY_TIME ) {
			continue;
		}

		ImGui::TextWrapped( "%s", msg->text );
		ImGui::SetScrollHereY( 1.0f );
	}
	ImGui::EndChild();

	if( chat.mode != ChatMode_None ) {
		RGB8 color = { 50, 50, 50 };
		if( chat.mode == ChatMode_SayTeam ) {
			color = CG_TeamColor( TEAM_ALLY );
		}

		ImGui::PushStyleColor( ImGuiCol_FrameBg, IM_COL32( color.r, color.g, color.b, 50 ) );

		ImGui::PushItemWidth( ImGui::GetWindowWidth() );
		ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText( "##chatinput", chat.input, sizeof( chat.input ), ImGuiInputTextFlags_EnterReturnsTrue );

		if( enter ) {
			SendChat();
		}

		ImGui::PopStyleColor();
	}

	ImGui::End();

	if( ImGui::IsKeyPressed( K_ESCAPE ) ) {
		CloseChat();
	}
}

void CG_FlashChatHighlight( const unsigned int fromIndex, const char *text ) {
	// dont highlight ourselves
	if( fromIndex == cgs.playerNum )
		return;

	// if we've been highlighted recently, dont let people spam it..
	bool eligible = !chat.lastHighlightTime || chat.lastHighlightTime + GAMECHAT_HIGHLIGHT_TIME < cls.realtime;

	// dont bother doing text match if we've been pinged recently
	if( !eligible )
		return;

	// do a case insensitive check for the local player name. remove all crap too
	char nameLower[MAX_STRING_CHARS];
	Q_strncpyz( nameLower, cgs.clientInfo[cgs.playerNum].name, MAX_STRING_CHARS );
	Q_strlwr( nameLower );

	char msgLower[MAX_CHAT_BYTES];
	Q_strncpyz( msgLower, text, MAX_CHAT_BYTES );
	Q_strlwr( msgLower );

	// TODO: text match fuzzy ? Levenshtien distance or something might be good here. or at least tokenizing and looking for word
	// this is probably shitty for some nicks
	bool hadNick = strstr( msgLower, nameLower ) != NULL;
	if( hadNick ) {
		trap_VID_FlashWindow();
		chat.lastHighlightTime = cls.realtime;
	}
}
