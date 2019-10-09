/*
Copyright (C) 2010 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "cg_local.h"
#include "client/client.h"
#include "qcommon/string.h"

#include "imgui/imgui.h"


/*
** CG_InitChat
*/
void CG_InitChat( cg_gamechat_t *chat ) {
	memset( chat, 0, sizeof( *chat ) );
}

/*
** CG_StackChatString
*/
void CG_StackChatString( cg_gamechat_t *chat, const char *str ) {
	chat->messages[chat->nextMsg].time = cg.realTime;
	Q_strncpyz( chat->messages[chat->nextMsg].text, str, sizeof( chat->messages[0].text ) );

	chat->lastMsgTime = cg.realTime;
	chat->nextMsg = ( chat->nextMsg + 1 ) % GAMECHAT_STACK_SIZE;
}

#define GAMECHAT_NOTIFY_TIME        5000
#define GAMECHAT_WAIT_IN_TIME       0
#define GAMECHAT_FADE_IN_TIME       100
#define GAMECHAT_WAIT_OUT_TIME      4000
#define GAMECHAT_HIGHLIGHT_TIME     4000
#define GAMECHAT_FADE_OUT_TIME      ( GAMECHAT_NOTIFY_TIME - GAMECHAT_WAIT_OUT_TIME )

/*
** CG_DrawChat
*/
void CG_DrawChat( cg_gamechat_t *chat ) {
	TempAllocator temp = cls.frame_arena->temp();

	ImGuiIO & io = ImGui::GetIO();
	Vec2 size = io.DisplaySize;
	size.y /= 4;

	int message_mode = (int)trap_Cvar_Value( "con_messageMode" );

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
	if( message_mode == 0 ) {
		flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse;
	}

	ImGui::SetNextWindowSize( ImVec2( size.x*0.5f, size.y ) );
	ImGui::SetNextWindowPos( ImVec2(0, size.y*2.5f), ImGuiCond_Always, ImVec2(0, 0.5f));
	ImGui::Begin( "chat", NULL, flags );

	int l;
	const cg_gamemessage_t *msg;
	const char *msg_text;
	bool background_drawn = false;

	bool chat_active = ( chat->lastMsgTime + GAMECHAT_WAIT_IN_TIME + GAMECHAT_FADE_IN_TIME > cg.realTime || message_mode );


	if( message_mode ) {
		RGB8 color = { 50, 50, 50 };
		if( message_mode == 2 ) {
			color = CG_TeamColor( TEAM_ALLY );
		}

		ImGui::SetCursorPos( Vec2( 20, size.y - 40 ) );

		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, 50 ) );
		ImGui::BeginChild( "chat", Vec2( size.x, 40 ) );

		ImGui::SetCursorPos( Vec2( 10, 10 ) );
		ImGui::Text("%s", chat_buffer );

		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	for( int i = 0; i < GAMECHAT_STACK_SIZE; i++ ) {
		l = chat->nextMsg - 1 - i;
		if( l < 0 ) {
			l = GAMECHAT_STACK_SIZE + l;
		}

		msg = &chat->messages[l];
		msg_text = msg->text;
		bool old_msg = !message_mode && ( cg.realTime > msg->time + GAMECHAT_NOTIFY_TIME );

		if( !background_drawn ) {
			if( old_msg ) {
				if( !( !chat_active && cg.realTime <= chat->lastActiveChangeTime + 200 ) ) {
					break;
				}
			}

			background_drawn = true;
		}

		// unless user is typing something, only display recent messages
		if( old_msg ) {
			break;
		}

		int Y = size.y - (i+3)*20;
		if( Y < 0 ) {
			break;
		}
		ImGui::SetCursorPos( Vec2( 20, Y ) );

		const char *cursor = COM_Parse( &msg_text );
		RGB8 team_color = { 255, 255, 255 };
		if( strcmp( cursor, "[SPEC]") == 0 ) {
			team_color.r = 125;
			team_color.g = 125;
			team_color.b = 125;
			cursor = COM_Parse( &msg_text );
		}
		else if( strcmp( cursor, "[TEAM]" ) == 0 ) {
			team_color = CG_TeamColor( TEAM_ALLY );
			cursor = COM_Parse( &msg_text );
		}
		else if( strcmp( cursor, "Console" ) == 0 ) {
			team_color.r = 50;
			team_color.g = 255;
			team_color.b = 50;
			cursor = COM_Parse( &msg_text );
		}
		
		int alpha = ( message_mode ? 255 : ( msg->time  - cg.realTime + GAMECHAT_NOTIFY_TIME )/20 );

		DynamicString name( &temp, "{}{}", ImGuiColorToken(team_color.r, team_color.g, team_color.b, alpha), cursor );
		ImGui::Text( "%s", name.c_str() );
		ImGui::SetCursorPosX( ImGui::CalcTextSize( name.c_str() ).x*1.1f );

		cursor = COM_Parse( &msg_text );

		ImGui::SameLine();
		DynamicString text( &temp, "{}{}", ImGuiColorToken(Max2( team_color.r - 50, 0 ), Max2( team_color.g - 50, 0 ), Max2( team_color.b - 50, 0 ), alpha), cursor );
		ImGui::Text( "%s", text.c_str() );
	}

	chat->lastActive = chat_active;

	ImGui::End();
}

void CG_FlashChatHighlight( const unsigned int fromIndex, const char *text ) {
	// dont highlight ourselves
	if( fromIndex == cgs.playerNum )
		return;

	// if we've been highlighted recently, dont let people spam it.. 
	bool eligible = !cg.chat.lastHighlightTime || cg.chat.lastHighlightTime + GAMECHAT_HIGHLIGHT_TIME < cg.realTime;

	// dont bother doing text match if we've been pinged recently
	if( !eligible )
		return;

	// do a case insensitive check for the local player name. remove all crap too
	char nameLower[MAX_STRING_CHARS];
	Q_strncpyz( nameLower, cgs.clientInfo[cgs.playerNum].name, MAX_STRING_CHARS );
	Q_strlwr( nameLower );
	const char *plainName = COM_RemoveColorTokens( nameLower );

	char msgLower[MAX_CHAT_BYTES];
	Q_strncpyz( msgLower, text, MAX_CHAT_BYTES );
	Q_strlwr( msgLower );

	const char *msgUncolored = COM_RemoveColorTokens( msgLower );

	// TODO: text match fuzzy ? Levenshtien distance or something might be good here. or at least tokenizing and looking for word
	// this is probably shitty for some nicks
	bool hadNick = strstr( msgUncolored, plainName ) != NULL;
	if( hadNick ) {
		trap_VID_FlashWindow();
		cg.chat.lastHighlightTime = cg.realTime;
	}
}
