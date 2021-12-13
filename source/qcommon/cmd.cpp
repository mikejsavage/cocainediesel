#include <algorithm> // std::sort

#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"

struct ConsoleCommand {
	char * name;
	ConsoleCommandCallback callback;
	TabCompletionCallback tab_completion_callback;
	bool disabled;
};

constexpr size_t MAX_COMMANDS = 1024;

static String< 1024 > command_buffer;

static ConsoleCommand commands[ MAX_COMMANDS ];
static Hashtable< MAX_COMMANDS * 2 > commands_hashtable;

static Span< const char > GrabLine( Span< const char > str ) {
	const char * newline = ( const char * ) memchr( str.ptr, '\n', str.n );
	return newline == NULL ? str : str.slice( 0, newline - str.ptr );
}

static ConsoleCommand * FindCommand( const char * name ) {
	u64 hash = CaseHash64( name );
	u64 idx;
	if( !commands_hashtable.get( hash, &idx ) )
		return NULL;
	return &commands[ idx ];
}

static void Cmd_TokenizeString( Span< const char > str ); // TODO: nuke

bool Cbuf_ExecuteLine( Span< const char > line, bool warn_on_invalid ) {
	Cmd_TokenizeString( line );

	if( Cmd_Argc() == 0 ) {
		return true;
	}

	const ConsoleCommand * command = FindCommand( Cmd_Argv( 0 ) );
	if( command != NULL ) {
		command->callback();
		return true;
	}

	if( Cvar_Command() ) {
		return true;
	}

	if( warn_on_invalid ) {
		Com_GGPrint( "Unknown command \"{}\"", Cmd_Argv( 0 ) );
	}

	return false;
}

void Cbuf_ExecuteLine( const char * line ) {
	Cbuf_ExecuteLine( MakeSpan( line ), true );
}

static void Cbuf_Execute( const char * str, bool skip_comments ) {
	Span< const char > cursor = MakeSpan( str );
	while( cursor.n > 0 ) {
		Span< const char > line = GrabLine( cursor );
		if( !skip_comments || !StartsWith( line, "//" ) ) {
			Cbuf_ExecuteLine( line, true );
		}

		cursor += line.n + 1;
	}
}

void Cbuf_AddLine( const char * text ) {
	size_t length_with_newline = strlen( text ) + 1;
	if( command_buffer.length() + length_with_newline > command_buffer.capacity() ) {
		Com_Printf( S_COLOR_YELLOW "Typed too much stuff...\n" );
		return;
	}

	command_buffer.append_raw( text, strlen( text ) );
	command_buffer += '\n';
}

void Cbuf_Execute() {
	Cbuf_Execute( command_buffer.c_str(), false );
	command_buffer.clear();
}

static const char * Argv( int argc, char ** argv, int i ) {
	return i < argc ? argv[ i ] : "";
}

static void ClearArg( int argc, char ** argv, int i ) {
	if( i < argc ) {
		argv[ i ] = NULL;
	}
}

void Cbuf_AddEarlyCommands( int argc, char ** argv ) {
	for( int i = 1; i < argc; i++ ) {
		if( StrCaseEqual( argv[ i ], "-set" ) || StrCaseEqual( argv[ i ], "+set" ) ) {
			Cbuf_Add( "set \"{}\" \"{}\"", Argv( argc, argv, i + 1 ), Argv( argc, argv, i + 2 ) );
			ClearArg( argc, argv, i );
			ClearArg( argc, argv, i + 1 );
			ClearArg( argc, argv, i + 2 );
			i += 2;
		}
		else if( StrCaseEqual( argv[ i ], "-exec" ) || StrCaseEqual( argv[ i ], "+exec" ) ) {
			Cbuf_Add( "exec \"{}\"", Argv( argc, argv, i + 1 ) );
			ClearArg( argc, argv, i );
			ClearArg( argc, argv, i + 1 );
			i += 1;
		}
		else if( StrCaseEqual( argv[ i ], "-config" ) || StrCaseEqual( argv[ i ], "+config" ) ) {
			Cbuf_Add( "config \"{}\"", Argv( argc, argv, i + 1 ) );
			ClearArg( argc, argv, i );
			ClearArg( argc, argv, i + 1 );
			i += 1;
		}
	}

	Cbuf_Execute();
}

void Cbuf_AddLateCommands( int argc, char ** argv ) {
	String< 1024 > buf;

	// TODO: should probably not roundtrip to string and retokenize
	for( int i = 1; i < argc; i++ ) {
		if( argv[ i ] == NULL )
			continue;

		if( StartsWith( argv[ i ], "-" ) || StartsWith( argv[ i ], "+" ) ) {
			Cbuf_ExecuteLine( buf.c_str() );
			buf.format( "{}", argv[ i ] + 1 );
		}
		else {
			buf.append( " \"{}\"", argv[ i ] );
		}
	}

	Cbuf_ExecuteLine( buf.c_str() );
}

static void ExecConfig( const char * path ) {
	char * config = ReadFileString( sys_allocator, path );
	defer { FREE( sys_allocator, config ); };
	if( config == NULL ) {
		Com_Printf( "Couldn't execute: %s\n", path );
		return;
	}

	Cbuf_Execute( config, true );
}

static void Cmd_Exec_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: exec <filename>\n" );
		return;
	}

	DynamicString path( sys_allocator, "{}/base/{}", HomeDirPath(), Cmd_Argv( 1 ) );
	if( FileExtension( path.c_str() ) == "" ) {
		path += ".cfg";
	}

	ExecConfig( path.c_str() );
}

static void Cmd_Config_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: config <filename>\n" );
		return;
	}

	DynamicString path( sys_allocator, "{}", Cmd_Argv( 1 ) );
	if( FileExtension( path.c_str() ) == "" ) {
		path += ".cfg";
	}

	ExecConfig( path.c_str() );
}

void ExecDefaultCfg() {
	DynamicString path( sys_allocator, "{}/base/default.cfg", RootDirPath() );
	ExecConfig( path.c_str() );
}

/* DUMB SHIT */
// TODO: just pass the span to commands and stop misusing this everywhere else

static int cmd_argc;
static char * cmd_argv[MAX_STRING_TOKENS];
static char cmd_args[MAX_STRING_CHARS];

int Cmd_Argc() {
	return cmd_argc;
}

const char * Cmd_Argv( int arg ) {
	return arg < cmd_argc ? cmd_argv[ arg ] : "";
}

char * Cmd_Args() {
	return cmd_args;
}

// TODO: this sucks
static void Cmd_TokenizeString( Span< const char > str ) {
	for( int i = 0; i < cmd_argc; i++ ) {
		FREE( sys_allocator, cmd_argv[ i ] );
	}

	strcpy( cmd_args, "" );

	cmd_argc = 0;

	while( cmd_argc < MAX_STRING_TOKENS ) {
		Span< const char > token = ParseToken( &str, Parse_StopOnNewLine );
		if( token.ptr == NULL )
			return;

		cmd_argv[ cmd_argc ] = ( *sys_allocator )( "{}", token );

		if( cmd_argc == 1 ) {
			Span< const char > rest_of_line( token.ptr, str.end() - token.ptr );
			if( rest_of_line.ptr[ -1 ] == '"' ) {
				rest_of_line.ptr--;
				rest_of_line.n++;
			}
			ggformat( cmd_args, sizeof( cmd_args ), "{}", rest_of_line );
			size_t n = strlen( cmd_args );
			while( n > 0 && cmd_args[ n - 1 ] == ' ' ) {
				cmd_args[ n - 1 ] = '\0';
				n--;
			}
		}

		cmd_argc++;
	}
}

void Cmd_TokenizeString( const char * str ) {
	Cmd_TokenizeString( MakeSpan( str ) );
}

void AddCommand( const char * name, ConsoleCommandCallback callback ) {
	assert( callback != NULL );

	ConsoleCommand * old_command = FindCommand( name );
	if( old_command != NULL ) {
		assert( old_command->callback == callback && old_command->disabled );
		old_command->disabled = false;
		return;
	}

	assert( commands_hashtable.size() < ARRAY_COUNT( commands ) );

	ConsoleCommand * command = &commands[ commands_hashtable.size() ];
	*command = { };
	command->name = CopyString( sys_allocator, name );
	command->callback = callback;

	u64 hash = CaseHash64( name );
	bool ok = commands_hashtable.add( hash, commands_hashtable.size() );
	assert( ok );
}

void RemoveCommand( const char * name ) {
	FindCommand( name )->disabled = true;
}

void SetTabCompletionCallback( const char * name, TabCompletionCallback callback ) {
	ConsoleCommand * command = FindCommand( name );
	assert( command->tab_completion_callback == NULL || command->tab_completion_callback == callback );
	command->tab_completion_callback = callback;
}

Span< const char * > Cmd_TabComplete( TempAllocator * a, const char * partial ) {
	NonRAIIDynamicArray< const char * > completions;
	completions.init( a );

	for( size_t i = 0; i < commands_hashtable.size(); i++ ) {
		const ConsoleCommand * command = &commands[ i ];
		if( CaseStartsWith( command->name, partial ) ) {
			completions.add( command->name );
		}
	}

	return completions.span();
}

static void AddMatchingFilesRecursive( DynamicArray< char * > * files, DynamicString * path, Span< const char > prefix, size_t skip, const char * extension ) {
	ListDirHandle scan = BeginListDir( sys_allocator, path->c_str() );

	const char * name;
	bool dir;
	while( ListDirNext( &scan, &name, &dir ) ) {
		// skip ., .., .git, etc
		if( name[ 0 ] == '.' )
			continue;

		size_t old_len = path->length();
		path->append( "/{}", name );
		if( dir ) {
			AddMatchingFilesRecursive( files, path, prefix, skip, extension );
		}
		else {
			bool prefix_matches = path->length() >= prefix.n + skip && Q_strnicmp( path->c_str() + skip, prefix.ptr, prefix.n ) == 0;
			bool ext_matches = StrCaseEqual( FileExtension( path->c_str() ), extension );
			if( prefix_matches && ext_matches ) {
				files->add( ( *sys_allocator )( "{}", path->span().slice( skip, path->length() ) ) );
			}
		}
		path->truncate( old_len );
	}
}

// TODO
const char ** Cmd_CompleteHomeDirFileList( const char * partial, const char * search_dir, const char * extension ) {
	DynamicString base_path( sys_allocator, "{}/{}", HomeDirPath(), search_dir );
	size_t skip = base_path.length();
	base_path += BasePath( partial );

	Span< const char > prefix = FileName( partial );

	DynamicArray< char * > files( sys_allocator );

	AddMatchingFilesRecursive( &files, &base_path, prefix, skip + 1, extension );

	std::sort( files.begin(), files.end(), SortCStringsComparator );

	size_t filenames_buffer_size = 0;
	for( const char * file : files ) {
		filenames_buffer_size += strlen( file ) + 1;
	}

	size_t pointers_buffer_size = ( files.size() + 1 ) * sizeof( char * ); // + 1 for trailing NULL
	size_t combined_buffer_size =  pointers_buffer_size + filenames_buffer_size;

	void * combined = Mem_TempMalloc( combined_buffer_size );
	char ** pointers = ( char ** ) combined;
	char * filenames = ( char * ) combined + pointers_buffer_size;

	size_t filenames_cursor = 0;
	for( size_t i = 0; i < files.size(); i++ ) {
		char * file = files[ i ];

		pointers[ i ] = filenames + filenames_cursor;
		memcpy( pointers[ i ], file, strlen( file ) + 1 );
		filenames_cursor += strlen( file ) + 1;

		FREE( sys_allocator, file );
	}

	pointers[ files.size() ] = NULL;

	return ( const char ** ) combined;
}

void Cmd_Init() {
	AddCommand( "exec", Cmd_Exec_f );
	AddCommand( "config", Cmd_Config_f );

	// SetTabCompletionCallback( "exec", CL_CompleteExecBuildList );
	// SetTabCompletionCallback( "config", CL_CompleteExecBuildList );

	command_buffer.clear();

	cmd_argc = 0;
}

void Cmd_Shutdown() {
	RemoveCommand( "exec" );
	RemoveCommand( "config" );

	for( int i = 0; i < cmd_argc; i++ ) {
		FREE( sys_allocator, cmd_argv[ i ] );
	}

	for( size_t i = 0; i < commands_hashtable.size(); i++ ) {
		ConsoleCommand * command = &commands[ i ];
		assert( command->disabled );
		FREE( sys_allocator, command->name );
	}
}
