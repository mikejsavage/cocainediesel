#include "cgame/cg_local.h"
#include "client/client.h"
#include "qcommon/string.h"

#include "imgui/imgui.h"

static char scoreboard_string[ MAX_STRING_CHARS ];

struct ScoreboardTeam {
	int score;
	int num_players;
};

struct ScoreboardPlayer {
	int id;
	int ping;
	int score;
	int kills;
	int state;
};

void SCR_UpdateScoreboardMessage( const char * string ) {
	Q_strncpyz( scoreboard_string, string, sizeof( scoreboard_string ) );
}

bool CG_ScoreboardShown() {
	if( strlen( scoreboard_string ) == 0 ) {
		return false;
	}

	if( cgs.demoPlaying || cg.frame.multipov ) {
		return cg.showScoreboard || ( GS_MatchState() > MATCH_STATE_PLAYTIME );
	}

	return ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD ) != 0;
}

static void ColumnCenterText( const char * str ) {
	float width = ImGui::CalcTextSize( str ).x;
	ImGui::SetCursorPosX( ImGui::GetColumnOffset() + 0.5f * ( ImGui::GetColumnWidth() - width ) );
	ImGui::Text( "%s", str );
}

static void WindowCenterText( const char * str ) {
	Vec2 text_size = ImGui::CalcTextSize( str );
	ImGui::SetCursorPos( 0.5f * ( ImGui::GetWindowSize() - text_size ) );
	ImGui::Text( "%s", str );
}

static void CenterText( const char * text, Vec2 box_size, Vec2 pos = Vec2( 0, 0 ) ) {
	Vec2 text_size = ImGui::CalcTextSize( text );
	ImGui::SetCursorPos( pos + ( box_size - text_size ) * 0.5f );
	ImGui::Text( "%s", text );
}

static void CenterTextWindow( const char * title, const char * text, Vec2 size ) {
	ImGui::BeginChild( title, size, true );
	CenterText( text, size );
	ImGui::EndChild();
}

static bool ParseInt( const char ** cursor, int * x ) {
	const char * token = COM_Parse( cursor );
	if( cursor == NULL )
		return false;
	*x = atoi( token );
	return true;
}

static bool ParseTeam( const char ** cursor, ScoreboardTeam * team ) {
	bool ok = true;
	ok = ok && ParseInt( cursor, &team->score );
	ok = ok && ParseInt( cursor, &team->num_players );
	return ok;
}

static bool ParsePlayer( const char ** cursor, ScoreboardPlayer * player ) {
	bool ok = true;
	ok = ok && ParseInt( cursor, &player->id );
	ok = ok && ( ( player->id >= 0 && size_t( player->id ) < ARRAY_COUNT( cg_entities ) ) || ( player->id < 0 && size_t( -( player->id + 1 ) ) < ARRAY_COUNT( cg_entities ) ) );
	ok = ok && ParseInt( cursor, &player->ping );
	ok = ok && ParseInt( cursor, &player->score );
	ok = ok && ParseInt( cursor, &player->kills );
	ok = ok && ParseInt( cursor, &player->state );
	return ok;
}

static void TeamScoreboard( TempAllocator & temp, const char ** cursor, int team, float col_width ) {
	bool warmup = GS_MatchState() == MATCH_STATE_WARMUP || GS_MatchState() == MATCH_STATE_COUNTDOWN;

	ScoreboardTeam team_info;
	if( !ParseTeam( cursor, &team_info ) )
		return;

	RGB8 color = CG_TeamColor( team );

	int slots = Max2( team_info.num_players, 5 );
	ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, 255 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 8 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 8 ) );

	int line_height = ImGui::GetFrameHeight();

	// score box
	{
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, 255 ) );
		ImGui::BeginChild( temp( "{}score", team ), ImVec2( 5 * line_height, slots * line_height ), false );
		ImGui::PushFont( cls.huge_font );
		WindowCenterText( temp( "{}", team_info.score ) );
		ImGui::PopFont();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	// players
	{
		ImGui::SameLine();

		// TODO: srgb?
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r / 2, color.g / 2, color.b / 2, 255 ) );
		ImGui::BeginChild( temp( "{}paddedplayers", team ), ImVec2( 0, slots * line_height ), false );

		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r * 0.75f, color.g * 0.75f, color.b * 0.75f, 255 ) );
		ImGui::BeginChild( temp( "{}players", team ), ImVec2( 0, team_info.num_players * line_height ), false );

		ImGui::Columns( 5, NULL, false );
		ImGui::SetColumnWidth( 0, line_height );
		ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
		ImGui::SetColumnWidth( 2, col_width );
		ImGui::SetColumnWidth( 3, col_width );
		ImGui::SetColumnWidth( 4, col_width );

		for( int i = 0; i < team_info.num_players; i++ ) {
			ScoreboardPlayer player;
			if( !ParsePlayer( cursor, &player ) )
				break;

			int id = player.id < 0 ? -( player.id + 1 ) : player.id;
			if( player.state != 0 ) {
				float dim = ImGui::GetTextLineHeight();
				ImGui::SetCursorPos(Vec2((line_height-dim)/2, (line_height-dim)/2 + line_height*i));
				if( warmup )
					ImGui::Image( CG_MediaShader( cgs.media.shaderTick ), ImVec2( dim, dim ) );
				else if( cg_entities[id+1].current.team == cg.predictedPlayerState.stats[STAT_TEAM] )
					ImGui::Image( CG_MediaShader( cgs.media.shaderBombIcon ), ImVec2( dim, dim ) );
			}
			ImGui::NextColumn();

			// player name
			u8 alpha = player.id >= 0 ? 255 : 75;
			DynamicString final_name( &temp );
			CL_ImGuiExpandColorTokens( &final_name, cgs.clientInfo[ id ].name, alpha );
			ImGui::AlignTextToFramePadding();
			ImGui::Text( "%s", final_name.c_str() );
			ImGui::NextColumn();

			ImGui::AlignTextToFramePadding();
			ColumnCenterText( temp( "{}", player.score ) );
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( temp( "{}", player.kills ) );
			ImGui::NextColumn();

			ImGui::AlignTextToFramePadding();
			ImGuiColorToken color( Min2( 255, player.ping ), 0, 0, 255 );
			ColumnCenterText( temp( "{}{}", color, player.ping ) );
			ImGui::NextColumn();
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	ImGui::PopStyleVar( 2 );
	ImGui::PopStyleColor();
}

void CG_DrawScoreboard() {
	TempAllocator temp = cls.frame_arena->temp();
	bool warmup = GS_MatchState() == MATCH_STATE_WARMUP || GS_MatchState() == MATCH_STATE_COUNTDOWN;

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

	const char * cursor = scoreboard_string;

	ImGuiIO & io = ImGui::GetIO();
	Vec2 size = io.DisplaySize * Vec2( 0.8f, 0.8f );

	ImGuiWindowFlags basic_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::SetNextWindowSize( ImVec2( size.x, -1 ) );
	ImGui::SetNextWindowPosCenter();
	ImGui::Begin( "scoreboard", NULL, basic_flags | ImGuiWindowFlags_NoBackground );
	ImGui::PopStyleVar();
	defer {
		ImGui::End();
		ImGui::PopStyleVar();
	};

	float col_width = 80;

	if( GS_TeamBasedGametype() ) {
		float score_width = 5 * ( ImGui::GetTextLineHeight() + 2 * 8 );

		{
			ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 2, 2 ) );
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboardheader", ImVec2( 0, ImGui::GetFrameHeight() ), false );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "ATTACKING" );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "SCORE" );
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "KILLS" );
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "PING" );
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}

		int myteam = cg.predictedPlayerState.stats[ STAT_TEAM ];
		if( myteam == TEAM_SPECTATOR )
			myteam = TEAM_ALPHA;

		int round;
		if( !ParseInt( &cursor, &round ) )
			return;

		TeamScoreboard( temp, &cursor, myteam, col_width );

		{
			ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 2, 2 ) );
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboarddivider", ImVec2( 0, ImGui::GetFrameHeight() ), false );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ImGui::AlignTextToFramePadding();
			ColumnCenterText( round == 0 ? "WARMUP" : temp( "ROUND {}", round ) );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}

		TeamScoreboard( temp, &cursor, myteam == TEAM_ALPHA ? TEAM_BETA : TEAM_ALPHA, col_width );

		{
			ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 2, 2 ) );
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboardfooter", ImVec2( 0, ImGui::GetFrameHeight() ), false );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "DEFENDING" );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
	}
	else {
		ScoreboardTeam team_info;
		if( !ParseTeam( &cursor, &team_info ) )
			return;

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 8 ) );
		ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 16 ) );

		int line_height = ImGui::GetFrameHeight();

		{
			ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 2, 2 ) );
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboardheader", ImVec2( 0, ImGui::GetFrameHeight() ), false );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, line_height );
			ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( team_info.score == 0 ? "WARMUP" : temp( "ROUND {}", team_info.score) );
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "SCORE" );
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "KILLS" );
			ImGui::NextColumn();
			ImGui::AlignTextToFramePadding();
			ColumnCenterText( "PING" );
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}

		// players
		{
			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, 255 ) );
			for( int i = 0; i < team_info.num_players; i++ ) {
				ScoreboardPlayer player;
				if( !ParsePlayer( &cursor, &player ) )
					break;

				RGB8 ply_color = TEAM_COLORS[i].rgb;
				bool alive = player.id >= 0;

				ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( ply_color.r * 0.75f * (float(alive)/2 + 0.5f), ply_color.g * 0.75f * (float(alive)/2 + 0.5f), ply_color.b * 0.75f * (float(alive)/2 + 0.5f), 255 ) );
				ImGui::BeginChild( temp( "players{}", i ), ImVec2( 0, line_height ), false );

				ImGui::Columns( 5, NULL, false );
				ImGui::SetColumnWidth( 0, line_height );
				ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
				ImGui::SetColumnWidth( 2, col_width );
				ImGui::SetColumnWidth( 3, col_width );
				ImGui::SetColumnWidth( 4, col_width );

				int id = alive ? player.id : -( player.id + 1 );
				float dim = ImGui::GetTextLineHeight();
				ImGui::SetCursorPos(Vec2((line_height-dim)/2, (line_height-dim)/2));
				if( player.state != 0 ) {
					if( warmup )
						ImGui::Image( CG_MediaShader( cgs.media.shaderTick ), ImVec2( dim, dim ) );
					else
						ImGui::Image( CG_MediaShader( cgs.media.shaderAlive ), ImVec2( dim, dim ) );
				}
				else if( !warmup ) {
					ImGui::Image(CG_MediaShader( cgs.media.shaderDead ), ImVec2( dim, dim ));
				}

				ImGui::NextColumn();

				// player name
				u8 alpha = alive ? 255 : 75;
				DynamicString final_name( &temp );
				CL_ImGuiExpandColorTokens( &final_name, cgs.clientInfo[ id ].name, alpha );
				ImGui::AlignTextToFramePadding();
				ImGui::Text( "%s", final_name.c_str() );
				ImGui::NextColumn();

				ImGui::AlignTextToFramePadding();
				ColumnCenterText( temp( "{}", player.score ) );
				ImGui::NextColumn();
				ImGui::AlignTextToFramePadding();
				ColumnCenterText( temp( "{}", player.kills ) );
				ImGui::NextColumn();

				ImGui::AlignTextToFramePadding();
				ImGuiColorToken color( Min2( 255, player.ping ), 0, 0, 255 );
				ColumnCenterText( temp( "{}{}", color, player.ping ) );
				ImGui::NextColumn();

				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
			ImGui::PopStyleColor();
		}

		ImGui::PopStyleVar( 2 );
	}

	int num_spectators;
	if( !ParseInt( &cursor, &num_spectators ) || num_spectators == 0 )
		return;

	DynamicString spectators( &temp, "Spectating: " );
	for( int i = 0; i < num_spectators; i++ ) {
		int id;
		if( !ParseInt( &cursor, &id ) )
			break;
		if( i > 0 )
			spectators += S_COLOR_WHITE ", ";
		spectators += cgs.clientInfo[ id ].name;
	}

	DynamicString expanded( &temp );
	CL_ImGuiExpandColorTokens( &expanded, spectators.c_str(), 200 );
	CenterTextWindow( "spec", expanded.c_str(), ImVec2( size.x, size.y/10 ) );
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
