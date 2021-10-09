#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/sys_fs.h"
#include "gameshared/q_shared.h"

#include "qcommon/qfiles.h"
#include "gameshared/q_collision.h"

#include "parsing.h"
#include "materials.h"

struct Material {
	Span< const char > name;
	u32 surface_flags;
	u32 content_flags;
};

static char * shader_files[ 1024 ];
static u32 num_shader_files;

static Material materials[ 1024 ];
static u32 num_materials;

static Span< const char > ParseLine( Span< const char > * line, Span< const char > str ) {
	return PEGCapture( line, str, []( Span< const char > str ) {
		str = SkipWhitespace( str );
		str = PEGNOrMore( str, 1, []( Span< const char > str ) {
			return PEGNotSet( str, "}\n" );
		} );
		return str;
	} );
}

static Span< const char > SkipPass( Span< const char > str ) {
	str = SkipToken( str, "{" );
	str = PEGNOrMore( str, 0, []( Span< const char > str ) {
		return PEGNotSet( str, "}" );
	} );
	str = SkipToken( str, "}" );
	return str;
}

static struct {
	const char * name;
	u32 surface_flags;
	u32 content_flags;
	u32 not_content_flags;
	bool nodraw;
} surface_parms[] = {
	{ "nodraw", SURF_NODRAW, 0, 0 },
	{ "nonsolid", 0, 0, CONTENTS_SOLID },
	{ "playerclip", 0, CONTENTS_PLAYERCLIP, CONTENTS_SOLID },
	{ "wallbangable", CONTENTS_WALLBANGABLE, CONTENTS_WALLBANGABLE, CONTENTS_SOLID },
	{ "ladder", SURF_LADDER, 0, 0 },
	{ "nowalljump", SURF_NOWALLJUMP, 0, 0 },
};

static void ParseMaterialLine( Material * material, Span< const char > line ) {
	Span< const char > command;
	line = ParseWord( &command, line );

	if( command == "surfaceparm" ) {
		Span< const char > arg;
		line = ParseWord( &arg, line );

		for( const auto parm : surface_parms ) {
			if( StrEqual( parm.name, arg ) ) {
				material->surface_flags |= parm.surface_flags;
				material->content_flags |= parm.content_flags;
				material->content_flags &= ~parm.not_content_flags;
				break;
			}
		}
	}
}

static Span< const char > ParseMaterial( Span< const char > str ) {
	ZoneScoped;

	if( num_materials == ARRAY_COUNT( materials ) ) {
		Fatal( "Too many materials" );
	}

	Material material = { };
	material.content_flags = CONTENTS_SOLID;

	str = ParseWord( &material.name, str );
	str = SkipToken( str, "{" );
	str = PEGNOrMore( str, 0, [&]( Span< const char > str ) {
		return PEGOr( str,
			SkipPass,
			[&]( Span< const char > str ) {
				Span< const char > line;
				str = ParseLine( &line, str );
				ParseMaterialLine( &material, line );
				return str;
			}
		);
	} );
	str = SkipToken( str, "}" );

	materials[ num_materials ] = material;
	num_materials++;

	return str;
}

static void LoadShaderFile( const char * path ) {
	ZoneScoped;
	ZoneText( path, strlen( path ) );

	if( num_shader_files == ARRAY_COUNT( shader_files ) ) {
		Fatal( "Too many shader files" );
	}

	size_t contents_len;
	char * contents = ReadFileString( sys_allocator, path, &contents_len );
	if( contents == NULL ) {
		Fatal( "Can't open %s", path );
	}

	shader_files[ num_shader_files ] = contents;
	num_shader_files++;

	Span< const char > str = Span< const char >( contents, contents_len );

	str = PEGNOrMore( str, 0, ParseMaterial );
	str = SkipWhitespace( str );

	bool ok = str.ptr != NULL && str.n == 0;
	if( !ok ) {
		Fatal( "Can't parse materials from %s", path );
	}
}

static void LoadMaterialsRecursive( Allocator * a, DynamicString * path ) {
	ListDirHandle scan = BeginListDir( a, path->c_str() );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		if( dir ) {
			LoadMaterialsRecursive( a, path );
		}
		else if( FileExtension( path->c_str() ) == ".shader" ) {
			LoadShaderFile( path->c_str() );
		}
		path->truncate( old_len );
	}
}

void InitMaterials() {
	ZoneScoped;

	num_shader_files = 0;
	num_materials = 0;

	DynamicString base( sys_allocator, "{}/base", RootDirPath() );
	LoadMaterialsRecursive( sys_allocator, &base );
}

void ShutdownMaterials() {
	ZoneScoped;

	for( u32 i = 0; i < num_shader_files; i++ ) {
		FREE( sys_allocator, shader_files[ i ] );
	}
}

void GetMaterialFlags( Span< const char > name, u32 * surface_flags, u32 * content_flags ) {
	*surface_flags = 0;
	*content_flags = CONTENTS_SOLID;

	for( u32 i = 0; i < num_materials; i++ ) {
		const Material * material = &materials[ i ];
		if( !StrEqual( material->name, name ) )
			continue;

		*surface_flags = material->surface_flags;
		*content_flags = material->content_flags;

		break;
	}
}

bool IsNodrawMaterial( Span< const char > name ) {
	for( u32 i = 0; i < num_materials; i++ ) {
		const Material * material = &materials[ i ];
		if( StrEqual( material->name, name ) ) {
			return material->surface_flags & SURF_NODRAW;
		}
	}

	return false;
}
