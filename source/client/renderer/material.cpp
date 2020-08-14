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
	void ( *func )( Material * material, const char * name, const char ** ptr );
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

struct MaterialLocation {
	const char * start;
	const char * end;
};

static MaterialLocation material_locations[ MAX_MATERIALS ];
static Hashtable< MAX_MATERIALS * 2 > material_locations_hashtable;

static Vec4 decal_uvwhs[ MAX_DECALS ];
static u32 num_decals;
static Hashtable< MAX_DECALS * 2 > decals_hashtable;
static TextureArray decals_atlases;

static u64 HashMaterialName( const char * name ) {
	// skip leading /
	while( *name == '/' )
		name++;
	return Hash64( name );
}

static const char * Shader_ParseString( const char ** ptr ) {
	if( **ptr == '\0' || **ptr == '}' )
		return "";
	return COM_ParseExt( ptr, false );
}

static Span< const char > Shader_ParseToken( const char ** ptr ) {
	Span< const char > span = ParseToken( ptr, Parse_StopOnNewLine );
	if( span.n == 0 || span[ 0 ] == '}' )
		return Span< const char >();
	return span;
}

static float Shader_ParseFloat( const char ** ptr ) {
	if( **ptr == '\0' || **ptr == '}' )
		return 0;
	return atof( COM_ParseExt( ptr, false ) );
}

static void Shader_ParseVector( const char ** ptr, float * v, unsigned int size ) {
	for( unsigned int i = 0; i < size; i++ ) {
		v[i] = Shader_ParseFloat( ptr );
	}
}

static void Shader_SkipLine( const char ** ptr ) {
	while( ptr ) {
		if( **ptr == '}' )
			return;
		const char * token = COM_ParseExt( ptr, false );
		if( strlen( token ) == 0 )
			return;
	}
}

static Wave Shader_ParseWave( const char ** ptr ) {
	Wave wave = { };
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "sin" ) ) {
		wave.type = WaveFunc_Sin;
	}
	else if( !strcmp( token, "triangle" ) ) {
		wave.type = WaveFunc_Triangle;
	}
	else if( !strcmp( token, "sawtooth" ) ) {
		wave.type = WaveFunc_Sawtooth;
	}
	else if( !strcmp( token, "inversesawtooth" ) ) {
		wave.type = WaveFunc_InverseSawtooth;
	}

	wave.args[ 0 ] = Shader_ParseFloat( ptr );
	wave.args[ 1 ] = Shader_ParseFloat( ptr );
	wave.args[ 2 ] = Shader_ParseFloat( ptr );
	wave.args[ 3 ] = Shader_ParseFloat( ptr );

	return wave;
}

static void Shader_Cull( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "disable" ) || !strcmp( token, "none" ) || !strcmp( token, "twosided" ) ) {
		material->double_sided = true;
	}
}

static void Shader_Discard( Material * material, const char * name, const char ** ptr ) {
	material->discard = true;
}

static void Shader_Decal( Material * material, const char * name, const char ** ptr ) {
	material->decal = true;
}

static void ParseMaterial( Material * material, const char * name, const char ** ptr );
static void Shader_Template( Material * material, const char * material_name, const char ** ptr ) {
	constexpr int MAX_ARGS = 9;

	const char * name = Shader_ParseString( ptr );
	if( !*name ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: missing template name in material %s\n", material_name );
		Shader_SkipLine( ptr );
		return;
	}

	u64 idx;
	if( !material_locations_hashtable.get( HashMaterialName( name ), &idx ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: material template %s not found\n", name );
		Shader_SkipLine( ptr );
		return;
	}

	MaterialLocation location = material_locations[ idx ];
	TempAllocator temp = cls.frame_arena.temp();
	DynamicString original( &temp );
	original.append_raw( location.start, location.end - location.start );

	// parse args
	Span< const char > args[ MAX_ARGS ];
	int num_args = 0;
	while( true ) {
		args[ num_args ] = Shader_ParseToken( ptr );
		if( args[ num_args ].n == 0 )
			break;

		if( num_args == MAX_ARGS ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: material template %s has too many arguments\n", name );
			break;
		}

		num_args++;
	}

	// expand template
	DynamicString expanded( &temp );

	const char * cursor = original.c_str();
	while( true ) {
		const char * dollar = strchr( cursor, '$' );
		if( dollar == NULL ) {
			expanded += cursor;
			break;
		}

		expanded.append_raw( cursor, dollar - cursor );

		if( dollar[ 1 ] == '\0' )
			break;

		if( dollar[ 1 ] >= '1' && dollar[ 1 ] <= '9' ) {
			int arg = dollar[ 1 ] - '1';
			expanded += args[ arg ];
		}
		else {
			expanded.append_raw( dollar, 2 );
		}

		cursor = dollar + strlen( "$0" );
	}

	const char * template_ptr = expanded.c_str();
	ParseMaterial( material, material_name, &template_ptr );
}

static const MaterialSpecKey shaderkeys[] = {
	{ "cull", Shader_Cull },
	{ "template", Shader_Template },
	{ "polygonoffset", Shader_Discard },
	{ "decal", Shader_Decal },

	{ NULL, NULL }
};

static void Shaderpass_AlphaTest( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	material->alpha_cutoff = atof( token );
}

static void Shaderpass_BlendFunc( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	if( strcmp( token, "blend" ) == 0 ) {
		material->blend_func = BlendFunc_Blend;
	}
	else if( strcmp( token, "add" ) == 0 ) {
		material->blend_func = BlendFunc_Add;
	}
	Shader_SkipLine( ptr );
}

static void Shaderpass_MapExt( Material * material, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	u64 idx;
	if( textures_hashtable.get( StringHash( token ).hash, &idx ) ) {
		material->texture = &textures[ idx ];
	}
}

static void Shaderpass_Map( Material * material, const char * name, const char ** ptr ) {
	Shaderpass_MapExt( material, ptr );
}

static Vec3 NormalizeColor( Vec3 color ) {
	float f = Max2( Max2( color.x, color.y ), color.z );
	return f > 1.0f ? color / f : color;
}

static void Shaderpass_RGBGen( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "wave" ) ) {
		material->rgbgen.type = ColorGenType_Wave;
		material->rgbgen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "colorwave" ) ) {
		material->rgbgen.type = ColorGenType_Wave;
		Shader_ParseVector( ptr, material->rgbgen.args, 3 );
		material->rgbgen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "entity" ) ) {
		material->rgbgen.type = ColorGenType_Entity;
	}
	else if( !strcmp( token, "entitycolorwave" ) ) {
		material->rgbgen.type = ColorGenType_EntityWave;
		Shader_ParseVector( ptr, material->rgbgen.args, 3 );
		material->rgbgen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "const" ) ) {
		material->rgbgen.type = ColorGenType_Constant;
		Vec3 color;
		Shader_ParseVector( ptr, color.ptr(), 3 );
		color = NormalizeColor( color );
		material->rgbgen.args[ 0 ] = sRGBToLinear( color.x );
		material->rgbgen.args[ 1 ] = sRGBToLinear( color.y );
		material->rgbgen.args[ 2 ] = sRGBToLinear( color.z );
	}
}

static void Shaderpass_AlphaGen( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "entity" ) ) {
		material->alphagen.type = ColorGenType_EntityWave;
	}
	else if( !strcmp( token, "wave" ) ) {
		material->alphagen.type = ColorGenType_Wave;
		material->alphagen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "const" ) ) {
		material->alphagen.type = ColorGenType_Constant;
		material->alphagen.args[0] = Shader_ParseFloat( ptr );
	}
}

static void Shaderpass_TcMod( Material * material, const char * name, const char ** ptr ) {
	if( material->tcmod.type != TCModFunc_None ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: material %s has multiple tcmods\n", name );
		Shader_SkipLine( ptr );
		return;
	}

	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "rotate" ) ) {
		material->tcmod.args[0] = -Shader_ParseFloat( ptr );
		if( !material->tcmod.args[0] ) {
			return;
		}
		material->tcmod.type = TCModFunc_Rotate;
	}
	else if( !strcmp( token, "scroll" ) ) {
		Shader_ParseVector( ptr, material->tcmod.args, 2 );
		material->tcmod.type = TCModFunc_Scroll;
	}
	else if( !strcmp( token, "stretch" ) ) {
		material->tcmod.wave = Shader_ParseWave( ptr );
		material->tcmod.type = TCModFunc_Stretch;
	}
	else {
		Shader_SkipLine( ptr );
		return;
	}
}

static const MaterialSpecKey shaderpasskeys[] = {
	{ "alphagen", Shaderpass_AlphaGen },
	{ "alphamaskclampmap", Shaderpass_Map },
	{ "alphatest", Shaderpass_AlphaTest },
	{ "blendfunc", Shaderpass_BlendFunc },
	{ "clampmap", Shaderpass_Map },
	{ "map", Shaderpass_Map },
	{ "rgbgen", Shaderpass_RGBGen },
	{ "tcmod", Shaderpass_TcMod },

	{ NULL, NULL }
};

static bool Shader_Parsetok( Material * material, const char * name, const MaterialSpecKey * keys, const char * token, const char ** ptr ) {
	for( const MaterialSpecKey * key = keys; key->keyword != NULL; key++ ) {
		if( !Q_stricmp( token, key->keyword ) ) {
			if( key->func ) {
				key->func( material, name, ptr );
			}
			if( *ptr && **ptr == '}' ) {
				*ptr = *ptr + 1;
				return true;
			}
			return false;
		}
	}

	Shader_SkipLine( ptr );

	return false;
}

static void Shader_Readpass( Material * material, const char * name, const char ** ptr ) {
	material->rgbgen.type = ColorGenType_Constant;
	material->rgbgen.args[ 0 ] = 1.0f;
	material->rgbgen.args[ 1 ] = 1.0f;
	material->rgbgen.args[ 2 ] = 1.0f;

	material->alphagen.type = ColorGenType_Constant;
	material->rgbgen.args[ 0 ] = 1.0f;

	while( ptr ) {
		const char * token = COM_ParseExt( ptr, true );
		if( !token[0] ) {
			break;
		}
		else if( token[0] == '}' ) {
			break;
		}
		else if( Shader_Parsetok( material, name, shaderpasskeys, token, ptr ) ) {
			break;
		}
	}
}

static void ParseMaterial( Material * material, const char * name, const char ** ptr ) {
	ZoneScoped;

	bool seen_pass = false;
	while( ptr != NULL ) {
		const char * token = COM_ParseExt( ptr, true );

		if( !token[ 0 ] ) {
			break;
		}
		else if( token[ 0 ] == '}' ) {
			break;
		}
		else if( token[ 0 ] == '{' ) {
			if( !seen_pass ) {
				Shader_Readpass( material, name, ptr );
				seen_pass = true;
			}
			else {
				Com_Printf( S_COLOR_YELLOW "WARNING: material %s has multiple passes\n", name );

				while( ptr ) { // skip
					token = COM_ParseExt( ptr, true );
					if( !token[0] || token[0] == '}' ) {
						break;
					}
				}
			}
		}
		else if( Shader_Parsetok( material, name, shaderkeys, token, ptr ) ) {
			break;
		}
	}
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

static void LoadMaterialFile( const char * path ) {
	Span< const char > data = AssetString( path );

	const char * ptr = data.ptr;
	while( ptr != NULL ) {
		const char * material_name = COM_ParseExt( &ptr, true );
		if( strlen( material_name ) == 0 )
			break;

		u64 hash = HashMaterialName( material_name );
		COM_ParseExt( &ptr, true ); // skip opening brace
		const char * start = ptr;

		u64 idx = num_materials;
		if( !materials_hashtable.get( hash, &idx ) ) {
			materials_hashtable.add( hash, idx );
			num_materials++;
		}

		materials[ idx ] = Material();
		materials[ idx ].name = hash;
		materials[ idx ].texture = &missing_texture;
		ParseMaterial( &materials[ idx ], material_name, &ptr );

		material_locations[ idx ] = { start, ptr };
		material_locations_hashtable.add( hash, idx );
	}

	material_locations_hashtable.clear();
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

static void PackDecalAtlas() {
	ZoneScoped;

	decals_hashtable.clear();

	stbrp_rect rects[ MAX_DECALS ];
	num_decals = 0;

	for( u32 i = 0; i < num_materials; i++ ) {
		if( !materials[ i ].decal )
			continue;

		if( materials[ i ].texture->format != TextureFormat_RGBA_U8_sRGB ) {
			Com_Printf( S_COLOR_YELLOW "Decals must be RGBA\n" );
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

	{
		ZoneScopedN( "Load materials" );

		for( const char * path : AssetPaths() ) {
			if( FileExtension( path ) == ".shader" && BaseName( path ) != "editor.shader" ) {
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

			LoadTexture( path, pixels, w, h, channels );

			changes = true;
		}
	}

	for( const char * path : ModifiedAssetPaths() ) {
		if( FileExtension( path ) == ".shader" && BaseName( path ) != "editor.shader" ) {
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

bool HasAlpha( TextureFormat format ) {
	return format == TextureFormat_A_U8 || format == TextureFormat_RA_U8 || format == TextureFormat_RGBA_U8 || format == TextureFormat_RGBA_U8_sRGB;
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
