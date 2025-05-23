#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_internal.h"

#include "qcommon/base.h"
#include "qcommon/string.h"
#include "qcommon/time.h"
#include "qcommon/utf8.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"

#include <algorithm>

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

struct SDL_Window;
extern SDL_Window * window;

void CL_InitImGui() {
	TracyZoneScoped;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplSDL3_InitForOther( window );

	ImGuiIO & io = ImGui::GetIO();
	io.IniFilename = NULL;
	io.ConfigInputTrickleEventQueue = false; // so we can open the game menu with escape
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	float scale = GetContentScale();

	{
		AddFontAsset( "fonts/Decalotype-Bold.ttf", 18.0f * scale );
		cls.huge_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 128.0f * scale );
		cls.huge_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 128.0f * scale );
		cls.large_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 64.0f * scale );
		cls.big_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 48.0f * scale );
		cls.medium_font = AddFontAsset( "fonts/Decalotype-Black.ttf", 28.0f * scale );
		cls.medium_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 28.0f * scale );
		cls.big_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 48.0f * scale );
		cls.large_italic_font = AddFontAsset( "fonts/Decalotype-BlackItalic.ttf", 64.0f * scale );
		cls.console_font = AddFontAsset( "fonts/Decalotype-Bold.ttf", 14.0f * scale );
		cls.license_italic_font = AddFontAsset( "fonts/sofachrome-rg-it.otf", 128.0f * scale );

		io.Fonts->Build();

		u8 * pixels;
		int width, height;
		io.Fonts->GetTexDataAsAlpha8( &pixels, &width, &height );

		atlas_texture = NewTexture( TextureConfig {
			.format = TextureFormat_A_U8,
			.width = checked_cast< u32 >( width ),
			.height = checked_cast< u32 >( height ),
			.data = pixels,
		} );
		atlas_material.texture = &atlas_texture;
		io.Fonts->TexID = ImGuiShaderAndMaterial( &atlas_material );
	}

	{
		ImGuiStyle & style = ImGui::GetStyle();
		style.WindowRounding = 0;
		style.FrameRounding = 0;
		style.TabRounding = 0;
		style.GrabRounding = 0;
		style.FramePadding = ImVec2( 16, 16 );
		style.FrameBorderSize = 0;
		style.WindowPadding = ImVec2( 32, 32 );
		style.WindowBorderSize = 0;
		style.PopupBorderSize = 0;
		style.DisabledAlpha = 0.125f;

		style.Colors[ ImGuiCol_Button ] = ImVec4( 0.125f, 0.125f, 0.125f, 1.f );
		style.Colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );

		style.Colors[ ImGuiCol_Tab ] = ImVec4( 0.125f, 0.125f, 0.125f, 1.f );
		style.Colors[ ImGuiCol_TabHovered ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_TabSelected ] = ImVec4( 0.5f, 0.5f, 0.5f, 1.f );
		style.Colors[ ImGuiCol_TabDimmed ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );
		style.Colors[ ImGuiCol_TabDimmedSelected ] = ImVec4( 0.25f, 0.25f, 0.25f, 1.f );

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

		style.ScaleAllSizes( scale );
	}
}

void CL_ShutdownImGui() {
	DeleteTexture( atlas_texture );

	ImGui_ImplSDL3_Shutdown();
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

	UniformBlock lodbias_uniforms = UploadMaterialStaticUniforms( 0.0f, 0.0f, -1.0f );

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

		VertexDescriptor vertex_descriptor = { };
		vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( ImDrawVert, pos ) };
		vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( ImDrawVert, uv ) };
		vertex_descriptor.attributes[ VertexAttribute_Color ] = VertexAttribute { VertexFormat_U8x4_01, 0, offsetof( ImDrawVert, col ) };
		vertex_descriptor.buffer_strides[ 0 ] = sizeof( ImDrawVert );

		DynamicDrawData dynamic_geometry = UploadDynamicGeometry(
			Span< const ImDrawVert >( cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size ).cast< const u8 >(),
			Span< const ImDrawIdx >( cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size ),
			vertex_descriptor
		);

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
						u32( Max2( scissor.mins.x, 0.f ) ),
						u32( Max2( scissor.mins.y, 0.f ) ),
						u32( scissor.maxs.x - scissor.mins.x ),
						u32( scissor.maxs.y - scissor.mins.y ),
					};

					pipeline.bind_uniform( "u_View", frame_static.ortho_view_uniforms );
					pipeline.bind_uniform( "u_Model", frame_static.identity_model_uniforms );
					pipeline.bind_uniform( "u_MaterialStatic", lodbias_uniforms );
					pipeline.bind_uniform( "u_MaterialDynamic", frame_static.identity_material_dynamic_uniforms );

					if( pcmd->TextureId.uniform_name != EMPTY_HASH ) {
						pipeline.bind_uniform( pcmd->TextureId.uniform_name, pcmd->TextureId.uniform_block );
					}

					pipeline.bind_texture_and_sampler( "u_BaseTexture", pcmd->TextureId.material->texture, Sampler_Standard );

					DrawDynamicGeometry( pipeline, dynamic_geometry, pcmd->ElemCount, pcmd->IdxOffset );
				}
			}
		}
	}
}

void CL_ImGuiBeginFrame() {
	TracyZoneScoped;

	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void CL_ImGuiEndFrame() {
	TracyZoneScoped;

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
		ImGui::GetWindowDrawList()->AddCallback( NULL, ( void * ) 1 ); // TODO: this is a hack to separate drawcalls into 2 passes
	}

	bool Hotkey( ImGuiKey key ) {
		return ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) && ImGui::IsKeyPressed( key, false );
	}

	ImVec2 CalcTextSize( Span< const char > str ) {
		return ImGui::CalcTextSize( str.begin(), str.end() );
	}

	void Text( Span< const char > str ) {
		ImGui::TextUnformatted( str.begin(), str.end() );
	}

	void PushID( Span< const char > id ) {
		ImGui::PushID( id.begin(), id.end() );
	}

	bool IsItemHoveredThisFrame() {
		// this function has two nuances:
		// - we need a nonzero delay or ImGui doesn't start the timer at all
		// - the timer can either be equal to DeltaTime if you start hovering, or 0 if you switch from hovering some other item
		float & delay = ImGui::GetStyle().HoverDelayShort;
		float old_delay = delay;
		delay = 0.00001f;
		defer { delay = old_delay; };
		return ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay ) && ImGui::GetCurrentContext()->HoverItemDelayTimer <= ImGui::GetIO().DeltaTime;
	}
}

ImGuiColorToken::ImGuiColorToken( u8 r, u8 g, u8 b, u8 a ) {
	token[ 0 ] = 033;
	token[ 1 ] = Max2( r, u8( 1 ) );
	token[ 2 ] = Max2( g, u8( 1 ) );
	token[ 3 ] = Max2( b, u8( 1 ) );
	token[ 4 ] = Max2( a, u8( 1 ) );
}

ImGuiColorToken::ImGuiColorToken( RGB8 rgb ) : ImGuiColorToken( rgb.r, rgb.g, rgb.b, 255 ) { }
ImGuiColorToken::ImGuiColorToken( RGBA8 rgba ) : ImGuiColorToken( rgba.r, rgba.g, rgba.b, rgba.a ) { }

void format( FormatBuffer * fb, const ImGuiColorToken & token, const FormatOpts & opts ) {
	format( fb, Span< const char >( ( const char * ) token.token, sizeof( token.token ) ), FormatOpts() );
}

void CenterTextY( Span< const char > str, float height ) {
	float text_height = ImGui::CalcTextSize( str ).y;
	ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 0.5f * ( height - text_height ) );
	ImGui::Text( str );
}

Vec4 CustomAttentionGettingColor( Vec4 from, Vec4 to, Time period ) {
	float t = Sin( cls.monotonicTime, period ) * 0.5f + 0.5f;
	return Lerp( from, t, to );
}

Vec4 AttentionGettingColor() {
	return CustomAttentionGettingColor( red.linear, diesel_yellow.linear, Milliseconds( 125 ) );
}

Vec4 PlantableColor() {
	return CustomAttentionGettingColor( diesel_green.linear * 0.8f, diesel_green.linear, Milliseconds( 125 ) );
}

Vec4 AttentionGettingRed() {
	return CustomAttentionGettingColor( diesel_red.linear * 0.8f, diesel_red.linear, Milliseconds( 125 ) );
}
