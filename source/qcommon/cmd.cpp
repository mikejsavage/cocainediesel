#include "qcommon/base.h"
#include "qcommon/cmd.h"
#include "qcommon/cvar.h"
#include "qcommon/fs.h"
#include "qcommon/hash.h"
#include "qcommon/hashmap.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"

#include "nanosort/nanosort.hpp"

struct ConsoleCommand {
	Span< const char > name;
	ConsoleCommandCallback callback;
	TabCompletionCallback tab_completion_callback;
	bool disabled;
};

static HashMap< ConsoleCommand, 1024 > commands;

static Span< const char > GrabLine( Span< const char > str, bool * eof ) {
	const char * newline = StrChr( str, '\n' );
	*eof = newline == NULL;
	return newline == NULL ? str : str.slice( 0, newline - str.ptr );
}

static ConsoleCommand * FindCommand( Span< const char > name ) {
	return commands.get( CaseHash64( name ) );
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

	int i = 0;

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

		i++;
		if( i > 500 ) abort();
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

	for( const ConsoleCommand & command : commands.span() ) {
		if( !command.disabled && CaseContains( command.name, partial ) ) {
			results.add( command.name );
		}
	}

	nanosort( results.begin(), results.end(), SortSpanStringsComparator );

	return results.span();
}

static void Cmd_Find_f( const Tokenized & args ) {
	Span< const char > needle = args.tokens.n < 2 ? ""_sp : args.tokens[ 1 ];

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

void AddCommand( Span< const char > name, ConsoleCommandCallback callback, TabCompletionCallback tab_completion_callback ) {
	ConsoleCommand * old_command = FindCommand( name );
	if( old_command != NULL ) {
		Assert( old_command->disabled );
		Assert( old_command->callback == callback && old_command->tab_completion_callback == tab_completion_callback );
		old_command->disabled = false;
		return;
	}

	Assert( !commands.full() );

	commands.add( CaseHash64( name ), ConsoleCommand {
		.name = name,
		.callback = callback,
		.tab_completion_callback = tab_completion_callback,
	} );
}

void RemoveCommand( Span< const char > name ) {
	FindCommand( name )->disabled = true;
}

Span< Span< const char > > TabCompleteCommand( TempAllocator * temp, Span< const char > partial ) {
	NonRAIIDynamicArray< Span< const char > > results( temp );

	for( const ConsoleCommand & command : commands.span() ) {
		if( !command.disabled && CaseStartsWith( command.name, partial ) ) {
			results.add( command.name );
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

Span< Span< const char > > TabCompleteFilename( TempAllocator * temp, Span< const char > partial, Span< const char > search_dir, Span< const char > extension ) {
	NonRAIIDynamicArray< Span< const char > > results( temp );
	size_t skip = search_dir.n + 1;

	for( Span< const char > file : ListDir( temp, search_dir, ListDir_Recurse ) ) {
		if( StrCaseEqual( FileExtension( file ), extension ) && CaseStartsWith( file + skip, partial ) ) {
			results.add( file + skip );
		}
	}

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
	AddCommand( "exec", Cmd_Exec_f, TabCompleteExec );
	AddCommand( "config", Cmd_Config_f, TabCompleteConfig );
	AddCommand( "find", Cmd_Find_f );
}

void Cmd_Shutdown() {
	RemoveCommand( "exec" );
	RemoveCommand( "config" );
	RemoveCommand( "find" );

	for( const ConsoleCommand & command : commands.span() ) {
		Assert( command.disabled );
	}
}
