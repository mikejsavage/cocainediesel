#pragma once

#include "qcommon/types.h"

void Cmd_Init();
void Cmd_Shutdown();

void Cbuf_AddLine( const char * text );
void Cbuf_Execute();
bool Cbuf_ExecuteLine( Span< const char > line, bool warn_on_invalid );
void Cbuf_ExecuteLine( const char * line );

void Cbuf_AddEarlyCommands( int argc, char ** argv );
void Cbuf_AddLateCommands( int argc, char ** argv );

template< typename... Rest >
void Cbuf_Add( const char * fmt, const Rest & ... rest ) {
	char buf[ 1024 ];
	ggformat( buf, sizeof( buf ), fmt, rest... );
	Cbuf_AddLine( buf );
}

using ConsoleCommandCallback = void ( * )();
using TabCompletionCallback = Span< const char * > ( * )( TempAllocator * a, const char * partial );

void AddCommand( const char * name, ConsoleCommandCallback function );
void SetTabCompletionCallback( const char * name, TabCompletionCallback callback );
void RemoveCommand( const char * name );

Span< const char * > TabCompleteCommand( TempAllocator * a, const char * partial );
Span< const char * > SearchCommands( Allocator * a, const char * partial );
Span< const char * > TabCompleteArgument( TempAllocator * a, const char * partial );
Span< const char * > TabCompleteFilename( TempAllocator * a, const char * partial, const char * search_dir, const char * extension );
Span< const char * > TabCompleteFilenameHomeDir( TempAllocator * a, const char * partial, const char * search_dir, const char * extension );

int Cmd_Argc();
const char * Cmd_Argv( int arg );
char * Cmd_Args();
void Cmd_TokenizeString( const char * text );

void ExecDefaultCfg();

