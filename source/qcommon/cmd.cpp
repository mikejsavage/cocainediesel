#include "qcommon/base.h"
#include "qcommon/cmd.h"
#include "qcommon/cvar.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"

#include "nanosort/nanosort.hpp"

struct ConsoleCommand {
	Span< const char > name;
	ConsoleCommandCallback callback;
	TabCompletionCallback tab_completion_callback;
	bool disabled;
};

constexpr size_t MAX_COMMANDS = 1024;

static ConsoleCommand commands[ MAX_COMMANDS ];
static Hashtable< MAX_COMMANDS * 2 > commands_hashtable;

static Span< const char > GrabLine( Span< const char > str, bool * eof ) {
	const char * newline = StrChr( str, '\n' );
	*eof = newline == NULL;
	return newline == NULL ? str : str.slice( 0, newline - str.ptr );
}

static ConsoleCommand * FindCommand( Span< const char > name ) {
	u64 hash = CaseHash64( name );
	u64 idx;
	if( !commands_hashtable.get( hash, &idx ) )
		return NULL;
	return &commands[ idx ];
}

bool Cmd_ExecuteLine( Allocator * a, Span< const char > line, bool warn_on_invalid ) {
	Tokenized args = Tokenize( a, line );
	defer { Free( a, args.tokens.ptr ); };
	if( args.tokens.n == 0 ) {
		return true;
	}

	const ConsoleCommand * command = FindCommand( args.tokens[ 0 ] );
	if( command != NULL && !command->disabled ) {
		command->callback( args );
		return true;
	}

	if( Cvar_Command( args ) ) {
		return true;
	}

	if( warn_on_invalid ) {
		Com_GGPrint( "Unknown command \"{}\"", args.tokens[ 0 ] );
	}

	return false;
}

static const char * Argv( Span< const char * > args, size_t i ) {
	return i < args.n ? args[ i ] : "";
}

static void ClearArg( Span< const char * > args, size_t i ) {
	if( i < args.n ) {
		args[ i ] = NULL;
	}
}

void Cmd_ExecuteEarlyCommands( Span< const char * > args ) {
	for( size_t i = 1; i < args.n; i++ ) {
		if( StrCaseEqual( args[ i ], "-set" ) || StrCaseEqual( args[ i ], "+set" ) ) {
			Cmd_Execute( sys_allocator, "set \"{}\" \"{}\"", Argv( args, i + 1 ), Argv( args, i + 2 ) );
			ClearArg( args, i );
			ClearArg( args, i + 1 );
			ClearArg( args, i + 2 );
			i += 2;
		}
		else if( StrCaseEqual( args[ i ], "-exec" ) || StrCaseEqual( args[ i ], "+exec" ) ) {
			Cmd_Execute( sys_allocator, "exec \"{}\"", Argv( args, i + 1 ) );
			ClearArg( args, i );
			ClearArg( args, i + 1 );
			i += 1;
		}
		else if( StrCaseEqual( args[ i ], "-config" ) || StrCaseEqual( args[ i ], "+config" ) ) {
			Cmd_Execute( sys_allocator, "config \"{}\"", Argv( args, i + 1 ) );
			ClearArg( args, i );
			ClearArg( args, i + 1 );
			i += 1;
		}
	}
}

void Cmd_ExecuteLateCommands( Span< const char * > args ) {
	String< 1024 > buf;

	// TODO: should probably not roundtrip to string and retokenize
	for( size_t i = 1; i < args.n; i++ ) {
		if( args[ i ] == NULL )
			continue;

		if( StartsWith( args[ i ], "-" ) || StartsWith( args[ i ], "+" ) ) {
			Cmd_ExecuteLine( sys_allocator, buf.span(), true );
			buf.format( "{}", args[ i ] + 1 );
		}
		else {
			buf.append( " \"{}\"", args[ i ] );
		}
	}

	Cmd_ExecuteLine( sys_allocator, buf.span(), true );
}

static void ExecConfig( const char * path ) {
	TracyZoneScoped;

	Span< char > config = ReadFileBinary( sys_allocator, path ).cast< char >();
	defer { Free( sys_allocator, config.ptr ); };
	if( config.ptr == NULL ) {
		Com_Printf( "Couldn't execute: %s\n", path );
		return;
	}

	Span< const char > cursor = config;
	while( cursor.n > 0 ) {
		bool eof;
		Span< const char > line = GrabLine( cursor, &eof );
		if( !StartsWith( line, "//" ) ) {
			Cmd_ExecuteLine( sys_allocator, line, true );
		}

		cursor += line.n;
		if( !eof ) {
			cursor++;
		}
	}
}

static void Cmd_Exec_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "Usage: exec <filename>\n" );
		return;
	}

	const char * fmt = is_public_build ? "{}/{}" : "{}/base/{}";
	DynamicString path( sys_allocator, fmt, HomeDirPath(), args.tokens[ 1 ] );
	if( FileExtension( path.c_str() ) == "" ) {
		path += ".cfg";
	}

	ExecConfig( path.c_str() );
}

static void Cmd_Config_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "Usage: config <filename>\n" );
		return;
	}

	DynamicString path( sys_allocator, "{}", args.tokens[ 1 ] );
	if( FileExtension( path.c_str() ) == "" ) {
		path += ".cfg";
	}

	ExecConfig( path.c_str() );
}

static Span< Span< const char > > SearchCommands( Allocator * a, Span< const char > partial ) {
	NonRAIIDynamicArray< Span< const char > > results( a );

	for( size_t i = 0; i < commands_hashtable.size(); i++ ) {
		const ConsoleCommand * command = &commands[ i ];
		if( !command->disabled && CaseContains( command->name, partial ) ) {
			results.add( command->name );
		}
	}

	nanosort( results.begin(), results.end(), SortSpanStringsComparator );

	return results.span();
}

static void Cmd_Find_f( const Tokenized & args ) {
	Span< const char > needle = args.tokens.n < 2 ? Span< const char >( "" ) : args.tokens[ 1 ];

	Span< Span< const char > > cmds = SearchCommands( sys_allocator, needle );
	Span< Span< const char > > cvars = SearchCvars( sys_allocator, needle );
	defer {
		Free( sys_allocator, cmds.ptr );
		Free( sys_allocator, cvars.ptr );
	};

	if( cmds.n == 0 && cvars.n == 0 ) {
		Com_Printf( "No results.\n" );
		return;
	}

	if( cmds.n > 0 ) {
		Com_Printf( "Found %zu command%s:\n", cmds.n, cmds.n == 1 ? "" : "s" );

		for( Span< const char > cmd : cmds ) {
			Com_GGPrint( "    {}", cmd );
		}
	}

	if( cvars.n > 0 ) {
		Com_Printf( "Found %zu cvar%s:\n", cvars.n, cvars.n == 1 ? "" : "s" );

		for( Span< const char > cvar : cvars ) {
			Com_GGPrint( "    {}", cvar );
		}
	}
}

void ExecDefaultCfg() {
	DynamicString path( sys_allocator, "{}/base/default.cfg", RootDirPath() );
	ExecConfig( path.c_str() );
}

void AddCommand( Span< const char > name, ConsoleCommandCallback callback ) {
	Assert( callback != NULL );

	ConsoleCommand * old_command = FindCommand( name );
	if( old_command != NULL ) {
		Assert( old_command->callback == callback && old_command->disabled );
		old_command->disabled = false;
		return;
	}

	Assert( commands_hashtable.size() < ARRAY_COUNT( commands ) );

	ConsoleCommand * command = &commands[ commands_hashtable.size() ];
	*command = { };
	command->name = name;
	command->callback = callback;

	u64 hash = CaseHash64( name );
	bool ok = commands_hashtable.add( hash, commands_hashtable.size() );
	Assert( ok );
}

void RemoveCommand( Span< const char > name ) {
	FindCommand( name )->disabled = true;
}

void SetTabCompletionCallback( Span< const char > name, TabCompletionCallback callback ) {
	ConsoleCommand * command = FindCommand( name );
	Assert( command->tab_completion_callback == NULL || command->tab_completion_callback == callback );
	command->tab_completion_callback = callback;
}

Span< Span< const char > > TabCompleteCommand( TempAllocator * temp, Span< const char > partial ) {
	NonRAIIDynamicArray< Span< const char > > results( temp );

	for( size_t i = 0; i < commands_hashtable.size(); i++ ) {
		const ConsoleCommand * command = &commands[ i ];
		if( !command->disabled && CaseStartsWith( command->name, partial ) ) {
			results.add( command->name );
		}
	}

	nanosort( results.begin(), results.end(), SortSpanStringsComparator );

	return results.span();
}

Span< Span< const char > > TabCompleteArgument( TempAllocator * temp, Span< const char > partial ) {
	Assert( partial != "" );

	Tokenized tokenized = Tokenize( temp, partial );

	const ConsoleCommand * command = FindCommand( tokenized.tokens[ 0 ] );
	if( command == NULL || command->tab_completion_callback == NULL ) {
		return { };
	}

	return command->tab_completion_callback( temp, tokenized.all_but_first );
}

static void FindMatchingFilesRecursive( TempAllocator * temp, NonRAIIDynamicArray< Span< const char > > * files, DynamicString * path, Span< const char > prefix, size_t skip, Span< const char > extension ) {
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
			FindMatchingFilesRecursive( temp, files, path, prefix, skip, extension );
		}
		else {
			bool prefix_matches = CaseStartsWith( path->span() + skip, prefix );
			bool ext_matches = StrCaseEqual( FileExtension( path->c_str() ), extension );
			if( prefix_matches && ext_matches ) {
				files->add( temp->sv( "{}", path->span().slice( skip, path->length() ) ) );
			}
		}
		path->truncate( old_len );
	}
}

Span< Span< const char > > TabCompleteFilename( TempAllocator * temp, Span< const char > partial, Span< const char > search_dir, Span< const char > extension ) {
	DynamicString base_path( sys_allocator, "{}", search_dir );

	NonRAIIDynamicArray< Span< const char > > results( temp );
	FindMatchingFilesRecursive( temp, &results, &base_path, partial, base_path.length() + 1, extension );

	nanosort( results.begin(), results.end(), SortSpanStringsComparator );

	return results.span();
}

Span< Span< const char > > TabCompleteFilenameHomeDir( TempAllocator * temp, Span< const char > partial, Span< const char > search_dir, Span< const char > extension ) {
	Span< const char > home_search_dir = temp->sv( "{}/{}", HomeDirPath(), search_dir );
	return TabCompleteFilename( temp, partial, home_search_dir, extension );
}

static Span< Span< const char > > TabCompleteExec( TempAllocator * temp, Span< const char > partial ) {
	return TabCompleteFilenameHomeDir( temp, partial, "base", ".cfg" );
}

static Span< Span< const char > > TabCompleteConfig( TempAllocator * temp, Span< const char > partial ) {
	return TabCompleteFilename( temp, partial, "base", ".cfg" );
}

void Cmd_Init() {
	AddCommand( "exec", Cmd_Exec_f );
	AddCommand( "config", Cmd_Config_f );
	AddCommand( "find", Cmd_Find_f );

	SetTabCompletionCallback( "exec", TabCompleteExec );
	SetTabCompletionCallback( "config", TabCompleteConfig );
}

void Cmd_Shutdown() {
	RemoveCommand( "exec" );
	RemoveCommand( "config" );
	RemoveCommand( "find" );

	for( size_t i = 0; i < commands_hashtable.size(); i++ ) {
		ConsoleCommand * command = &commands[ i ];
		Assert( command->disabled );
	}
}
