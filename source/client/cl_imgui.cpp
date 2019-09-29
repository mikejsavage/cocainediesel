#include "qcommon/base.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/sdl/sdl_window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

void CL_InitImGui() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL( sdl_window, NULL );

	ImGuiIO & io = ImGui::GetIO();
	io.IniFilename = NULL;
	io.KeyMap[ ImGuiKey_Tab ] = K_TAB;
	io.KeyMap[ ImGuiKey_LeftArrow ] = K_LEFTARROW;
	io.KeyMap[ ImGuiKey_RightArrow ] = K_RIGHTARROW;
	io.KeyMap[ ImGuiKey_UpArrow ] = K_UPARROW;
	io.KeyMap[ ImGuiKey_DownArrow ] = K_DOWNARROW;
	io.KeyMap[ ImGuiKey_PageUp ] = K_PGUP;
	io.KeyMap[ ImGuiKey_PageDown ] = K_PGDN;
	io.KeyMap[ ImGuiKey_Home ] = K_HOME;
	io.KeyMap[ ImGuiKey_End ] = K_END;
	io.KeyMap[ ImGuiKey_Insert ] = K_INS;
	io.KeyMap[ ImGuiKey_Delete ] = K_DEL;
	io.KeyMap[ ImGuiKey_Backspace ] = K_BACKSPACE;
	io.KeyMap[ ImGuiKey_Space ] = K_SPACE;
	io.KeyMap[ ImGuiKey_Enter ] = K_ENTER;
	io.KeyMap[ ImGuiKey_Escape ] = K_ESCAPE;
	io.KeyMap[ ImGuiKey_A ] = 'a';
	io.KeyMap[ ImGuiKey_C ] = 'c';
	io.KeyMap[ ImGuiKey_V ] = 'v';
	io.KeyMap[ ImGuiKey_X ] = 'x';
	io.KeyMap[ ImGuiKey_Y ] = 'y';
	io.KeyMap[ ImGuiKey_Z ] = 'z';
}

void CL_ShutdownImGui() {
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

static void SubmitDrawCalls() {
	ImDrawData * draw_data = ImGui::GetDrawData();

	ImGuiIO& io = ImGui::GetIO();
	int fb_width = int( draw_data->DisplaySize.x * io.DisplayFramebufferScale.x );
	int fb_height = int( draw_data->DisplaySize.y * io.DisplayFramebufferScale.y );
	if( fb_width <= 0 || fb_height <= 0 )
		return;
	draw_data->ScaleClipRects( io.DisplayFramebufferScale );

	ImVec2 pos = draw_data->DisplayPos;
	for( int n = 0; n < draw_data->CmdListsCount; n++ ) {
		const ImDrawList * cmd_list = draw_data->CmdLists[ n ];
		u16 idx_buffer_offset = 0;

		MeshConfig config;
		config.unified_buffer = NewVertexBuffer( cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
		config.positions_offset = offsetof( ImDrawVert, pos );
		config.tex_coords_offset = offsetof( ImDrawVert, uv );
		config.colors_offset = offsetof( ImDrawVert, col );
		config.positions_format = VertexFormat_Floatx2;
		config.stride = sizeof( ImDrawVert );
		config.indices = NewIndexBuffer( cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( u16 ) );
		Mesh mesh = NewMesh( config );
		DeferDeleteMesh( mesh );

		for( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ ) {
			const ImDrawCmd * pcmd = &cmd_list->CmdBuffer[ cmd_i ];
			if( pcmd->UserCallback ) {
				pcmd->UserCallback( cmd_list, pcmd );
			}
			else {
				MinMax2 scissor = MinMax2( Vec2( pcmd->ClipRect.x - pos.x, pcmd->ClipRect.y - pos.y ), Vec2( pcmd->ClipRect.z - pos.x, pcmd->ClipRect.w - pos.y ) );
				if( scissor.mins.x < fb_width && scissor.mins.y < fb_height && scissor.maxs.x >= 0.0f && scissor.maxs.y >= 0.0f ) {
					PipelineState pipeline;
					pipeline.pass = frame_static.ui_pass;
					pipeline.shader = &shaders.standard_vertexcolors;
					pipeline.depth_func = DepthFunc_Disabled;
					pipeline.blend_func = BlendFunc_Blend;
					pipeline.cull_face = CullFace_Disabled;
					pipeline.write_depth = false;
					pipeline.scissor.x = scissor.mins.x;
					pipeline.scissor.y = scissor.mins.y;
					pipeline.scissor.w = scissor.maxs.x - scissor.mins.x;
					pipeline.scissor.h = scissor.maxs.y - scissor.mins.y;

					pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
					pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

					Texture texture = { };
					texture.texture = u32( uintptr_t( pcmd->TextureId ) );
					pipeline.set_texture( "u_BaseTexture", texture );

					DrawMesh( mesh, pipeline, pcmd->ElemCount, idx_buffer_offset * sizeof( u16 ) );
				}
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}
}

void CL_ImGuiBeginFrame() {
	ImGui_ImplSDL2_NewFrame( sdl_window );
	ImGui::NewFrame();

	// ImGui::ShowDemoWindow();
}

void CL_ImGuiEndFrame() {
	ImGui::Render();
	SubmitDrawCalls();
}

ImGuiColorToken::ImGuiColorToken( u8 r, u8 g, u8 b, u8 a ) {
	token[ 0 ] = 033;
	token[ 1 ] = Max2( r, u8( 1 ) );
	token[ 2 ] = Max2( g, u8( 1 ) );
	token[ 3 ] = Max2( b, u8( 1 ) );
	token[ 4 ] = Max2( a, u8( 1 ) );
	token[ 5 ] = 0;
}

void format( FormatBuffer * fb, const ImGuiColorToken & token, const FormatOpts & opts ) {
	format( fb, ( const char * ) token.token );
}
