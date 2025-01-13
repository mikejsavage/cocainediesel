#pragma once

#include "qcommon/types.h"
#include "qcommon/serialization.h"

struct FontMetadata {
	struct Glyph {
		MinMax2 bounds; // y-up
		// MinMax2 tight_uv_bounds; // y-down
		MinMax2 padded_uv_bounds; // y-down
		float advance;
	};

	float glyph_padding;
	float dSDF_dTexel;
	float ascent;
	// float descent;

	Glyph glyphs[ 256 ];
};

inline void Serialize( SerializationBuffer * buf, FontMetadata & font ) {
	// *buf & font.glyph_padding & font.dSDF_dTexel & font.ascent & font.descent;
	// for( FontMetadata::Glyph & glyph : font.glyphs ) {
	// 	*buf & glyph.bounds & glyph.tight_uv_bounds & glyph.padded_uv_bounds & glyph.advance;
	// }
	*buf & font.glyph_padding & font.dSDF_dTexel & font.ascent;
	for( FontMetadata::Glyph & glyph : font.glyphs ) {
		*buf & glyph.bounds & glyph.padded_uv_bounds & glyph.advance;
	}
}
