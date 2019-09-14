#include "cgame/cg_local.h"
#include "client/client.h"
#include "qcommon/string.h"

#include "imgui/imgui.h"

static char scoreboardString[ MAX_STRING_CHARS ];

struct ScoreboardPlayer {
	int state, ID, score, kills, ping;
};

void SCR_UpdateScoreboardMessage( const char *string ) {
	Q_strncpyz( scoreboardString, string, sizeof( scoreboardString ) );
}

bool CG_ScoreboardShown() {
	if( scoreboardString[0] != '&' ) { // nothing to draw
		return false;
	}

	if( cgs.demoPlaying || cg.frame.multipov ) {
		return cg.showScoreboard || ( GS_MatchState() > MATCH_STATE_PLAYTIME );
	}

	return ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD ) != 0;
}

static void CenterText( const char * text, Vec2 box_size, Vec2 pos = Vec2( 0, 0 ) ) {
	Vec2 text_size = ImGui::CalcTextSize( text );
	ImGui::SetCursorPos( pos + ( box_size - text_size ) * 0.5f );
	ImGui::Text( "%s", text );
}

static void CenterTextWindow( const char * title, const char * text, Vec2 size, ImGuiWindowFlags flags ) {
	ImGui::BeginChild( title, size, flags );
	CenterText( text, size );
	ImGui::EndChild();
}

static bool ParsePlayer( const char ** token, ScoreboardPlayer * player, char * starter ) { //true means that there is a player after this one.
	char * last;
	int * values[] = { &(player->ID), &(player->ping), &(player->score), &(player->kills), &(player->state) };
	int i = 0;

	if(strcmp(starter, "&p") != 0)
		return false;

	while( (last = COM_Parse( token )) != NULL) {
		if( last[0] == '&' ) {
			starter = last;
			return true;
		}
		*values[i] = atoi(last);
		i++;
	}

	return false;
}
	

void CG_DrawScoreboard() {
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );

	const char * token = scoreboardString;
	char * last = COM_Parse( &token );
	bool warmup = GS_MatchState() == MATCH_STATE_WARMUP || GS_MatchState() == MATCH_STATE_COUNTDOWN;
	ScoreboardPlayer player;

	ImGuiIO & io = ImGui::GetIO();
	ImVec2 size = io.DisplaySize;

	size.x *= 0.6f;
	size.y *= 0.8f;
	const float tab_height = size.y/22; //use float for precision

	ImGuiWindowFlags basic_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::SetNextWindowSize( ImVec2( size.x, -1 ) );
	ImGui::SetNextWindowPosCenter();

	if( GS_TeamBasedGametype() ) {
		//whole background window
		ImGui::Begin( "scoreboard", NULL, basic_flags | ImGuiWindowFlags_NoBackground );

		//team tab
		while( strcmp(last, "&s") != 0 ) {
			int team = atoi(COM_Parse(&token));
			RGB8 color = CG_TeamColor( team );

			//team name and score tab
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, 100 ) );
			ImGui::BeginChild( team, ImVec2( size.x, size.y/10 ), basic_flags );
			ImGui::PushFont( io.DisplaySize.x > 1280 ? cls.large_font : cls.medium_font );
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 100 ) );
			CenterTextWindow( String<16>("{}score", team), COM_Parse(&token), ImVec2( size.x/10, size.y/10 ), basic_flags );
			ImGui::PopStyleColor();
			CenterText( GS_DefaultTeamName(team), ImVec2( size.x, size.y/10 ) );
			ImGui::PopFont();
			ImGui::EndChild();
			ImGui::PopStyleColor();

			int num_players = Max2( 5, atoi(COM_Parse(&token)) );


			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( 75, 75, 75, 100 ) );
			ImGui::BeginChild( String<16>("{}", team), ImVec2(size.x, size.y/25), basic_flags );

			CenterText( "Score", ImVec2( size.x/10, size.y/25 ), ImVec2(size.x*7/10, 0) );
			CenterText( "Kills", ImVec2( size.x/10, size.y/25 ), ImVec2(size.x*8/10, 0) );
			CenterText( "Ping", ImVec2( size.x/10, size.y/25 ), ImVec2(size.x*9/10, 0) );

			ImGui::EndChild();
			ImGui::PopStyleColor();

			//players infos tab
			int height = 0;
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, 75 ));
			ImGui::BeginChild( String<16>("{}players", team), ImVec2(size.x, tab_height*num_players), basic_flags );

			last = COM_Parse(&token);
			while( ParsePlayer( &token, &player, last ) ) {
				int ID = (player.ID < 0 ? -1 - player.ID : player.ID);
				if(player.state) {
					ImGui::SetCursorPos(ImVec2(tab_height/8, height + tab_height/8));
					if( warmup )
						ImGui::Image( CG_MediaShader( cgs.media.shaderTick ), ImVec2(tab_height/1.25, tab_height/1.25) );
					else if( cg_entities[ID+1].current.team == cg.predictedPlayerState.stats[STAT_TEAM] )
						ImGui::Image( CG_MediaShader( cgs.media.shaderBombIcon ), ImVec2(tab_height/1.25, tab_height/1.25) );
				}

				//player name
				uint8_t a = 255;
				TempAllocator tmp = cls.frame_arena->temp();
				DynamicString final_name( &tmp );
				//if player is dead
				if( player.ID < 0 ) a = 75;

				CL_ImGuiExpandColorTokens( &final_name, cgs.clientInfo[ID].name, a );
				ImVec2 t_size = ImGui::CalcTextSize(final_name.c_str());
				ImGui::SetCursorPos( ImVec2(tab_height, height + (tab_height - t_size.y)/2 ) );
				ImGui::Text( "%s", final_name.c_str() );

				CenterText( String<8>("{}", player.score), ImVec2( size.x/10, tab_height ), ImVec2(size.x*7/10, height) );

				CenterText( String<8>("{}", player.kills), ImVec2( size.x/10, tab_height ), ImVec2(size.x*8/10, height ) );

				uint8_t escape[] = { 033, 255, Max2(1, 255 - player.ping), Max2(1, 255 - player.ping*2), 255 };
				CenterText( String<16>("{}{}", (char *)escape, player.ping), ImVec2( size.x/10, tab_height ), ImVec2( size.x*9/10, height ) );

				height += tab_height;
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
	} else {
		int num_players = atoi(COM_Parse(&token));

		ImGui::Begin( "scoreboard", NULL, basic_flags | ImGuiWindowFlags_NoBackground );
		ImGui::PushFont( cls.large_font );
		ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( 255, 255, 255, 100 ) );
		CenterTextWindow( "title", "Gladiator", ImVec2( size.x, size.y/10 ), basic_flags );
		ImGui::PopStyleColor();
		ImGui::PopFont();

		ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( 75, 75, 75, 100 ) );
		ImGui::BeginChild( "scoreboard", ImVec2( size.x, size.y/25 ), basic_flags );

		CenterText( "Score", ImVec2( size.x/10, size.y/25 ), ImVec2( size.x*7/10, 0 ) );
		CenterText( "Kills", ImVec2( size.x/10, size.y/25 ), ImVec2( size.x*8/10, 0 ) );
		CenterText( "Ping", ImVec2( size.x/10, size.y/25 ), ImVec2( size.x*9/10, 0 ) );

		ImGui::EndChild();
		ImGui::PopStyleColor();

		if(num_players) {
			int height = 0;
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32( 255, 255, 255, 75 ) );
			ImGui::BeginChild( "players", ImVec2(size.x, tab_height*num_players), basic_flags );
			//players tab
			last = COM_Parse(&token);
			while( ParsePlayer( &token, &player, last ) ) {
				ImGui::SetCursorPos(ImVec2(tab_height/8, height + tab_height/8));
				if(player.state) {
					if(warmup)		ImGui::Image(CG_MediaShader( cgs.media.shaderTick ), ImVec2(tab_height/1.25, tab_height/1.25), ImVec2(0, 0), ImVec2(1, 1), ImColor(255,255,255,255), ImColor(0,0,0,0));
					else			ImGui::Image(CG_MediaShader( cgs.media.shaderAlive ), ImVec2(tab_height/1.25, tab_height/1.25), ImVec2(0, 0), ImVec2(1, 1), ImColor(255,255,255,255), ImColor(0,0,0,0));
				} else if(!warmup)	ImGui::Image(CG_MediaShader( cgs.media.shaderDead ), ImVec2(tab_height/1.25, tab_height/1.25), ImVec2(0, 0), ImVec2(1, 1), ImColor(255,255,255,255), ImColor(0,0,0,0));

				//player name
				uint8_t a = 255;
				TempAllocator tmp = cls.frame_arena->temp();
				DynamicString final_name( &tmp );
				//if player is dead
				if( player.ID < 0 ) {
					player.ID = -1 - player.ID;
					a = 75;
				}
				CL_ImGuiExpandColorTokens( &final_name, cgs.clientInfo[player.ID].name, a );
				ImVec2 t_size = ImGui::CalcTextSize(final_name.c_str());
				ImGui::SetCursorPos( ImVec2(tab_height, height + (tab_height - t_size.y)/2 ) );
				ImGui::Text( "%s", final_name.c_str() );

				CenterText( String<16>("{}", player.score), ImVec2( size.x/10, tab_height ), ImVec2( size.x*7/10, height ) );

				CenterText( String<16>("{}", player.kills), ImVec2( size.x/10, tab_height ), ImVec2( size.x*8/10, height ) );

				uint8_t escape[] = { 033, 255, Max2(1, 255 - player.ping), Max2(1, 255 - player.ping*2), 255 };
				CenterText( String<16>("{}{}", (char *)escape, player.ping), ImVec2( size.x/10, tab_height ), ImVec2( size.x*9/10, height ) );

				height += tab_height;
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
	}

	//spectators
	if(*(last = COM_Parse(&token)) == NULL) { //if no spectators
		ImGui::End();
		ImGui::PopStyleVar( 2 );
		return;
	}

	TempAllocator tmp = cls.frame_arena->temp();
	DynamicString final_str( &tmp );
	String< 256 > spectators = "Spectating: ";

	while(*last) {
		spectators += cgs.clientInfo[atoi(last)].name;
		COM_Parse(&token);
		if(*(last = COM_Parse(&token))) {
			spectators += S_COLOR_WHITE ", ";
		}
	}
	CL_ImGuiExpandColorTokens( &final_str, spectators, 200 );
	CenterTextWindow("spec", final_str.c_str(), ImVec2(size.x, size.y/10), basic_flags | ImGuiWindowFlags_NoBackground);

	ImGui::End();

	ImGui::PopStyleVar( 2 );
}

void CG_ScoresOn_f() {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = true;
	}
	else {
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 1" );
	}
}

void CG_ScoresOff_f() {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = false;
	}
	else {
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 0" );
	}
}
