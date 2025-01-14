#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/serialization.h"
#include "qcommon/span2d.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"

#include "tools/dieselfont/font_metadata.h"

#include "msdfgen/msdfgen.h"

#include "stb/stb_image_write.h"
#include "stb/stb_rect_pack.h"
#include "stb/stb_truetype.h"

static msdfgen::Point2 Point( stbtt_vertex_type x, stbtt_vertex_type y ) {
	return msdfgen::Point2( x / 64.0, y / 64.0 );
}

static msdfgen::Shape STBShapeToMSDFShape( const stbtt_fontinfo * font, u32 codepoint ) {
	TracyZoneScoped;

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
				Fatal( "TTF only" );
			} break;
		}

		cursor = Point( v.x, v.y );
	}

	return shape;
}

// TODO: at some point we should roll our own msdfgen because the algorithm is
// pretty simple and the reference library is not good
#if 0
struct GlyphContours {
	struct Color {
		bool r, g, b;
	};

	static constexpr Color two_colors[] = {
		{ true, true, false }, // 6
		{ false, true, true }, // 3
		{ true, false, true }, // 4
	};

	struct Bezier {
		Vec2 a, b, c;
	};

	struct Contour {
		size_t first;
		size_t n;
	};

	Span< Bezier > beziers;
	Span< Contour > contours;
};

static Vec2 Point2( stbtt_vertex_type x, stbtt_vertex_type y ) {
	return Vec2( x / 64.0f, y / 64.0f );
}

static GlyphContours STBShapeToDiesel( Allocator * a, const stbtt_fontinfo * font, u32 codepoint ) {
	TracyZoneScoped;

	stbtt_vertex * verts;
	int num_verts = stbtt_GetCodepointShape( font, checked_cast< int >( codepoint ), &verts );

	NonRAIIDynamicArray< GlyphContours::Bezier > beziers( a );
	NonRAIIDynamicArray< GlyphContours::Contour > contours( a );

	Vec2 cursor = Vec2( 0.0f );

	for( int i = 0; i < num_verts; i++ ) {
		const stbtt_vertex & v = verts[ i ];
		switch( v.type ) {
			case STBTT_vmove: {
				if( contours.size() == 0 || contours[ contours.size() - 1 ].n > 0 ) {
					contours.add( { .first = beziers.size() } );
				}
			} break;

			case STBTT_vline: {
				Vec2 end = Point2( v.x, v.y );
				if( end != cursor ) { // this check is from msdf-atlas-gen
					if( contours.size() == 0 ) {
						Fatal( "Bad font" );
					}
					beziers.add( { cursor, ( cursor + end ) * 0.5f, end } );
					contours[ contours.size() - 1 ].n++;
				}
			} break;

			case STBTT_vcurve: {
				// msdf-c checks if cursor == control || control == end and sets control to the midpoint
				Vec2 end = Point2( v.x, v.y );
				Vec2 control = Point2( v.cx, v.cy );
				if( end != cursor ) {
					if( contours.size() == 0 ) {
						Fatal( "Bad font" );
					}
					beziers.add( { cursor, control, end } );
					contours[ contours.size() - 1 ].n++;
				}
			} break;

			case STBTT_vcubic: {
				Fatal( "TTF only" );
			} break;
		}

		cursor = Point2( v.x, v.y );
	}

	return { beziers.span(), contours.span() };
}

static float CosAcosDiv3( float x ) {
	x = sqrtf( 0.5f + 0.5f * x );
	return x * ( x * ( x * ( x * -0.008972f + 0.039071f ) - 0.107074f ) + 0.576975f ) + 0.5f;
}

static float Sign( float x ) {
	if( x == 0.0f )
		return 0.0f;
	return x < 0.0f ? -1.0f : 1.0f;
}

static float Cross( Vec2 a, Vec2 b ) { return a.x * b.y - a.y * b.x; }
static Vec2 Sign( Vec2 v ) { return Vec2( Sign( v.x ), Sign( v.y ) ); }
static Vec2 Abs( Vec2 v ) { return Vec2( fabsf( v.x ), fabsf( v.y ) ); }
static Vec2 Pow( Vec2 v, float e ) { return Vec2( powf( v.x, e ), powf( v.y, e ) ); }
static Vec3 Clamp( float lo, Vec3 v, float hi ) { return Vec3( Clamp( lo, v.x, hi ), Clamp( lo, v.y, hi ), Clamp( lo, v.z, hi ) ); }

static float DistanceToBezier( GlyphContours::Bezier bezier, Vec2 pos ) {
	Vec2 a = bezier.b - bezier.a;
	Vec2 b = bezier.a - 2.0f * bezier.b + bezier.c;
	Vec2 c = a * 2.0f;
	Vec2 d = bezier.a - pos;

	// cubic to be solved (kx*=3 and ky*=3)
	float kk = 1.0f / Dot( b, b );
	float kx = kk * Dot( a, b );
	float ky = kk * (2.0f*Dot(a,a)+Dot(d,b))/3.0f;
	float kz = kk * Dot(d,a);

	float res = 0.0f;
	float sgn = 0.0f;

	float p  = ky - kx*kx;
	float q  = kx*(2.0f*kx*kx - 3.0f*ky) + kz;
	float p3 = p*p*p;
	float q2 = q*q;
	float h  = q2 + 4.0f*p3;

	if( h >= 0.0f ) { // 1 root
		h = sqrtf( h );
		Vec2 x = (Vec2(h,-h)-q)/2.0f;

#if 0
		// When p≈0 and p<0, h-q has catastrophic cancelation. So, we do
		// h=√(q²+4p³)=q·√(1+4p³/q²)=q·√(1+w) instead. Now we approximate
		// √ by a linear Taylor expansion into h≈q(1+½w) so that the q's
		// cancel each other in h-q. Expanding and simplifying further we
		// get x=vec2(p³/q,-p³/q-q). And using a second degree Taylor
		// expansion instead: x=vec2(k,-k-q) with k=(1-p³/q²)·p³/q
		if( abs(p)<0.001 )
		{
			float k = p3/q;              // linear approx
										 //float k = (1.0-p3/q2)*p3/q;  // quadratic approx
			x = vec2(k,-k-q);
		}
#endif

		Vec2 uv = Sign(x)*Pow(Abs(x), 1.0f / 3.0f);
		float t = uv.x + uv.y;

		// from NinjaKoala - single newton iteration to account for cancellation
		t -= (t*(t*t+3.0f*p)+q)/(3.0f*t*t+3.0f*p);

		t = Clamp( 0.0f, t-kx, 1.0f );
		Vec2  w = d+(c+b*t)*t;
		res = Dot( w, w );
		sgn = Cross(c+2.0*b*t,w);
	}
	else { // 3 roots
		float z = sqrtf( -p );
#if 0
		float v = acos(q/(p*z*2.0))/3.0;
		float m = cos(v);
		float n = sin(v);
#else
		float m = CosAcosDiv3( q/(p*z*2.0f) );
		float n = sqrtf(1.0f - m*m);
#endif
		n *= sqrtf( 3.0f );
		Vec3  t = Clamp( 0.0f, Vec3(m+m,-n-m,n-m)*z-kx, 1.0f );
		Vec2  qx=d+(c+b*t.x)*t.x; float dx=Dot(qx, qx), sx=Cross(a+b*t.x,qx);
		Vec2  qy=d+(c+b*t.y)*t.y; float dy=Dot(qy, qy), sy=Cross(a+b*t.y,qy);
		if( dx < dy ) {
			res=dx;sgn=sx;
		}
		else {
			res=dy;sgn=sy;
		}
	}

	{
		Vec2 tangent0 = bezier.b - bezier.a;
		Vec2 o = pos - bezier.a;
		if( Dot( tangent0, o ) < 0.0f ) {
			Vec2 proj = bezier.a + Dot( o, tangent0 ) / Dot( tangent0, tangent0 ) * tangent0;
			float dist = Dot( pos - proj, pos - proj );
			if( dist < res ) {
				res = dist;
				sgn = -Cross( tangent0, o );
			}
		}
	}

	{
		Vec2 tangent1 = bezier.c - bezier.b;
		Vec2 o = pos - bezier.c;
		if( Dot( tangent1, o ) > 0.0f ) {
			Vec2 proj = bezier.c + Dot( o, tangent1 ) / Dot( tangent1, tangent1 ) * tangent1;
			float dist = Dot( pos - proj, pos - proj );
			if( dist < res ) {
				res = dist;
				sgn = -Cross( tangent1, o );
			}
		}
	}

	return sqrtf( res ) * Sign( sgn );
}
#endif

static void CopySpan2DFlipY( Span2D< RGB8 > dst, Span2D< const RGB8 > src ) {
	Assert( src.w == dst.w && src.h == dst.h );
	for( u32 i = 0; i < dst.h; i++ ) {
		memcpy( dst.row( dst.h - i - 1 ).ptr, src.row( i ).ptr, dst.row( i ).num_bytes() );
	}
}

int main( int argc, char ** argv ) {
	if( argc == 1 ) {
		printf( "Usage: %s <font.ttf>\n", argv[ 0 ] );
		return 1;
	}

	constexpr u32 codepoint_ranges[][ 2 ] = {
		{ 0x20, 0xff }, // ASCII
		// { 0x3131, 0x3163 }, // Korean
		// { 0xac00, 0xd7a3 }, // Korean
	};

	constexpr int atlas_width = 2048; // pixels
	constexpr int atlas_height = 1024; // pixels
	constexpr size_t atlas_glyph_embox_size = 64; // pixels
	constexpr float range_in_ems = 8.0f;

	constexpr size_t arena_size = 1024 * 1024 * 100; // 100MB
	ArenaAllocator arena( sys_allocator->allocate( arena_size, 16 ), arena_size );

	Span< const char > ttf_path = MakeSpan( argv[ 1 ] );
	Span< u8 > ttf = ReadFileBinary( &arena, argv[ 1 ] );
	if( ttf.ptr == NULL ) {
		Fatal( "Can't read %s", argv[ 1 ] );
	}

	stbtt_fontinfo font;
	font.userdata = &arena;
	if( stbtt_InitFont( &font, ttf.ptr, stbtt_GetFontOffsetForIndex( ttf.ptr, 0 ) ) == 0 ) {
		Fatal( "bad font" );
	}

	int ascent;
	stbtt_GetFontVMetrics( &font, &ascent, NULL, 0 );
	// float scale = stbtt_ScaleForPixelHeight( &font, 1.0f );
	float scale = stbtt_ScaleForMappingEmToPixels( &font, 1.0f );

	float dSDF_dUV = 1.0f / ( scale * atlas_glyph_embox_size * 64.0f * range_in_ems * 2 );
	float padding = range_in_ems * scale * atlas_glyph_embox_size * 64.0f; // TODO: give 64 a meaningful name
	printf( "dSDF_dUV %f\n", dSDF_dUV );
	printf( "scale %g\n", scale );
	// TODO: decouple padding from range
	printf( "padding %f\n", range_in_ems * scale * atlas_glyph_embox_size * 64 );
	printf( "ascent %f\n", ascent * scale );

	size_t max_codepoints = 0;
	for( auto [ lo, hi ] : codepoint_ranges ) {
		max_codepoints += hi - lo;
	}

	stbrp_rect * rects = AllocMany< stbrp_rect >( &arena, max_codepoints );
	memset( rects, 0, sizeof( stbrp_rect ) * max_codepoints );

	struct GlyphBox {
		u32 codepoint;
		size_t rect_idx;
		int x0, y0, x1, y1;
	};
	GlyphBox * glyphs = AllocMany< GlyphBox >( &arena, max_codepoints );

	size_t num_glyphs = 0;
	for( auto [ lo, hi ] : codepoint_ranges ) {
		for( u32 codepoint = lo; codepoint < hi; codepoint++ ) {
			TempAllocator temp = arena.temp();
			font.userdata = &temp;

			if( stbtt_FindGlyphIndex( &font, checked_cast< int >( codepoint ) ) == 0 )
				continue;

			GlyphBox * glyph = &glyphs[ num_glyphs ];
			*glyph = { .codepoint = codepoint };
			stbtt_GetCodepointBox( &font, codepoint, &glyph->x0, &glyph->y0, &glyph->x1, &glyph->y1 );

			rects[ num_glyphs ] = {
				.w = checked_cast< stbrp_coord >( ceilf( ( glyph->x1 - glyph->x0 ) * atlas_glyph_embox_size * scale + 2.0f * padding ) ),
				.h = checked_cast< stbrp_coord >( ceilf( ( glyph->y1 - glyph->y0 ) * atlas_glyph_embox_size * scale + 2.0f * padding ) ),
			};

			num_glyphs++;
		}
	}

	{
		TracyZoneScopedN( "stbrp_pack_rects" );

		stbrp_node * nodes = AllocMany< stbrp_node >( &arena, num_glyphs );
		stbrp_context packer;
		stbrp_init_target( &packer, atlas_width, atlas_height, nodes, num_glyphs );
		if( stbrp_pack_rects( &packer, rects, num_glyphs ) != 1 ) {
			Fatal( "Can't pack" );
		}
	}

	Span2D< RGB8 > atlas = AllocSpan2D< RGB8 >( &arena, atlas_width, atlas_height );
	memset( atlas.ptr, 0, atlas.num_bytes() );

	FontMetadata metadata = {
		.glyph_padding = padding / atlas_glyph_embox_size,
		.dSDF_dTexel = dSDF_dUV,
		.ascent = ascent * scale,
		// .descent = descent * scale,
	};

	for( size_t i = 0; i < num_glyphs; i++ ) {
		TempAllocator temp = arena.temp();
		font.userdata = &temp;

		TracyZoneScopedN( "Generate MSDF" );
		TracyZoneSpan( temp.sv( "{}", i ) );

		const GlyphBox & glyph = glyphs[ i ];
		u32 codepoint = glyph.codepoint;

		int advance, lsb;
		stbtt_GetCodepointHMetrics( &font, codepoint, &advance, &lsb );

		msdfgen::Shape shape = STBShapeToMSDFShape( &font, codepoint );
		// no idea why the translation needs to be * 0.5f
		msdfgen::Projection projection(
			msdfgen::Vector2( scale * 64.0f * atlas_glyph_embox_size ),
			msdfgen::Vector2( ( padding - glyph.x0 * scale * 64.0f ) * 0.5f, ( padding - glyph.y0 * scale * 64.0f ) * 0.5f )
		);
		msdfgen::Range range( -range_in_ems, range_in_ems );

		stbrp_rect uvs = rects[ i ];
		Span2D< Vec3 > pixels = AllocSpan2D< Vec3 >( &temp, uvs.w, uvs.h );
		msdfgen::BitmapRef< float, 3 > bitmap( pixels.ptr->ptr(), pixels.w, pixels.h );
		{
			TracyZoneScopedN( "edgeColoringSimple" );
			msdfgen::edgeColoringSimple( shape, 3 );
		}
		{
			TracyZoneScopedN( "generateMSDF" );
			msdfgen::generateMSDF( bitmap, shape, projection, range );
		}

		Span2D< RGB8 > pixels8 = AllocSpan2D< RGB8 >( &temp, pixels.w, pixels.h );
		{
			TracyZoneScopedN( "Vec3 -> RGB8" );
			for( size_t y = 0; y < pixels.h; y++ ) {
				for( size_t x = 0; x < pixels.w; x++ ) {
					pixels8( x, y ) = RGB8(
						Quantize01< u8 >( Clamp01( pixels( x, y ).x ) ),
						Quantize01< u8 >( Clamp01( pixels( x, y ).y ) ),
						Quantize01< u8 >( Clamp01( pixels( x, y ).z ) )
					);
				}
			}
		}

		CopySpan2DFlipY( atlas.slice( uvs.x, uvs.y, uvs.w, uvs.h ), pixels8 );

		Assert( codepoint < ARRAY_COUNT( metadata.glyphs ) );
		Vec2 atlas_size = Vec2( atlas_width, atlas_height );
		metadata.glyphs[ codepoint ] = FontMetadata::Glyph {
			.bounds = MinMax2( scale * Vec2( glyph.x0, glyph.y0 ), scale * Vec2( glyph.x1, glyph.y1 ) ),
			// .tight_uv_bounds = { },
			.padded_uv_bounds = MinMax2( Vec2( uvs.x, uvs.y + uvs.h ) / atlas_size, Vec2( uvs.x + uvs.w, uvs.y ) / atlas_size ),
			.advance = advance * scale,
		};
	}

	{
		TracyZoneScopedN( "stbi_write_png" );
		const char * atlas_path = arena( "{}.png", StripExtension( ttf_path ) );
		CreatePathForFile( &arena, atlas_path );
		stbi_write_png( atlas_path, atlas.w, atlas.h, 3, atlas.ptr, 0 );
	}

	DynamicArray< u8 > serialized( &arena );
	Serialize( metadata, &serialized );
	WriteFile( &arena, arena( "{}.msdf", StripExtension( ttf_path ) ), serialized.ptr(), serialized.num_bytes() );

	Free( sys_allocator, arena.get_memory() );

	return 0;
}
