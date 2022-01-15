#include <algorithm> // std::sort

#include "qcommon/base.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "qcommon/span2d.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/threadpool.h"
#include "client/renderer/renderer.h"
#include "client/renderer/dds.h"
#include "cgame/cg_dynamics.h"

#include "stb/stb_image.h"
#include "stb/stb_rect_pack.h"

struct MaterialSpecKey {
	const char * keyword;
	void ( *func )( Material * material, Span< const char > name, Span< const char > * data );
};

constexpr u32 MAX_TEXTURES = 4096;
constexpr u32 MAX_MATERIALS = 4096;

constexpr u32 MAX_DECALS = 4096;
constexpr int DECAL_ATLAS_SIZE = 2048;
constexpr int DECAL_ATLAS_BLOCK_SIZE = DECAL_ATLAS_SIZE / 4;

static Texture textures[ MAX_TEXTURES ];
static void * texture_stb_data[ MAX_TEXTURES ];
static Span< const BC4Block > texture_bc4_data[ MAX_TEXTURES ];
static u32 num_textures;
static Hashtable< MAX_TEXTURES * 2 > textures_hashtable;

static Texture missing_texture;
static Material missing_material;

static Material materials[ MAX_MATERIALS ];
static u32 num_materials;
static Hashtable< MAX_MATERIALS * 2 > materials_hashtable;

Material world_material;
Material wallbang_material;

static Vec4 decal_uvwhs[ MAX_DECALS ];
static u32 num_decals;
static Hashtable< MAX_DECALS * 2 > decals_hashtable;
static TextureArray decals_atlases;

static UniformBlock material_static_uniforms[ MAX_MATERIALS ];
static Hashtable< MAX_MATERIALS * 2 > material_static_uniforms_hashtable;
static u32 num_material_static_uniforms;

void ClearMaterialStaticUniforms() {
	material_static_uniforms_hashtable.clear();
	num_material_static_uniforms = 0;
}

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
		case TextureFormat_BC1_sRGB:
			return 4;

		case TextureFormat_BC3_sRGB:
			return 8;

		case TextureFormat_BC4:
			return 4;

		case TextureFormat_BC5:
			return 8;

		default:
			assert( false );
			return 0;
	}
}

static u32 MipmappedByteSize( u32 w, u32 h, u32 levels, TextureFormat format ) {
	u32 size = 0;

	for( u32 i = 0; i < levels; i++ ) {
		size += ( ( w >> i ) * ( h >> i ) * BitsPerPixel( format ) ) / 8;
	}

	return size;
}

static u64 HashMaterialName( Span< const char > name ) {
	// skip leading /
	while( name != "" && name[ 0 ] == '/' )
		name++;
	return Hash64( name );
}

static u64 HashMaterialName( const char * str ) {
	return HashMaterialName( MakeSpan( str ) );
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

static void ParseCull( Material * material, Span< const char > name, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "disable" || token == "none" || token == "twosided" ) {
		material->double_sided = true;
	}
}

static void ParseDecal( Material * material, Span< const char > name, Span< const char > * data ) {
	material->decal = true;
}

static void ParseMaskOutlines( Material * material, Span< const char > name, Span< const char > * data ) {
	material->mask_outlines = true;
}

static void ParseShaded( Material * material, Span< const char > name, Span< const char > * data ) {
	material->shaded = true;
}

static void ParseSpecular( Material * material, Span< const char > name, Span< const char > * data ) {
	material->specular = ParseMaterialFloat( data );
}

static void ParseShininess( Material * material, Span< const char > name, Span< const char > * data ) {
	material->shininess = ParseMaterialFloat( data );
}

static const MaterialSpecKey shaderkeys[] = {
	{ "cull", ParseCull },
	{ "decal", ParseDecal },
	{ "maskoutlines", ParseMaskOutlines },
	{ "shaded", ParseShaded },
	{ "specular", ParseSpecular },
	{ "shininess", ParseShininess },

	{ }
};

static void ParseBlendFunc( Material * material, Span< const char > name, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "blend" ) {
		material->blend_func = BlendFunc_Blend;
	}
	else if( token == "add" ) {
		material->blend_func = BlendFunc_Add;
	}
	SkipToEndOfLine( data );
}

static void ParseMap( Material * material, Span< const char > name, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	u64 idx;
	if( textures_hashtable.get( StringHash( token ).hash, &idx ) ) {
		material->texture = &textures[ idx ];
	}
}

static Vec3 NormalizeColor( Vec3 color ) {
	float f = Max2( Max2( color.x, color.y ), color.z );
	return f > 1.0f ? color / f : color;
}

static void ParseRGBGen( Material * material, Span< const char > name, Span< const char > * data ) {
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

static void ParseAlphaGen( Material * material, Span< const char > name, Span< const char > * data ) {
	Span< const char > token = ParseMaterialToken( data );
	if( token == "entity" ) {
		material->alphagen.type = ColorGenType_EntityWave;
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

static void ParseTCMod( Material * material, Span< const char > name, Span< const char > * data ) {
	if( material->tcmod.type != TCModFunc_None ) {
		Com_GGPrint( S_COLOR_YELLOW "WARNING: material {} has multiple tcmods", name );
		SkipToEndOfLine( data );
		return;
	}

	Span< const char > token = ParseMaterialToken( data );
	if( token == "rotate" ) {
		material->tcmod.args[0] = -ParseMaterialFloat( data );
		material->tcmod.type = TCModFunc_Rotate;
	}
	else if( token == "scroll" ) {
		ParseVector( data, material->tcmod.args, 2 );
		material->tcmod.type = TCModFunc_Scroll;
	}
	else if( token == "stretch" ) {
		material->tcmod.wave = ParseWave( data );
		material->tcmod.type = TCModFunc_Stretch;
	}
	else {
		SkipToEndOfLine( data );
	}
}

static const MaterialSpecKey shaderpasskeys[] = {
	{ "alphagen", ParseAlphaGen },
	{ "blendfunc", ParseBlendFunc },
	{ "map", ParseMap },
	{ "rgbgen", ParseRGBGen },
	{ "tcmod", ParseTCMod },

	{ }
};

static void ParseMaterialKey( Material * material, Span< const char > name, const MaterialSpecKey * keys, Span< const char > token, Span< const char > * data ) {
	for( const MaterialSpecKey * key = keys; key->keyword != NULL; key++ ) {
		if( StrCaseEqual( token, key->keyword ) ) {
			key->func( material, name, data );
			return;
		}
	}

	SkipToEndOfLine( data );
}

static void ParseMaterialPass( Material * material, Span< const char > name, Span< const char > * data ) {
	material->rgbgen.type = ColorGenType_Constant;
	material->rgbgen.args[ 0 ] = 1.0f;
	material->rgbgen.args[ 1 ] = 1.0f;
	material->rgbgen.args[ 2 ] = 1.0f;

	material->alphagen.type = ColorGenType_Constant;
	material->rgbgen.args[ 0 ] = 1.0f;

	while( true ) {
		Span< const char > token = ParseToken( data, Parse_DontStopOnNewLine );
		if( token == "" || token == "}" )
			break;

		ParseMaterialKey( material, name, shaderpasskeys, token, data );
	}
}

static bool ParseMaterial( Material * material, Span< const char > name, Span< const char > * data ) {
	ZoneScoped;

	bool seen_pass = false;
	while( true ) {
		Span< const char > token = ParseToken( data, Parse_DontStopOnNewLine );
		if( token == "" ) {
			Com_GGPrint( S_COLOR_YELLOW "WARNING: hit end of file inside a material ({})", name );
			return false;
		}

		if( token == "}" ) {
			break;
		}
		else if( token == "{" ) {
			if( !seen_pass ) {
				ParseMaterialPass( material, name, data );
				seen_pass = true;
			}
			else {
				Com_GGPrint( S_COLOR_YELLOW "WARNING: material {} has multiple passes", name );
				return false;
			}
		}
		else {
			ParseMaterialKey( material, name, shaderkeys, token, data );
		}
	}

	return true;
}

static void UnloadTexture( u64 idx ) {
	stbi_image_free( texture_stb_data[ idx ] );

	texture_stb_data[ idx ] = NULL;
	texture_bc4_data[ idx ] = Span< const BC4Block >();

	DeleteTexture( textures[ idx ] );
}

static u64 AddTexture( const char * name, u64 hash, const TextureConfig & config ) {
	ZoneScoped;

	assert( num_textures < ARRAY_COUNT( textures ) );

	u64 idx = num_textures;
	if( !textures_hashtable.get( hash, &idx ) ) {
		textures_hashtable.add( hash, num_textures );

		materials[ num_materials ] = Material();
		materials[ num_materials ].texture = &textures[ num_textures ];
		materials[ num_materials ].name = CopyString( sys_allocator, name );
		materials_hashtable.add( hash, num_materials );

		num_textures++;
		num_materials++;
	}
	else {
		if( CompressedTextureFormat( config.format ) && !CompressedTextureFormat( textures[ idx ].format ) ) {
			return U64_MAX;
		}

		UnloadTexture( idx );
	}

	textures[ idx ] = NewTexture( config );
	return idx;
}

static void LoadBuiltinTextures() {
	ZoneScoped;

	{
		u8 white = 255;

		TextureConfig config;
		config.width = 1;
		config.height = 1;
		config.data = &white;
		config.format = TextureFormat_R_U8;

		AddTexture( "$whiteimage", Hash64( "$whiteimage" ), config );
	}

	{
		u8 black = 0;

		TextureConfig config;
		config.width = 1;
		config.height = 1;
		config.data = &black;
		config.format = TextureFormat_R_U8;

		AddTexture( "$whiteimage", Hash64( "$blackimage" ), config );
	}

	{
		constexpr RGB8 pixels[] = {
			RGB8( 255, 0, 255 ),
			RGB8( 0, 0, 0 ),
			RGB8( 255, 255, 255 ),
			RGB8( 255, 0, 255 ),
		};

		TextureConfig config;
		config.width = 2;
		config.height = 2;
		config.data = pixels;
		config.filter = TextureFilter_Point;
		config.format = TextureFormat_RGB_U8;

		missing_texture = NewTexture( config );
	}
}

static void LoadSTBTexture( const char * path, u8 * pixels, int w, int h, int channels, const char * failure_reason ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	if( pixels == NULL ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't load texture from %s: %s\n", path, failure_reason );
		return;
	}

	constexpr TextureFormat formats[] = {
		TextureFormat_R_U8,
		TextureFormat_RA_U8,
		TextureFormat_RGB_U8_sRGB,
		TextureFormat_RGBA_U8_sRGB,
	};

	TextureConfig config;
	config.width = checked_cast< u32 >( w );
	config.height = checked_cast< u32 >( h );
	config.data = pixels;
	config.format = formats[ channels - 1 ];

	size_t idx = AddTexture( path, Hash64( StripExtension( path ) ), config );
	texture_stb_data[ idx ] = pixels;
}

static void LoadDDSTexture( const char * path ) {
	Span< const u8 > dds = AssetBinary( path );
	if( dds.num_bytes() < sizeof( DDSHeader ) ) {
		Com_GGPrint( S_COLOR_YELLOW "{} is too small to be a DDS file", path );
		return;
	}

	const DDSHeader * header = ( const DDSHeader * ) dds.ptr;

	TextureConfig config;
	config.width = header->width;
	config.height = header->height;
	config.data = dds.ptr + sizeof( DDSHeader );

	if( header->magic != DDSMagic ) {
		Com_GGPrint( S_COLOR_YELLOW "{} isn't a DDS file", path );
		return;
	}

	if( header->width % 4 != 0 || header->height % 4 != 0 ) {
		Com_GGPrint( S_COLOR_YELLOW "{} dimensions must be a multiple of 4 ({}x{})", path, header->width, header->height );
		return;
	}

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

	config.num_mipmaps = Max2( header->mipmap_count, u32( 1 ) );
	size_t expected_data_size = MipmappedByteSize( config.width, config.height, config.num_mipmaps, config.format );
	if( dds.num_bytes() - sizeof( DDSHeader ) != expected_data_size ) {
		Com_GGPrint( S_COLOR_YELLOW "{} has bad data size. Got {}, expected {}", path, dds.num_bytes() - sizeof( DDSHeader ), expected_data_size );
		return;
	}

	size_t idx = AddTexture( path, Hash64( StripExtension( path ) ), config );
	texture_bc4_data[ idx ] = ( dds + sizeof( DDSHeader ) ).cast< const BC4Block >();
}

static void LoadMaterialFile( const char * path ) {
	Span< const char > data = AssetString( path );

	while( data != "" ) {
		Span< const char > name = ParseToken( &data, Parse_DontStopOnNewLine );
		if( name == "" )
			break;

		u64 hash = HashMaterialName( name );
		ParseToken( &data, Parse_DontStopOnNewLine );

		Material material = Material();
		if( !ParseMaterial( &material, name, &data ) )
			break;

		u64 idx = num_materials;
		if( !materials_hashtable.get( hash, &idx ) ) {
			materials_hashtable.add( hash, idx );
			material.name = ( *sys_allocator )( "{}", name );
			num_materials++;
		}
		else {
			material.name = materials[ idx ].name;
		}

		material.hash = hash;
		materials[ idx ] = material;
	}
}

struct DecodeSTBTextureJob {
	struct {
		const char * path;
		Span< const u8 > data;
	} in;

	struct {
		int width, height;
		int channels;
		u8 * pixels;
		const char * failure_reason;
	} out;
};

struct DecalAtlasLayer {
	BC4Block blocks[ DECAL_ATLAS_BLOCK_SIZE * DECAL_ATLAS_BLOCK_SIZE ];
};

static BC4Block FastBC4( Span2D< const RGBA8 > rgba ) {
	BC4Block result;

	result.data[ 0 ] = 255;
	result.data[ 1 ] = 0;

	constexpr u8 selector_lut[] = { 1, 7, 6, 5, 4, 3, 2, 0 };

	u64 selectors = 0;
	for( size_t i = 0; i < 16; i++ ) {
		u64 selector = selector_lut[ rgba( i % 4, i / 4 ).a >> 5 ];
		selectors |= selector << ( i * 3 );
	}

	memcpy( &result.data[ 2 ], &selectors, 6 );

	return result;
}

static Span2D< BC4Block > RGBAToBC4( Span2D< const RGBA8 > rgba ) {
	ZoneScoped;

	Span2D< BC4Block > bc4 = ALLOC_SPAN2D( sys_allocator, BC4Block, rgba.w / 4, rgba.h / 4 );

	for( u32 row = 0; row < bc4.h; row++ ) {
		for( u32 col = 0; col < bc4.w; col++ ) {
			Span2D< const RGBA8 > rgba_block = rgba.slice( col * 4, row * 4, 4, 4 );
			bc4( col, row ) = FastBC4( rgba_block );
		}
	}

	return bc4;
}

static Span2D< const BC4Block > GetMipmap( const Material * material, u32 mipmap ) {
	u64 texture_idx = material->texture - textures;
	u32 w = material->texture->width / 4;
	u32 h = material->texture->height / 4;

	Span< const BC4Block > cursor = texture_bc4_data[ texture_idx ];
	for( u32 i = 0; i < mipmap; i++ ) {
		cursor += ( w >> i ) * ( h >> i );
	}

	u32 mip_w = w >> mipmap;
	u32 mip_h = h >> mipmap;

	return Span2D< const BC4Block >( cursor.slice( 0, mip_w * mip_h ).ptr, mip_w, mip_h );
}

static void PackDecalAtlas() {
	ZoneScoped;

	decals_hashtable.clear();

	// make a list of textures to be packed
	stbrp_rect rects[ MAX_DECALS ];
	num_decals = 0;

	u32 num_mipmaps = U32_MAX;

	for( u32 i = 0; i < num_materials; i++ ) {
		const Texture * texture = materials[ i ].texture;
		if( !materials[ i ].decal || texture == NULL )
			continue;

		if( texture->format != TextureFormat_RGBA_U8_sRGB && texture->format != TextureFormat_BC4 ) {
			Com_GGPrint( S_COLOR_YELLOW "Decals must be RGBA or BC4 ({})", materials[ i ].name );
			continue;
		}

		if( texture->width % 4 != 0 || texture->height % 4 != 0 ) {
			Com_GGPrint( S_COLOR_YELLOW "Decal dimensions must be a multiple of 4 ({} is {}x{})", materials[ i ].name, texture->width, texture->height );
			continue;
		}

		if( texture->num_mipmaps < 3 ) {
			Com_GGPrint( S_COLOR_YELLOW "{} has a small number of mipmaps ({}) and will mess up the decal atlas", materials[ i ].name, texture->num_mipmaps );
		}

		assert( num_decals < ARRAY_COUNT( rects ) );

		stbrp_rect * rect = &rects[ num_decals ];
		num_decals++;

		rect->id = i;
		rect->w = texture->width;
		rect->h = texture->height;

		num_mipmaps = Min2( texture->num_mipmaps, num_mipmaps );
	}

	// rect packing
	u32 num_unpacked = num_decals;
	u32 num_layers = 0;
	while( true ) {
		ZoneScopedN( "stb_rect_pack iteration" );

		stbrp_node nodes[ MAX_TEXTURES ];
		stbrp_context packer;
		stbrp_init_target( &packer, DECAL_ATLAS_SIZE, DECAL_ATLAS_SIZE, nodes, ARRAY_COUNT( nodes ) );
		stbrp_setup_allow_out_of_mem( &packer, 1 );

		bool all_packed = stbrp_pack_rects( &packer, rects, num_unpacked ) != 0;
		bool none_packed = true;

		static RGBA8 pixels[ DECAL_ATLAS_SIZE * DECAL_ATLAS_SIZE ];
		Span2D< RGBA8 > image( pixels, DECAL_ATLAS_SIZE, DECAL_ATLAS_SIZE );
		for( u32 i = 0; i < num_unpacked; i++ ) {
			if( !rects[ i ].was_packed )
				continue;
			none_packed = false;

			const Material * material = &materials[ rects[ i ].id ];

			size_t decal_idx = decals_hashtable.size();
			decals_hashtable.add( material->hash, decal_idx );
			decal_uvwhs[ decal_idx ].x = rects[ i ].x / float( DECAL_ATLAS_SIZE ) + num_layers;
			decal_uvwhs[ decal_idx ].y = rects[ i ].y / float( DECAL_ATLAS_SIZE );
			decal_uvwhs[ decal_idx ].z = material->texture->width / float( DECAL_ATLAS_SIZE );
			decal_uvwhs[ decal_idx ].w = material->texture->height / float( DECAL_ATLAS_SIZE );
		}

		num_layers++;
		if( all_packed )
			break;

		if( none_packed ) {
			Fatal( "Can't pack decals" );
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
	for( u32 i = 0; i < num_decals; i++ ) {
		const Material * material = &materials[ rects[ i ].id ];
		if( material->texture->format != TextureFormat_RGBA_U8_sRGB )
			continue;

		u64 texture_idx = material->texture - textures;
		Span2D< const RGBA8 > rgba = Span2D< const RGBA8 >( ( const RGBA8 * ) texture_stb_data[ texture_idx ], material->texture->width, material->texture->height );
		Span2D< const BC4Block > bc4 = RGBAToBC4( rgba );
		texture_bc4_data[ texture_idx ] = Span< const BC4Block >( bc4.ptr, bc4.w * bc4.h );
	}

	// copy texture data into atlases, convert RGBA to BC4 as needed
	u32 num_blocks = 0;
	for( u32 i = 0; i < num_mipmaps; i++ ) {
		num_blocks += Square( DECAL_ATLAS_BLOCK_SIZE >> i );
	}
	num_blocks *= num_layers;

	Span< BC4Block > blocks = ALLOC_SPAN( sys_allocator, BC4Block, num_blocks );
	memset( blocks.ptr, 0, blocks.num_bytes() );
	defer { FREE( sys_allocator, blocks.ptr ); };

	Span< BC4Block > cursor = blocks;

	for( u32 mipmap_idx = 0; mipmap_idx < num_mipmaps; mipmap_idx++ ) {
		u32 mipmap_dim = DECAL_ATLAS_BLOCK_SIZE >> mipmap_idx;
		u32 mipmap_blocks = mipmap_dim * mipmap_dim * num_layers;
		Span< BC4Block > layer_mipmap = cursor.slice( 0, mipmap_blocks );
		cursor += mipmap_blocks;

		for( u32 i = 0; i < num_decals; i++ ) {
			const Material * material = &materials[ rects[ i ].id ];
			u64 decal_idx;
			bool ok = decals_hashtable.get( material->hash, &decal_idx );
			assert( ok );

			u32 layer_idx = u32( decal_uvwhs[ decal_idx ].x );
			Span2D< BC4Block > layer( layer_mipmap + mipmap_dim * mipmap_dim * layer_idx, mipmap_dim, mipmap_dim );

			Span2D< const BC4Block > bc4 = GetMipmap( material, mipmap_idx );

			u32 mipped_x = rects[ i ].x >> mipmap_idx;
			u32 mipped_y = rects[ i ].y >> mipmap_idx;
			assert( mipped_x % 4 == 0 && mipped_y % 4 == 0 );
			CopySpan2D( layer.slice( mipped_x / 4, mipped_y / 4, bc4.w, bc4.h ), bc4 );
		}
	}

	// free temporary bc4s
	for( u32 i = 0; i < num_decals; i++ ) {
		const Material * material = &materials[ rects[ i ].id ];
		if( material->texture->format != TextureFormat_RGBA_U8_sRGB )
			continue;

		u64 texture_idx = material->texture - textures;
		FREE( sys_allocator, const_cast< BC4Block * >( texture_bc4_data[ texture_idx ].ptr ) );
		texture_bc4_data[ texture_idx ] = Span< const BC4Block >();
	}

	// upload atlases
	{
		ZoneScopedN( "Upload atlas" );

		DeleteTextureArray( decals_atlases );

		TextureArrayConfig config;
		config.width = DECAL_ATLAS_SIZE;
		config.height = DECAL_ATLAS_SIZE;
		config.num_mipmaps = num_mipmaps;
		config.layers = num_layers;
		config.data = blocks.ptr;
		config.format = TextureFormat_BC4;

		decals_atlases = NewTextureArray( config );
	}
}

void InitMaterials() {
	ZoneScoped;

	num_textures = 0;
	num_materials = 0;
	ClearMaterialStaticUniforms();

	world_material = Material();
	world_material.rgbgen.args[ 0 ] = 0.17f;
	world_material.rgbgen.args[ 1 ] = 0.17f;
	world_material.rgbgen.args[ 2 ] = 0.17f;
	world_material.rgbgen.args[ 3 ] = 1.0f;
	world_material.specular = 3.0f;
	world_material.shininess = 8.0f;

	wallbang_material = Material();
	wallbang_material.rgbgen.args[ 0 ] = 0.17f * 2.0f;
	wallbang_material.rgbgen.args[ 1 ] = 0.17f * 2.0f;
	wallbang_material.rgbgen.args[ 2 ] = 0.17f * 2.0f;
	wallbang_material.rgbgen.args[ 3 ] = 1.0f;
	wallbang_material.specular = 3.0f;
	wallbang_material.shininess = 8.0f;

	LoadBuiltinTextures();

	{
		ZoneScopedN( "Load disk textures" );

		DynamicArray< DecodeSTBTextureJob > jobs( sys_allocator );
		{
			ZoneScopedN( "Build job list" );

			for( const char * path : AssetPaths() ) {
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

			std::sort( jobs.begin(), jobs.end(), []( const DecodeSTBTextureJob & a, const DecodeSTBTextureJob & b ) {
				return a.in.data.n > b.in.data.n;
			} );
		}

		ParallelFor( jobs.span(), []( TempAllocator * temp, void * data ) {
			DecodeSTBTextureJob * job = ( DecodeSTBTextureJob * ) data;

			ZoneScopedN( "stbi_load_from_memory" );
			ZoneText( job->in.path, strlen( job->in.path ) );

			job->out.pixels = stbi_load_from_memory( job->in.data.ptr, job->in.data.num_bytes(), &job->out.width, &job->out.height, &job->out.channels, 0 );
		} );

		for( DecodeSTBTextureJob job : jobs ) {
			LoadSTBTexture( job.in.path, job.out.pixels, job.out.width, job.out.height, job.out.channels, stbi_failure_reason() );
		}
	}

	{
		ZoneScopedN( "Load materials" );

		for( const char * path : AssetPaths() ) {
			// game crashes if we load materials with no texture,
			// skip editor.shader until we convert asset pointers
			// to asset hashes
			if( FileExtension( path ) == ".shader" && FileName( path ) != "editor.shader" ) {
				LoadMaterialFile( path );
			}
		}
	}

	missing_material = Material();
	missing_material.texture = &missing_texture;

	PackDecalAtlas();
}

void HotloadMaterials() {
	ZoneScoped;

	bool changes = false;

	for( const char * path : ModifiedAssetPaths() ) {
		Span< const char > ext = FileExtension( path );

		if( ext == ".png" || ext == ".jpg" ) {
			Span< const u8 > data = AssetBinary( path );

			int w, h, channels;
			u8 * pixels;
			{
				ZoneScopedN( "stbi_load_from_memory" );
				ZoneText( path, strlen( path ) );
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

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".shader" && FileName( path ) != "editor.shader" ) {
			LoadMaterialFile( path );
			changes = true;
		}
	}

	if( changes ) {
		PackDecalAtlas();
	}
}

void ShutdownMaterials() {
	for( u32 i = 0; i < num_textures; i++ ) {
		UnloadTexture( i );
	}

	for( u32 i = 0; i < num_materials; i++ ) {
		FREE( sys_allocator, materials[ i ].name );
	}

	DeleteTexture( missing_texture );
	DeleteTextureArray( decals_atlases );
}

bool TryFindMaterial( StringHash name, const Material ** material ) {
	u64 idx;
	if( !materials_hashtable.get( name.hash, &idx ) )
		return false;
	*material = &materials[ idx ];
	return true;
}

const Material * FindMaterial( StringHash name, const Material * def ) {
	const Material * material;
	if( !TryFindMaterial( name, &material ) )
		return def != NULL ? def : &missing_material;
	return material;
}

const Material * FindMaterial( const char * name, const Material * def ) {
	return FindMaterial( StringHash( HashMaterialName( name ) ), def );
}

bool TryFindDecal( StringHash name, Vec4 * uvwh ) {
	u64 idx;
	if( !decals_hashtable.get( name.hash, &idx ) )
		return false;
	*uvwh = decal_uvwhs[ idx ];
	return true;
}

TextureArray DecalAtlasTextureArray() {
	return decals_atlases;
}

Vec2 HalfPixelSize( const Material * material ) {
	return 0.5f / Vec2( material->texture->width, material->texture->height );
}

static float EvaluateWaveFunc( Wave wave ) {
	float t = PositiveMod( ( cls.gametime % 1000 ) / 1000.0f * wave.args[ 3 ] + wave.args[ 2 ], 1.0f );
	float v = 0.0f;
	switch( wave.type ) {
		case WaveFunc_Sin:
			 v = sinf( t * PI * 2.0f );
			 break;

		case WaveFunc_Triangle:
			 v = t < 0.5 ? t * 4 - 1 : 1 - ( t - 0.5f ) * 4;
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

PipelineState MaterialToPipelineState( const Material * material, Vec4 color, bool skinned, GPUMaterial * gpu_material ) {
	if( material == &world_material || material == &wallbang_material ) {
		PipelineState pipeline;
		pipeline.shader = &shaders.world;
		pipeline.pass = frame_static.world_opaque_pass;
		pipeline.set_uniform( "u_Fog", frame_static.fog_uniforms );
		pipeline.set_texture( "u_BlueNoiseTexture", BlueNoiseTexture() );
		pipeline.set_uniform( "u_BlueNoiseTextureParams", frame_static.blue_noise_uniforms );
		color.x = material->rgbgen.args[ 0 ];
		color.y = material->rgbgen.args[ 1 ];
		color.z = material->rgbgen.args[ 2 ];
		pipeline.set_uniform( "u_MaterialStatic", UploadMaterialStaticUniforms( Vec2( 0.0f ), material->specular, material->shininess ) );
		pipeline.set_uniform( "u_MaterialDynamic", UploadMaterialDynamicUniforms( color, Vec3( 0.0f ), Vec3( 0.0f ) ) );
		pipeline.set_texture_array( "u_ShadowmapTextureArray", frame_static.shadowmap_texture_array );
		pipeline.set_uniform( "u_ShadowMaps", frame_static.shadow_uniforms );
		pipeline.set_texture_array( "u_DecalAtlases", DecalAtlasTextureArray() );
		AddDynamicsToPipeline( &pipeline );
		return pipeline;
	}

	if( material->mask_outlines ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.world_opaque_pass;
		pipeline.shader = &shaders.depth_only;
		return pipeline;
	}

	// evaluate rgbgen/alphagen
	if( material->rgbgen.type == ColorGenType_Constant ) {
		color.x = material->rgbgen.args[ 0 ];
		color.y = material->rgbgen.args[ 1 ];
		color.z = material->rgbgen.args[ 2 ];
	}
	else if( material->rgbgen.type == ColorGenType_Wave || material->rgbgen.type == ColorGenType_EntityWave ) {
		float wave = EvaluateWaveFunc( material->rgbgen.wave );
		if( material->rgbgen.type == ColorGenType_EntityWave ) {
			color.x += wave;
			color.y += wave;
			color.z += wave;
		}
		else {
			color.x = wave;
			color.y = wave;
			color.z = wave;
		}
	}

	if( material->alphagen.type == ColorGenType_Constant ) {
		color.w = material->rgbgen.args[ 0 ];
	}
	else if( material->alphagen.type == ColorGenType_Wave || material->alphagen.type == ColorGenType_EntityWave ) {
		float wave = EvaluateWaveFunc( material->rgbgen.wave );
		if( material->alphagen.type == ColorGenType_EntityWave ) {
			color.w += wave;
		}
		else {
			color.w = wave;
		}
	}

	// evaluate tcmod
	Vec3 tcmod_row0 = Vec3( 1.0f, 0.0f, 0.0f );
	Vec3 tcmod_row1 = Vec3( 0.0f, 1.0f, 0.0f );
	if( material->tcmod.type == TCModFunc_Scroll ) {
		float s = float( PositiveMod( double( material->tcmod.args[ 0 ] ) * double( cls.gametime / 1000.0 ), 1.0 ) );
		float t = float( PositiveMod( double( material->tcmod.args[ 1 ] ) * double( cls.gametime / 1000.0 ), 1.0 ) );
		tcmod_row0 = Vec3( 1, 0, s );
		tcmod_row1 = Vec3( 0, 1, t );
	}
	else if( material->tcmod.type == TCModFunc_Rotate ) {
		float degrees = float( PositiveMod( double( material->tcmod.args[ 0 ] ) * double( cls.gametime / 1000.0 ), 360.0 ) );
		float s = sinf( Radians( degrees ) );
		float c = cosf( Radians( degrees ) );
		// keep centered on (0.5, 0.5)
		tcmod_row0 = Vec3( c, -s, 0.5f * ( 1.0f + s - c ) );
		tcmod_row1 = Vec3( s, c, 0.5f * ( 1.0f - s - c ) );
	}
	else if( material->tcmod.type == TCModFunc_Stretch ) {
		float wave = EvaluateWaveFunc( material->rgbgen.wave );
		float scale = wave == 0 ? 1.0f : 1.0f / wave;
		// keep centered on (0.5, 0.5)
		float offset = 0.5f - 0.5f * scale;
		tcmod_row0 = Vec3( scale, 0, offset );
		tcmod_row1 = Vec3( 0, scale, offset );
	}

	PipelineState pipeline;
	pipeline.pass = material->blend_func == BlendFunc_Disabled ? frame_static.nonworld_opaque_pass : frame_static.transparent_pass;
	pipeline.cull_face = material->double_sided ? CullFace_Disabled : CullFace_Back;
	pipeline.blend_func = material->blend_func;

	if( material->blend_func != BlendFunc_Disabled ) {
		pipeline.write_depth = false;
	}

	pipeline.set_texture( "u_BaseTexture", material->texture );

	{
		u64 hash = Hash64( u64( material ) );

		u64 idx = num_material_static_uniforms;
		if( !material_static_uniforms_hashtable.get( hash, &idx ) ) {
			assert( num_material_static_uniforms < ARRAY_COUNT( material_static_uniforms ) );
			material_static_uniforms_hashtable.add( hash, num_material_static_uniforms );
			material_static_uniforms[ idx ] = UploadMaterialStaticUniforms( Vec2( material->texture->width, material->texture->height ), material->specular, material->shininess );
			num_material_static_uniforms++;
		}

		pipeline.set_uniform( "u_MaterialStatic", material_static_uniforms[ idx ] );
	}

	if( skinned || gpu_material == NULL ) {
		pipeline.set_uniform( "u_MaterialDynamic", UploadMaterialDynamicUniforms( color, tcmod_row0, tcmod_row1 ) );
	}
	if( gpu_material != NULL ) {
		// instanced matrial
		gpu_material->color = color;
		gpu_material->tcmod[ 0 ] = tcmod_row0;
		gpu_material->tcmod[ 1 ] = tcmod_row1;
	}

	if( skinned ) {
		if( material->shaded ) {
			pipeline.shader = &shaders.standard_skinned_shaded;
			AddDynamicsToPipeline( &pipeline );
		}
		else {
			pipeline.shader = &shaders.standard_skinned;
		}
	}
	else {
		if( material->shaded ) {
			pipeline.shader = &shaders.standard_shaded_instanced;
			AddDynamicsToPipeline( &pipeline );
		}
		else {
			pipeline.shader = &shaders.standard_instanced;
		}
	}

	return pipeline;
}
