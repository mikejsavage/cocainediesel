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

#include "qcommon/base.h"
#include "qcommon/assets.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"
#include "client/client.h"
#include "client/renderer/renderer.h"

#include "stb/stb_image.h"

struct MaterialSpecKey {
	const char * keyword;
	void ( *func )( Material * material, const char * name, const char ** ptr );
};

constexpr u32 MAX_TEXTURES = 4096;
constexpr u32 MAX_MATERIALS = 4096;

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

static u64 HashMaterialName( const char * name ) {
	// skip leading /
	while( *name == '/' )
		name++;

	// hash lowercase name
	u64 hash = Hash64( "" );
	size_t len = strlen( name );
	for( size_t i = 0; i < len; i++ ) {
		char lower = tolower( name[ i ] );
		hash = Hash64( &lower, 1, hash );
	}

	return hash;
}

static const char * Shader_ParseString( const char ** ptr ) {
	if( **ptr == '\0' || **ptr == '}' )
		return "";
	char * token = COM_ParseExt( ptr, false );
	return Q_strlwr( token );
}

static Span< const char > Shader_ParseSpan( const char ** ptr ) {
	Span< const char > span = ParseSpan( ptr, true );
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
	unsigned int i;
	bool bracket;

	if( !size ) {
		return;
	}
	if( size == 1 ) {
		Shader_ParseFloat( ptr );
		return;
	}

	const char *token = Shader_ParseString( ptr );
	if( !strcmp( token, "(" ) ) {
		bracket = true;
		token = Shader_ParseString( ptr );
	} else if( token[0] == '(' ) {
		bracket = true;
		token = &token[1];
	} else {
		bracket = false;
	}

	v[0] = atof( token );
	for( i = 1; i < size - 1; i++ )
		v[i] = Shader_ParseFloat( ptr );

	token = Shader_ParseString( ptr );
	if( !token[0] ) {
		v[i] = 0;
	} else if( token[strlen( token ) - 1] == ')' ) {
		char buf[ 128 ];
		Q_strncpyz( buf, token, sizeof( buf ) );
		buf[ strlen( buf ) - 1 ] = '\0';
		v[i] = atof( buf );
	} else {
		v[i] = atof( token );
		if( bracket ) {
			Shader_ParseString( ptr );
		}
	}
}

// TODO: should stop on }
static void Shader_SkipLine( const char ** ptr ) {
	while( ptr ) {
		const char * token = COM_ParseExt( ptr, false );
		if( strlen( token ) == 0 )
			return;
	}
}

static Wave Shader_ParseWave( const char ** ptr ) {
	Wave wave;
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "sin" ) ) {
		wave.type = WaveFunc_Sin;
	}
	else if( !strcmp( token, "triangle" ) ) {
		wave.type = WaveFunc_Triangle;
	}
	else if( !strcmp( token, "square" ) ) {
		wave.type = WaveFunc_Square;
	}
	else if( !strcmp( token, "sawtooth" ) ) {
		wave.type = WaveFunc_Sawtooth;
	}
	else if( !strcmp( token, "inversesawtooth" ) ) {
		wave.type = WaveFunc_InverseSawtooth;
	}
	else if( !strcmp( token, "noise" ) ) {
		wave.type = WaveFunc_Noise;
	}
	else {
		wave.type = WaveFunc_None;
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
	TempAllocator temp = cls.frame_arena->temp();
	DynamicString original( &temp );
	original.append_raw( location.start, location.end - location.start );

	// parse args
	Span< const char > args[ MAX_ARGS ];
	int num_args = 0;
	while( true ) {
		args[ num_args ] = Shader_ParseSpan( ptr );
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

static void Shaderpass_MapExt( Material * material, SamplerType sampler, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );

	material->textures[ 0 ].texture = FindTexture( StringHash( token ) );
	material->textures[ 0 ].sampler = sampler;
	material->num_anim_frames = 0;
	material->anim_fps = 0;
}

static void Shaderpass_AnimMapExt( Material * material, SamplerType sampler, const char ** ptr ) {
	material->anim_fps = Shader_ParseFloat( ptr );
	material->num_anim_frames = 0;

	for( ;; ) {
		const char *token = Shader_ParseString( ptr );
		if( !token[0] ) {
			break;
		}
		if( material->num_anim_frames < ARRAY_COUNT( material->textures ) ) {
			material->textures[ material->num_anim_frames++ ].texture = FindTexture( StringHash( token ) );
			material->textures[ material->num_anim_frames++ ].sampler = sampler;
		}
	}

	if( material->num_anim_frames == 0 ) {
		material->anim_fps = 0;
	}
}

static void Shaderpass_Map( Material * material, const char * name, const char ** ptr ) {
	Shaderpass_MapExt( material, SamplerType_Normal, ptr );
}

static void Shaderpass_ClampMap( Material * material, const char * name, const char ** ptr ) {
	Shaderpass_MapExt( material, SamplerType_Clamp, ptr );
}

static void Shaderpass_AnimMap( Material * material, const char * name, const char ** ptr ) {
	Shaderpass_AnimMapExt( material, SamplerType_Normal, ptr );
}

static void Shaderpass_AlphaMaskClampMap( Material * material, const char * name, const char ** ptr ) {
	Shaderpass_MapExt( material, SamplerType_ClampAlphaMask, ptr );
}

static void Shaderpass_AnimClampMap( Material * material, const char * name, const char ** ptr ) {
	Shaderpass_AnimMapExt( material, SamplerType_Clamp, ptr );
}

static void ColorNormalize( const vec3_t in, vec3_t out ) {
	float f = Max2( Max2( in[0], in[1] ), in[2] );

	if( f > 1.0f ) {
		f = 1.0f / f;
		VectorScale( in, f, out );
	} else {
		VectorCopy( in, out );
	}
}

static void Shaderpass_RGBGen( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "wave" ) ) {
		material->rgbgen.type = ColorGenType_Wave;
		material->rgbgen.args[0] = 1.0f;
		material->rgbgen.args[1] = 1.0f;
		material->rgbgen.args[2] = 1.0f;
		material->rgbgen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "colorwave" ) ) {
		material->rgbgen.type = ColorGenType_Wave;
		Shader_ParseVector( ptr, material->rgbgen.args, 3 );
		material->rgbgen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "entity" ) ) {
		material->rgbgen.type = ColorGenType_EntityWave;
		material->rgbgen.wave.type = WaveFunc_None;
	}
	else if( !strcmp( token, "entitycolorwave" ) ) {
		material->rgbgen.type = ColorGenType_EntityWave;
		Shader_ParseVector( ptr, material->rgbgen.args, 3 );
		material->rgbgen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "vertex" ) ) {
		material->rgbgen.type = ColorGenType_Vertex;
	}
	else if( !strcmp( token, "const" ) ) {
		material->rgbgen.type = ColorGenType_Constant;
		vec3_t color;
		Shader_ParseVector( ptr, color, 3 );
		ColorNormalize( color, material->rgbgen.args );
	}
}

static void Shaderpass_AlphaGen( Material * material, const char * name, const char ** ptr ) {
	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "vertex" ) ) {
		material->alphagen.type = ColorGenType_Vertex;
	}
	else if( !strcmp( token, "entity" ) ) {
		material->alphagen.type = ColorGenType_EntityWave;
	}
	else if( !strcmp( token, "wave" ) ) {
		material->alphagen.type = ColorGenType_Wave;
		material->alphagen.wave = Shader_ParseWave( ptr );
	}
	else if( !strcmp( token, "const" ) ) {
		material->alphagen.type = ColorGenType_Constant;
		material->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
	}
}

static void Shaderpass_TcMod( Material * material, const char * name, const char ** ptr ) {
	if( material->num_tcmods == ARRAY_COUNT( material->tcmods ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: material %s has too many tcmods\n", name );
		Shader_SkipLine( ptr );
		return;
	}

	TCMod * tcmod = &material->tcmods[ material->num_tcmods ];

	const char * token = Shader_ParseString( ptr );
	if( !strcmp( token, "rotate" ) ) {
		tcmod->args[0] = -Shader_ParseFloat( ptr ) / 360.0f;
		if( !tcmod->args[0] ) {
			return;
		}
		tcmod->type = TCModFunc_Rotate;
	}
	else if( !strcmp( token, "scale" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TCModFunc_Scale;
	}
	else if( !strcmp( token, "scroll" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TCModFunc_Scroll;
	}
	else if( !strcmp( token, "stretch" ) ) {
		Wave wave = Shader_ParseWave( ptr );
		tcmod->args[0] = wave.type;
		for( int i = 1; i < 5; i++ )
			tcmod->args[ i ] = wave.args[ i - 1 ];
		tcmod->type = TCModFunc_Stretch;
	}
	else if( !strcmp( token, "turb" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 4 );
		tcmod->type = TCModFunc_Turb;
	}
	else {
		Shader_SkipLine( ptr );
		return;
	}

	material->num_tcmods++;
}

static const MaterialSpecKey shaderpasskeys[] = {
	{ "alphagen", Shaderpass_AlphaGen },
	{ "alphamaskclampmap", Shaderpass_AlphaMaskClampMap },
	{ "alphatest", Shaderpass_AlphaTest },
	{ "animclampmap", Shaderpass_AnimClampMap },
	{ "animmap", Shaderpass_AnimMap },
	{ "blendfunc", Shaderpass_BlendFunc },
	{ "clampmap", Shaderpass_ClampMap },
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
	material->alphagen.type = ColorGenType_Unknown;

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

	if( material->alphagen.type == ColorGenType_Unknown ) {
		if( material->rgbgen.type == ColorGenType_Vertex ) {
			material->alphagen.type = ColorGenType_Vertex;
		}
		else {
			material->alphagen.type = ColorGenType_Constant;
		}
	}
}

static void ParseMaterial( Material * material, const char * name, const char ** ptr ) {
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

	if( material->blend_func == BlendFunc_Disabled ) {
		bool has_transparent_texture = false;
		for( u8 i = 0; i < Max2( u8( 1 ), material->num_anim_frames ); i++ ) {
			if( HasAlpha( material->textures[ i ].texture.format ) ) {
				has_transparent_texture = true;
				break;
			}
		}

		if( has_transparent_texture ) {
			material->blend_func = BlendFunc_Add;
		}
	}
}

static void AddTexture( u64 hash, const TextureConfig & config ) {
	Texture texture = NewTexture( config );

	textures[ num_textures ] = texture;
	textures_hashtable.add( hash, num_textures );
	num_textures++;

	materials[ num_materials ] = { };
	materials[ num_materials ].textures[ 0 ].texture = texture;
	materials_hashtable.add( hash, num_materials );
	num_materials++;
}

static void LoadBuiltinTextures() {
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

static void LoadDiskTextures() {
	for( const char * path : AssetPaths() ) {
		const char * ext = COM_FileExtension( path );
		if( ext == NULL || ( strcmp( ext, ".png" ) != 0 && strcmp( ext, ".jpg" ) != 0 ) )
			continue;

		assert( num_textures < ARRAY_COUNT( textures ) );

		Span< const u8 > data = AssetBinary( path );

		int w, h, channels;
		u8 * pixels = stbi_load_from_memory( data.ptr, data.num_bytes(), &w, &h, &channels, 0 );
		defer { stbi_image_free( pixels ); };

		if( pixels == NULL ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: couldn't load texture from %s\n", path );
			continue;
		}

		constexpr TextureFormat formats[] = {
			TextureFormat_R_U8,
			TextureFormat_RG_U8,
			TextureFormat_RGB_U8_sRGB,
			TextureFormat_RGBA_U8_sRGB,
		};

		TextureConfig config;
		config.width = checked_cast< u32 >( w );
		config.height = checked_cast< u32 >( h );
		config.data = pixels;
		config.format = formats[ channels - 1 ];

		AddTexture( Hash64( path, strlen( path ) - strlen( ext ) ), config );
	}
}

void InitMaterials() {
	num_textures = 0;
	num_materials = 0;

	world_material = { };

	LoadBuiltinTextures();
	LoadDiskTextures();

	for( const char * path : AssetPaths() ) {
		const char * ext = COM_FileExtension( path );
		if( ext == NULL || strcmp( ext, ".shader" ) != 0 )
			continue;

		Span< const char > data = AssetString( path );

		const char * ptr = data.ptr;
		while( ptr != NULL ) {
			const char * material_name = COM_ParseExt( &ptr, true );
			if( strlen( material_name ) == 0 )
				break;

			u64 hash = HashMaterialName( material_name );
			COM_ParseExt( &ptr, true ); // skip { TODO check
			const char * start = ptr;

			u64 idx = num_materials;
			if( !materials_hashtable.get( hash, &idx ) ) {
				materials_hashtable.add( hash, idx );
				num_materials++;
			}

			materials[ idx ] = { }; // TODO: defaults
			ParseMaterial( &materials[ idx ], material_name, &ptr );

			material_locations[ idx ] = { start, ptr };
			material_locations_hashtable.add( hash, idx );
		}

		material_locations_hashtable.clear();
	}

	missing_material = { };
	missing_material.textures[ 0 ].texture = missing_texture;
}

void ShutdownMaterials() {
	for( u32 i = 0; i < num_textures; i++ ) {
		DeleteTexture( textures[ i ] );
	}

	DeleteTexture( missing_texture );
}

bool TryFindTexture( StringHash name, Texture * texture ) {
	u64 idx;
	if( !textures_hashtable.get( name.hash, &idx ) )
		return false;
	*texture = textures[ idx ];
	return true;
}

Texture FindTexture( StringHash name ) {
	Texture texture;
	if( !TryFindTexture( name, &texture ) )
		return missing_texture;
	return texture;
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
