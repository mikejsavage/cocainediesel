#include "r_local.h"
#include "client/client.h"

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/string.h"
#include "qcommon/utf8.h"
#include "qcommon/hash.h"
#include "qcommon/serialization.h"

#include "freetype/ft2build.h"
#include FT_FREETYPE_H

struct Glyph {
	MinMax2 bounds;
	MinMax2 uv_bounds;
	float advance;
};

struct Font {
	u32 path_hash;
	FT_Face face; // TODO: unused. might get used for kerning?
	image_t * atlas;

	float glyph_padding;
	float pixel_range;
	float ascent;

	Glyph glyphs[ 256 ];
};

static FT_Library freetype;

static Font fonts[ 64 ];
static size_t num_fonts;

static mesh_vbo_t * text_vbo;
static int vertex_offset;
static int index_offset;

bool R_InitText() {
	int err = FT_Init_FreeType( &freetype );
	if( err != 0 ) {
		Com_Printf( S_COLOR_RED "Error initializing FreeType library: %d\n", err );
		return false;
	}

	num_fonts = 0;
	text_vbo = R_CreateMeshVBO( NULL, 1000 * 1000, 1000 * 1000, 0, VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT, VBO_TAG_STREAM, 0 );

	return true;
}

void R_ShutdownText() {
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
	TempAllocator temp = cls.frame_arena->temp();

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

		u8 * data;
		int len = FS_LoadFile( msdf_path.c_str(), ( void ** ) &data, NULL, 0 );
		if( data == NULL ) {
			Com_Printf( S_COLOR_RED "Couldn't read file %s\n", msdf_path.c_str() );
			return NULL;
		}

		bool ok = Deserialize( *font, ( const char * ) data, len );
		FS_FreeFile( data );

		if( !ok ) {
			Com_Printf( S_COLOR_RED "Couldn't load MSDF spec from %s\n", msdf_path.c_str() );
			return NULL;
		}
	}

	// load MSDF atlas
	{
		DynamicString atlas_path( &temp, "{}_atlas.png", path );
		font->atlas = R_FindImage( atlas_path.c_str(), NULL, IT_NOMIPMAP, 0, IMAGE_TAG_GENERIC );

		if( font->atlas == NULL ) {
			Com_Printf( S_COLOR_RED "Couldn't load MSDF atlas %s\n", atlas_path.c_str() );
			return NULL;
		}
	}

	// load ttf
	{
		DynamicString ttf_path( &temp, "{}.ttf", path );

		u8 * data;
		int len = FS_LoadFile( ttf_path.c_str(), ( void ** ) &data, NULL, 0 );
		if( data == NULL ) {
			Com_Printf( S_COLOR_RED "Couldn't read file %s\n", ttf_path.c_str() );
			return NULL;
		}

		int err = FT_New_Memory_Face( freetype, ( const FT_Byte * ) data, len, 0, &font->face );
		FS_FreeFile( data );

		if( err != 0 ) {
			Com_Printf( S_COLOR_RED "Couldn't load font face from %s\n", ttf_path.c_str() );
			return NULL;
		}
	}

	num_fonts++;

	return font;
}

void TouchFonts() {
	R_TouchMeshVBO( text_vbo );
	for( size_t i = 0; i < num_fonts; i++ ) {
		R_TouchImage( fonts[ i ].atlas, IMAGE_TAG_GENERIC );
	}
}

void ResetTextVBO() {
	vertex_offset = 0;
	index_offset = 0;
}

static Vec4 ToClip( float x, float y ) {
	Vec2 clip = Vec2( x / glConfig.width, y / glConfig.height ) * 2.0f - 1.0f;
	return Vec4( clip, 0, 1 );
}

// TODO: probably belongs somewhere else
static float Area( MinMax2 bounds ) {
	return ( bounds.maxs.x - bounds.mins.x ) * ( bounds.maxs.y - bounds.mins.y );
}

void DrawText( const Font * font, float pixel_size, Span< const char > str, float x, float y, RGBA8 color, bool border, RGBA8 border_color ) {
	if( font == NULL )
		return;

	TempAllocator temp = cls.frame_arena->temp();
	DynamicArray< Vec4 > positions( &temp, str.n * 4 );
	DynamicArray< u16 > indices( &temp, str.n * 6 );
	DynamicArray< Vec2 > uvs( &temp, str.n * 4 );

	y = glConfig.height - y - pixel_size * font->ascent;

	u32 state = 0;
	for( size_t i = 0; i < str.n; i++ ) {
		u32 c = 0;
		if( DecodeUTF8( &state, &c, str[ i ] ) != 0 )
			continue;
		if( c > 255 )
			c = '?';

		const Glyph * glyph = &font->glyphs[ c ];

		if( Area( glyph->bounds ) > 0 ) {
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

			indices.add( bl_idx );
			indices.add( tl_idx );
			indices.add( br_idx );

			indices.add( tl_idx );
			indices.add( tr_idx );
			indices.add( br_idx );
		}

		x += pixel_size * glyph->advance;
		// TODO: kerning
	}

	mesh_t mesh = { };
	mesh.numVerts = positions.size();
	mesh.numElems = indices.size();
	mesh.xyzArray = ( vec4_t * ) positions.ptr();
	mesh.stArray = ( vec2_t * ) uvs.ptr();
	mesh.elems = indices.ptr();

	R_UploadVBOVertexData( text_vbo, vertex_offset, VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT, &mesh );
	R_UploadVBOElemData( text_vbo, vertex_offset, index_offset, &mesh );

	shaderpass_t pass = { };
	pass.rgbgen.type = RGB_GEN_IDENTITY;
	pass.alphagen.type = ALPHA_GEN_IDENTITY;
	pass.tcgen = TC_GEN_NONE;
	pass.images[0] = font->atlas;
	pass.flags = GLSTATE_SRCBLEND_SRC_ALPHA | GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLSTATE_NO_DEPTH_TEST;
	pass.program_type = GLSL_PROGRAM_TYPE_TEXT;

	shader_t shader = { };
	shader.vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT;
	shader.sort = SHADER_SORT_NEAREST;
	shader.numpasses = 1;
	shader.name = "text";
	shader.passes = &pass;

	RB_BindShader( NULL, &shader );
	RB_BindVBO( text_vbo->index, GL_TRIANGLES );
	RB_SetTextParams( color, border_color, border, font->pixel_range );
	RB_DrawElements( vertex_offset, mesh.numVerts, index_offset, mesh.numElems );

	vertex_offset += mesh.numVerts;
	index_offset += mesh.numElems;
}

void DrawText( const Font * font, float pixel_size, const char * str, float x, float y, RGBA8 color, bool border, RGBA8 border_color ) {
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

void DrawText( const Font * font, float pixel_size, const char * str, Alignment align, float x, float y, RGBA8 color, bool border, RGBA8 border_color ) {
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
