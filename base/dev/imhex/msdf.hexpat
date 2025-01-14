import "std/io.pat";

struct Vec2 { float x, y; };
struct MinMax2 { Vec2 mins, maxs; };

using Glyph;
fn FormatGlyph( Glyph glyph ) {
    return std::format( "{:.3}x{:.3} adv {:.3}",
        glyph.padded_uv_bounds.maxs.x - glyph.padded_uv_bounds.mins.x,
        glyph.padded_uv_bounds.maxs.y - glyph.padded_uv_bounds.mins.y,
        glyph.advance );
};

struct Glyph {
    MinMax2 bounds;
    // MinMax2 tight_uv_bounds;
    MinMax2 padded_uv_bounds;
    float advance;
} [[format( "FormatGlyph" )]];

struct FontMetadata {
    float glyph_padding;
    float dSDF_dTexel;
    float ascent;

    Glyph glyphs[ 256 ];
};

FontMetadata font @ 0x0;
