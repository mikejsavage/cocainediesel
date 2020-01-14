#include "cgame/cg_local.h"
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
		return cg.showScoreboard || ( GS_MatchState( &client_gs ) >= MATCH_STATE_POSTMATCH );
	}

	return cg.predictedPlayerState.show_scoreboard;
}

static bool ParseInt( const char ** cursor, int * x ) {
	Span< const char > token = ParseToken( cursor, Parse_DontStopOnNewLine );
	return SpanToInt( token, x );
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

static void DrawPlayerScoreboard( TempAllocator & temp, ScoreboardPlayer player, const char ** cursor, float line_height ) {
	bool alive = player.id >= 0;
	int id = alive ? player.id : -( player.id + 1 );

	// icon
	bool warmup = GS_MatchState( &client_gs ) == MATCH_STATE_WARMUP || GS_MatchState( &client_gs ) == MATCH_STATE_COUNTDOWN;
	const Material * icon = NULL;

	if( warmup ) {
		icon = player.state != 0 ? cgs.media.shaderReady : NULL;
	}
	else {
		bool carrier = player.state != 0 && ( ISREALSPECTATOR() || cg_entities[ id + 1 ].current.team == cg.predictedPlayerState.team );
		if( alive ) {
			icon = carrier ? cgs.media.shaderBombIcon : cgs.media.shaderAlive;
		}
		else {
			icon = cgs.media.shaderDead;
		}
	}

	if( icon != NULL ) {
		float dim = ImGui::GetTextLineHeight();
		ImGui::SetCursorPos( ImGui::GetCursorPos() - Vec2( ( dim - line_height ) * 0.5f ) );

		Vec2 half_pixel = 0.5f / Vec2( icon->texture->width, icon->texture->height );
		ImGui::Image( icon, Vec2( dim ), half_pixel, 1.0f - half_pixel, vec4_black );
	}

	ImGui::NextColumn();

	// player name
	u8 alpha = player.id >= 0 ? 255 : 75;
	DynamicString final_name( &temp, "{}{}", ImGuiColorToken( 0, 0, 0, alpha ), cgs.clientInfo[ id ].name );
	ImGui::AlignTextToFramePadding();
	ImGui::Text( "%s", final_name.c_str() );
	ImGui::NextColumn();

	// stats
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

static void DrawTeamScoreboard( TempAllocator & temp, const char ** cursor, int team, float col_width, u8 alpha ) {
	ScoreboardTeam team_info;
	if( !ParseTeam( cursor, &team_info ) )
		return;

	RGB8 color = CG_TeamColor( team );

	int slots = Max2( team_info.num_players, 5 );
	ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, alpha ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 8 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 8 ) );

	int line_height = ImGui::GetFrameHeight();

	// score box
	{
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, alpha ) );
		ImGui::BeginChild( temp( "{}score", team ), ImVec2( 5 * line_height, slots * line_height ) );
		ImGui::PushFont( cls.huge_font );
		WindowCenterTextXY( temp( "{}", team_info.score ) );
		ImGui::PopFont();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	// players
	{
		ImGui::SameLine();

		// TODO: srgb?
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r / 2, color.g / 2, color.b / 2, alpha ) );
		ImGui::BeginChild( temp( "{}paddedplayers", team ), ImVec2( 0, slots * line_height ) );

		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r * 0.75f, color.g * 0.75f, color.b * 0.75f, alpha ) );
		ImGui::BeginChild( temp( "{}players", team ), ImVec2( 0, team_info.num_players * line_height ) );

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

			DrawPlayerScoreboard( temp, player, cursor, line_height );
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
	ZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

	const char * cursor = scoreboard_string;

	const ImGuiIO & io = ImGui::GetIO();
	float width_frac = Lerp( 0.8f, Clamp01( Unlerp( 1024.0f, io.DisplaySize.x, 1920.0f ) ), 0.6f );
	Vec2 size = io.DisplaySize * Vec2( width_frac, 0.8f );

	ImGuiWindowFlags basic_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;

	ImGui::SetNextWindowSize( ImVec2( size.x, -1 ) );
	ImGui::SetNextWindowPos( io.DisplaySize * 0.5f, 0, Vec2( 0.5f ) );
	ImGui::Begin( "scoreboard", WindowZOrder_Scoreboard, basic_flags | ImGuiWindowFlags_NoBackground );

	float padding = 4;
	float separator_height = ImGui::GetTextLineHeight() + 2 * padding;
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, padding ) );

	defer {
		ImGui::PopStyleVar();
		ImGui::End();
		ImGui::PopStyleVar( 2 );
	};

	float col_width = 80;
	u8 alpha = 242;

	if( GS_TeamBasedGametype( &client_gs ) ) {
		float score_width = 5 * ( ImGui::GetTextLineHeight() + 2 * 8 );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
			ImGui::BeginChild( "scoreboardheader", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ColumnCenterText( "ATTACKING" );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ColumnCenterText( "SCORE" );
			ImGui::NextColumn();
			ColumnCenterText( "KILLS" );
			ImGui::NextColumn();
			ColumnCenterText( "PING" );
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		int round;
		if( !ParseInt( &cursor, &round ) )
			return;

		DrawTeamScoreboard( temp, &cursor, TEAM_ALPHA, col_width, alpha );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
			ImGui::BeginChild( "scoreboarddivider", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ColumnCenterText( round == 0 ? "WARMUP" : temp( "ROUND {}", round ) );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		DrawTeamScoreboard( temp, &cursor, TEAM_BETA, col_width, alpha );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
			ImGui::BeginChild( "scoreboardfooter", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ColumnCenterText( "DEFENDING" );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
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
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
			ImGui::BeginChild( "scoreboardheader", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, line_height );
			ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ImGui::NextColumn();
			ColumnCenterText( team_info.score == 0 ? "WARMUP" : temp( "ROUND {}", team_info.score) );
			ImGui::NextColumn();
			ColumnCenterText( "SCORE" );
			ImGui::NextColumn();
			ColumnCenterText( "KILLS" );
			ImGui::NextColumn();
			ColumnCenterText( "PING" );
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		// players
		{
			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, alpha ) );
			for( int i = 0; i < team_info.num_players; i++ ) {
				ScoreboardPlayer player;
				if( !ParsePlayer( &cursor, &player ) )
					break;

				RGB8 team_color = TEAM_COLORS[ i % ARRAY_COUNT( TEAM_COLORS ) ].rgb;
				bool alive = player.id >= 0;

				float bg_scale = alive ? 0.75f : 0.5f;
				team_color.r *= bg_scale;
				team_color.g *= bg_scale;
				team_color.b *= bg_scale;

				ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( team_color.r, team_color.g, team_color.b, alpha ) );
				ImGui::BeginChild( temp( "players{}", i ), ImVec2( 0, line_height ) );

				ImGui::Columns( 5, NULL, false );
				ImGui::SetColumnWidth( 0, line_height );
				ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
				ImGui::SetColumnWidth( 2, col_width );
				ImGui::SetColumnWidth( 3, col_width );
				ImGui::SetColumnWidth( 4, col_width );

				DrawPlayerScoreboard( temp, player, &cursor, line_height );

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

	ImGui::Dummy( ImVec2( 0, separator_height ) );

	ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
	ImGui::BeginChild( "spectators", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

	{
		DynamicString spectators( &temp, "Spectating: " );
		for( int i = 0; i < num_spectators; i++ ) {
			int id;
			if( !ParseInt( &cursor, &id ) )
				break;
			if( i > 0 )
				spectators += ", ";
			spectators += cgs.clientInfo[ id ].name;
		}

		ImGui::Text( "%s", spectators.c_str() );
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void CG_ScoresOn_f() {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = true;
	}
	else {
		Cbuf_ExecuteText( EXEC_NOW, "svscore 1" );
	}
}

void CG_ScoresOff_f() {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = false;
	}
	else {
		Cbuf_ExecuteText( EXEC_NOW, "svscore 0" );
	}
}
