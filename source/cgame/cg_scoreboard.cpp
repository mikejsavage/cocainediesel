#include "cgame/cg_local.h"
#include "qcommon/string.h"
#include "client/renderer/renderer.h"

#include "imgui/imgui.h"

bool CG_ScoreboardShown() {
	if( GS_MatchState( &client_gs ) > MATCH_STATE_PLAYTIME ) {
		return true;
	}

	if( cgs.demoPlaying || cg.frame.multipov ) {
		return cg.showScoreboard;
	}

	return cg.showScoreboard;
}

static void DrawPlayerScoreboard( TempAllocator & temp, int playerIndice, float line_height ) {
	PlayerState * player = &client_gs.gameState.players[ playerIndice - 1 ];
	// icon
	bool warmup = GS_MatchState( &client_gs ) == MATCH_STATE_WARMUP || GS_MatchState( &client_gs ) == MATCH_STATE_COUNTDOWN;
	const Material * icon = NULL;

	if( warmup ) {
		icon = player->state ? cgs.media.shaderReady : NULL;
	}
	else {
		bool carrier = player->state && ( ISREALSPECTATOR() || cg_entities[ playerIndice ].current.team == cg.predictedPlayerState.team );
		if( player->alive ) {
			icon = carrier ? cgs.media.shaderBombIcon : cgs.media.shaderAlive;
		}
		else {
			icon = cgs.media.shaderDead;
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
	DynamicString final_name( &temp, "{}{}", ImGuiColorToken( 0, 0, 0, alpha ), cgs.clientInfo[ playerIndice - 1 ].name );
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

static void DrawTeamScoreboard( TempAllocator & temp, int team, float col_width, u8 alpha ) {
	TeamState * team_info = &client_gs.gameState.teams[ team ];

	RGB8 color = CG_TeamColor( team );

	int slots = Max2( team_info->numplayers, (u8)5 );
	ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 0, 0, 0, alpha ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 8 ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 8 ) );

	int line_height = ImGui::GetFrameHeight();

	// score box
	{
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( color.r, color.g, color.b, alpha ) );
		ImGui::BeginChild( temp( "{}score", team ), ImVec2( 5 * line_height, slots * line_height ) );
		ImGui::PushFont( cls.huge_font );
		WindowCenterTextXY( temp( "{}", team_info->score ) );
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
		ImGui::BeginChild( temp( "{}players", team ), ImVec2( 0, team_info->numplayers * line_height ) );

		ImGui::Columns( 5, NULL, false );
		ImGui::SetColumnWidth( 0, line_height );
		ImGui::SetColumnWidth( 1, ImGui::GetWindowWidth() - col_width * 3 - 32 );
		ImGui::SetColumnWidth( 2, col_width );
		ImGui::SetColumnWidth( 3, col_width );
		ImGui::SetColumnWidth( 4, col_width );

		for( int i = 0; i < team_info->numplayers; i++ ) {
			DrawPlayerScoreboard( temp, team_info->playerIndices[ i ], line_height );
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

	const ImGuiIO & io = ImGui::GetIO();
	float width_frac = Lerp( 0.8f, Unlerp01( 1024.0f, io.DisplaySize.x, 1920.0f ), 0.6f );
	Vec2 size = io.DisplaySize * Vec2( width_frac, 0.8f );

	ImGuiWindowFlags basic_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;

	ImGui::SetNextWindowSize( ImVec2( size.x, -1 ) );
	ImGui::SetNextWindowPos( io.DisplaySize * 0.5f, 0, Vec2( 0.5f ) );
	ImGui::Begin( "scoreboard", WindowZOrder_Scoreboard, basic_flags | ImGuiWindowFlags_NoBackground );

	float padding = 4;
	float separator_height = ImGui::GetTextLineHeight() + 2 * padding;
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, padding ) );

	bool warmup = GS_MatchState( &client_gs ) == MATCH_STATE_WARMUP;

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

		DrawTeamScoreboard( temp, TEAM_ALPHA, col_width, alpha );

		{
			ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
			ImGui::BeginChild( "scoreboarddivider", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );

			ImGui::Columns( 5, NULL, false );
			ImGui::SetColumnWidth( 0, score_width );
			ImGui::SetColumnWidth( 1, size.x - score_width - col_width * 3 );
			ImGui::SetColumnWidth( 2, col_width );
			ImGui::SetColumnWidth( 3, col_width );
			ImGui::SetColumnWidth( 4, col_width );

			u8 round = client_gs.gameState.teams[ TEAM_ALPHA ].score + client_gs.gameState.teams[ TEAM_BETA ].score; //hackish but I'm bored
			ColumnCenterText( warmup ? "WARMUP" : temp( "ROUND {}", round ) );
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

		DrawTeamScoreboard( temp, TEAM_BETA, col_width, alpha );

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
		TeamState * team_info = &client_gs.gameState.teams[ TEAM_PLAYERS ];

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
			ColumnCenterText( warmup ? "WARMUP" : temp( "ROUND {}", client_gs.gameState.teams[ TEAM_PLAYERS ].score ) );
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
			for( int i = 0; i < team_info->numplayers; i++ ) {
				PlayerState * player = &client_gs.gameState.players[ team_info->playerIndices[ i ] - 1 ];

				RGB8 team_color = TEAM_COLORS[ i % ARRAY_COUNT( TEAM_COLORS ) ];

				float bg_scale = player->alive ? 0.75f : 0.5f;
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

				DrawPlayerScoreboard( temp, team_info->playerIndices[ i ], line_height );

				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
			ImGui::PopStyleColor();
		}

		ImGui::PopStyleVar( 2 );
	}

	ImGui::Dummy( ImVec2( 0, separator_height ) );

	TeamState * team_spec = &client_gs.gameState.teams[ TEAM_SPECTATOR ];
	
	if( team_spec->numplayers > 0 ) {
		ImGui::PushStyleColor( ImGuiCol_ChildBg, IM_COL32( 0, 0, 0, alpha ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 0 ) );
		ImGui::BeginChild( "spectators", ImVec2( 0, separator_height ), false, ImGuiWindowFlags_AlwaysUseWindowPadding );


		DynamicString spectators( &temp, "Spectating: " );
		for( int i = 0; i < team_spec->numplayers; i++ ) {
			if( i > 0 )
				spectators += ", ";
			spectators += cgs.clientInfo[ team_spec->playerIndices[ i ] - 1 ].name;
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
