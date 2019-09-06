#include "cgame/cg_local.h"
#include "client/client.h"
#include "qcommon/string.h"

#include "imgui/imgui.h"

static char scoreboardString[ MAX_STRING_CHARS ];

void SCR_UpdateScoreboardMessage( const char *string ) {
	Q_strncpyz( scoreboardString, string, sizeof( scoreboardString ) );
	printf( "%s\n", string );
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

void CG_DrawScoreboard() {
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );

	const char * token = scoreboardString;
	char * last = COM_Parse( &token );
	bool warmup = GS_MatchState() == MATCH_STATE_WARMUP || GS_MatchState() == MATCH_STATE_COUNTDOWN;

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
			int num_players = 0;
			for(int i = 7; scoreboardString[i] != 't' && scoreboardString[i] != 's'; i++ ) if(scoreboardString[i] == 'p') num_players++;
			if(num_players < 5) num_players = 5;


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

			last = COM_Parse(&token);


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
			while( strcmp(last, "&t") != 0 && strcmp(last, "&s") != 0 ) {
				if(atoi(COM_Parse(&token))) {
					ImGui::SetCursorPos(ImVec2(tab_height/8, height + tab_height/8));
					if(warmup)	ImGui::Image( CG_MediaShader( cgs.media.shaderTick ), ImVec2(tab_height/1.25, tab_height/1.25) );
					else		ImGui::Image( CG_MediaShader( cgs.media.shaderBombIcon ), ImVec2(tab_height/1.25, tab_height/1.25) );
				}

				//player name
				int ply = atoi(COM_Parse(&token));
				uint8_t a = 255;
				TempAllocator tmp = cls.frame_arena->temp();
				DynamicString final_name( &tmp );
				//if player is dead
				if( ply < 0 ) {
					ply = -1 - ply;
					a = 75;
				}
				CL_ImGuiExpandColorTokens( &final_name, cgs.clientInfo[ply].name, a );
				ImVec2 t_size = ImGui::CalcTextSize(final_name.c_str());
				ImGui::SetCursorPos( ImVec2(tab_height, height + (tab_height - t_size.y)/2 ) );
				ImGui::Text( "%s", final_name.c_str() );

				CenterText( COM_Parse(&token), ImVec2( size.x/10, tab_height ), ImVec2(size.x*7/10, height) );

				CenterText( COM_Parse(&token), ImVec2( size.x/10, tab_height ), ImVec2(size.x*8/10, height ) );

				int ping = atoi(COM_Parse(&token));
				uint8_t escape[] = { 033, 255, Max2(1, 255 - ping), Max2(1, 255 - ping*2), 255 };
				CenterText( String<16>("{}{}", (char *)escape, ping), ImVec2( size.x/10, tab_height ), ImVec2( size.x*9/10, height ) );

				last = COM_Parse(&token);
				height += tab_height;
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
	} else {
		COM_Parse(&token);
		COM_Parse(&token);
		last = COM_Parse(&token);

		int num_players = 0;
		for(int i = 7; scoreboardString[i] != 's'; i++ ) if(scoreboardString[i] == 'p') num_players++;

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
			while( strcmp(last, "&s") != 0 ) {
				ImGui::SetCursorPos(ImVec2(tab_height/8, height + tab_height/8));
				if(atoi(COM_Parse(&token))) {
					if(warmup)		ImGui::Image(CG_MediaShader( cgs.media.shaderTick ), ImVec2(tab_height/1.25, tab_height/1.25), ImVec2(0, 0), ImVec2(1, 1), ImColor(255,255,255,255), ImColor(0,0,0,0));
					else			ImGui::Image(CG_MediaShader( cgs.media.shaderAlive ), ImVec2(tab_height/1.25, tab_height/1.25), ImVec2(0, 0), ImVec2(1, 1), ImColor(255,255,255,255), ImColor(0,0,0,0));
				} else if(!warmup)	ImGui::Image(CG_MediaShader( cgs.media.shaderDead ), ImVec2(tab_height/1.25, tab_height/1.25), ImVec2(0, 0), ImVec2(1, 1), ImColor(255,255,255,255), ImColor(0,0,0,0));

				//player name
				int ply = atoi(COM_Parse(&token));
				uint8_t a = 255;
				TempAllocator tmp = cls.frame_arena->temp();
				DynamicString final_name( &tmp );
				//if player is dead
				if( ply < 0 ) {
					ply = -1 - ply;
					a = 75;
				}
				CL_ImGuiExpandColorTokens( &final_name, cgs.clientInfo[ply].name, a );
				ImVec2 t_size = ImGui::CalcTextSize(final_name.c_str());
				ImGui::SetCursorPos( ImVec2(tab_height, height + (tab_height - t_size.y)/2 ) );
				ImGui::Text( "%s", final_name.c_str() );

				CenterText( COM_Parse(&token), ImVec2( size.x/10, tab_height ), ImVec2( size.x*7/10, height ) );

				CenterText( COM_Parse(&token), ImVec2( size.x/10, tab_height ), ImVec2( size.x*8/10, height ) );

				int ping = atoi(COM_Parse(&token));
				uint8_t escape[] = { 033, 255, Max2(1, 255 - ping), Max2(1, 255 - ping*2), 255 };
				CenterText( String<16>("{}{}", (char *)escape, ping), ImVec2( size.x/10, tab_height ), ImVec2( size.x*9/10, height ) );

				last = COM_Parse(&token);
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
