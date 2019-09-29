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



struct PlayerMsg {
	int team;
	int id;
};

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


static bool ParseInt( const char ** cursor, int * x ) {
	const char * token = COM_Parse( cursor );
	if( cursor == NULL )
		return false;
	*x = atoi( token );
	return true;
}


static bool ParsePlayer( const char ** cursor, PlayerMsg * player ) {
	bool ok = true;
	ok = ok && ParseInt( cursor, &player->team );
	ok = ok && ParseInt( cursor, &player->id );
	return ok;
}

/*
** CG_DrawChat
*/
void CG_DrawChat( cg_gamechat_t *chat ) {
	TempAllocator temp = cls.frame_arena->temp();

	ImGuiIO & io = ImGui::GetIO();
	Vec2 size = io.DisplaySize;
	size.y /= 4;

	ImGui::SetNextWindowSize( ImVec2( size.x*0.95f, size.y ) );
	ImGui::SetNextWindowPos( ImVec2(0, size.y*3), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::Begin( "chat", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground );

	int l;
	int message_mode;
	const cg_gamemessage_t *msg;
	const char *msg_text;
	char tstr[GAMECHAT_STRING_SIZE];
	bool chat_active = false;
	bool background_drawn = false;

	message_mode = (int)trap_Cvar_Value( "con_messageMode" );
	chat_active = ( chat->lastMsgTime + GAMECHAT_WAIT_IN_TIME + GAMECHAT_FADE_IN_TIME > cg.realTime || message_mode );

	if( chat_active ) {
		
	} else {

	}

	for( int i = 0; i < GAMECHAT_STACK_SIZE; i++ ) {
		l = chat->nextMsg - 1 - i;
		if( l < 0 ) {
			l = GAMECHAT_STACK_SIZE + l;
		}

		PlayerMsg player;
		msg = &chat->messages[l];
		msg_text = msg->text;
		ParsePlayer( &msg_text, &player );

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

		ImGui::SetCursorPos( Vec2( 20, size.y - (i+1)*20 ) );

		if( msg_text != NULL ) {
			int alpha = ( message_mode ? 255 : ((msg->time) - cg.realTime + GAMECHAT_NOTIFY_TIME)/20);

			if( player.team == TEAM_SPECTATOR ) {
				DynamicString name( &temp, "{}{}", ImGuiColorToken(150, 150, 150, alpha), cgs.clientInfo[ player.id ].name );
				ImGui::Text( "%s", name.c_str() );
				ImGui::SetCursorPosX( ImGui::CalcTextSize( name.c_str() ).x*1.1f );
			} else if( player.team > 0 ) {
				RGB8 team_color = CG_TeamColor( player.team );
				DynamicString name( &temp, "{}{}", ImGuiColorToken(team_color.r, team_color.g, team_color.b, alpha), cgs.clientInfo[ player.id ].name );
				ImGui::Text( "%s", name.c_str() );
				ImGui::SetCursorPosX( ImGui::CalcTextSize( name.c_str() ).x*1.1f );
			}


			ImGui::SameLine();
			DynamicString text( &temp, "{}{}", ImGuiColorToken(255, 255, 255, alpha), msg_text );
			ImGui::Text( "%s", text.c_str() );
		}
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
