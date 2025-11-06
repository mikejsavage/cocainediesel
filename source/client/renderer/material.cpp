#include "qcommon/base.h"
#include "qcommon/hash.h"
#include "qcommon/hashmap.h"
#include "qcommon/pool.h"
#include "qcommon/string.h"
#include "qcommon/span2d.h"
#include "qcommon/srgb.h"
#include "qcommon/threadpool.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/renderer/api.h"
#include "client/renderer/dds.h"
#include "client/renderer/shader.h"
#include "client/renderer/material.h"
#include "client/renderer/private.h"
#include "cgame/cg_dynamics.h"

#include "nanosort/nanosort.hpp"

#include "stb/stb_image.h"
#include "stb/stb_rect_pack.h"

struct MaterialSpecKey {
	Span< const char > keyword;
	void ( *func )( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data );
};

constexpr u32 MAX_SPRITES = 4096;
constexpr int SPRITE_ATLAS_SIZE = 2048;
constexpr int SPRITE_ATLAS_BLOCK_SIZE = SPRITE_ATLAS_SIZE / 4;

inline HashPool< Texture, MaxMaterials > textures; // NOTE(mike): this is inline so the backends can read it
static HashPool< Material2, MaxMaterials > materials;

static PoolHandle< Texture > missing_texture;
static PoolHandle< Material2 > missing_material;

static HashMap< Sprite, MAX_SPRITES > sprites;
static PoolHandle< Texture > sprite_atlas;
static PoolHandle< BindGroup > sprite_atlas_bind_group;

bool CompressedTextureFormat( TextureFormat format ) {
	switch( format ) {
		case TextureFormat_BC1_sRGB:
		case TextureFormat_BC3_sRGB:
		case TextureFormat_BC4:
		case TextureFormat_BC5:
			return true;

		default:
			return false;
	}
}

u32 BitsPerPixel( TextureFormat format ) {
	switch( format ) {
		case TextureFormat_R_U8: return 8;
		case TextureFormat_R_U8_sRGB: return 8;
		case TextureFormat_R_S8: return 8;
		case TextureFormat_R_UI8: return 8;

		case TextureFormat_A_U8: return 8;

		case TextureFormat_RA_U8: return 16;

		case TextureFormat_RGBA_U8: return 32;
		case TextureFormat_RGBA_U8_sRGB: return 32;

		case TextureFormat_BC1_sRGB: return 4;
		case TextureFormat_BC3_sRGB: return 8;
		case TextureFormat_BC4: return 4;
		case TextureFormat_BC5: return 8;

		default:
			Assert( false );
			return 0;
	}
}

u32 BlockSize( TextureFormat format ) {
	switch( format ) {
		case TextureFormat_BC1_sRGB:
		case TextureFormat_BC3_sRGB:
		case TextureFormat_BC4:
		case TextureFormat_BC5:
			return 4;

		default:
			return 1;
	}
}

static u32 MipmappedByteSize( u32 w, u32 h, u32 levels, TextureFormat format ) {
	u32 size = 0;

	for( u32 i = 0; i < levels; i++ ) {
		size += ( ( w >> i ) * ( h >> i ) * BitsPerPixel( format ) ) / 8;
	}

	return size;
}

static Span< const char > ParseMaterialToken( Span< const char > * data ) {
	Span< const char > token = ParseToken( data, Parse_StopOnNewLine );
	if( token == "" ) {
		return Span< const char >();
	}

	if( token == "}" ) {
		data->ptr--;
		data->n++;
		return Span< const char >();
	}

	return token;
}

static float ParseMaterialFloat( Span< const char > * data ) {
	return ParseFloat( data, 0.0f, Parse_StopOnNewLine );
}

static void ParseVector( Span< const char > * data, float * v, size_t n ) {
	for( size_t i = 0; i < n; i++ ) {
		v[ i ] = ParseMaterialFloat( data );
	}
}

static void SkipToEndOfLine( Span< const char > * data ) {
	while( true ) {
		Span< const char > token = ParseToken( data, Parse_StopOnNewLine );
		if( token == "" || token == "}" )
			break;
	}
}

static Wave ParseWave( Span< const char > * data ) {
	Wave wave = { };
	Span< const char > token = ParseMaterialToken( data );
	if( token == "sin" ) {
		wave.type = WaveFunc_Sin;
	}
	else if( token == "triangle" ) {
		wave.type = WaveFunc_Triangle;
	}
	else if( token == "sawtooth" ) {
		wave.type = WaveFunc_Sawtooth;
	}
	else if( token == "inversesawtooth" ) {
		wave.type = WaveFunc_InverseSawtooth;
	}

	ParseVector( data, wave.args, 4 );

	return wave;
}

static void ParseCull( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "front" ) {
		material->dynamic_state.cull_face = CullFace_Front;
	}
	else if( token == "disable" || token == "none" || token == "twosided" ) {
		material->dynamic_state.cull_face = CullFace_Disabled;
	}
}

static void ParseShaded( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	material->shaded = true;
	if( material->world ) {
		Com_GGPrint( S_COLOR_YELLOW "{}: world and shaded don't mix", name );
	}
}

static void ParseWorld( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	material->world = true;
	if( material->shaded ) {
		Com_GGPrint( S_COLOR_YELLOW "{}: world and shaded don't mix", name );
	}
}

static void ParseSpecular( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	material->properties.specular = ParseMaterialFloat( data );
}

static void ParseShininess( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	material->properties.shininess = ParseMaterialFloat( data );
}

static void ParseBlendFunc( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "blend" ) {
		material->blend_func = BlendFunc_Blend;
	}
	else if( token == "add" ) {
		material->blend_func = BlendFunc_Add;
	}
	SkipToEndOfLine( data );
}

static Vec3 NormalizeColor( Vec3 color ) {
	float f = Max2( Max2( color.x, color.y ), color.z );
	return f > 1.0f ? color / f : color;
}

static void ParseRGBGen( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "wave" ) {
		material->rgbgen.type = ColorGenType_Wave;
		material->rgbgen.wave = ParseWave( data );
	}
	else if( token == "colorwave" ) {
		material->rgbgen.type = ColorGenType_Wave;
		ParseVector( data, material->rgbgen.args, 3 );
		material->rgbgen.wave = ParseWave( data );
	}
	else if( token == "entity" ) {
		material->rgbgen.type = ColorGenType_Entity;

		Span< const char > scale = ParseMaterialToken( data );
		if( scale != "" ) {
			material->rgbgen.args[ 0 ] = SpanToFloat( scale, 0.0f );
		}
	}
	else if( token == "entitycolorwave" ) {
		material->rgbgen.type = ColorGenType_EntityWave;
		ParseVector( data, material->rgbgen.args, 3 );
		material->rgbgen.wave = ParseWave( data );
	}
	else if( token == "const" ) {
		material->rgbgen.type = ColorGenType_Constant;
		Vec3 color;
		ParseVector( data, color.ptr(), 3 );
		color = NormalizeColor( color );
		material->rgbgen.args[ 0 ] = sRGBToLinear( color.x );
		material->rgbgen.args[ 1 ] = sRGBToLinear( color.y );
		material->rgbgen.args[ 2 ] = sRGBToLinear( color.z );
	}
}

static void ParseAlphaGen( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "entity" ) {
		material->alphagen.type = ColorGenType_Entity;
	}
	else if( token == "wave" ) {
		material->alphagen.type = ColorGenType_Wave;
		material->alphagen.wave = ParseWave( data );
	}
	else if( token == "const" ) {
		material->alphagen.type = ColorGenType_Constant;
		material->alphagen.args[0] = ParseMaterialFloat( data );
	}
}

static PoolHandle< Texture > FindTexture( Span< const char > name ) {
	/*
	 * if the lookup fails, reserve a slot in the texture pool but fill it with
	 * a dummy texture, then return missing_texture until something gets
	 * hotloaded over the dummy
	 */

	u64 hash = StringHash( name ).hash;
	Optional< PoolHandle< Texture > > handle = textures.get( hash );
	if( handle.exists ) {
		if( textures[ handle.value ].handle == BackendTexture( 0 ) )
			return missing_texture;
		return handle.value;
	}

	Optional< PoolHandle< Texture > > new_handle = textures.add( hash );
	if( !new_handle.exists )
		return missing_texture;
	textures[ new_handle.value ] = { };
	return new_handle.value;
}

static void ParseTexture( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( StartsWith( token, "." ) ) {
		TempAllocator temp = cls.frame_arena.temp();
		material->texture = FindTexture( temp.sv( "{}{}", BasePath( path ), token + 1 ) );
	}
	else {
		material->texture = FindTexture( token );
	}
}

static void SkipComment( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	SkipToEndOfLine( data );
}

static constexpr MaterialSpecKey shaderkeys[] = {
	{ "sprite", SkipComment }, // NOMERGE
	{ "alphagen", ParseAlphaGen },
	{ "blendfunc", ParseBlendFunc },
	{ "cull", ParseCull },
	{ "rgbgen", ParseRGBGen },
	{ "shaded", ParseShaded },
	{ "world", ParseWorld },
	{ "shininess", ParseShininess },
	{ "specular", ParseSpecular },
	{ "texture", ParseTexture },
	{ "//", SkipComment },
};

static void ParseMaterialKey( MaterialDescriptor * material, Span< const char > token, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	for( MaterialSpecKey key : shaderkeys ) {
		if( StrCaseEqual( token, key.keyword ) ) {
			key.func( material, name, path, data );
			return;
		}
	}

	Com_GGPrint( S_COLOR_YELLOW "Unrecognised material key ({}): {}", name, token );

	SkipToEndOfLine( data );
}

static bool ParseMaterial( MaterialDescriptor * material, Span< const char > name, Span< const char > path, Span< const char > * data ) {
	TracyZoneScoped;

	material->texture = FindTexture( MakeSpan( "white" ) );

	while( true ) {
		Span< const char > token = ParseToken( data, Parse_DontStopOnNewLine );
		if( token == "" ) {
			Com_GGPrint( S_COLOR_YELLOW "WARNING: hit end of file inside a material ({})", name );
			return false;
		}

		if( token == "}" ) {
			break;
		}
		else {
			ParseMaterialKey( material, token, name, path, data );
		}
	}

	return true;
}

static void UnloadTexture( Texture * texture ) {
	stbi_image_free( texture->stb_data );
	texture->stb_data = NULL;
	texture->bc4_data = Span< const BC4Block >();
}

static void UnloadTexture( PoolHandle< Texture > texture ) {
	UnloadTexture( &textures[ texture ] );
}

static Material2 MaterialFromDescriptor( Span< const char > name, const MaterialDescriptor & desc ) {
	RenderPass pass;
	PoolHandle< RenderPipeline > shader = shaders.standard; // TODO: shader from desc
	if( desc.blend_func == BlendFunc_Disabled ) {
		pass = desc.outlined ? RenderPass_NonworldOpaqueOutlined : RenderPass_NonworldOpaque;
	}
	else {
		pass = RenderPass_Transparent;
	}

	TempAllocator temp = cls.frame_arena.temp();
	GPUBuffer properties = NewBuffer( temp( "{} properties", name ), desc.properties );

	return Material2 {
		.name = name,
		.render_pass = pass,
		.shader = shader,
		.bind_group = NewMaterialBindGroup( temp( "{}", name ), textures[ desc.texture ].handle, desc.sampler, properties ),
		.texture = desc.texture,
		.dynamic_state = desc.dynamic_state,
		.rgbgen = desc.rgbgen,
		.alphagen = desc.alphagen,
	};
}

static Optional< PoolHandle< Material2 > > AddMaterial( Span< const char > name, const MaterialDescriptor & descriptor ) {
	u64 hash = Hash64( name );

	Optional< PoolHandle< Material2 > > old_handle = materials.get( hash );
	if( old_handle.exists ) {
		materials[ old_handle.value ] = MaterialFromDescriptor( name, descriptor );
		return old_handle;
	}

	Optional< PoolHandle< Material2 > > new_handle = materials.add( hash );
	if( new_handle.exists ) {
		materials[ new_handle.value ] = MaterialFromDescriptor( name, descriptor );
		return new_handle;
	}

	Com_Printf( S_COLOR_YELLOW "Too many materials!\n" );
	return NONE;
}

static Texture MakeTexture( const TextureConfig & config, u64 hash, Optional< PoolHandle< Texture > > old_texture ) {
	return Texture {
		.name = config.name,
		.hash = hash,
		.handle = NewBackendTexture( config, old_texture.exists ? MakeOptional( textures[ old_texture.value ].handle ) : NONE ),
		.format = config.format,
		.width = config.width,
		.height = config.height,
		.depth = config.num_layers,
		.num_mipmaps = config.num_mipmaps,
	};
}

PoolHandle< BindGroup > NewMaterialBindGroup( const char * name, PoolHandle< Texture > texture, SamplerType sampler, MaterialProperties properties ) {
	TempAllocator temp = cls.frame_arena.temp();
	return NewMaterialBindGroup( name, textures[ texture ].handle, sampler, NewBuffer( temp( "{} properties", name ), properties ) );
}

// NOMERGE unify this and addtexture
PoolHandle< Texture > NewTexture( const TextureConfig & config, Optional< PoolHandle< Texture > > old_texture ) {
	Assert( config.name != "" );

	u64 hash = Hash64( config.name );

	Optional< PoolHandle< Texture > > handle = old_texture.exists ? old_texture : textures.get( hash );
	if( !handle.exists ) {
		handle = textures.add( hash );
		if( !handle.exists ) {
			Com_Printf( S_COLOR_YELLOW "Too many textures!\n" );
			return missing_texture;
		}
	}

	textures[ handle.value ] = MakeTexture( config, hash, old_texture );

	return handle.value;
}

TextureFormat GetTextureFormat( PoolHandle< Texture > texture ) {
	return textures[ texture ].format;
}

u32 TextureWidth( PoolHandle< Material2 > material ) {
	return textures[ materials[ material ].texture ].width;
}

u32 TextureHeight( PoolHandle< Material2 > material ) {
	return textures[ materials[ material ].texture ].height;
}

[[nodiscard]] static Optional< PoolHandle< Texture > > AddTexture( const TextureConfig & config ) {
	TracyZoneScoped;

	u64 hash = Hash64( config.name );

	Optional< PoolHandle< Texture > > handle;
	Optional< PoolHandle< Texture > > old_handle = textures.get( hash );
	if( old_handle.exists ) {
		if( CompressedTextureFormat( config.format ) && !CompressedTextureFormat( textures[ old_handle.value ].format ) ) {
			return NONE;
		}

		UnloadTexture( old_handle.value );
		handle = old_handle;
	}
	else {
		handle = textures.add( hash );
	}

	if( !handle.exists ) {
		Com_Printf( S_COLOR_YELLOW "Too many textures!\n" );
		return NONE;
	}

	textures[ handle.value ] = MakeTexture( config, hash, old_handle );

	return handle;
}

static void LoadSTBTexture( Span< const char > path, u8 * pixels, int w, int h, int channels, const char * failure_reason ) {
	TracyZoneScoped;
	TracyZoneSpan( path );

	if( pixels == NULL ) {
		Assert( failure_reason != NULL );
		Com_GGPrint( S_COLOR_YELLOW "WARNING: couldn't load texture from {}: {}", path, failure_reason );
		return;
	}

	constexpr TextureFormat formats[] = {
		TextureFormat_R_U8,
		TextureFormat_RA_U8,
		{ },
		TextureFormat_RGBA_U8_sRGB,
	};

	TextureConfig config = {
		.name = StripExtension( path ),
		.format = formats[ channels - 1 ],
		.width = checked_cast< u32 >( w ),
		.height = checked_cast< u32 >( h ),
		.data = pixels,
	};

	Optional< PoolHandle< Texture > > texture = AddTexture( config );
	if( texture.exists ) {
		textures[ texture.value ].stb_data = pixels;
	}
}

static void LoadDDSTexture( Span< const char > path ) {
	Span< const u8 > dds = AssetBinary( path );
	if( dds.num_bytes() < sizeof( DDSHeader ) ) {
		Com_GGPrint( S_COLOR_YELLOW "{} is too small to be a DDS file", path );
		return;
	}

	const DDSHeader * header = align_cast< const DDSHeader >( dds.ptr );
	if( header->magic != DDSMagic ) {
		Com_GGPrint( S_COLOR_YELLOW "{} isn't a DDS file", path );
		return;
	}

	if( header->width % 4 != 0 || header->height % 4 != 0 ) {
		Com_GGPrint( S_COLOR_YELLOW "{} dimensions must be a multiple of 4 ({}x{})", path, header->width, header->height );
		return;
	}

	TextureConfig config = {
		.name = StripExtension( path ),
		.width = header->width,
		.height = header->height,
		.data = dds.ptr + sizeof( DDSHeader ),
		.num_mipmaps = Max2( header->mipmap_count, u32( 1 ) ),
	};

	switch( header->format ) {
		case DDSTextureFormat_BC1:
			config.format = TextureFormat_BC1_sRGB;
			break;

		case DDSTextureFormat_BC3:
			config.format = TextureFormat_BC3_sRGB;
			break;

		case DDSTextureFormat_BC4:
			config.format = TextureFormat_BC4;
			break;

		case DDSTextureFormat_BC5:
			config.format = TextureFormat_BC5;
			break;

		default:
			Com_GGPrint( S_COLOR_YELLOW "{} isn't a BC format ({})", path, header->format );
			return;
	}

	size_t expected_data_size = MipmappedByteSize( config.width, config.height, config.num_mipmaps, config.format );
	if( dds.num_bytes() - sizeof( DDSHeader ) != expected_data_size ) {
		Com_GGPrint( S_COLOR_YELLOW "{} has bad data size. Got {}, expected {}", path, dds.num_bytes() - sizeof( DDSHeader ), expected_data_size );
		return;
	}

	Optional< PoolHandle< Texture > > texture = AddTexture( config );
	if( texture.exists ) {
		textures[ texture.value ].bc4_data = ( dds + sizeof( DDSHeader ) ).cast< const BC4Block >();
	}
}

static void LoadMaterialFile( Span< const char > path ) {
	Span< const char > data = AssetString( path );

	while( data != "" ) {
		Span< const char > name = ParseToken( &data, Parse_DontStopOnNewLine );
		if( name == "" )
			break;

		ParseToken( &data, Parse_DontStopOnNewLine );

		MaterialDescriptor material = { };
		if( !ParseMaterial( &material, name, path, &data ) )
			break;
		AddMaterial( name, material );
	}
}

struct DecodeSTBTextureJob {
	struct {
		Span< const char > path;
		Span< const u8 > data;
		bool atlased;
	} in;

	struct {
		int width, height;
		int channels;
		u8 * pixels;
		Span< const char > failure_reason;
	} out;
};

struct SpriteAtlasLayer {
	BC4Block blocks[ SPRITE_ATLAS_BLOCK_SIZE * SPRITE_ATLAS_BLOCK_SIZE ];
};

static BC4Block FastBC4( Span2D< const RGBA8 > rgba ) {
	BC4Block result;

	result.endpoints[ 0 ] = 255;
	result.endpoints[ 1 ] = 0;

	constexpr u8 index_lut[] = { 1, 7, 6, 5, 4, 3, 2, 0 };

	u64 indices = 0;
	for( size_t i = 0; i < 16; i++ ) {
		u64 index = index_lut[ rgba( i % 4, i / 4 ).a >> 5 ];
		indices |= index << ( i * 3 );
	}

	memcpy( result.indices, &indices, sizeof( result.indices ) );

	return result;
}

static Span2D< BC4Block > RGBAToBC4( Span2D< const RGBA8 > rgba ) {
	TracyZoneScoped;

	Span2D< BC4Block > bc4 = AllocSpan2D< BC4Block >( sys_allocator, rgba.w / 4, rgba.h / 4 );

	for( u32 row = 0; row < bc4.h; row++ ) {
		for( u32 col = 0; col < bc4.w; col++ ) {
			Span2D< const RGBA8 > rgba_block = rgba.slice( col * 4, row * 4, 4, 4 );
			bc4( col, row ) = FastBC4( rgba_block );
		}
	}

	return bc4;
}

static Span2D< const BC4Block > GetMipmap( const Texture * texture, u32 mipmap ) {
	u32 w = texture->width / 4;
	u32 h = texture->height / 4;

	Span< const BC4Block > cursor = texture->bc4_data;
	for( u32 i = 0; i < mipmap; i++ ) {
		cursor += ( w >> i ) * ( h >> i );
	}

	u32 mip_w = w >> mipmap;
	u32 mip_h = h >> mipmap;

	return Span2D< const BC4Block >( cursor.slice( 0, mip_w * mip_h ).ptr, mip_w, mip_h );
}

static Vec4 TrimTexture( Span2D< const BC4Block > bc4 ) {
	TracyZoneScoped;

	u32 min_x = bc4.w;
	u32 min_y = bc4.h;
	u32 max_x = 0;
	u32 max_y = 0;

	for( u32 y = 0; y < bc4.h; y++ ) {
		for( u32 x = 0; x < bc4.w; x++ ) {
			const BC4Block & block = bc4( x, y );
			if( block.endpoints[ 0 ] == 0 && block.endpoints[ 1 ] == 0 )
				continue;

			min_x = Min2( min_x, x );
			min_y = Min2( min_y, y );
			max_x = Max2( max_x, x );
			max_y = Max2( max_y, y );
		}
	}

	Vec4 trim = Vec4(
		float( min_x ) / float( bc4.w ),
		float( min_y ) / float( bc4.h ),
		float( max_x - min_x + 1 ) / float( bc4.w ),
		float( max_y - min_y + 1 ) / float( bc4.h )
	);

	return trim;
}

static void PackSpriteAtlas() {
	TracyZoneScoped;

	sprites.clear();

	// make a list of textures to be packed
	stbrp_rect rects[ MAX_SPRITES ];
	size_t num_sprites = 0;

	u32 num_mipmaps = U32_MAX;

	for( u32 i = 0; i < textures.span().n; i++ ) {
		const Texture * texture = &textures.span()[ i ];
		if( !texture->atlased )
			continue;

		if( texture->format != TextureFormat_RGBA_U8_sRGB && texture->format != TextureFormat_BC4 ) {
			Com_GGPrint( S_COLOR_YELLOW "Sprites must be RGBA or BC4 ({})", texture->name.span() );
			continue;
		}

		if( texture->width % 4 != 0 || texture->height % 4 != 0 ) {
			Com_GGPrint( S_COLOR_YELLOW "Sprite dimensions must be a multiple of 4 ({} is {}x{})", texture->name.span(), texture->width, texture->height );
			continue;
		}

		if( texture->num_mipmaps < 3 ) {
			Com_GGPrint( S_COLOR_YELLOW "{} has a small number of mipmaps ({}) and will mess up the sprite atlas", texture->name.span(), texture->num_mipmaps );
		}

		if( num_sprites == ARRAY_COUNT( rects ) ) {
			Com_GGPrint( S_COLOR_YELLOW "Too many sprites!" );
			break;
		}

		stbrp_rect * rect = &rects[ num_sprites ];
		num_sprites++;

		rect->id = i;
		rect->w = texture->width;
		rect->h = texture->height;

		num_mipmaps = Min2( texture->num_mipmaps, num_mipmaps );
	}

	if( num_sprites == 0 ) {
		num_mipmaps = 1;
	}

	// rect packing
	u32 num_unpacked = num_sprites;
	u32 num_layers = 0;
	while( true ) {
		TracyZoneScopedN( "stb_rect_pack iteration" );

		stbrp_node nodes[ MAX_SPRITES ];
		stbrp_context packer;
		stbrp_init_target( &packer, SPRITE_ATLAS_SIZE, SPRITE_ATLAS_SIZE, nodes, ARRAY_COUNT( nodes ) );
		stbrp_setup_allow_out_of_mem( &packer, 1 );

		bool all_packed = stbrp_pack_rects( &packer, rects, num_unpacked ) == 1;
		bool none_packed = true;

		for( u32 i = 0; i < num_unpacked; i++ ) {
			if( !rects[ i ].was_packed )
				continue;
			none_packed = false;

			const Texture * texture = &textures.span()[ rects[ i ].id ];

			sprites.add( texture->hash, Sprite {
				.uvwh = Vec4(
					rects[ i ].x / float( SPRITE_ATLAS_SIZE ) + num_layers,
					rects[ i ].y / float( SPRITE_ATLAS_SIZE ),
					texture->width / float( SPRITE_ATLAS_SIZE ),
					texture->height / float( SPRITE_ATLAS_SIZE )
				),
			} );
		}

		num_layers++;
		if( all_packed )
			break;

		if( none_packed ) {
			Fatal( "Can't pack sprites" );
		}

		// repack rects array
		for( u32 i = 0; i < num_unpacked; i++ ) {
			if( !rects[ i ].was_packed )
				continue;

			num_unpacked--;
			Swap2( &rects[ num_unpacked ], &rects[ i ] );
			i--;
		}
	}

	// convert pngs to temporary bc4s
	for( u32 i = 0; i < num_sprites; i++ ) {
		Texture * texture = &textures.span()[ rects[ i ].id ];
		if( texture->format != TextureFormat_RGBA_U8_sRGB )
			continue;

		Span2D< const RGBA8 > rgba = Span2D< const RGBA8 >( ( const RGBA8 * ) texture->stb_data, texture->width, texture->height );
		Span2D< const BC4Block > bc4 = RGBAToBC4( rgba );
		texture->bc4_data = Span< const BC4Block >( bc4.ptr, bc4.w * bc4.h );
	}

	// copy texture data into atlases
	u32 num_blocks = 0;
	for( u32 i = 0; i < num_mipmaps; i++ ) {
		num_blocks += Square( SPRITE_ATLAS_BLOCK_SIZE >> i );
	}
	num_blocks *= num_layers;

	Span< BC4Block > blocks = AllocSpan< BC4Block >( sys_allocator, num_blocks );
	memset( blocks.ptr, 0, blocks.num_bytes() );
	defer { Free( sys_allocator, blocks.ptr ); };

	Span< BC4Block > cursor = blocks;

	for( u32 mipmap_idx = 0; mipmap_idx < num_mipmaps; mipmap_idx++ ) {
		u32 mipmap_dim = SPRITE_ATLAS_BLOCK_SIZE >> mipmap_idx;
		u32 mipmap_blocks = mipmap_dim * mipmap_dim * num_layers;
		Span< BC4Block > layer_mipmap = cursor.slice( 0, mipmap_blocks );
		cursor += mipmap_blocks;

		for( u32 i = 0; i < num_sprites; i++ ) {
			const Texture * texture = &textures.span()[ rects[ i ].id ];

			Sprite * sprite = sprites.get( texture->hash );
			Assert( sprite != NULL );

			u32 layer_idx = u32( sprite->uvwh.x );
			Span2D< BC4Block > layer( layer_mipmap + mipmap_dim * mipmap_dim * layer_idx, mipmap_dim, mipmap_dim );

			Span2D< const BC4Block > bc4 = GetMipmap( texture, mipmap_idx );

			if( mipmap_idx == 0 ) {
				sprite->trim = TrimTexture( bc4 );
			}

			u32 mipped_x = rects[ i ].x >> mipmap_idx;
			u32 mipped_y = rects[ i ].y >> mipmap_idx;
			Assert( mipped_x % 4 == 0 && mipped_y % 4 == 0 );
			CopySpan2D( layer.slice( mipped_x / 4, mipped_y / 4, bc4.w, bc4.h ), bc4 );
		}
	}

	// free temporary bc4s
	for( size_t i = 0; i < sprites.size(); i++ ) {
		Texture * texture = &textures.span()[ rects[ i ].id ];
		const Material2 * material = &materials.span()[ rects[ i ].id ];
		if( GetTextureFormat( material->texture ) != TextureFormat_RGBA_U8_sRGB )
			continue;

		Free( sys_allocator, const_cast< BC4Block * >( texture->bc4_data.ptr ) );
		texture->bc4_data = Span< const BC4Block >();
	}

	// upload atlases
	{
		TracyZoneScopedN( "Upload atlas" );

		sprite_atlas = NewTexture( TextureConfig {
			.name = "Sprite atlas",
			.format = TextureFormat_BC4,
			.width = SPRITE_ATLAS_SIZE,
			.height = SPRITE_ATLAS_SIZE,
			.num_layers = num_layers,
			.num_mipmaps = num_mipmaps,
			.data = blocks.ptr,
		}, sprite_atlas );
	}
}

static void LoadBuiltinMaterials() {
	TracyZoneScoped;

	{
		constexpr RGBA8 pixels[] = {
			RGBA8( 255, 0, 255, 255 ),
			RGBA8( 0, 0, 0, 255 ),
			RGBA8( 255, 255, 255, 255 ),
			RGBA8( 255, 0, 255, 255 ),
		};

		missing_texture = NewTexture( TextureConfig {
			.name = "Missing texture",
			.format = TextureFormat_RGBA_U8_sRGB,
			.width = 2,
			.height = 2,
			.data = pixels,
		} );
	}

	missing_material = Unwrap( AddMaterial( "missing", MaterialDescriptor {
		.outlined = false,
		.texture = missing_texture,
	} ) );

	{
		u8 white = 255;
		TextureConfig config = TextureConfig {
			.name = "white",
			.format = TextureFormat_R_U8,
			.width = 1,
			.height = 1,
			.data = &white,
		};

		Unwrap( AddTexture( config ) );
	}

	{
		MaterialDescriptor world_material = MaterialDescriptor {
			.rgbgen = { .args = { 0.17f, 0.17f, 0.17f, 1.0f } },
			.world = true,
			.properties = {
				.specular = 3.0f,
				.shininess = 8.0f,
			},
		};

		MaterialDescriptor wallbang_material = MaterialDescriptor {
			.rgbgen = {
				.args = {
					world_material.rgbgen.args[ 0 ] * 0.45f,
					world_material.rgbgen.args[ 1 ] * 0.45f,
					world_material.rgbgen.args[ 2 ] * 0.45f,
					1.0f,
				},
			},
			.world = true,
			.properties = world_material.properties,
		};

		Unwrap( AddMaterial( "editor/world", world_material ) );
		Unwrap( AddMaterial( "world", world_material ) );

		Unwrap( AddMaterial( "editor/wallbangable", wallbang_material ) );
		Unwrap( AddMaterial( "wallbangable", wallbang_material ) );

		// for use in models, wallbangable is for collision geometry
		Unwrap( AddMaterial( "wallbang_visible", wallbang_material ) );
	}

	Unwrap( AddMaterial( "textures/editor/glass", MaterialDescriptor {
		.rgbgen = { .args = { 0.0f, 0.35f, 0.8f } },
		.world = true,
		.properties = {
			.specular = 100.0f,
			.shininess = 100.0f,
		},
	} ) );
}

static bool ParseCDTextureAtlased( Span< const char > texture_path ) {
	TempAllocator temp = cls.frame_arena.temp();

	Span< const char > cdtexture_path = temp.sv( "{}.cdtexture", StripExtension( texture_path ) );
	Span< const char > cdtexture = AssetString( cdtexture_path );

	// TODO: parse this properly
	if( cdtexture == "" || cdtexture == "\n" )
		return false;
	if( cdtexture == "atlased" || cdtexture == "atlased\n" )
		return true;

	Com_GGPrint( S_COLOR_YELLOW "Bad cdtexture: {}", cdtexture_path );
	return false;
}

void InitMaterials() {
	TracyZoneScoped;

	textures.clear();
	materials.clear();

	LoadBuiltinMaterials();

	{
		TracyZoneScopedN( "Load disk textures" );

		DynamicArray< DecodeSTBTextureJob > jobs( sys_allocator );
		{
			TracyZoneScopedN( "Build job list" );

			for( Span< const char > path : AssetPaths() ) {
				Span< const char > ext = FileExtension( path );

				if( ext == ".png" || ext == ".jpg" ) {
					DecodeSTBTextureJob job;
					job.in.path = path;
					job.in.data = AssetBinary( path );

					jobs.add( job );
				}

				if( ext == ".dds" ) {
					LoadDDSTexture( path );
				}
			}

			nanosort( jobs.begin(), jobs.end(), []( const DecodeSTBTextureJob & a, const DecodeSTBTextureJob & b ) {
				return a.in.data.n > b.in.data.n;
			} );
		}

		ParallelFor( jobs.span(), []( TempAllocator * temp, void * data ) {
			DecodeSTBTextureJob * job = ( DecodeSTBTextureJob * ) data;

			TracyZoneScopedN( "stbi_load_from_memory" );
			TracyZoneSpan( job->in.path );

			job->out.pixels = stbi_load_from_memory( job->in.data.ptr, job->in.data.num_bytes(), &job->out.width, &job->out.height, &job->out.channels, 0 );

			if( job->out.channels == 3 ) {
				TracyZoneScopedN( "RGB -> RGBA" );

				// stb_image uses sys_allocator so this is ok
				size_t num_pixels = checked_cast< size_t >( job->out.width * job->out.height );
				RGBA8 * rgba_pixels = AllocMany< RGBA8 >( sys_allocator, num_pixels );
				for( size_t i = 0; i < num_pixels; i++ ) {
					rgba_pixels[ i ] = RGBA8(
						job->out.pixels[ i * 3 + 0 ],
						job->out.pixels[ i * 3 + 1 ],
						job->out.pixels[ i * 3 + 2 ],
						255
					);
				}

				stbi_image_free( job->out.pixels );
				job->out.pixels = ( u8 * ) rgba_pixels;
				job->out.channels = 4;
			}

		} );

		for( DecodeSTBTextureJob job : jobs ) {
			LoadSTBTexture( job.in.path, job.out.pixels, job.out.width, job.out.height, job.out.channels, stbi_failure_reason() );
		}
	}

	{
		TracyZoneScopedN( "Load materials" );

		for( Span< const char > path : AssetPaths() ) {
			if( FileExtension( path ) == ".cdmaterial" ) {
				LoadMaterialFile( path );
			}
		}
	}

	{
		PackSpriteAtlas();
		GPUBuffer buffer = NewBuffer( "Sprite atlas properties", MaterialProperties { } );
		sprite_atlas_bind_group = NewMaterialBindGroup( "Sprite atlas", textures[ sprite_atlas ].handle, Sampler_Standard, buffer );
	}
}

void HotloadMaterials() {
	TracyZoneScoped;

	bool changes = false;

	for( Span< const char > path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );

		if( ext == ".png" || ext == ".jpg" ) {
			Span< const u8 > data = AssetBinary( path );

			int w, h, channels;
			u8 * pixels;
			{
				TracyZoneScopedN( "stbi_load_from_memory" );
				TracyZoneSpan( path );
				pixels = stbi_load_from_memory( data.ptr, data.num_bytes(), &w, &h, &channels, 0 );
			}

			LoadSTBTexture( path, pixels, w, h, channels, stbi_failure_reason() );

			changes = true;
		}

		if( ext == ".dds" ) {
			LoadDDSTexture( path );
			changes = true;
		}
	}

	for( Span< const char > path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".cdmaterial" ) {
			LoadMaterialFile( path );
			changes = true;
		}
	}

	if( changes ) {
		PackSpriteAtlas();
	}
}

void ShutdownMaterials() {
	for( Texture & texture : textures.span() ) {
		UnloadTexture( &texture );
	}

	// for( u32 i = 0; i < materials_hashtable.size(); i++ ) {
	// 	Free( sys_allocator, materials[ i ].name.ptr );
	// }
	// Free( sys_allocator, missing_material.name.ptr );
}

Optional< PoolHandle< Material2 > > TryFindMaterial( StringHash name ) {
	return materials.get( name.hash );
}

PoolHandle< Material2 > FindMaterial( StringHash name ) {
	return Default( TryFindMaterial( name ), missing_material );
}

PoolHandle< Material2 > FindMaterial( const char * name ) {
	return FindMaterial( StringHash( name ) );
}

Optional< Sprite > TryFindSprite( StringHash name ) {
	const Sprite * sprite = sprites.get( name.hash );
	return sprite == NULL ? NONE : MakeOptional( *sprite );
}

PoolHandle< Texture > SpriteAtlasTexture() {
	return sprite_atlas;
}

PoolHandle< BindGroup > SpriteAtlasBindGroup() {
	return sprite_atlas_bind_group;
}

Vec2 HalfPixelSize( PoolHandle< Material2 > material ) {
	return 0.5f / Vec2( TextureWidth( material ), TextureHeight( material ) );
}

RenderPass MaterialRenderPass( PoolHandle< Material2 > material ) {
	return materials[ material ].render_pass;
}

PoolHandle< BindGroup > MaterialBindGroup( PoolHandle< Material2 > material ) {
	return materials[ material ].bind_group;
}

PipelineState MaterialPipelineState( PoolHandle< Material2 > handle ) {
	const Material2 & material = materials[ handle ];
	return PipelineState {
		.shader = material.shader,
		.dynamic_state = material.dynamic_state,
		.material_bind_group = material.bind_group,
	};
}

static float EvaluateWaveFunc( Wave wave ) {
	float t = PositiveMod( ( cls.gametime % 1000 ) / 1000.0f * wave.args[ 3 ] + wave.args[ 2 ], 1.0f );
	float v = 0.0f;
	switch( wave.type ) {
		case WaveFunc_Sin:
			 v = sinf( t * PI * 2.0f );
			 break;

		case WaveFunc_Triangle:
			 v = t < 0.5f ? t * 4.0f - 1.0f : 1.0f - ( t - 0.5f ) * 4.0f;
			 break;

		case WaveFunc_Sawtooth:
			 v = t;
			 break;

		case WaveFunc_InverseSawtooth:
			 v = 1 - t;
			 break;
	}

	return wave.args[ 0 ] + wave.args[ 1 ] * v;
}

Vec4 EvaluateMaterialColor( PoolHandle< Material2 > handle, Vec4 entity_color ) {
	const Material2 & material = materials[ handle ];

	Vec4 color = entity_color;
	switch( material.rgbgen.type ) {
		case ColorGenType_Constant:
			color.x = material.rgbgen.args[ 0 ];
			color.y = material.rgbgen.args[ 1 ];
			color.z = material.rgbgen.args[ 2 ];
			break;

		case ColorGenType_Entity:
			color = Vec4( entity_color.xyz() * material.rgbgen.args[ 0 ], entity_color.w );
			break;

		case ColorGenType_Wave:
			color = Vec4( Vec3( EvaluateWaveFunc( material.rgbgen.wave ) ), entity_color.w );
			break;

		case ColorGenType_EntityWave:
			color = Vec4( entity_color.xyz() + EvaluateWaveFunc( material.rgbgen.wave ), entity_color.w );
			break;
	}

	switch( material.alphagen.type ) {
		case ColorGenType_Constant:
			color.w = material.alphagen.args[ 0 ];
			break;

		case ColorGenType_Entity:
			break;

		case ColorGenType_Wave:
		case ColorGenType_EntityWave: {
			float wave = EvaluateWaveFunc( material.alphagen.wave );
			color.w = ( material.alphagen.type == ColorGenType_EntityWave ? color.w : 0.0f ) + wave;
		} break;
	}

	return color;
}

// PipelineState MaterialToPipelineState( const Material & material, Vec4 entity_color, bool skinned ) {
// 	TracyZoneScoped;
//
// 	PipelineState pipeline;
// 	if( material->blend_func == BlendFunc_Disabled ) {
// 		pipeline.pass = material->outlined ? frame_static.nonworld_opaque_outlined_pass : frame_static.nonworld_opaque_pass;
// 	}
// 	else {
// 		pipeline.pass = frame_static.transparent_pass;
// 	}
//
// 	if( material->blend_func != BlendFunc_Disabled ) {
// 		pipeline.write_depth = false;
// 	}
//
// 	pipeline.bind_texture_and_sampler( "u_BaseTexture", material->texture, Sampler_Standard );
//
// 	else if( skinned ) {
// 		if( material->shaded ) {
// 			pipeline.shader = &shaders.standard_skinned_shaded;
// 			AddDynamicsToPipeline( &pipeline );
// 		}
// 		else {
// 			pipeline.shader = &shaders.standard_skinned;
// 		}
// 	}
// 	else {
// 		if( material->shaded ) {
// 			pipeline.shader = &shaders.standard_shaded_instanced;
// 			AddDynamicsToPipeline( &pipeline );
// 		}
// 		else {
// 			pipeline.shader = &shaders.standard_instanced;
// 		}
// 	}
//
// 	return pipeline;
// }
