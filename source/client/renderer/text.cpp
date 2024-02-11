#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/utf8.h"
#include "qcommon/hash.h"
#include "qcommon/serialization.h"

#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
#include "client/client.h"
#include "client/assets.h"

#include "imgui/imgui.h"

#include "stb/stb_image.h"

struct Glyph {
	MinMax2 bounds;
	MinMax2 uv_bounds;
	float advance;
};

struct Font {
	u32 path_hash;
	Texture atlas;
	Material material;

	float glyph_padding;
	float dSDF_dTexel;
	float ascent;

	Glyph glyphs[ 256 ];
};

static Font fonts[ 64 ];
static size_t num_fonts;

void InitText() {
	num_fonts = 0;
}

void ShutdownText() {
	for( size_t i = 0; i < num_fonts; i++ ) {
		DeleteTexture( fonts[ i ].atlas );
	}
}

static void Serialize( SerializationBuffer * buf, Font & font ) {
	*buf & font.glyph_padding & font.dSDF_dTexel & font.ascent;
	for( Glyph & glyph : font.glyphs ) {
		*buf & glyph.bounds & glyph.uv_bounds & glyph.advance;
	}
}

const Font * RegisterFont( const char * path ) {
	TempAllocator temp = cls.frame_arena.temp();

	u32 path_hash = Hash32( path );
	for( size_t i = 0; i < num_fonts; i++ ) {
		if( fonts[ i ].path_hash == path_hash ) {
			return &fonts[ i ];
		}
	}

	Assert( num_fonts < ARRAY_COUNT( fonts ) );

	Font * font = &fonts[ num_fonts ];
	font->path_hash = path_hash;

	// load MSDF spec
	{
		Span< const char > msdf_path = temp.sv( "{}.msdf", path );
		Span< const char > data = AssetBinary( msdf_path ).cast< const char >();
		if( data.ptr == NULL ) {
			Com_GGPrint( S_COLOR_RED "Couldn't read file {}", msdf_path );
			return NULL;
		}

		if( !Deserialize( NULL, font, data.ptr, data.n ) ) {
			Com_GGPrint( S_COLOR_RED "Couldn't load MSDF spec from {}", msdf_path );
			return NULL;
		}
	}

	// load MSDF atlas
	{
		Span< const char > atlas_path = temp.sv( "{}.png", path );
		Span< const u8 > data = AssetBinary( atlas_path );

		int w, h, channels;
		u8 * pixels = stbi_load_from_memory( data.ptr, data.num_bytes(), &w, &h, &channels, 4 );
		defer { stbi_image_free( pixels ); };

		if( pixels == NULL || channels != 3 ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: couldn't load atlas from %s\n", path );
			return NULL;
		}

		TextureConfig config;
		config.format = TextureFormat_RGBA_U8;
		config.width = checked_cast< u32 >( w );
		config.height = checked_cast< u32 >( h );
		config.data = pixels;

		font->atlas = NewTexture( config );
		font->material.texture = &font->atlas;
	}

	num_fonts++;

	return font;
}

static void DrawText( const Font * font, float pixel_size, Span< const char > str, float x, float y, Vec4 color, bool border, Vec4 border_color ) {
	if( font == NULL )
		return;

	y += pixel_size * font->ascent;

	ImGuiShaderAndMaterial sam;
	sam.shader = &shaders.text;
	sam.material = &font->material;
	sam.uniform_name = "u_Text";
	sam.uniform_block = UploadUniformBlock( color, border_color, font->dSDF_dTexel, border ? 1 : 0 );

	ImDrawList * bg = ImGui::GetBackgroundDrawList();
	bg->PushTextureID( sam );

	u32 state = 0;
	u32 c = 0;
	for( size_t i = 0; i < str.n; i++ ) {
		if( DecodeUTF8( &state, &c, str[ i ] ) != 0 )
			continue;
		if( c > 255 )
			c = '?';

		const Glyph * glyph = &font->glyphs[ c ];

		if( glyph->bounds.mins.x != glyph->bounds.maxs.x && glyph->bounds.mins.y != glyph->bounds.maxs.y ) {
			// TODO: this is bogus. it should expand glyphs by 1 or
			// 2 pixels to allow for border/antialiasing, up to a
			// limit determined by font->glyph_padding
			Vec2 mins = Vec2( x, y ) + pixel_size * ( glyph->bounds.mins - font->glyph_padding );
			Vec2 maxs = Vec2( x, y ) + pixel_size * ( glyph->bounds.maxs + font->glyph_padding );
			bg->PrimReserve( 6, 4 );
			bg->PrimRectUV( mins, maxs, glyph->uv_bounds.mins, glyph->uv_bounds.maxs, IM_COL32_WHITE );
		}

		x += pixel_size * glyph->advance;
		// TODO: kerning
	}

	bg->PopTextureID();
}

void DrawText( const Font * font, float pixel_size, const char * str, float x, float y, Vec4 color, bool border ) {
	Vec4 border_color = Vec4( 0, 0, 0, color.w );
	DrawText( font, pixel_size, MakeSpan( str ), x, y, color, border, border_color );
}

void DrawText( const Font * font, float pixel_size, const char * str, float x, float y, Vec4 color, Vec4 border_color ) {
	DrawText( font, pixel_size, MakeSpan( str ), x, y, color, true, border_color );
}

MinMax2 TextBounds( const Font * font, float pixel_size, const char * str ) {
	float width = 0.0f;
	MinMax1 y_extents = MinMax1::Empty();

	u32 state = 0;
	u32 c = 0;
	const Glyph * glyph = NULL;

	for( const char * p = str; *p != '\0'; p++ ) {
		if( DecodeUTF8( &state, &c, *p ) != 0 )
			continue;
		if( c > 255 )
			c = '?';

		glyph = &font->glyphs[ c ];
		width += glyph->advance;
		y_extents.lo = Min2( glyph->bounds.mins.y, y_extents.lo );
		y_extents.hi = Max2( glyph->bounds.maxs.y, y_extents.hi );
	}

	if( glyph == NULL )
		return MinMax2( Vec2( 0 ), Vec2( 0 ) );

	width -= glyph->advance;
	width += glyph->bounds.maxs.x - glyph->bounds.mins.x;

	return MinMax2( pixel_size * Vec2( 0, y_extents.lo ), pixel_size * Vec2( width, y_extents.hi ) );
}

static void DrawText( const Font * font, float pixel_size, const char * str, Alignment align, float x, float y, Vec4 color, bool border, Vec4 border_color ) {
	MinMax2 bounds = TextBounds( font, pixel_size, str );

	if( align.x == XAlignment_Center ) {
		x -= bounds.maxs.x / 2.0f;
	}
	else if( align.x == XAlignment_Right ) {
		x -= bounds.maxs.x;
	}

	y -= pixel_size * font->ascent;
	if( align.y == YAlignment_Top ) {
		y += bounds.maxs.y - bounds.mins.y;
	}
	else if( align.y == YAlignment_Middle ) {
		y += ( bounds.maxs.y - bounds.mins.y ) / 2.0f;
	}

	DrawText( font, pixel_size, MakeSpan( str ), x, y, color, border, border_color );
}

void DrawText( const Font * font, float pixel_size, const char * str, Alignment align, float x, float y, Vec4 color, bool border ) {
	Vec4 border_color = Vec4( 0, 0, 0, color.w );
	DrawText( font, pixel_size, str, align, x, y, color, border, border_color );
}

void DrawText( const Font * font, float pixel_size, const char * str, Alignment align, float x, float y, Vec4 color, Vec4 border_color ) {
	DrawText( font, pixel_size, str, align, x, y, color, true, border_color );
}
