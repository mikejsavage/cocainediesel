#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_internal.h"

#include "qcommon/base.h"
#include "qcommon/string.h"
#include "qcommon/time.h"
#include "qcommon/utf8.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"

#include "nanosort/nanosort.hpp"

static Texture atlas_texture;
static Material atlas_material;

static ImFont * AddFontAsset( StringHash path, float pixel_size, bool idi_nahui = false ) {
	Span< const u8 > data = AssetBinary( path );
	ImFontConfig config;
	config.FontData = ( void * ) data.ptr;
	config.FontDataOwnedByAtlas = false;
	config.FontDataSize = data.n;
	config.SizePixels = pixel_size;
	if( idi_nahui ) {
		config.GlyphRanges = ImGui::GetIO().Fonts->GetGlyphRangesCyrillic();
	}
	return ImGui::GetIO().Fonts->AddFont( &config );
}

struct GLFWwindow;
extern GLFWwindow * window;

void CL_InitImGui() {
	TracyZoneScoped;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL( window, false );

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
		AddFontAsset( "fonts/Decalotype-Bold.ttf", 18.0f );
		cls.huge_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 128.0f );
		cls.large_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 64.0f );
		cls.big_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 48.0f );
		cls.medium_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 28.0f );
		cls.medium_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 28.0f );
		cls.big_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 48.0f );
		cls.large_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 64.0f );
		cls.console_font = AddFontAsset( "fonts/Decalotype-Bold.ttf", 14.0f );
		cls.idi_nahui_font = AddFontAsset( "fonts/OpenSans-SemiBold.ttf", 24.0f, true );

		io.Fonts->Build();

		u8 * pixels;
		int width, height;
		io.Fonts->GetTexDataAsAlpha8( &pixels, &width, &height );

		TextureConfig config;
		config.format = TextureFormat_A_U8;
		config.width = width;
		config.height = height;
		config.data = pixels;

		atlas_texture = NewTexture( config );
		atlas_material.texture = &atlas_texture;
		io.Fonts->TexID = ImGuiShaderAndMaterial( &atlas_material );
	}

	{
		ImGuiStyle & style = ImGui::GetStyle();
		style.WindowRounding = 0;
		style.FrameRounding = 0;
		style.GrabRounding = 0;
		style.FramePadding = ImVec2( 16, 16 );
		style.FrameBorderSize = 0;
		style.WindowPadding = ImVec2( 32, 32 );
		style.WindowBorderSize = 0;
		style.PopupBorderSize = 0;

		style.Colors[ ImGuiCol_Button ] = ImVec4( 0.125f, 0.125f, 0.125f, 1.f );
		style.Colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );

		style.Colors[ ImGuiCol_Tab ] = ImVec4( 0.125f, 0.125f, 0.125f, 1.f );
		style.Colors[ ImGuiCol_TabHovered ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_TabActive ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );
		style.Colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );

		style.Colors[ ImGuiCol_FrameBg ] = ImVec4( 0.125f, 0.125f, 0.125f, 1.f );
		style.Colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );

		style.Colors[ ImGuiCol_SliderGrab ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );
		style.Colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.75f, 0.75f, 0.75f, 1.f );

		style.Colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.125f, 0.125f, 0.125f, 0.5f );
		style.Colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );
		style.Colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );
		style.Colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.75f, 0.75f, 0.75f, 1.f );

		style.Colors[ ImGuiCol_CheckMark ] = ImVec4( 0.25f, 1.f, 0.f, 1.f );

		style.Colors[ ImGuiCol_Header ] = ImVec4( 0.125f, 0.125f, 0.125f, 1.f );
		style.Colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );
		style.Colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.75f, 0.75f, 0.75f, 1.f );

		style.Colors[ ImGuiCol_WindowBg ] = ImColor( 0x1a, 0x1a, 0x1a );
		style.ItemSpacing.y = 16;
	}
}

void CL_ShutdownImGui() {
	DeleteTexture( atlas_texture );

	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

static void SubmitDrawCalls() {
	TracyZoneScoped;

	ImDrawData * draw_data = ImGui::GetDrawData();

	const ImGuiIO & io = ImGui::GetIO();
	int fb_width = int( draw_data->DisplaySize.x * io.DisplayFramebufferScale.x );
	int fb_height = int( draw_data->DisplaySize.y * io.DisplayFramebufferScale.y );
	if( fb_width <= 0 || fb_height <= 0 )
		return;
	draw_data->ScaleClipRects( io.DisplayFramebufferScale );

	u32 pass = 0;

	ImVec2 pos = draw_data->DisplayPos;
	for( int n = 0; n < draw_data->CmdListsCount; n++ ) {
		TempAllocator temp = cls.frame_arena.temp();

		const ImDrawList * cmd_list = draw_data->CmdLists[ n ];

		// TODO: this is a hack to separate drawcalls into 2 passes
		if( cmd_list->CmdBuffer.Size > 0 ) {
			const ImDrawCmd * cmd = &cmd_list->CmdBuffer[ 0 ];
			u32 new_pass = u32( uintptr_t( cmd->UserCallbackData ) );
			if( new_pass != 0 ) {
				pass = new_pass;
			}
		}

		if( cmd_list->VtxBuffer.Size == 0 || cmd_list->IdxBuffer.Size == 0 ) {
			continue;
		}

		MeshConfig config = { };
		config.name = temp( "ImGui - {}", n );
		config.vertex_buffers[ 0 ] = NewGPUBuffer( cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ), temp( "ImGui vertices - {}", n ) );
		config.vertex_descriptor.buffer_strides[ 0 ] = sizeof( ImDrawVert );
		config.vertex_descriptor.attributes[ VertexAttribute_Position ] = { VertexFormat_Floatx2, 0, offsetof( ImDrawVert, pos ) };
		config.set_attribute( VertexAttribute_TexCoord, 0, offsetof( ImDrawVert, uv ) );
		config.set_attribute( VertexAttribute_Color, 0, offsetof( ImDrawVert, col ) );
		config.index_buffer = NewGPUBuffer( cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( u16 ), temp( "ImGui indices - {}", n ) );
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
					pipeline.pass = pass == 0 ? frame_static.ui_pass : frame_static.post_ui_pass;
					pipeline.shader = pcmd->TextureId.shader;
					pipeline.depth_func = DepthFunc_Disabled;
					pipeline.blend_func = BlendFunc_Blend;
					pipeline.cull_face = CullFace_Disabled;
					pipeline.write_depth = false;
					pipeline.scissor = {
						u32( scissor.mins.x ),
						u32( scissor.mins.y ),
						u32( scissor.maxs.x - scissor.mins.x ),
						u32( scissor.maxs.y - scissor.mins.y ),
					};

					pipeline.bind_uniform( "u_View", frame_static.ortho_view_uniforms );
					pipeline.bind_uniform( "u_Model", frame_static.identity_model_uniforms );
					pipeline.bind_uniform( "u_MaterialStatic", frame_static.identity_material_static_uniforms );
					pipeline.bind_uniform( "u_MaterialDynamic", frame_static.identity_material_dynamic_uniforms );

					if( pcmd->TextureId.uniform_name != EMPTY_HASH ) {
						pipeline.bind_uniform( pcmd->TextureId.uniform_name, pcmd->TextureId.uniform_block );
					}

					pipeline.bind_texture_and_sampler( "u_BaseTexture", pcmd->TextureId.material->texture, Sampler_LodBiasMinusOne );

					DrawMesh( mesh, pipeline, pcmd->ElemCount, pcmd->IdxOffset );
				}
			}
		}
	}
}

void CL_ImGuiBeginFrame() {
	TracyZoneScoped;

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void CL_ImGuiEndFrame() {
	TracyZoneScoped;

	// ImGui::ShowDemoWindow();

	ImGuiContext * ctx = ImGui::GetCurrentContext();
	nanosort( ctx->Windows.begin(), ctx->Windows.end(),
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
		ImGui::GetWindowDrawList()->AddCallback( NULL, ( void * ) 1 ); // TODO: this is a hack to separate drawcalls into 2 passes
	}

	bool Hotkey( int key ) {
		return ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) && ImGui::IsKeyPressed( key, false );
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

void CenterTextY( const char * str, float height ) {
	float text_height = ImGui::CalcTextSize( str ).y;
	ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 0.5f * ( height - text_height ) );
	ImGui::Text( "%s", str );
}

void CellCenter( float item_width ) {
	float cell_width = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX( ImGui::GetCursorPosX() + 0.5f * ( cell_width - item_width ) );
}

void CellCenterText( const char * str ) {
	CellCenter( ImGui::CalcTextSize( str ).x );
	ImGui::Text( "%s", str );
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

Vec4 CustomAttentionGettingColor( Vec4 from, Vec4 to, Time period ) {
	float t = Sin( cls.monotonicTime, period ) * 0.5f + 1.0f;
	return Lerp( from, t, to );
}

Vec4 AttentionGettingColor() {
	return CustomAttentionGettingColor( vec4_red, sRGBToLinear( rgba8_diesel_yellow ), Milliseconds( 125 ) );
}

Vec4 PlantableColor() {
	return CustomAttentionGettingColor( vec4_dark, sRGBToLinear( rgba8_diesel_green ), Milliseconds( 125 ) );
}
