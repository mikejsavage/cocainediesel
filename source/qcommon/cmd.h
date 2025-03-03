#pragma once

#include "qcommon/types.h"

void Cmd_Init();
void Cmd_Shutdown();

bool Cmd_ExecuteLine( Allocator * a, Span< const char > line, bool warn_on_invalid );

template< typename... Rest >
void Cmd_Execute( Allocator * a, const char * fmt, const Rest & ... rest ) {
	Span< char > command = a->sv( fmt, rest... );
	defer { Free( a, command.ptr ); };
	Cmd_ExecuteLine( a, command, true );
}

void Cmd_ExecuteEarlyCommands( Span< const char * > args );
void Cmd_ExecuteLateCommands( Span< const char * > args );

using ConsoleCommandCallback = void ( * )( const Tokenized & args );
using TabCompletionCallback = Span< Span< const char > > ( * )( TempAllocator * temp, Span< const char > partial );

void AddCommand( Span< const char > name, ConsoleCommandCallback function );
void SetTabCompletionCallback( Span< const char > name, TabCompletionCallback callback );
void RemoveCommand( Span< const char > name );

Span< Span< const char > > TabCompleteCommand( TempAllocator * temp, Span< const char > partial );
Span< Span< const char > > TabCompleteArgument( TempAllocator * temp, Span< const char > partial );
Span< Span< const char > > TabCompleteFilename( TempAllocator * temp, Span< const char > partial, Span< const char > search_dir, Span< const char > extension );
Span< Span< const char > > TabCompleteFilenameHomeDir( TempAllocator * temp, Span< const char > partial, Span< const char > search_dir, Span< const char > extension );

void ExecDefaultCfg();
