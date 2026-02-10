#include "qcommon/base.h"
#include "client/client.h"
#include "client/clay.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
#include "cgame/cg_local.h"

#include "imgui/imgui.h"

static constexpr size_t TEXT_ARENA_SIZE = Megabytes( 1 );
static Clay_Arena clay_arena;
static ArenaAllocator text_arena;

static bool show_debugger;

static const Font * fonts[ ClayFont_Count ];

static void ClayErrorHandler( Clay_ErrorData error ) {
	// NOTE(mike): normally you can't print a Clay_String with %s but they're all static strings so it works
	Fatal( "%s", error.errorText.chars );
}

static Vec4 ClayToCD( Clay_Color color ) {
	RGBA8 clamped = RGBA8(
		Clamp( 0.0f, color.r, 255.0f ),
		Clamp( 0.0f, color.g, 255.0f ),
		Clamp( 0.0f, color.b, 255.0f ),
		Clamp( 0.0f, color.a, 255.0f )
	);
	return Vec4( sRGBToLinear( clamped ) );
}

static ImU32 ClayToImGui( Clay_Color clay ) {
	RGBA8 rgba8 = LinearTosRGB( ClayToCD( clay ) );
	return IM_COL32( rgba8.r, rgba8.g, rgba8.b, rgba8.a );
}

void InitClay() {
	show_debugger = false;

	AddCommand( "toggleuidebugger", []( const Tokenized & args ) {
		show_debugger = !show_debugger;
		Clay_SetDebugModeEnabled( show_debugger );
	} );

	u32 size = Clay_MinMemorySize();
	clay_arena = Clay_CreateArenaWithCapacityAndMemory( size, sys_allocator->allocate( size, 16 ) );
	text_arena = ArenaAllocator( AllocMany< char >( sys_allocator, TEXT_ARENA_SIZE ), TEXT_ARENA_SIZE );

	fonts[ ClayFont_Regular ] = RegisterFont( "fonts/Decalotype-Bold" );
	fonts[ ClayFont_Bold ] = RegisterFont( "fonts/Decalotype-Black" );
	fonts[ ClayFont_Italic ] = RegisterFont( "fonts/Decalotype-BoldItalic" );
	fonts[ ClayFont_BoldItalic ] = RegisterFont( "fonts/Decalotype-BlackItalic" );

	Clay_Initialize( clay_arena, { }, { .errorHandlerFunction = ClayErrorHandler } );

	auto measure_text = []( Clay_StringSlice text, Clay_TextElementConfig * config, void * user_data ) -> Clay_Dimensions {
		const Font * font = fonts[ config->fontId ];
		MinMax2 bounds = TextBaselineBounds( font, config->fontSize, Span< const char >( text.chars, text.length ) );
		Vec2 dims = bounds.maxs - bounds.mins;
		return { dims.x, dims.y };
	};
	Clay_SetMeasureTextFunction( measure_text, NULL );
}

void ShutdownClay() {
	Free( sys_allocator, clay_arena.memory );
	Free( sys_allocator, text_arena.get_memory() );
	Clay_SetCurrentContext( NULL );

	RemoveCommand( "toggleuidebugger" );
}

void ClayBeginFrame() {
	TracyZoneScopedN( "Clay_BeginLayout" );
	Clay_SetLayoutDimensions( Clay_Dimensions { frame_static.viewport.x, frame_static.viewport.y } );
	Clay_BeginLayout();

	text_arena.clear();
}

Clay_String AllocateClayString( Span< const char > str ) {
	char * r = AllocMany< char >( &text_arena, str.n );
	memcpy( r, str.ptr, str.n );
	return Clay_String { .length = checked_cast< s32 >( str.n ), .chars = r };
}

enum Corner {
	// ordered by CW rotations where 0deg = +X
	Corner_BottomRight,
	Corner_BottomLeft,
	Corner_TopLeft,
	Corner_TopRight,
};

static float Remap_0To1_1ToNeg1( float x ) {
	return ( 1.0f - x ) * 2.0f - 1.0f;
}

static void DrawBorderCorner( Corner corner, Clay_BoundingBox bounds, const Clay_BorderRenderData & border, float width, Clay_Color color ) {
	struct {
		float x, y, radius;
	} corners[] = {
		{ 1.0f, 1.0f, border.cornerRadius.bottomRight },
		{ 0.0f, 1.0f, border.cornerRadius.bottomLeft },
		{ 0.0f, 0.0f, border.cornerRadius.topLeft },
		{ 1.0f, 0.0f, border.cornerRadius.topRight },
	};

	float cx = corners[ corner ].x;
	float cy = corners[ corner ].y;
	float r = corners[ corner ].radius;
	float angle = int( corner ) * 90.0f;

	Vec2 arc_origin = Vec2(
		Lerp( bounds.x, cx, bounds.x + bounds.width ) + r * Remap_0To1_1ToNeg1( cx ),
		Lerp( bounds.y, cy, bounds.y + bounds.height ) + r * Remap_0To1_1ToNeg1( cy )
	);

	ImDrawList * draw_list = ImGui::GetBackgroundDrawList();
	draw_list->PathArcTo( arc_origin, r - width * 0.5f, Radians( angle ), Radians( angle + 90.0f ) );
	draw_list->PathStroke( ClayToImGui( color ), 0, width );
}

void DrawClayText( Span<const char> text, const Clay_TextElementConfig & config, const Clay_BoundingBox & bounds, Optional< FittedTextShadow > shadow, XAlignment alignment, const Clay_Padding & padding ) {
	Vec2 mins = Vec2( bounds.x + padding.left, bounds.y + padding.top );
	Vec2 maxs = Vec2( bounds.x + bounds.width - padding.right, bounds.y + bounds.height - padding.bottom );

	if( shadow.exists ) {
		float angle = Default( shadow.value.angle, 45.0f );
		Vec2 offset = Vec2( cosf( Radians( angle ) ), sinf( Radians( angle ) ) ) * shadow.value.offset;
		DrawFittedText(
			fonts[ config.fontId ], text, MinMax2( mins, maxs ) + offset,
			alignment, shadow.value.color );
	}

	DrawFittedText(
		fonts[ config.fontId ], text, MinMax2( mins, maxs ), alignment,
		ClayToCD( config.textColor ), { } );
}

void ClaySubmitFrame() {
	Clay_RenderCommandArray render_commands;
	{
		TracyZoneScopedN( "Clay_EndLayout" );
		render_commands = Clay_EndLayout();
	}

	{
		TracyZoneScopedN( "Clay submit draw calls" );
		for( s32 i = 0; i < render_commands.length; i++ ) {
			const Clay_RenderCommand & command = render_commands.internalArray[ i ];
			const Clay_BoundingBox & bounds = command.boundingBox;
			switch( command.commandType ) {
				case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
					const Clay_RectangleRenderData & rect = command.renderData.rectangle;
					ImDrawList * draw_list = ImGui::GetBackgroundDrawList();
					draw_list->AddRectFilled( Vec2( bounds.x, bounds.y ), Vec2( bounds.x + bounds.width, bounds.y + bounds.height ), ClayToImGui( rect.backgroundColor ), rect.cornerRadius.topLeft );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_BORDER: {
					const Clay_BorderRenderData & border = command.renderData.border;

					// TODO: rounded corners are wrong pretty much all of the time, transparent borders are drawn with overlap
					// should probably draw the edges and corners separately

					// top
					Draw2DBox( bounds.x + border.cornerRadius.topLeft, bounds.y,
						bounds.width - border.cornerRadius.topLeft - border.cornerRadius.topRight, border.width.top,
						cls.white_material, ClayToCD( border.color ) );
					// right
					Draw2DBox( bounds.x + bounds.width - border.width.right, bounds.y + border.cornerRadius.topRight,
						border.width.right, bounds.height - border.cornerRadius.topRight - border.cornerRadius.bottomRight,
						cls.white_material, ClayToCD( border.color ) );
					// left
					Draw2DBox( bounds.x, bounds.y + border.cornerRadius.topLeft,
						border.width.left, bounds.height - border.cornerRadius.topLeft - border.cornerRadius.bottomLeft,
						cls.white_material, ClayToCD( border.color ) );
					// bottom
					Draw2DBox( bounds.x + border.cornerRadius.bottomLeft, bounds.y + bounds.height - border.width.bottom,
						bounds.width - border.cornerRadius.bottomLeft - border.cornerRadius.bottomRight, border.width.bottom,
						cls.white_material, ClayToCD( border.color ) );

					// this doesn't do the right thing for different border sizes/colours
					DrawBorderCorner( Corner_TopLeft, bounds, border, border.width.top, border.color );
					DrawBorderCorner( Corner_TopRight, bounds, border, border.width.right, border.color );
					DrawBorderCorner( Corner_BottomLeft, bounds, border, border.width.bottom, border.color );
					DrawBorderCorner( Corner_BottomRight, bounds, border, border.width.right, border.color );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_TEXT: {
					const Clay_TextRenderData & text = command.renderData.text;
					const Optional< Vec4 > * border_color = ( const Optional< Vec4 > * ) command.userData;
					DrawText( fonts[ text.fontId ], text.fontSize,
						Span< const char >( text.stringContents.chars, text.stringContents.length ),
						bounds.x, bounds.y,
						ClayToCD( text.textColor ), *border_color );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
					const Vec4 * tint = ( const Vec4 * ) command.userData;
					const Clay_ImageRenderData & image = command.renderData.image;
					Draw2DBox( bounds.x, bounds.y, bounds.width, bounds.height, FindMaterial( StringHash( bit_cast< u64 >( image.imageData ) ) ), *tint );
				} break;

				case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
					ImGui::PushClipRect( Vec2( bounds.x, bounds.y ), Vec2( bounds.x + bounds.width, bounds.y + bounds.height ), true );
					break;

				case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
					ImGui::PopClipRect();
					break;

				case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
					const ClayCustomElementConfig * config = ( const ClayCustomElementConfig * ) command.renderData.custom.customData;
					switch( config->type ) {
						case ClayCustomElementType_Callback:
							config->callback.f( bounds, config->callback.userdata );
							break;

						case ClayCustomElementType_Lua:
							CG_DrawClayLuaHUDElement( bounds, config );
							break;

						case ClayCustomElementType_FittedText: {
							Span< const char > text = Span< const char >( config->fitted_text.text.chars, config->fitted_text.text.length );
							DrawClayText( text, config->fitted_text.config, bounds, config->fitted_text.shadow, config->fitted_text.alignment, config->fitted_text.padding );
						} break;
					}
				} break;

				default:
					assert( false );
					break;
			}
		}
	}
}

ArenaAllocator * ClayAllocator() {
	return &text_arena;
}
