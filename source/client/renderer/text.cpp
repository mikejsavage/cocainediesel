#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"
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

static BoundedDynamicArray< Font, 64 > fonts;

void InitText() {
	fonts.clear();
}

void ShutdownText() {
	for( Font font : fonts ) {
		DeleteTexture( font.atlas );
	}
}

static void Serialize( SerializationBuffer * buf, Font & font ) {
	*buf & font.glyph_padding & font.dSDF_dTexel & font.ascent;
	for( Glyph & glyph : font.glyphs ) {
		*buf & glyph.bounds & glyph.uv_bounds & glyph.advance;
	}
}

const Font * RegisterFont( Span< const char > path ) {
	TempAllocator temp = cls.frame_arena.temp();

	u32 path_hash = Hash32( path );
	for( const Font & font : fonts ) {
		if( font.path_hash == path_hash ) {
			return &font;
		}
	}

	Font font = { };
	font.path_hash = path_hash;

	// load MSDF spec
	{
		Span< const char > msdf_path = temp.sv( "{}.msdf", path );
		Span< const char > data = AssetBinary( msdf_path ).cast< const char >();
		if( data.ptr == NULL ) {
			Com_GGPrint( S_COLOR_RED "Couldn't read file {}", msdf_path );
			return NULL;
		}

		if( !Deserialize( NULL, &font, data.ptr, data.n ) ) {
			Com_GGPrint( S_COLOR_RED "Couldn't load MSDF spec from {}", msdf_path );
			return NULL;
		}
	}

	// load MSDF atlas
	// we need to load it ourselves because the normal texture loader treats 4 channel PNGs as sRGB
	{
		Span< const char > atlas_path = temp.sv( "{}.png", path );
		Span< const u8 > data = AssetBinary( atlas_path );

		int w, h, channels;
		u8 * pixels = stbi_load_from_memory( data.ptr, data.num_bytes(), &w, &h, &channels, 4 );
		defer { stbi_image_free( pixels ); };

		if( pixels == NULL || channels != 3 ) {
			Com_GGPrint( S_COLOR_YELLOW "WARNING: couldn't load atlas from {}", path );
			return NULL;
		}

		font.atlas = NewTexture( TextureConfig {
			.format = TextureFormat_RGBA_U8,
			.width = checked_cast< u32 >( w ),
			.height = checked_cast< u32 >( h ),
			.data = pixels,
		} );
		font.material.texture = &font.atlas;
	}

	Optional< Font * > slot = fonts.add();
	if( !slot.exists ) {
		Com_Printf( S_COLOR_YELLOW "Too many fonts\n" );
		DeleteTexture( font.atlas );
		return NULL;
	}

	*slot.value = font;
	slot.value->material.texture = &slot.value->atlas;

	return slot.value;
}

static float Area( const MinMax2 & rect ) {
	float w = Max2( 0.0f, rect.maxs.x - rect.mins.x );
	float h = Max2( 0.0f, rect.maxs.y - rect.mins.y );
	return w * h;
}

void DrawText( const Font * font, float pixel_size, Span< const char > str, float x, float y, Vec4 color, Optional< Vec4 > border_color ) {
	if( font == NULL )
		return;

	y += pixel_size * font->ascent;

	ImGuiShaderAndMaterial sam;
	sam.shader = &shaders.text;
	sam.material = &font->material;
	sam.uniform_name = "u_Text";
	sam.uniform_block = UploadUniformBlock( color, Default( border_color, Vec4( 0.0f ) ), font->dSDF_dTexel, border_color.exists ? 1 : 0 );

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

		if( Area( glyph->bounds ) > 0.0f ) {
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

void DrawText( const Font * font, float pixel_size, const char * str, float x, float y, Vec4 color, Optional< Vec4 > border_color ) {
	return DrawText( font, pixel_size, MakeSpan( str ), x, y, color, border_color );
}

MinMax2 TextBounds( const Font * font, float pixel_size, Span< const char > str ) {
	float width = 0.0f;
	MinMax1 y_extents = MinMax1::Empty();

	u32 state = 0;
	u32 c = 0;
	const Glyph * glyph = NULL;

	for( size_t i = 0; i < str.n; i++ ) {
		if( DecodeUTF8( &state, &c, str[ i ] ) != 0 )
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

MinMax2 TextBounds( const Font * font, float pixel_size, const char * str ) {
	return TextBounds( font, pixel_size, MakeSpan( str ) );
}

void DrawText( const Font * font, float pixel_size, const char * str, Alignment align, float x, float y, Vec4 color, Optional< Vec4 > border_color ) {
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

	DrawText( font, pixel_size, MakeSpan( str ), x, y, color, border_color );
}

void Draw3DText( const Font * font, float size, Span< const char > str, Alignment align, Vec3 origin, EulerDegrees3 angles, Vec4 color ) {
	struct TextVertex {
		Vec3 position;
		Vec2 uv;
	};

	TempAllocator temp = cls.frame_arena.temp();
	DynamicArray< TextVertex > vertices( &temp, 4 * str.n );
	DynamicArray< u16 > indices( &temp, 6 * str.n );

	Mat3x4 rotation = Mat4Rotation( angles );
	Vec3 dx = rotation.row0().xyz() * size;
	Vec3 dy = rotation.row2().xyz() * size;

	Vec3 cursor = origin + dy; // TODO: do alignment properly. really need baseline alignment for this
	u32 state = 0;
	u32 c = 0;
	for( size_t i = 0; i < str.n; i++ ) {
		if( DecodeUTF8( &state, &c, str[ i ] ) != 0 )
			continue;
		if( c > 255 )
			c = '?';

		const Glyph * glyph = &font->glyphs[ c ];

		if( Area( glyph->bounds ) > 0.0f ) {
			size_t base_index = vertices.size();

			Vec2 mins = glyph->bounds.mins - font->glyph_padding;
			Vec2 maxs = glyph->bounds.maxs + font->glyph_padding;

			// TODO: padding for antialiasing/borders!!!!!!!!!!!!!!!!!! very hard because size is in world space and not screen pixels now
			// note that world mins.y goes with uv maxs.y because world space is y-up and textures are y-down
			vertices.add( TextVertex {
				.position = cursor + mins.x * dx + mins.y * dy,
				.uv = Vec2( glyph->uv_bounds.mins.x, glyph->uv_bounds.maxs.y ),
			} );
			vertices.add( TextVertex {
				.position = cursor + maxs.x * dx + mins.y * dy,
				.uv = Vec2( glyph->uv_bounds.maxs.x, glyph->uv_bounds.maxs.y ),
			} );
			vertices.add( TextVertex {
				.position = cursor + mins.x * dx + maxs.y * dy,
				.uv = Vec2( glyph->uv_bounds.mins.x, glyph->uv_bounds.mins.y ),
			} );
			vertices.add( TextVertex {
				.position = cursor + maxs.x * dx + maxs.y * dy,
				.uv = Vec2( glyph->uv_bounds.maxs.x, glyph->uv_bounds.mins.y ),
			} );

			indices.add( base_index + 0 );
			indices.add( base_index + 3 );
			indices.add( base_index + 2 );

			indices.add( base_index + 0 );
			indices.add( base_index + 1 );
			indices.add( base_index + 3 );
		}

		cursor += dx * glyph->advance;
	}

	UniformBlock text_uniforms = UploadUniformBlock( color, color, font->dSDF_dTexel, 0 );

	VertexDescriptor vertex_descriptor = { };
	vertex_descriptor.attributes[ VertexAttribute_Position ] = VertexAttribute { VertexFormat_Floatx3, 0, offsetof( TextVertex, position ) };
	vertex_descriptor.attributes[ VertexAttribute_TexCoord ] = VertexAttribute { VertexFormat_Floatx2, 0, offsetof( TextVertex, uv ) };
	vertex_descriptor.buffer_strides[ 0 ] = sizeof( TextVertex );
	DynamicDrawData draw_data = UploadDynamicGeometry( vertices.span().cast< const u8 >(), indices.span(), vertex_descriptor );

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.nonworld_opaque_pass;
		pipeline.shader = &shaders.text;
		pipeline.cull_face = CullFace_Disabled;
		pipeline.alpha_to_coverage = true;
		pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
		pipeline.bind_uniform( "u_Text", text_uniforms );
		pipeline.bind_texture_and_sampler( "u_BaseTexture", font->material.texture, Sampler_Standard );

		DrawDynamicGeometry( pipeline, draw_data );
	}

	{
		for( u32 i = 0; i < frame_static.shadow_parameters.num_cascades; i++ ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.shadowmap_pass[ i ];
			pipeline.shader = &shaders.text_alphatest;
			pipeline.cull_face = CullFace_Disabled;
			pipeline.clamp_depth = true;
			pipeline.bind_uniform( "u_View", frame_static.shadowmap_view_uniforms[ i ] );
			pipeline.bind_uniform( "u_Text", text_uniforms );
			pipeline.bind_texture_and_sampler( "u_BaseTexture", font->material.texture, Sampler_Standard );

			DrawDynamicGeometry( pipeline, draw_data );
		}
	}
}
