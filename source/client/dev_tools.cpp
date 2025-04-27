#include "imgui/imgui.h"

#include "qcommon/base.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/dev_tools.h"
#include "client/renderer/renderer.h"
#include "client/renderer/skybox.h"
#include "cgame/cg_local.h"

DevToolCleanupCallback DrawModelViewer() {
	static char * selected_model;
	static ImGuiTextFilter filter;
	static float distance;
	static EulerDegrees2 angles;

	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width * 0.2f, frame_static.viewport_height ) );
	ImGui::Begin( "modelviewer", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );

	if( ImGui::IsWindowAppearing() ) {
		selected_model = CopyString( sys_allocator, "players/rigg/model" );
		filter = ImGuiTextFilter();
		distance = 50.0f;
		angles = EulerDegrees2( 20.0f, 135.0f );
	}

	ImGui::AlignTextToFramePadding();
	ImGui::Text( "Model" );
	ImGui::SameLine();

	if( ImGui::BeginCombo( "##model", selected_model ) ) {
		filter.Draw( "##filter" );
		if( ImGui::IsWindowAppearing() ) {
			ImGui::SetKeyboardFocusHere( -1 );
		}

		for( Span< const char > path : AssetPaths() ) {
			if( EndsWith( path, ".glb" ) && filter.PassFilter( path.begin(), path.end() ) ) {
				TempAllocator temp = cls.frame_arena.temp();
				Span< const char > model = StripExtension( path );
				bool selected = StrEqual( model, selected_model );
				if( ImGui::Selectable( temp( "{}", model ), selected ) ) {
					Free( sys_allocator, selected_model );
					selected_model = ( *sys_allocator )( "{}", model );
				}
			}
		}

		ImGui::EndCombo();
	}

	ImGui::AlignTextToFramePadding();
	ImGui::Text( "Distance" );
	ImGui::SameLine();
	ImGui::SliderFloat( "##distance", &distance, 0.0f, 500.0f, "%.0f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat );

	ImGui::AlignTextToFramePadding();
	ImGui::Text( "Pitch" );
	ImGui::SameLine();
	ImGui::SliderFloat( "##pitch", &angles.pitch, -180.0f, 180.0f, "%.0f", ImGuiSliderFlags_NoRoundToFormat );

	ImGui::AlignTextToFramePadding();
	ImGui::Text( "Yaw" );
	ImGui::SameLine();
	ImGui::SliderFloat( "##yaw", &angles.yaw, 0.0f, 360.0f, "%.0f", ImGuiSliderFlags_NoRoundToFormat );

	if( ImGui::Button( "Exit" ) ) {
		UI_ShowMainMenu();
	}

	Vec3 camera_forward;
	AngleVectors( EulerDegrees3( angles ), &camera_forward, NULL, NULL );
	RendererSetView( distance * -camera_forward, EulerDegrees3( angles ), 90.0f );

	Optional< ModelRenderData > maybe_model = FindModelRenderData( StringHash( selected_model ) );
	if( maybe_model.exists ) {
		DrawModelConfig config = {
			.draw_model = { .enabled = true },
			.draw_shadows = { .enabled = true },
		};
		DrawModel( config, maybe_model.value, Mat3x4::Identity(), CG_TeamColorVec4( Team_One ) );
	}

	DrawSkybox( cls.shadertoy_time );
	DrawModelInstances();

	ImGui::End();

	return []() {
		Free( sys_allocator, selected_model );
		selected_model = NULL;
	};
}
