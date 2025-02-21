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
#include "qcommon/string.h"

#include "nanosort/nanosort.hpp"

struct ConfigEntry {
	Span< char > name;
	Span< char > value;
};

constexpr size_t MAX_CVARS = 1024;

static Cvar cvars[ MAX_CVARS ];
static Hashtable< MAX_CVARS * 2 > cvars_hashtable;

static ConfigEntry config_entries[ MAX_CVARS ];
static Hashtable< MAX_CVARS * 2 > config_entries_hashtable;

bool userinfo_modified;

bool Cvar_CheatsAllowed() {
	return Com_ClientState() < CA_CONNECTED || CL_DemoPlaying() || ( Com_ServerState() && Cvar_Bool( "sv_cheats" ) );
}

static bool Cvar_InfoValidate( Span< const char > s, bool name ) {
	size_t max_len = name ? MAX_INFO_KEY : MAX_INFO_VALUE;
	return s.n < max_len && StrChr( s, '\\' ) == NULL && StrChr( s, '"' ) == NULL && StrChr( s, ';' ) == NULL;
}

static Cvar * FindCvar( Span< const char > name ) {
	u64 hash = CaseHash64( name );
	u64 idx;
	if( !cvars_hashtable.get( hash, &idx ) )
		return NULL;
	return &cvars[ idx ];
}

Span< const char > Cvar_String( Span< const char > name ) {
	return MakeSpan( FindCvar( name )->value );
}

int Cvar_Integer( Span< const char > name ) {
	return FindCvar( name )->integer;
}

float Cvar_Float( Span< const char > name ) {
	return FindCvar( name )->number;
}

bool Cvar_Bool( Span< const char > name ) {
	return Cvar_Integer( name ) != 0;
}

void SetCvar( Cvar * cvar, Span< const char > value ) {
	if( cvar->value != NULL && StrEqual( value, cvar->value ) ) {
		return;
	}

	Free( sys_allocator, cvar->value );
	cvar->value = ( *sys_allocator )( "{}", value );
	cvar->number = SpanToFloat( MakeSpan( cvar->value ), 0.0f );
	cvar->integer = SpanToInt( MakeSpan( cvar->value ), 0 );
	cvar->modified = true;

	if( HasAllBits( cvar->flags, CvarFlag_UserInfo ) ) {
		userinfo_modified = true;
	}
}

void Cvar_SetInteger( Span< const char > name, int value ) {
	char buf[ 32 ];
	snprintf( buf, sizeof( buf ), "%d", value );
	Cvar_Set( name, MakeSpan( buf ) );
}

Cvar * NewCvar( Span< const char > name, Span< const char > value, CvarFlags flags ) {
	if( HasAnyBit( flags, CvarFlag_UserInfo | CvarFlag_ServerInfo ) ) {
		Assert( Cvar_InfoValidate( name, true ) );
		Assert( Cvar_InfoValidate( value, true ) );
	}

	Cvar * old_cvar = FindCvar( name );
	if( old_cvar != NULL ) {
		Assert( StrEqual( old_cvar->default_value, value ) );
		Assert( old_cvar->flags == flags );
		return old_cvar;
	}

	Assert( cvars_hashtable.size() < ARRAY_COUNT( cvars ) );

	Cvar * cvar = &cvars[ cvars_hashtable.size() ];
	*cvar = { };
	cvar->name = ( *sys_allocator )( "{}", name );
	cvar->default_value = CloneSpan( sys_allocator, value );
	cvar->flags = CvarFlags( flags );

	u64 hash = CaseHash64( name );
	bool ok = cvars_hashtable.add( hash, cvars_hashtable.size() );
	Assert( ok );

	u64 idx;
	if( !HasAllBits( flags, CvarFlag_ReadOnly ) && config_entries_hashtable.get( hash, &idx ) ) {
		SetCvar( cvar, config_entries[ idx ].value );
		cvar->from_config = true;
	}
	else {
		SetCvar( cvar, value );
	}

	return cvar;
}

void Cvar_ForceSet( Span< const char > name, Span< const char > value ) {
	Cvar * cvar = FindCvar( name );
	Assert( cvar != NULL );
	SetCvar( cvar, value );
}

void Cvar_Set( Span< const char > name, Span< const char > value ) {
	Cvar * cvar = FindCvar( name );
	if( cvar == NULL ) {
		Com_GGPrint( "No such cvar: {}", name );
		return;
	}

	if( HasAnyBit( cvar->flags, CvarFlag_UserInfo | CvarFlag_ServerInfo ) ) {
		if( !Cvar_InfoValidate( value, false ) ) {
			Com_Printf( "invalid info cvar value\n" );
			return;
		}
	}

	bool read_only = HasAnyBit( cvar->flags, CvarFlag_ReadOnly ) || ( is_public_build && HasAnyBit( cvar->flags, CvarFlag_Developer ) );
	if( read_only ) {
		Com_GGPrint( "{} is write protected.", name );
		return;
	}

	if( HasAllBits( cvar->flags, CvarFlag_Cheat ) && !StrEqual( value, cvar->default_value ) ) {
		if( !Cvar_CheatsAllowed() ) {
			Com_GGPrint( "{} is cheat protected.", name );
			return;
		}
	}

	if( HasAllBits( cvar->flags, CvarFlag_ServerReadOnly ) && Com_ServerState() ) {
		Com_GGPrint( "Can't change {} while the server is running.", cvar->name );
		return;
	}

	if( HasAllBits( cvar->flags, CvarFlag_LinuxOnly ) && !IFDEF( PLATFORM_LINUX ) ) {
		Com_GGPrint( "{} is a Linux only cvar.", cvar->name );
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
		if( HasAllBits( cvar->flags, CvarFlag_Cheat ) ) {
			SetCvar( cvar, cvar->default_value );
		}
	}
}

Span< Span< const char > > TabCompleteCvar( TempAllocator * a, Span< const char > partial ) {
	NonRAIIDynamicArray< Span< const char > > results( a );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( CaseStartsWith( MakeSpan( cvar->name ), partial ) ) {
			results.add( MakeSpan( cvar->name ) );
		}
	}

	nanosort( results.begin(), results.end(), SortSpanStringsComparator );

	return results.span();
}

Span< Span< const char > > SearchCvars( Allocator * a, Span< const char > partial ) {
	NonRAIIDynamicArray< Span< const char > > results( a );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( CaseContains( MakeSpan( cvar->name ), partial ) ) {
			results.add( MakeSpan( cvar->name ) );
		}
	}

	nanosort( results.begin(), results.end(), SortSpanStringsComparator );

	return results.span();
}

bool Cvar_Command( const Tokenized & args ) {
	Cvar * cvar = FindCvar( args.tokens[ 0 ] );
	if( cvar == NULL )
		return false;

	if( args.tokens.n == 2 ) {
		Cvar_Set( args.tokens[ 0 ], args.tokens[ 1 ] );
	}
	else if( args.tokens.n == 1 ) {
		Com_GGPrint( "\"{}\" is \"{}\" default: \"{}\"", cvar->name, cvar->value, cvar->default_value );
	}
	else {
		Com_Printf( "Usage: <cvar> <value>\n" );
	}

	return true;
}

static void SetConfigCvar( const Tokenized & args ) {
	if( args.tokens.n != 3 ) {
		Com_Printf( "usage: set <variable> <value>\n" );
		return;
	}

	// TODO: if the variable already exists we should set it

	u64 hash = CaseHash64( args.tokens[ 1 ] );
	u64 idx = config_entries_hashtable.size();
	if( !config_entries_hashtable.get( hash, &idx ) ) {
		if( !config_entries_hashtable.add( hash, idx ) ) {
			Com_Printf( S_COLOR_YELLOW "Too many config entries!" );
			return;
		}

		config_entries[ idx ].name = CloneSpan( sys_allocator, args.tokens[ 1 ] );
		config_entries[ idx ].value = { };
	}

	Free( sys_allocator, config_entries[ idx ].value.ptr );
	config_entries[ idx ].value = CloneSpan( sys_allocator, args.tokens[ 2 ] );
}

static void Cvar_Reset_f( const Tokenized & args ) {
	if( args.tokens.n != 2 ) {
		Com_Printf( "usage: reset <variable>\n" );
		return;
	}

	Cvar * cvar = FindCvar( args.tokens[ 1 ] );
	if( cvar == NULL ) {
		Com_GGPrint( "No such cvar: {}", args.tokens[ 1 ] );
		return;
	}
	Cvar_Set( args.tokens[ 1 ], cvar->default_value );
	cvar->from_config = false;
}

Span< const char > Cvar_MakeConfig( Allocator * a ) {
	DynamicArray< Span< char > > lines( a );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( !HasAllBits( cvar->flags, CvarFlag_Archive ) )
			continue;
		if( !cvar->from_config && StrEqual( cvar->value, cvar->default_value ) )
			continue;
		lines.add( a->sv( "set {} \"{}\"\r\n", cvar->name, cvar->value ) );
	}

	for( size_t i = 0; i < config_entries_hashtable.size(); i++ ) {
		const ConfigEntry * entry = &config_entries[ i ];
		if( FindCvar( entry->name ) != NULL )
			continue;
		lines.add( a->sv( "set {} \"{}\"\r\n", entry->name, entry->value ) );
	}

	nanosort( lines.begin(), lines.end(), SortSpanStringsComparator );

	NonRAIIDynamicArray< char > config( a );
	for( Span< char > line : lines ) {
		config.add_many( line );
		Free( a, line.ptr );
	}

	return config.span();
}

static const char * MakeInfoString( CvarFlags flag ) {
	static char info[ MAX_INFO_STRING ];

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		const Cvar * cvar = &cvars[ i ];
		if( HasAllBits( cvar->flags, flag ) ) {
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

void Cvar_Init() {
	cvars_hashtable.clear();
	config_entries_hashtable.clear();

	AddCommand( "set", SetConfigCvar );
	AddCommand( "reset", Cvar_Reset_f );
}

void Cvar_Shutdown() {
	RemoveCommand( "set" );
	RemoveCommand( "reset" );

	for( size_t i = 0; i < cvars_hashtable.size(); i++ ) {
		Free( sys_allocator, cvars[ i ].name );
		Free( sys_allocator, cvars[ i ].value );
		Free( sys_allocator, cvars[ i ].default_value.ptr );
	}

	for( size_t i = 0; i < config_entries_hashtable.size(); i++ ) {
		Free( sys_allocator, config_entries[ i ].name.ptr );
		Free( sys_allocator, config_entries[ i ].value.ptr );
	}
}
