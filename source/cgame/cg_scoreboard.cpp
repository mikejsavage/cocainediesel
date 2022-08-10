#include "cgame/cg_local.h"
#include "qcommon/string.h"
#include "client/renderer/renderer.h"

#include "imgui/imgui.h"

bool CG_ScoreboardShown() {
	if( client_gs.gameState.match_state > MatchState_Playing ) {
		return true;
	}

	return cg.showScoreboard;
}

static void DrawPlayerScoreboard( TempAllocator & temp, int playerIndex, float line_height ) {
	SyncScoreboardPlayer * player = &client_gs.gameState.players[ playerIndex - 1 ];

	// icon
	bool warmup = client_gs.gameState.match_state == MatchState_Warmup || client_gs.gameState.match_state == MatchState_Countdown;
	const Material * icon = NULL;

	if( warmup ) {
		icon = player->ready ? FindMaterial( "hud/icons/ready" ) : NULL;
	}
	else {
		bool carrier = player->carrier && ( ISREALSPECTATOR() || cg_entities[ playerIndex ].current.team == cg.predictedPlayerState.team );
		if( player->alive ) {
			icon = carrier ? FindMaterial( "hud/icons/bomb" ) : FindMaterial( "hud/icons/alive" );
		}
		else {
			icon = FindMaterial( "hud/icons/dead" );
		}
	}

	if( icon != NULL ) {
		float dim = ImGui::GetTextLineHeight();
		ImGui::SetCursorPos( ImGui::GetCursorPos() - Vec2( ( dim - line_height ) * 0.5f ) );

		Vec2 half_pixel = HalfPixelSize( icon );
		ImGui::Image( icon, Vec2( dim ), half_pixel, 1.0f - half_pixel, vec4_black );
	}

	ImGui::NextColumn();

	// player name
	u8 alpha = player->alive ? 255 : 75;
	DynamicString final_name( &temp, "{}{}", ImGuiColorToken( 0, 0, 0, alpha ), PlayerName( playerIndex - 1 ) );
	ImGui::AlignTextToFramePadding();
	ImGui::Text( "%s", final_name.c_str() );
	ImGui::NextColumn();

	// stats
	ImGui::AlignTextToFramePadding();
	ColumnCenterText( temp( "{}", player->score ) );
	ImGui::NextColumn();
	ImGui::AlignTextToFramePadding();
	ColumnCenterText( temp( "{}", player->kills ) );
	ImGui::NextColumn();
	ImGui::AlignTextToFramePadding();
	ColumnCenterText( temp( "{}", player->ping ) );
	ImGui::NextColumn();
}

static void DrawTeamScoreboard( TempAllocator & temp, Team team, ImFont * score_font, u8 min_tabs, float col_width ) {
	const SyncTeamState * team_info = &client_gs.gameState.teams[ team ];

	RGB8 color = CG_TeamColor( team );

	u8 slots = Max2( team_info->num_players, min_tabs );
	ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, 255 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 8 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 8 ) );

	int line_height = ImGui::GetFrameHeight();

	// score box
	{
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, 255 ) );
		ImGui::BeginChild( temp( "{}score", team ), ImVec2( min_tabs * line_height, slots * line_height ) );
		ImGui::PushFont( score_font );
		WindowCenterTextXY( temp( "{}", team_info->score ) );
		ImGui::PopFont();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	// players
	{
		ImGui::SameLine();

		// TODO: srgb?
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r / 2, color.g / 2, color.b / 2, 255 ) );
		ImGui::BeginChild( temp( "{}paddedplayers", team ), ImVec2( 0, slots * line_height ) );

		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r * 0.75f, color.g * 0.75f, color.b * 0.75f, 255 ) );
		ImGui::BeginChild( temp( "{}players", team ), ImVec2( 0, team_info->num_players * line_height ) );

		ImGui::Columns( 5, NULL, false );
		ImGui::SetColumnWidth( 0, line_height );
		ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
		ImGui::SetColumnWidth( 2, col_width );
		ImGui::SetColumnWidth( 3, col_width );
		ImGui::SetColumnWidth( 4, col_width );

		for( u8 i = 0; i < team_info->num_players; i++ ) {
			DrawPlayerScoreboard( temp, team_info->player_indices[ i ], line_height );
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
	TracyZoneScoped;

	TempAllocator temp = cls.frame_arena.temp();

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

	const ImGuiIO & io = ImGui::GetIO();
	float width_frac = Lerp( 0.8f, Unlerp01( 1024.0f, io.DisplaySize.x, 1920.0f ), 0.6f );
	Vec2 size = io.DisplaySize * Vec2( width_frac, 0.8f );

	ImGui::SetNextWindowSize( ImVec2( size.x, -1 ) );
	ImGui::SetNextWindowPos( io.DisplaySize * 0.5f, 0, Vec2( 0.5f ) );
	ImGui::Begin( "scoreboard", WindowZOrder_Scoreboard, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground );

	float padding = 4;
	float separator_height = ImGui::GetTextLineHeight() + 2 * padding;
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, padding ) );

	bool warmup = client_gs.gameState.match_state < MatchState_Playing;

	defer {
		ImGui::PopStyleVar();
		ImGui::End();
		ImGui::PopStyleVar( 2 );
	};

	float col_width = 80;

	if( client_gs.gameState.gametype == Gametype_Bomb ) {
		float score_width = 5 * ( ImGui::GetTextLineHeight() + 2 * 8 );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboardheader", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ColumnCenterText( client_gs.gameState.bomb.attacking_team == Team_One ? "ATTACKING" : "DEFENDING" );
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

		DrawTeamScoreboard( temp, Team_One, cls.huge_font, 5, col_width );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboarddivider", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ColumnCenterText( warmup ? "WARMUP" : temp( "ROUND {}", client_gs.gameState.round_num ) );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		DrawTeamScoreboard( temp, Team_Two, cls.huge_font, 5, col_width );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
			ImGui::BeginChild( "scoreboardfooter", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			ColumnCenterText( client_gs.gameState.bomb.attacking_team == Team_Two ? "ATTACKING" : "DEFENDING" );
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
		for( Team i = Team_One; i < Team_Count; i = Team( i + 1 ) ) {
			DrawTeamScoreboard( temp, i, cls.large_font, 2, col_width );
		}
		// const SyncTeamState * team_info = &client_gs.gameState.teams[ TEAM_PLAYERS ];
                //
		// ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 8 ) );
		// ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 16 ) );
                //
		// int line_height = ImGui::GetFrameHeight();
                //
		// {
		// 	ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
		// 	ImGui::BeginChild( "scoreboardheader", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );
                //
		// 	ImGui::Columns( 5, NULL, false );
		// 	ImGui::SetColumnWidth( 0, line_height );
		// 	ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
		// 	ImGui::SetColumnWidth( 2, col_width );
		// 	ImGui::SetColumnWidth( 3, col_width );
		// 	ImGui::SetColumnWidth( 4, col_width );
                //
		// 	ImGui::NextColumn();
		// 	ColumnCenterText( warmup ? "WARMUP" : temp( "ROUND {}", client_gs.gameState.round_num ) );
		// 	ImGui::NextColumn();
		// 	ColumnCenterText( "SCORE" );
		// 	ImGui::NextColumn();
		// 	ColumnCenterText( "KILLS" );
		// 	ImGui::NextColumn();
		// 	ColumnCenterText( "PING" );
		// 	ImGui::NextColumn();
                //
		// 	ImGui::EndChild();
		// 	ImGui::PopStyleColor();
		// }
                //
		// // players
		// {
		// 	ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, alpha ) );
		// 	for( u8 i = 0; i < team_info->num_players; i++ ) {
		// 		SyncScoreboardPlayer * player = &client_gs.gameState.players[ team_info->player_indices[ i ] - 1 ];
                //
		// 		RGB8 team_color = CG_TeamColor( i % 2 == 0 ? Team_One : Team_Two );
                //
		// 		float bg_scale = player->alive ? 0.75f : 0.5f;
		// 		team_color.r *= bg_scale;
		// 		team_color.g *= bg_scale;
		// 		team_color.b *= bg_scale;
                //
		// 		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( team_color.r, team_color.g, team_color.b, alpha ) );
		// 		ImGui::BeginChild( temp( "players{}", i ), ImVec2( 0, line_height ) );
                //
		// 		ImGui::Columns( 5, NULL, false );
		// 		ImGui::SetColumnWidth( 0, line_height );
		// 		ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
		// 		ImGui::SetColumnWidth( 2, col_width );
		// 		ImGui::SetColumnWidth( 3, col_width );
		// 		ImGui::SetColumnWidth( 4, col_width );
                //
		// 		DrawPlayerScoreboard( temp, team_info->player_indices[ i ], line_height );
                //
		// 		ImGui::EndChild();
		// 		ImGui::PopStyleColor();
		// 	}
		// 	ImGui::PopStyleColor();
		// }
                //
		// ImGui::PopStyleVar( 2 );
	}

	ImGui::Dummy( ImVec2( 0, separator_height ) );

	SyncTeamState * team_spec = &client_gs.gameState.teams[ Team_None ];

	if( team_spec->num_players > 0 ) {
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, 255 ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 0 ) );
		ImGui::BeginChild( "spectators", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );


		DynamicString spectators( &temp, "Spectating: " );
		for( u8 i = 0; i < team_spec->num_players; i++ ) {
			if( i > 0 )
				spectators += ", ";
			spectators += PlayerName( team_spec->player_indices[ i ] - 1 );
		}

		ImGui::Text( "%s", spectators.c_str() );

		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}
}

void CG_ScoresOn_f() {
	cg.showScoreboard = true;
}

void CG_ScoresOff_f() {
	cg.showScoreboard = false;
}
