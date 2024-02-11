#pragma once

#include "qcommon/types.h"

enum CvarFlags : u32 {
	CvarFlag_Archive        = 1 << 0,
	CvarFlag_UserInfo       = 1 << 1,
	CvarFlag_ServerInfo     = 1 << 2,
	CvarFlag_Cheat          = 1 << 3,
	CvarFlag_ReadOnly       = 1 << 4,
	CvarFlag_ServerReadOnly = 1 << 5,
	CvarFlag_Developer      = 1 << 6,
};

constexpr CvarFlags operator|( CvarFlags a, CvarFlags b ) {
	return CvarFlags( u32( a ) | u32( b ) );
}

struct Cvar {
	char * name;
	char * value;
	Span< char > default_value;

	CvarFlags flags;
	bool from_config;
	float number;
	int integer;
	bool modified;
};

// this is set each time a CVAR_USERINFO variable is changed so
// that the client knows to send it to the server
extern bool userinfo_modified;

Cvar * NewCvar( Span< const char > name, Span< const char > value, CvarFlags flags = CvarFlags( 0 ) );

Span< const char > Cvar_String( Span< const char > name );
int Cvar_Integer( Span< const char > name );
float Cvar_Float( Span< const char > name );
bool Cvar_Bool( Span< const char > name );

void Cvar_Set( Span< const char > name, Span< const char > value );
void Cvar_SetInteger( Span< const char > name, int value );
void Cvar_ForceSet( Span< const char > name, Span< const char > value );

bool Cvar_Command( const Tokenized & args );

Span< Span< const char > > TabCompleteCvar( TempAllocator * a, Span< const char > partial );
Span< Span< const char > > SearchCvars( Allocator * a, Span< const char > partial );

bool Cvar_CheatsAllowed();
void ResetCheatCvars();

Span< const char > Cvar_MakeConfig( Allocator * a );

void Cvar_Init();
void Cvar_Shutdown();
const char * Cvar_GetUserInfo();
const char * Cvar_GetServerInfo();
