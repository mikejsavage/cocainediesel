#include "qcommon/base.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"
#include "client/client.h"
#include "client/sdl/sdl_window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

ImFont * large_font;
ImFont * medium_font;
ImFont * console_font;

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
		TempAllocator temp = cls.frame_arena->temp();

		const ImDrawList * cmd_list = draw_data->CmdLists[n];
		u16 idx_buffer_offset = 0;

		vec4_t * verts = ALLOC_MANY( &temp, vec4_t, cmd_list->VtxBuffer.Size );
		vec2_t * uvs = ALLOC_MANY( &temp, vec2_t, cmd_list->VtxBuffer.Size );
		byte_vec4_t * colors = ALLOC_MANY( &temp, byte_vec4_t, cmd_list->VtxBuffer.Size );

		for( int i = 0; i < cmd_list->VtxBuffer.Size; i++ ) {
			const ImDrawVert & v = cmd_list->VtxBuffer.Data[ i ];
			verts[ i ][ 0 ] = v.pos.x;
			verts[ i ][ 1 ] = v.pos.y;
			verts[ i ][ 2 ] = 0;
			verts[ i ][ 3 ] = 1;
			uvs[ i ][ 0 ] = v.uv.x;
			uvs[ i ][ 1 ] = v.uv.y;
			memcpy( &colors[ i ], &v.col, sizeof( byte_vec4_t ) );
		}

		Span< u16 > indices = ALLOC_SPAN( &temp, u16, cmd_list->IdxBuffer.Size );
		memcpy( indices.ptr, cmd_list->IdxBuffer.Data, indices.num_bytes() );

		for( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ ) {
			const ImDrawCmd * pcmd = &cmd_list->CmdBuffer[cmd_i];
			if( pcmd->UserCallback ) {
				pcmd->UserCallback( cmd_list, pcmd );
			}
			else {
				ImVec4 clip_rect = ImVec4( pcmd->ClipRect.x - pos.x, pcmd->ClipRect.y - pos.y, pcmd->ClipRect.z - pos.x, pcmd->ClipRect.w - pos.y );
				if( clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f ) {
					re.Scissor( int( clip_rect.x ), int( clip_rect.y ), int( clip_rect.z - clip_rect.x ), int( clip_rect.w - clip_rect.y ) );

					poly_t poly = { };
					poly.numverts = cmd_list->VtxBuffer.Size;
					poly.verts = verts;
					poly.stcoords = uvs;
					poly.colors = colors;
					poly.numelems = pcmd->ElemCount;
					poly.elems = indices.ptr + idx_buffer_offset;
					poly.shader = ( shader_s * ) pcmd->TextureId;
					R_DrawDynamicPoly( &poly );
				}
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}

	re.ResetScissor();
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

void CL_ImGuiExpandColorTokens( DynamicString * result, const char * original, u8 alpha ) {
	assert( alpha > 0 );

	const char * p = original;
	const char * end = p + strlen( p );

	if( alpha != 255 ) {
		*result += ImGuiColorToken( 0, 0, 0, alpha );
	}

	while( p < end ) {
		char token;
		const char * before = FindNextColorToken( p, &token );

		if( before == NULL ) {
			result->append_raw( p, end - p );
			break;
		}

		result->append_raw( p, before - p );

		if( token == '^' ) {
			*result += "^";
		}
		else {
			const vec4_t & c = color_table[ token - '0' ];
			u8 r = u8( c[ 0 ] * 255.0f );
			u8 g = u8( c[ 1 ] * 255.0f );
			u8 b = u8( c[ 2 ] * 255.0f );
			*result += ImGuiColorToken( r, g, b, alpha );
		}

		p = before + 2;
	}
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
