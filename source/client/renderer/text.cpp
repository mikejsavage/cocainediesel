#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/assets.h"
#include "qcommon/array.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"
#include "qcommon/hash.h"
#include "qcommon/serialization.h"

#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
#include "client/client.h"

#include "freetype/ft2build.h"
#include FT_FREETYPE_H
#include "stb/stb_image.h"

static constexpr size_t MAX_CHARS_PER_FRAME = 100000;

struct Glyph {
	MinMax2 bounds;
	MinMax2 uv_bounds;
	float advance;
};

struct Font {
	u32 path_hash;
	FT_Face face; // TODO: unused. might get used for kerning?
	Texture atlas;

	float glyph_padding;
	float pixel_range;
	float ascent;

	Glyph glyphs[ 256 ];
};

static FT_Library freetype;

static Font fonts[ 64 ];
static size_t num_fonts;

bool InitText() {
	int err = FT_Init_FreeType( &freetype );
	if( err != 0 ) {
		Com_Printf( S_COLOR_RED "Error initializing FreeType library: %d\n", err );
		return false;
	}

	num_fonts = 0;

	return true;
}

void ShutdownText() {
	for( size_t i = 0; i < num_fonts; i++ ) {
		FT_Done_Face( fonts[ i ].face );
	}
	FT_Done_FreeType( freetype );
}

static void Serialize( SerializationBuffer * buf, Font & font ) {
	*buf & font.glyph_padding & font.pixel_range & font.ascent;
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

	assert( num_fonts < ARRAY_COUNT( fonts ) );

	Font * font = &fonts[ num_fonts ];
	font->path_hash = path_hash;

	// load MSDF spec
	{
		DynamicString msdf_path( &temp, "{}.msdf", path );
		Span< const char > data = AssetBinary( msdf_path.c_str() ).cast< char >();
		if( data.ptr == NULL ) {
			Com_Printf( S_COLOR_RED "Couldn't read file %s\n", msdf_path.c_str() );
			return NULL;
		}

		if( !Deserialize( *font, data.ptr, data.n ) ) {
			Com_Printf( S_COLOR_RED "Couldn't load MSDF spec from %s\n", msdf_path.c_str() );
			return NULL;
		}
	}

	// load MSDF atlas
	{
		DynamicString atlas_path( &temp, "{}.png", path );
		Span< const u8 > data = AssetBinary( atlas_path.c_str() );

		int w, h, channels;
		u8 * pixels = stbi_load_from_memory( data.ptr, data.num_bytes(), &w, &h, &channels, 0 );
		defer { stbi_image_free( pixels ); };

		if( pixels == NULL || channels != 3 ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: couldn't load atlas from %s\n", path );
			return NULL;
		}

		TextureConfig config;
		config.width = checked_cast< u32 >( w );
		config.height = checked_cast< u32 >( h );
		config.data = pixels;
		config.format = TextureFormat_RGB_U8;

		font->atlas = NewTexture( config );
	}

	// load ttf
	{
		DynamicString ttf_path( &temp, "{}.ttf", path );
		Span< const FT_Byte > data = AssetBinary( ttf_path.c_str() ).cast< FT_Byte >();
		if( data.ptr == NULL ) {
			Com_Printf( S_COLOR_RED "Couldn't read file %s\n", ttf_path.c_str() );
			return NULL;
		}

		int err = FT_New_Memory_Face( freetype, data.ptr, data.n, 0, &font->face );
		if( err != 0 ) {
			Com_Printf( S_COLOR_RED "Couldn't load font face from %s\n", ttf_path.c_str() );
			return NULL;
		}
	}

	num_fonts++;

	return font;
}

static Vec3 ToClip( float x, float y ) {
	Vec2 clip = Vec2( x, y ) / frame_static.viewport * 2.0f - 1.0f;
	return Vec3( clip, 0 );
}

void DrawText( const Font * font, float pixel_size, Span< const char > str, float x, float y, Vec4 color, bool border, Vec4 border_color ) {
	if( font == NULL )
		return;

	TempAllocator temp = cls.frame_arena.temp();
	DynamicArray< Vec3 > positions( &temp, str.n * 4 );
	DynamicArray< Vec2 > uvs( &temp, str.n * 4 );
	DynamicArray< RGBA8 > colors( &temp, str.n * 4 );
	DynamicArray< u16 > indices( &temp, str.n * 6 );

	u16 base_index = DynamicMeshBaseIndex();

	y = frame_static.viewport_height - y - pixel_size * font->ascent;

	u32 state = 0;
	for( size_t i = 0; i < str.n; i++ ) {
		u32 c = 0;
		if( DecodeUTF8( &state, &c, str[ i ] ) != 0 )
			continue;
		if( c > 255 )
			c = '?';

		const Glyph * glyph = &font->glyphs[ c ];

		if( glyph->bounds.mins.x != glyph->bounds.maxs.x && glyph->bounds.mins.y != glyph->bounds.maxs.y ) {
			// TODO: this is bogus. it should expand glyphs by 1 or
			// 2 pixels to allow for border/antialiasing, up to a
			// limit determined by font->glyph_padding
			MinMax2 bounds = MinMax2(
				Vec2( x, y ) + pixel_size * ( glyph->bounds.mins - font->glyph_padding ),
				Vec2( x, y ) + pixel_size * ( glyph->bounds.maxs + font->glyph_padding )
			);

			u16 bl_idx = positions.add( ToClip( bounds.mins.x, bounds.mins.y ) );
			u16 br_idx = positions.add( ToClip( bounds.maxs.x, bounds.mins.y ) );
			u16 tl_idx = positions.add( ToClip( bounds.mins.x, bounds.maxs.y ) );
			u16 tr_idx = positions.add( ToClip( bounds.maxs.x, bounds.maxs.y ) );

			uvs.add( Vec2( glyph->uv_bounds.mins.x, glyph->uv_bounds.mins.y ) );
			uvs.add( Vec2( glyph->uv_bounds.maxs.x, glyph->uv_bounds.mins.y ) );
			uvs.add( Vec2( glyph->uv_bounds.mins.x, glyph->uv_bounds.maxs.y ) );
			uvs.add( Vec2( glyph->uv_bounds.maxs.x, glyph->uv_bounds.maxs.y ) );

			colors.add( rgba8_white ); // TODO
			colors.add( rgba8_white );
			colors.add( rgba8_white );
			colors.add( rgba8_white );

			indices.add( bl_idx + base_index );
			indices.add( br_idx + base_index );
			indices.add( tl_idx + base_index );

			indices.add( tl_idx + base_index );
			indices.add( br_idx + base_index );
			indices.add( tr_idx + base_index );
		}

		x += pixel_size * glyph->advance;
		// TODO: kerning
	}

	PipelineState pipeline;
	pipeline.pass = frame_static.ui_pass;
	pipeline.shader = &shaders.text;
	pipeline.depth_func = DepthFunc_Disabled;
	pipeline.blend_func = BlendFunc_Blend;
	pipeline.write_depth = false;
	pipeline.set_uniform( "u_Text", UploadUniformBlock(
		color, border_color,
		Vec2( font->atlas.width, font->atlas.height ),
		font->pixel_range, border ? 1 : 0 ) );
	pipeline.set_texture( "u_Atlas", font->atlas );

	DynamicMesh mesh;
	mesh.positions = positions.ptr();
	mesh.uvs = uvs.ptr();
	mesh.colors = colors.ptr();
	mesh.indices = indices.ptr();
	mesh.num_vertices = positions.size();
	mesh.num_indices = indices.size();

	DrawDynamicMesh( pipeline, mesh );
}

void DrawText( const Font * font, float pixel_size, const char * str, float x, float y, Vec4 color, bool border, Vec4 border_color ) {
	DrawText( font, pixel_size, Span< const char >( str, strlen( str ) ), x, y, color, border, border_color );
}

MinMax2 TextBounds( const Font * font, float pixel_size, const char * str ) {
	float width = 0.0f;
	MinMax1 y_extents = MinMax1::Empty();

	u32 state = 0;
	const Glyph * glyph = NULL;

	for( const char * p = str; *p != '\0'; p++ ) {
		u32 c = 0;
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

void DrawText( const Font * font, float pixel_size, const char * str, Alignment align, float x, float y, Vec4 color, bool border, Vec4 border_color ) {
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

	DrawText( font, pixel_size, str, x, y, color, border, border_color );
}
