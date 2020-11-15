/*
Copyright (C) 1999 Stephen C. Taylor
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

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

static Texture textures[ MAX_TEXTURES ];
static u32 num_textures;
static Hashtable< MAX_TEXTURES * 2 > textures_hashtable;

static Texture missing_texture;
static Material missing_material;

static Material materials[ MAX_MATERIALS ];
static u32 num_materials;
static Hashtable< MAX_MATERIALS * 2 > materials_hashtable;

Material world_material;

static Vec4 decal_uvwhs[ MAX_DECALS ];
static u32 num_decals;
static Hashtable< MAX_DECALS * 2 > decals_hashtable;
static TextureArray decals_atlases;

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
	Span< const char > token = ParseToken( data, Parse_StopOnNewLine );
	float x;
	SpanToFloat( token, &x );
	return x;
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

static void ParseDiscard( Material * material, Span< const char > name, Span< const char > * data ) {
	material->discard = true;
}

static void ParseDecal( Material * material, Span< const char > name, Span< const char > * data ) {
	material->decal = true;
}

static void ParseMaskOutlines( Material * material, Span< const char > name, Span< const char > * data ) {
	material->mask_outlines = true;
}

static const MaterialSpecKey shaderkeys[] = {
	{ "cull", ParseCull },
	{ "polygonoffset", ParseDiscard },
	{ "decal", ParseDecal },
	{ "maskoutlines", ParseMaskOutlines },

	{ }
};

static void ParseAlphaTest( Material * material, Span< const char > name, Span< const char > * data ) {
	material->alpha_cutoff = ParseMaterialFloat( data );
}

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
	{ "alphatest", ParseAlphaTest },
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

static Texture * AddTexture( u64 hash, const TextureConfig & config ) {
	ZoneScoped;

	assert( num_textures < ARRAY_COUNT( textures ) );

	u64 idx = num_textures;
	if( !textures_hashtable.get( hash, &idx ) ) {
		textures_hashtable.add( hash, num_textures );

		materials[ num_materials ] = Material();
		materials[ num_materials ].texture = &textures[ num_textures ];
		materials_hashtable.add( hash, num_materials );

		num_textures++;
		num_materials++;
	}
	else {
		DeleteTexture( textures[ idx ] );
		stbi_image_free( const_cast< void * >( textures[ idx ].data ) );
	}

	textures[ idx ] = NewTexture( config );
	return &textures[ idx ];
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

		AddTexture( Hash64( "$whiteimage" ), config );
	}

	{
		u8 black = 0;

		TextureConfig config;
		config.width = 1;
		config.height = 1;
		config.data = &black;
		config.format = TextureFormat_R_U8;

		AddTexture( Hash64( "$blackimage" ), config );
	}

	{
		u8 data[ 16 * 16 ];
		Span2D< u8 > image( data, 16, 16 );

		for( int y = 0; y < 16; y++ ) {
			for( int x = 0; x < 16; x++ ) {
				float d = Length( Vec2( x - 7.5f, y - 7.5f ) );
				float a = Unlerp01( 1.0f, d, 7.0f );
				image( x, y ) = 255 * ( 1.0f - a );
			}
		}

		TextureConfig config;
		config.width = 16;
		config.height = 16;
		config.data = data;
		config.format = TextureFormat_A_U8;

		AddTexture( Hash64( "$particle" ), config );
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

static void LoadTexture( const char * path, u8 * pixels, int w, int h, int channels ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	if( pixels == NULL ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't load texture from %s\n", path );
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

	Span< const char > ext = FileExtension( path );
	Texture * texture = AddTexture( Hash64( path, strlen( path ) - ext.n ), config );
	texture->data = pixels;
}

static void LoadMaterialFile( const char * path, Span< const char > * material_names ) {
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
			num_materials++;
		}

		material_names[ idx ] = name;
		materials[ idx ] = material;
		materials[ idx ].name = hash;
	}
}

struct DecodeTextureJob {
	struct {
		const char * path;
		Span< const u8 > data;
	} in;

	struct {
		int width, height;
		int channels;
		u8 * pixels;
	} out;
};

static void CopyImage( Span2D< RGBA8 > dst, int x, int y, const Texture * texture ) {
	Span2D< const RGBA8 > src( ( const RGBA8 * ) texture->data, texture->width, texture->height );
	for( u32 row = 0; row < texture->height; row++ ) {
		memcpy( &dst( x, y + row ), &src( 0, row ), sizeof( RGBA8 ) * texture->width );
	}
}

static void PackDecalAtlas( Span< const char > * material_names ) {
	ZoneScoped;

	decals_hashtable.clear();

	stbrp_rect rects[ MAX_DECALS ];
	num_decals = 0;

	for( u32 i = 0; i < num_materials; i++ ) {
		if( !materials[ i ].decal || materials[ i ].texture == NULL )
			continue;

		if( materials[ i ].texture->format != TextureFormat_RGBA_U8_sRGB ) {
			Com_GGPrint( S_COLOR_YELLOW "Decals must be RGBA ({})", material_names[ i ] );
			continue;
		}

		assert( num_decals < ARRAY_COUNT( rects ) );

		stbrp_rect * rect = &rects[ num_decals ];
		num_decals++;

		rect->id = i;
		rect->w = materials[ i ].texture->width;
		rect->h = materials[ i ].texture->height;
	}

	u32 num_to_pack = num_decals;
	u32 num_atlases = 0;
	while( true ) {
		stbrp_node nodes[ MAX_TEXTURES ];
		stbrp_context packer;
		stbrp_init_target( &packer, DECAL_ATLAS_SIZE, DECAL_ATLAS_SIZE, nodes, ARRAY_COUNT( nodes ) );
		stbrp_setup_allow_out_of_mem( &packer, 1 );

		bool all_packed = stbrp_pack_rects( &packer, rects, num_to_pack ) != 0;
		bool none_packed = true;

		static RGBA8 pixels[ DECAL_ATLAS_SIZE * DECAL_ATLAS_SIZE ];
		Span2D< RGBA8 > image( pixels, DECAL_ATLAS_SIZE, DECAL_ATLAS_SIZE );
		for( u32 i = 0; i < num_to_pack; i++ ) {
			if( !rects[ i ].was_packed )
				continue;
			none_packed = false;

			const Material * material = &materials[ rects[ i ].id ];

			size_t decal_idx = decals_hashtable.size();
			decals_hashtable.add( material->name, decal_idx );
			decal_uvwhs[ decal_idx ].x = rects[ i ].x / float( DECAL_ATLAS_SIZE ) + num_atlases;
			decal_uvwhs[ decal_idx ].y = rects[ i ].y / float( DECAL_ATLAS_SIZE );
			decal_uvwhs[ decal_idx ].z = material->texture->width / float( DECAL_ATLAS_SIZE );
			decal_uvwhs[ decal_idx ].w = material->texture->height / float( DECAL_ATLAS_SIZE );
		}

		num_atlases++;
		if( all_packed )
			break;

		if( none_packed ) {
			Com_Error( ERR_DROP, "Can't pack decals" );
		}

		// repack rects array
		for( u32 i = 0; i < num_to_pack; i++ ) {
			if( !rects[ i ].was_packed )
				continue;

			num_to_pack--;
			Swap2( &rects[ num_to_pack ], &rects[ i ] );
			i--;
		}
	}

	RGBA8 * pixels = ALLOC_MANY( sys_allocator, RGBA8, DECAL_ATLAS_SIZE * DECAL_ATLAS_SIZE * num_atlases );
	memset( pixels, 0, DECAL_ATLAS_SIZE * DECAL_ATLAS_SIZE * num_atlases * sizeof( RGBA8 ) );
	defer { FREE( sys_allocator, pixels ); };

	for( u32 i = 0; i < num_decals; i++ ) {
		const Material * material = &materials[ rects[ i ].id ];
		u64 decal_idx;
		bool ok = decals_hashtable.get( material->name, &decal_idx );
		assert( ok );

		u32 layer = u32( decal_uvwhs[ decal_idx ].x );
		Span2D< RGBA8 > atlas( pixels + DECAL_ATLAS_SIZE * DECAL_ATLAS_SIZE * layer, DECAL_ATLAS_SIZE, DECAL_ATLAS_SIZE );

		CopyImage( atlas, rects[ i ].x, rects[ i ].y, material->texture );
	}

	// upload atlases
	{
		ZoneScopedN( "Upload atlas" );

		DeleteTextureArray( decals_atlases );

		TextureArrayConfig config;
		config.width = DECAL_ATLAS_SIZE;
		config.height = DECAL_ATLAS_SIZE;
		config.layers = num_atlases;
		config.data = pixels;

		decals_atlases = NewAtlasTextureArray( config );
	}

	TracyPlot( "Atlas VRAM", s64( num_atlases * DECAL_ATLAS_SIZE * DECAL_ATLAS_SIZE * sizeof( pixels[ 0 ] ) ) );
}

void InitMaterials() {
	ZoneScoped;

	num_textures = 0;
	num_materials = 0;

	world_material = { };

	LoadBuiltinTextures();

	{
		ZoneScopedN( "Load disk textures" );

		DynamicArray< DecodeTextureJob > jobs( sys_allocator );
		{
			ZoneScopedN( "Build job list" );

			for( const char * path : AssetPaths() ) {
				Span< const char > ext = FileExtension( path );
				if( ext == ".png" || ext == ".jpg" ) {
					DecodeTextureJob job;
					job.in.path = path;
					job.in.data = AssetBinary( path );

					jobs.add( job );
				}
			}

			std::sort( jobs.begin(), jobs.end(), []( const DecodeTextureJob & a, const DecodeTextureJob & b ) {
				return a.in.data.n > b.in.data.n;
			} );
		}

		ParallelFor( jobs.span(), []( TempAllocator * temp, void * data ) {
			DecodeTextureJob * job = ( DecodeTextureJob * ) data;

			ZoneScopedN( "stbi_load_from_memory" );
			ZoneText( job->in.path, strlen( job->in.path ) );

			job->out.pixels = stbi_load_from_memory( job->in.data.ptr, job->in.data.num_bytes(), &job->out.width, &job->out.height, &job->out.channels, 0 );
		} );

		for( DecodeTextureJob job : jobs ) {
			LoadTexture( job.in.path, job.out.pixels, job.out.width, job.out.height, job.out.channels );
		}
	}

	Span< const char > material_names[ MAX_MATERIALS ];

	{
		ZoneScopedN( "Load materials" );

		for( const char * path : AssetPaths() ) {
			// game crashes if we load materials with no texture,
			// skip editor.shader until we convert asset pointers
			// to asset hashes
			if( FileExtension( path ) == ".shader" && BaseName( path ) != "editor.shader" ) {
				LoadMaterialFile( path, material_names );
			}
		}
	}

	missing_material = Material();
	missing_material.texture = &missing_texture;

	PackDecalAtlas( material_names );
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

			LoadTexture( path, pixels, w, h, channels );

			changes = true;
		}
	}

	Span< const char > material_names[ MAX_MATERIALS ] = { };

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".shader" && BaseName( path ) != "editor.shader" ) {
			LoadMaterialFile( path, material_names );
			changes = true;
		}
	}

	if( changes ) {
		PackDecalAtlas( material_names );
	}
}

void ShutdownMaterials() {
	for( u32 i = 0; i < num_textures; i++ ) {
		DeleteTexture( textures[ i ] );
		stbi_image_free( const_cast< void * >( textures[ i ].data ) );
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

PipelineState MaterialToPipelineState( const Material * material, Vec4 color, bool skinned ) {
	if( material == &world_material ) {
		PipelineState pipeline;
		pipeline.shader = &shaders.world;
		pipeline.pass = frame_static.world_opaque_pass;
		pipeline.set_uniform( "u_Fog", frame_static.fog_uniforms );
		pipeline.set_texture( "u_BlueNoiseTexture", BlueNoiseTexture() );
		pipeline.set_uniform( "u_BlueNoiseTextureParams", frame_static.blue_noise_uniforms );
		return pipeline;
	}

	if( material->mask_outlines ) {
		PipelineState pipeline;
		pipeline.pass = frame_static.write_world_gbuffer_pass;
		pipeline.shader = &shaders.write_world_gbuffer;
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
	Vec3 tcmod_row0, tcmod_row1;
	if( material->tcmod.type == TCModFunc_None ) {
		tcmod_row0 = Vec3( 1, 0, 0 );
		tcmod_row1 = Vec3( 0, 1, 0 );
	}
	else if( material->tcmod.type == TCModFunc_Scroll ) {
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
	pipeline.set_uniform( "u_Material", UploadMaterialUniforms( color, Vec2( material->texture->width, material->texture->height ), material->alpha_cutoff, tcmod_row0, tcmod_row1 ) );

	if( material->alpha_cutoff > 0 ) {
		pipeline.shader = &shaders.standard_alphatest;
	}
	else if( skinned ) {
		pipeline.shader = &shaders.standard_skinned;
	}
	else {
		pipeline.shader = &shaders.standard;
	}

	return pipeline;
}
