#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/span2d.h"
#include "gameshared/q_math.h"

#include "msdf/msdfgen.h"

#include "stb/stb_image_write.h"
#include "stb/stb_rect_pack.h"
#include "stb/stb_truetype.h"

#if GG
msdfgen::Range GlyphGeometry::getBoxRange() const {
	return box.range;
}

msdfgen::Projection GlyphGeometry::getBoxProjection() const {
	return msdfgen::Projection(msdfgen::Vector2(box.scale), box.translate);
}
#endif

static msdfgen::Point2 Point( stbtt_vertex_type x, stbtt_vertex_type y ) {
	return msdfgen::Point2( x / 64.0, y / 64.0 );
}

static msdfgen::Shape STBShapeToMSDFShape( const stbtt_fontinfo * font, u32 codepoint ) {
	stbtt_vertex * verts;
	int num_verts = stbtt_GetCodepointShape( font, checked_cast< int >( codepoint ), &verts );

	msdfgen::Shape shape{};
	msdfgen::Contour * contour = NULL;
	msdfgen::Point2 cursor{};

	for( int i = 0; i < num_verts; i++ ) {
		const stbtt_vertex & v = verts[ i ];
		switch( v.type ) {
			case STBTT_vmove: {
				if( contour == NULL || !contour->edges.empty() ) {
					contour = &shape.addContour();
				}
			} break;

			case STBTT_vline: {
				msdfgen::Point2 end = Point( v.x, v.y );
				if( end != cursor ) { // this check is from msdf-atlas-gen
					contour->addEdge( msdfgen::EdgeHolder( cursor, end ) );
				}
			} break;

			case STBTT_vcurve: {
				// msdf-c checks if cursor == control || control == end and sets control to the midpoint
				msdfgen::Point2 end = Point( v.x, v.y );
				msdfgen::Point2 control = Point( v.cx, v.cy );
				if( end != cursor ) {
					contour->addEdge( msdfgen::EdgeHolder( cursor, control, end ) );
				}
			} break;

			case STBTT_vcubic: {
				Fatal( "fuck otf" );
			} break;
		}

		cursor = Point( v.x, v.y );
	}

	return shape;
}

int main( int argv, char ** argc ) {
	constexpr size_t arena_size = 1024 * 1024 * 100; // 100MB
	ArenaAllocator arena( sys_allocator->allocate( arena_size, 16 ), arena_size );

	Span< u8 > ttf = ReadFileBinary( &arena, "base/fonts/Decalotype-Black.ttf" );

	stbtt_fontinfo font;
	font.userdata = &arena;
	if( stbtt_InitFont( &font, ttf.ptr, stbtt_GetFontOffsetForIndex( ttf.ptr, 0 ) ) == 0 ) {
		Fatal( "bad font" );
	}

	int ascent, descent;
	stbtt_GetFontVMetrics( &font, &ascent, &descent, 0 );
	float scale = stbtt_ScaleForPixelHeight( &font, 1.0f );

#if GG
	// Funit to pixel scale
	float scale = stbtt_ScaleForMappingEmToPixels(font, h);

	// get glyph bounding box (scaled later)
	int ix0, iy0, ix1, iy1;
	float xoff = .5, yoff = .5;
	stbtt_GetGlyphBox(font, stbtt_FindGlyphIndex(font,c), &ix0, &iy0, &ix1, &iy1);

	if (autofit) {
		// calculate new height
		float newh = h + (h - (iy1 - iy0)*scale) - 4;

		// calculate new scale
		// see 'stbtt_ScaleForMappingEmToPixels' in stb_truetype.h
		uint8_t *p = font->data + font->head + 18;
		int unitsPerEm = p[0]*256 + p[1];
		scale = newh / unitsPerEm;

		// make sure we are centered
		xoff = .0;
		yoff = .0;
	}

	// get left offset and advance
	int left_bearing, advance;
	stbtt_GetGlyphHMetrics(font, stbtt_FindGlyphIndex(font,c), &advance, &left_bearing);
	left_bearing *= scale;

	// calculate offset for centering glyph on bitmap
	int translate_x = (w/2)-((ix1 - ix0)*scale)/2-left_bearing;
	int translate_y = (h/2)-((iy1 - iy0)*scale)/2-iy0*scale;

	// set the glyph metrics
	// (pre-scale them)
	if (metrics) {
		metrics->left_bearing = left_bearing;
		metrics->advance      = advance*scale;
		metrics->ix0          = ix0*scale;
		metrics->ix1          = ix1*scale;
		metrics->iy0          = iy0*scale;
		metrics->iy1          = iy1*scale;
	}

	stbtt_vertex *verts;
	int num_verts = stbtt_GetGlyphShape(font, stbtt_FindGlyphIndex(font, c), &verts);
#endif

	for( uint32_t i = 0; i <= 255; i++ ) {
		msdfgen::Shape shape = STBShapeToMSDFShape( &font, i );
		msdfgen::Projection projection( msdfgen::Vector2( 1.0 ), msdfgen::Vector2( 10.0 ) );
		msdfgen::Range range( 5.0 );

		Span2D< Vec3 > pixels = AllocSpan2D< Vec3 >( &arena, 64, 64 );
		msdfgen::BitmapRef< float, 3 > bitmap( pixels.ptr->ptr(), pixels.w, pixels.h );
		msdfgen::generateMSDF( bitmap, shape, projection, range );

		Span2D< RGB8 > pixels8 = AllocSpan2D< RGB8 >( &arena, pixels.w, pixels.h );
		for( size_t y = 0; y < pixels.h; y++ ) {
			for( size_t x = 0; x < pixels.w; x++ ) {
				pixels8( x, y ) = RGB8(
					Quantize11< u8 >( Clamp( -1.0, pixels( x, y ).x / 5.0, 1.0 ) ),
					Quantize11< u8 >( Clamp( -1.0, pixels( x, y ).y / 5.0, 1.0 ) ),
					Quantize11< u8 >( Clamp( -1.0, pixels( x, y ).z / 5.0, 1.0 ) )
				);
			}
		}

		const char * path = arena( "msdf/{}.jpg", i );
		CreatePathForFile( &arena, path );
		stbi_write_png( path, pixels8.w, pixels8.h, 3, pixels8.ptr, 0 );
	}

	Free( sys_allocator, arena.get_memory() );

	return 0;
}
