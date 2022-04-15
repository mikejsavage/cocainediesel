/*
Copyright (C) 2008 Chasseur de bots

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

#include "qcommon/qcommon.h"
#include "qcommon/array.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "qcommon/sort.h"
#include "qcommon/string.h"

struct ConfigEntry {
	char * name;
	char * value;
};

constexpr size_t MAX_CVARS = 1024;

static Cvar cvars[ MAX_CVARS ];
static Hashtable< MAX_CVARS * 2 > cvars_hashtable;

static ConfigEntry config_entries[ MAX_CVARS ];
static Hashtable< MAX_CVARS * 2 > config_entries_hashtable;

bool userinfo_modified;

bool HasFlag( u64 bits, u64 bit ) {
	return ( bits & bit ) != 0;
}

bool Cvar_CheatsAllowed() {
	return Com_ClientState() < CA_CONNECTED || Com_DemoPlaying() || ( Com_ServerState() && Cvar_Bool( "sv_cheats" ) );
}

static bool Cvar_InfoValidate( const char *s, bool name ) {
	size_t max_len = name ? MAX_INFO_KEY : MAX_INFO_VALUE;
	return strlen( s ) < max_len
		&& strchr( s, '\\' ) == NULL
		&& strchr( s, '"' ) == NULL
		&& strchr( s, ';' ) == NULL;
}

static Cvar * FindCvar( const char * name ) {
	u64 hash = CaseHash64( name );
	u64 idx;
	if( !cvars_hashtable.get( hash, &idx ) )
		return NULL;
	return &cvars[ idx ];
}

bool IsCvar( const char * name ) {
	return FindCvar( name ) != NULL;
}

const char * Cvar_String( const char * name ) {
	return FindCvar( name )->value;
}

int Cvar_Integer( const char * name ) {
	return FindCvar( name )->integer;
}

float Cvar_Float( const char * name ) {
	return FindCvar( name )->number;
}

bool Cvar_Bool( const char * name ) {
	return Cvar_Integer( name ) != 0;
}

void SetCvar( Cvar * cvar, const char * value ) {
	cvar->modified = cvar->value == NULL || !StrEqual( value, cvar->value );

	FREE( sys_allocator, cvar->value );
	cvar->value = CopyString( sys_allocator, value );
	cvar->number = SpanToFloat( MakeSpan( cvar->value ), 0.0f );
	cvar->integer = Q_rint( cvar->number );

	if( HasFlag( cvar->flags, CvarFlag_UserInfo ) ) {
		userinfo_modified = true;
	}
}

void Cvar_SetInteger( const char * name, int value ) {
	char buf[ 32 ];
	snprintf( buf, sizeof( buf ), "%d", value );
	Cvar_Set( name, buf );
}

Cvar * NewCvar( const char * name, const char * value, u32 flags ) {
	if( HasFlag( flags, CvarFlag_UserInfo ) || HasFlag( flags, CvarFlag_ServerInfo ) ) {
		assert( Cvar_InfoValidate( name, true ) );
		assert( Cvar_InfoValidate( value, true ) );
	}

	Cvar * old_cvar = FindCvar( name );
	if( old_cvar != NULL ) {
		assert( StrEqual( old_cvar->default_value, value ) );
		assert( ( old_cvar->flags & ~CvarFlag_FromConfig ) == CvarFlags( flags ) );
		return old_cvar;
	}

	assert( cvars_hashtable.size() < ARRAY_COUNT( cvars ) );

	Cvar * cvar = &cvars[ cvars_hashtable.size() ];
	*cvar = { };
	cvar->name = CopyString( sys_allocator, name );
	cvar->default_value = CopyString( sys_allocator, value );
	cvar->flags = CvarFlags( flags );

	u64 hash = CaseHash64( name );
	bool ok = cvars_hashtable.add( hash, cvars_hashtable.size() );
	assert( ok );

	u64 idx;
	if( !HasFlag( flags, CvarFlag_ReadOnly ) && config_entries_hashtable.get( hash, &idx ) ) {
		SetCvar( cvar, config_entries[ idx ].value );
		cvar->flags = CvarFlags( cvar->flags | CvarFlag_FromConfig );
	}
	else {
		SetCvar( cvar, value );
	}

	return cvar;
}

void Cvar_ForceSet( const char * name, const char * value ) {
	Cvar * cvar = FindCvar( name );
	assert( cvar != NULL );
	SetCvar( cvar, value );
}

void Cvar_Set( const char * name, const char * value ) {
	Cvar * cvar = FindCvar( name );
	if( cvar == NULL ) {
		Com_Printf( "No such cvar: %s\n", name );
		return;
	}

	if( HasFlag( cvar->flags, CvarFlag_UserInfo ) || HasFlag( cvar->flags, CvarFlag_ServerInfo ) ) {
		if( !Cvar_InfoValidate( value, false ) ) {
			Com_Printf( "invalid info cvar value\n" );
			return;
		}
	}

	bool read_only = HasFlag( cvar->flags, CvarFlag_ReadOnly ) || ( is_public_build && HasFlag( cvar->flags, CvarFlag_Developer ) );
	if( read_only ) {
		Com_Printf( "%s is write protected.\n", name );
		return;
	}

	if( HasFlag( cvar->flags, CvarFlag_Cheat ) && !StrEqual( value, cvar->default_value ) ) {
		if( !Cvar_CheatsAllowed() ) {
			Com_Printf( "%s is cheat protected.\n", name );
			return;
		}
	}

	if( HasFlag( cvar->flags, CvarFlag_ServerReadOnly ) && Com_ServerState() ) {
		Com_Printf( "Can't change %s while the server is running.\n", cvar->name );
		return;
	}

	SetCvar( cvar, value );
}

void ResetCheatCvars() {
	if( Cvar_CheatsAllowed() ) {
		return;
	}

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		Cvar * cvar = &cvars[ i ];
		if( HasFlag( cvar->flags, CvarFlag_Cheat ) ) {
			SetCvar( cvar, cvar->default_value );
		}
	}
}

Span< const char * > TabCompleteCvar( TempAllocator * a, const char * partial ) {
	NonRAIIDynamicArray< const char * > results( a );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( CaseStartsWith( cvar->name, partial ) ) {
			results.add( cvar->name );
		}
	}

	Sort( results.begin(), results.end(), SortCStringsComparator );

	return results.span();
}

Span< const char * > SearchCvars( Allocator * a, const char * partial ) {
	NonRAIIDynamicArray< const char * > results( a );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( CaseContains( cvar->name, partial ) ) {
			results.add( cvar->name );
		}
	}

	Sort( results.begin(), results.end(), SortCStringsComparator );

	return results.span();
}

/*
* Cvar_Command
*
* Handles variable inspection and changing from the console
*
* Called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
* command.  Returns true if the command was a variable reference that
* was handled. (print or change)
*/
bool Cvar_Command() {
	Cvar * cvar = FindCvar( Cmd_Argv( 0 ) );
	if( cvar == NULL )
		return false;

	if( Cmd_Argc() > 1 ) {
		Cvar_Set( Cmd_Argv( 0 ), Cmd_Argv( 1 ) );
	}
	else {
		Com_Printf( "\"%s\" is \"%s\" default: \"%s\"\n", cvar->name, cvar->value, cvar->default_value );
	}

	return true;
}

static void SetConfigCvar() {
	if( Cmd_Argc() != 3 ) {
		Com_Printf( "usage: set <variable> <value>\n" );
		return;
	}

	u64 hash = CaseHash64( Cmd_Argv( 1 ) );
	u64 idx = config_entries_hashtable.size();
	if( !config_entries_hashtable.get( hash, &idx ) ) {
		if( !config_entries_hashtable.add( hash, idx ) ) {
			Com_Printf( S_COLOR_YELLOW "Too many config entries!" );
			return;
		}

		config_entries[ idx ].name = CopyString( sys_allocator, Cmd_Argv( 1 ) );
		config_entries[ idx ].value = NULL;
	}

	FREE( sys_allocator, config_entries[ idx ].value );
	config_entries[ idx ].value = CopyString( sys_allocator, Cmd_Argv( 2 ) );
}

static void Cvar_Reset_f() {
	if( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: reset <variable>\n" );
		return;
	}

	Cvar * cvar = FindCvar( Cmd_Argv( 1 ) );
	if( cvar == NULL )
		return;
	Cvar_Set( cvar->name, cvar->default_value );
	cvar->flags = CvarFlags( cvar->flags & ~CvarFlag_FromConfig );
}

void Cvar_WriteVariables( DynamicString * config ) {
	DynamicArray< char * > lines( sys_allocator );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( !HasFlag( cvar->flags, CvarFlag_Archive ) )
			continue;
		if( !HasFlag( cvar->flags, CvarFlag_FromConfig ) && StrEqual( cvar->value, cvar->default_value ) )
			continue;
		lines.add( ( *sys_allocator )( "set {} \"{}\"\r\n", cvar->name, cvar->value ) );
	}

	for( size_t i = 0; i < config_entries_hashtable.size(); i++ ) {
		const ConfigEntry * entry = &config_entries[ i ];
		if( IsCvar( entry->name ) )
			continue;
		lines.add( ( *sys_allocator )( "set {} \"{}\"\r\n", entry->name, entry->value ) );
	}

	Sort( lines.begin(), lines.end(), SortCStringsComparator );

	for( char * line : lines ) {
		config->append_raw( line, strlen( line ) );
		FREE( sys_allocator, line );
	}
}

static const char * MakeInfoString( CvarFlags flag ) {
	static char info[ MAX_INFO_STRING ];

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( HasFlag( cvar->flags, flag ) ) {
			Info_SetValueForKey( info, cvar->name, cvar->value );
		}
	}

	return info;
}

const char * Cvar_GetUserInfo() {
	return MakeInfoString( CvarFlag_UserInfo );
}

const char * Cvar_GetServerInfo() {
	return MakeInfoString( CvarFlag_ServerInfo );
}

void Cvar_PreInit() {
	cvars_hashtable.clear();
	config_entries_hashtable.clear();

	AddCommand( "set", SetConfigCvar );
	AddCommand( "seta", SetConfigCvar );
	AddCommand( "setau", SetConfigCvar );
	AddCommand( "setas", SetConfigCvar );
}

void Cvar_Init() {
	RemoveCommand( "set" );
	RemoveCommand( "seta" );
	RemoveCommand( "setau" );
	RemoveCommand( "setas" );

	AddCommand( "reset", Cvar_Reset_f );
}

void Cvar_Shutdown() {
	RemoveCommand( "reset" );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		FREE( sys_allocator, cvars[ i ].name );
		FREE( sys_allocator, cvars[ i ].value );
		FREE( sys_allocator, cvars[ i ].default_value );
	}

	for( size_t i = 0; i < config_entries_hashtable.size(); i++ ) {
		FREE( sys_allocator, config_entries[ i ].name );
		FREE( sys_allocator, config_entries[ i ].value );
	}
}
