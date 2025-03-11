#include "imgui/imgui.h"

#include "qcommon/base.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/dev_tools.h"
#include "client/renderer/renderer.h"
#include "client/renderer/skybox.h"
#include "cgame/cg_local.h"

static char * selected_model;
static ImGuiTextFilter filter;

DevToolCleanupCallback DrawModelViewer() {
	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( frame_static.viewport_width * 0.2f, frame_static.viewport_height ) );
	ImGui::Begin( "modelviewer", WindowZOrder_Menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_Interactive );
	ImGui::BeginChild( "modelviewer2" ); // NOTE(mike): need a child window or the combo doesn't work, something to do with Z order sorting

	if( ImGui::IsWindowAppearing() ) {
		selected_model = CopyString( sys_allocator, "players/rigg/model" );
		filter = ImGuiTextFilter();
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

	if( ImGui::Button( "Exit" ) ) {
		UI_ShowMainMenu();
	}

	RendererSetView( Vec3( 50, -50, 40 ), EulerDegrees3( 20, 135, 0 ), 90 );

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

	ImGui::EndChild();
	ImGui::End();

	return []() {
		Free( sys_allocator, selected_model );
		selected_model = NULL;
	};
}
