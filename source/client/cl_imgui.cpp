#include <algorithm>

#include "qcommon/base.h"
#include "qcommon/assets.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"
#include "client/client.h"
#include "client/renderer/renderer.h"
#include "client/sdl/sdl_window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_freetype.h"

static Texture atlas_texture;
static Material atlas_material;

static ImFont * AddFontAsset( StringHash path, float pixel_size ) {
	Span< const u8 > data = AssetBinary( path );
	ImFontConfig config;
	config.FontData = ( void * ) data.ptr;
	config.FontDataOwnedByAtlas = false;
	config.FontDataSize = data.n;
	config.SizePixels = pixel_size;
	return ImGui::GetIO().Fonts->AddFont( &config );
}

void CL_InitImGui() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL( sdl_window, NULL );

	ImGuiIO & io = ImGui::GetIO();

	{
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
		io.KeyMap[ ImGuiKey_KeyPadEnter ] = KP_ENTER;
		io.KeyMap[ ImGuiKey_A ] = 'a';
		io.KeyMap[ ImGuiKey_C ] = 'c';
		io.KeyMap[ ImGuiKey_V ] = 'v';
		io.KeyMap[ ImGuiKey_X ] = 'x';
		io.KeyMap[ ImGuiKey_Y ] = 'y';
		io.KeyMap[ ImGuiKey_Z ] = 'z';
	}

	{
		AddFontAsset( "fonts/Montserrat-SemiBold.ttf", 18.0f );
		cls.huge_font = AddFontAsset( "fonts/Montserrat-Bold.ttf", 128.0f );
		cls.large_font = AddFontAsset( "fonts/Montserrat-Bold.ttf", 64.0f );
		cls.big_font = AddFontAsset( "fonts/Montserrat-Bold.ttf", 48.0f );
		cls.medium_font = AddFontAsset( "fonts/Montserrat-Bold.ttf", 28.0f );
		cls.console_font = AddFontAsset( "fonts/Montserrat-SemiBold.ttf", 14.0f );

		ImGuiFreeType::BuildFontAtlas( io.Fonts );

		u8 * pixels;
		int width, height;
		io.Fonts->GetTexDataAsAlpha8( &pixels, &width, &height );

		TextureConfig config;
		config.width = width;
		config.height = height;
		config.data = pixels;
		config.format = TextureFormat_A_U8;

		atlas_texture = NewTexture( config );
		atlas_material.texture = &atlas_texture;
		io.Fonts->TexID = ImGuiShaderAndMaterial( &atlas_material );
	}

	{
		ImGuiStyle & style = ImGui::GetStyle();
		style.WindowRounding = 0;
		style.FrameRounding = 1;
		style.GrabRounding = 2;
		style.FramePadding = ImVec2( 8, 8 );
		style.FrameBorderSize = 0;
		style.WindowPadding = ImVec2( 16, 16 );
		style.WindowBorderSize = 0;
		style.PopupBorderSize = 0;
		style.Colors[ ImGuiCol_WindowBg ] = ImColor( 0x1a, 0x1a, 0x1a );
		style.ItemSpacing.y = 8;
	}

}

void CL_ShutdownImGui() {
	DeleteTexture( atlas_texture );

	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

static void SubmitDrawCalls() {
	ZoneScoped;

	ImDrawData * draw_data = ImGui::GetDrawData();

	const ImGuiIO & io = ImGui::GetIO();
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
					pipeline.shader = pcmd->TextureId.shader;
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
					pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );

					if( pcmd->TextureId.uniform_name != EMPTY_HASH ) {
						pipeline.set_uniform( pcmd->TextureId.uniform_name, pcmd->TextureId.uniform_block );
					}

					pipeline.set_texture( "u_BaseTexture", pcmd->TextureId.material->texture );

					DrawMesh( mesh, pipeline, pcmd->ElemCount, idx_buffer_offset * sizeof( u16 ) );
				}
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}
}

void CL_ImGuiBeginFrame() {
	ZoneScoped;

	ImGui_ImplSDL2_NewFrame( sdl_window );
	ImGui::NewFrame();
}

void CL_ImGuiEndFrame() {
	ZoneScoped;

	// ImGui::ShowDemoWindow();

	ImGuiContext * ctx = ImGui::GetCurrentContext();
	std::stable_sort( ctx->Windows.begin(), ctx->Windows.end(),
		[]( const ImGuiWindow * a, const ImGuiWindow * b ) {
			return a->BeginOrderWithinContext < b->BeginOrderWithinContext;
		}
	);

	ImGui::Render();
	SubmitDrawCalls();
}

namespace ImGui {
	void Begin( const char * name, WindowZOrder z_order, ImGuiWindowFlags flags ) {
		ImGui::Begin( name, NULL, flags );
		ImGui::GetCurrentWindow()->BeginOrderWithinContext = z_order;
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

ImGuiColorToken::ImGuiColorToken( RGB8 rgb ) : ImGuiColorToken( rgb.r, rgb.g, rgb.b, 255 ) { }
ImGuiColorToken::ImGuiColorToken( RGBA8 rgba ) : ImGuiColorToken( rgba.r, rgba.g, rgba.b, rgba.a ) { }

void format( FormatBuffer * fb, const ImGuiColorToken & token, const FormatOpts & opts ) {
	format( fb, ( const char * ) token.token );
}

void ColumnCenterText( const char * str ) {
	float width = ImGui::CalcTextSize( str ).x;
	ImGui::SetCursorPosX( ImGui::GetColumnOffset() + 0.5f * ( ImGui::GetColumnWidth() - width ) );
	ImGui::Text( "%s", str );
}

void ColumnRightText( const char * str ) {
	float width = ImGui::CalcTextSize( str ).x;
	ImGui::SetCursorPosX( ImGui::GetColumnOffset() + ImGui::GetColumnWidth() - width );
	ImGui::Text( "%s", str );
}

void WindowCenterTextXY( const char * str ) {
	Vec2 text_size = ImGui::CalcTextSize( str );
	ImGui::SetCursorPos( 0.5f * ( ImGui::GetWindowSize() - text_size ) );
	ImGui::Text( "%s", str );
}

Vec4 AttentionGettingColor() {
	float t = sinf( cls.monotonicTime / 20.0f ) * 0.5f + 1.0f;
	return Lerp( vec4_red, t, vec4_yellow );
}
